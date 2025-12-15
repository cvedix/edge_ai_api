#include "core/performance_monitor.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

void PerformanceMonitor::recordRequest(const std::string& endpoint,
                                      std::chrono::milliseconds latency,
                                      bool success) {
    // PHASE 2 OPTIMIZATION: Lock only to find/create metrics entry, then release
    EndpointMetrics* metrics_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_ptr = &endpoint_metrics_[endpoint];
    }
    // Lock released - now update atomic counters without lock
    
    // Update counters using atomic operations (no lock needed)
    metrics_ptr->total_requests.fetch_add(1, std::memory_order_relaxed);
    if (success) {
        metrics_ptr->successful_requests.fetch_add(1, std::memory_order_relaxed);
    } else {
        metrics_ptr->failed_requests.fetch_add(1, std::memory_order_relaxed);
    }
    
    // Update latency statistics
    uint64_t latency_ms = latency.count();
    metrics_ptr->total_latency_ms.fetch_add(latency_ms, std::memory_order_relaxed);
    
    // Calculate average (need to read current values)
    uint64_t total = metrics_ptr->total_requests.load(std::memory_order_relaxed);
    uint64_t total_latency = metrics_ptr->total_latency_ms.load(std::memory_order_relaxed);
    if (total > 0) {
        double new_avg = static_cast<double>(total_latency) / total;
        // Use compare-and-swap loop for atomic update
        double current_avg = metrics_ptr->avg_latency_ms.load(std::memory_order_relaxed);
        while (!metrics_ptr->avg_latency_ms.compare_exchange_weak(current_avg, new_avg, 
                                                                 std::memory_order_relaxed)) {
            // Recalculate if value changed
            total = metrics_ptr->total_requests.load(std::memory_order_relaxed);
            total_latency = metrics_ptr->total_latency_ms.load(std::memory_order_relaxed);
            if (total > 0) {
                new_avg = static_cast<double>(total_latency) / total;
            } else {
                break;
            }
        }
    }
    
    // Update min/max using compare-and-swap
    double current_min = metrics_ptr->min_latency_ms.load(std::memory_order_relaxed);
    while (latency_ms < current_min && 
           !metrics_ptr->min_latency_ms.compare_exchange_weak(current_min, static_cast<double>(latency_ms),
                                                              std::memory_order_relaxed)) {
        // Retry if value changed
    }
    
    double current_max = metrics_ptr->max_latency_ms.load(std::memory_order_relaxed);
    while (latency_ms > current_max && 
           !metrics_ptr->max_latency_ms.compare_exchange_weak(current_max, static_cast<double>(latency_ms),
                                                              std::memory_order_relaxed)) {
        // Retry if value changed
    }
}

PerformanceMonitor::EndpointMetricsSnapshot PerformanceMonitor::getEndpointMetrics(const std::string& endpoint) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = endpoint_metrics_.find(endpoint);
    if (it != endpoint_metrics_.end()) {
        const auto& metrics = it->second;
        EndpointMetricsSnapshot snapshot;
        snapshot.total_requests = metrics.total_requests.load();
        snapshot.successful_requests = metrics.successful_requests.load();
        snapshot.failed_requests = metrics.failed_requests.load();
        snapshot.avg_latency_ms = metrics.avg_latency_ms.load();
        snapshot.max_latency_ms = metrics.max_latency_ms.load();
        snapshot.min_latency_ms = metrics.min_latency_ms.load();
        return snapshot;
    }
    
    return EndpointMetricsSnapshot{};
}

Json::Value PerformanceMonitor::getMetricsJSON() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Json::Value root;
    Json::Value endpoints(Json::objectValue);
    
    for (const auto& [endpoint, metrics] : endpoint_metrics_) {
        Json::Value endpoint_data;
        endpoint_data["total_requests"] = static_cast<Json::UInt64>(metrics.total_requests.load());
        endpoint_data["successful_requests"] = static_cast<Json::UInt64>(metrics.successful_requests.load());
        endpoint_data["failed_requests"] = static_cast<Json::UInt64>(metrics.failed_requests.load());
        endpoint_data["avg_latency_ms"] = metrics.avg_latency_ms.load();
        endpoint_data["max_latency_ms"] = metrics.max_latency_ms.load();
        endpoint_data["min_latency_ms"] = metrics.min_latency_ms.load();
        
        endpoints[endpoint] = endpoint_data;
    }
    
    root["endpoints"] = endpoints;
    
    auto overall = getOverallStats();
    Json::Value overall_data;
    overall_data["total_requests"] = static_cast<Json::UInt64>(overall.total_requests);
    overall_data["successful_requests"] = static_cast<Json::UInt64>(overall.successful_requests);
    overall_data["failed_requests"] = static_cast<Json::UInt64>(overall.failed_requests);
    overall_data["avg_latency_ms"] = overall.avg_latency_ms;
    overall_data["throughput_rps"] = overall.throughput_rps;
    
    root["overall"] = overall_data;
    
    return root;
}

std::string PerformanceMonitor::getPrometheusMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    
    for (const auto& [endpoint, metrics] : endpoint_metrics_) {
        // Escape endpoint name for Prometheus
        std::string safe_endpoint = endpoint;
        std::replace(safe_endpoint.begin(), safe_endpoint.end(), '/', '_');
        std::replace(safe_endpoint.begin(), safe_endpoint.end(), '-', '_');
        
        oss << "# HELP http_requests_total Total number of HTTP requests\n";
        oss << "# TYPE http_requests_total counter\n";
        oss << "http_requests_total{endpoint=\"" << endpoint << "\"} " 
            << metrics.total_requests.load() << "\n";
        
        oss << "# HELP http_requests_successful Total number of successful HTTP requests\n";
        oss << "# TYPE http_requests_successful counter\n";
        oss << "http_requests_successful{endpoint=\"" << endpoint << "\"} " 
            << metrics.successful_requests.load() << "\n";
        
        oss << "# HELP http_requests_failed Total number of failed HTTP requests\n";
        oss << "# TYPE http_requests_failed counter\n";
        oss << "http_requests_failed{endpoint=\"" << endpoint << "\"} " 
            << metrics.failed_requests.load() << "\n";
        
        oss << "# HELP http_request_latency_ms HTTP request latency in milliseconds\n";
        oss << "# TYPE http_request_latency_ms histogram\n";
        oss << "http_request_latency_ms{endpoint=\"" << endpoint << "\",quantile=\"0.5\"} " 
            << metrics.avg_latency_ms.load() << "\n";
        oss << "http_request_latency_ms{endpoint=\"" << endpoint << "\",quantile=\"0.95\"} " 
            << metrics.max_latency_ms.load() << "\n";
        oss << "http_request_latency_ms{endpoint=\"" << endpoint << "\",quantile=\"0.99\"} " 
            << metrics.max_latency_ms.load() << "\n";
    }
    
    return oss.str();
}

PerformanceMonitor::OverallStats PerformanceMonitor::getOverallStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    OverallStats stats{};
    double total_latency = 0.0;
    uint64_t total_count = 0;
    
    for (const auto& [endpoint, metrics] : endpoint_metrics_) {
        stats.total_requests += metrics.total_requests.load();
        stats.successful_requests += metrics.successful_requests.load();
        stats.failed_requests += metrics.failed_requests.load();
        
        total_latency += metrics.avg_latency_ms.load() * metrics.total_requests.load();
        total_count += metrics.total_requests.load();
    }
    
    // ✅ Safe division: check total_count before dividing
    if (total_count > 0) {
        stats.avg_latency_ms = total_latency / static_cast<double>(total_count);
    } else {
        stats.avg_latency_ms = 0.0;
    }
    
    stats.throughput_rps = calculateThroughput();
    
    return stats;
}

double PerformanceMonitor::calculateThroughput() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - start_time_).count();
    
    // ✅ Safe division: check elapsed before dividing
    if (elapsed <= 0) {
        return 0.0;
    }
    
    uint64_t total = 0;
    for (const auto& [endpoint, metrics] : endpoint_metrics_) {
        total += metrics.total_requests.load();
    }
    
    return static_cast<double>(total) / static_cast<double>(elapsed);
}

void PerformanceMonitor::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    endpoint_metrics_.clear();
    start_time_ = std::chrono::steady_clock::now();
}

