/**
 * @file performance_profiler.h
 * @brief Performance Profiler để xác định bottleneck trong hệ thống
 *
 * Profiler này đo lường:
 * 1. CPU usage per thread
 * 2. Lock contention (mutex wait times)
 * 3. Memory allocations/copies
 * 4. I/O operations (RTSP/RTMP)
 * 5. Frame processing times
 */

#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace PerformanceProfiler {

/**
 * @brief Đo thời gian lock contention
 */
class LockProfiler {
public:
  struct LockStats {
    std::atomic<uint64_t> total_waits{0};
    std::atomic<uint64_t> total_wait_time_us{0};
    std::atomic<uint64_t> max_wait_time_us{0};
    std::atomic<uint64_t> contention_count{0}; // Số lần phải chờ
  };

  class ScopedLock {
  public:
    ScopedLock(LockProfiler &profiler, const std::string &lock_name,
               std::mutex &mtx)
        : profiler_(profiler), lock_name_(lock_name), mutex_(mtx) {
      wait_start_ = std::chrono::steady_clock::now();
      mutex_.lock();
      wait_end_ = std::chrono::steady_clock::now();

      auto wait_time = std::chrono::duration_cast<std::chrono::microseconds>(
                           wait_end_ - wait_start_)
                           .count();

      if (wait_time > 0) {
        profiler_.recordWait(lock_name_, wait_time);
      }

      hold_start_ = std::chrono::steady_clock::now();
    }

    ~ScopedLock() {
      auto hold_end = std::chrono::steady_clock::now();
      auto hold_time = std::chrono::duration_cast<std::chrono::microseconds>(
                           hold_end - hold_start_)
                           .count();
      profiler_.recordHold(lock_name_, hold_time);
      mutex_.unlock();
    }

  private:
    LockProfiler &profiler_;
    std::string lock_name_;
    std::mutex &mutex_;
    std::chrono::steady_clock::time_point wait_start_;
    std::chrono::steady_clock::time_point wait_end_;
    std::chrono::steady_clock::time_point hold_start_;
  };

  void recordWait(const std::string &lock_name, uint64_t wait_time_us) {
    auto &stats = getStats(lock_name);
    stats.total_waits.fetch_add(1);
    stats.total_wait_time_us.fetch_add(wait_time_us);
    stats.contention_count.fetch_add(1);

    uint64_t current_max = stats.max_wait_time_us.load();
    while (wait_time_us > current_max &&
           !stats.max_wait_time_us.compare_exchange_weak(current_max,
                                                         wait_time_us)) {
      // Retry
    }
  }

  void recordHold(const std::string &lock_name, uint64_t hold_time_us) {
    // Track how long locks are held
    (void)lock_name;
    (void)hold_time_us;
  }

  LockStats getStats(const std::string &lock_name) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_[lock_name];
  }

  std::unordered_map<std::string, LockStats> getAllStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
  }

private:
  mutable std::mutex stats_mutex_;
  mutable std::unordered_map<std::string, LockStats> stats_;

  LockStats &getStats(const std::string &lock_name) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_[lock_name];
  }
};

/**
 * @brief Đo memory allocations và copies
 */
class MemoryProfiler {
public:
  struct MemoryStats {
    std::atomic<uint64_t> total_allocations{0};
    std::atomic<uint64_t> total_bytes_allocated{0};
    std::atomic<uint64_t> total_copies{0};
    std::atomic<uint64_t> total_bytes_copied{0};
    std::atomic<uint64_t> peak_memory_bytes{0};
  };

  void recordAllocation(const std::string &tag, size_t bytes) {
    auto &stats = getStats(tag);
    stats.total_allocations.fetch_add(1);
    stats.total_bytes_allocated.fetch_add(bytes);

    // Update peak
    uint64_t current_peak = stats.peak_memory_bytes.load();
    while (
        bytes > current_peak &&
        !stats.peak_memory_bytes.compare_exchange_weak(current_peak, bytes)) {
      // Retry
    }
  }

  void recordCopy(const std::string &tag, size_t bytes) {
    auto &stats = getStats(tag);
    stats.total_copies.fetch_add(1);
    stats.total_bytes_copied.fetch_add(bytes);
  }

  MemoryStats getStats(const std::string &tag) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = stats_.find(tag);
    if (it != stats_.end()) {
      return it->second;
    }
    return MemoryStats{};
  }

  std::unordered_map<std::string, MemoryStats> getAllStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
  }

private:
  mutable std::mutex stats_mutex_;
  mutable std::unordered_map<std::string, MemoryStats> stats_;

  MemoryStats &getStats(const std::string &tag) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_[tag];
  }
};

/**
 * @brief Đo frame processing times
 */
class FrameProfiler {
public:
  struct FrameStats {
    std::atomic<uint64_t> frames_processed{0};
    std::atomic<uint64_t> total_processing_time_us{0};
    std::atomic<uint64_t> max_processing_time_us{0};
    std::atomic<uint64_t> min_processing_time_us{UINT64_MAX};
    std::atomic<uint64_t> dropped_frames{0};
  };

  class ScopedFrame {
  public:
    ScopedFrame(FrameProfiler &profiler, const std::string &stage_name)
        : profiler_(profiler), stage_name_(stage_name) {
      start_time_ = std::chrono::steady_clock::now();
    }

    ~ScopedFrame() {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                          end_time - start_time_)
                          .count();
      profiler_.recordFrame(stage_name_, duration);
    }

  private:
    FrameProfiler &profiler_;
    std::string stage_name_;
    std::chrono::steady_clock::time_point start_time_;
  };

  void recordFrame(const std::string &stage_name, uint64_t processing_time_us) {
    auto &stats = getStats(stage_name);
    stats.frames_processed.fetch_add(1);
    stats.total_processing_time_us.fetch_add(processing_time_us);

    // Update max
    uint64_t current_max = stats.max_processing_time_us.load();
    while (processing_time_us > current_max &&
           !stats.max_processing_time_us.compare_exchange_weak(
               current_max, processing_time_us)) {
      // Retry
    }

    // Update min
    uint64_t current_min = stats.min_processing_time_us.load();
    while (processing_time_us < current_min &&
           !stats.min_processing_time_us.compare_exchange_weak(
               current_min, processing_time_us)) {
      // Retry
    }
  }

  void recordDroppedFrame(const std::string &stage_name) {
    auto &stats = getStats(stage_name);
    stats.dropped_frames.fetch_add(1);
  }

  FrameStats getStats(const std::string &stage_name) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = stats_.find(stage_name);
    if (it != stats_.end()) {
      return it->second;
    }
    return FrameStats{};
  }

  std::unordered_map<std::string, FrameStats> getAllStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
  }

private:
  mutable std::mutex stats_mutex_;
  mutable std::unordered_map<std::string, FrameStats> stats_;

  FrameStats &getStats(const std::string &stage_name) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_[stage_name];
  }
};

/**
 * @brief Đo I/O operations (RTSP/RTMP)
 */
class IOProfiler {
public:
  struct IOStats {
    std::atomic<uint64_t> total_operations{0};
    std::atomic<uint64_t> total_time_us{0};
    std::atomic<uint64_t> max_time_us{0};
    std::atomic<uint64_t> bytes_transferred{0};
    std::atomic<uint64_t> errors{0};
  };

  class ScopedIO {
  public:
    ScopedIO(IOProfiler &profiler, const std::string &io_name)
        : profiler_(profiler), io_name_(io_name) {
      start_time_ = std::chrono::steady_clock::now();
    }

    ~ScopedIO() {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                          end_time - start_time_)
                          .count();
      profiler_.recordIO(io_name_, duration, 0, false);
    }

    void setBytes(size_t bytes) { bytes_ = bytes; }
    void setError(bool error) { error_ = error; }

  private:
    IOProfiler &profiler_;
    std::string io_name_;
    std::chrono::steady_clock::time_point start_time_;
    size_t bytes_{0};
    bool error_{false};
  };

  void recordIO(const std::string &io_name, uint64_t time_us, size_t bytes,
                bool error) {
    auto &stats = getStats(io_name);
    stats.total_operations.fetch_add(1);
    stats.total_time_us.fetch_add(time_us);
    stats.bytes_transferred.fetch_add(bytes);
    if (error) {
      stats.errors.fetch_add(1);
    }

    // Update max
    uint64_t current_max = stats.max_time_us.load();
    while (time_us > current_max &&
           !stats.max_time_us.compare_exchange_weak(current_max, time_us)) {
      // Retry
    }
  }

  IOStats getStats(const std::string &io_name) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = stats_.find(io_name);
    if (it != stats_.end()) {
      return it->second;
    }
    return IOStats{};
  }

  std::unordered_map<std::string, IOStats> getAllStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
  }

private:
  mutable std::mutex stats_mutex_;
  mutable std::unordered_map<std::string, IOStats> stats_;

  IOStats &getStats(const std::string &io_name) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_[io_name];
  }
};

/**
 * @brief CPU usage profiler per thread
 */
class CPUProfiler {
public:
  struct CPUStats {
    std::atomic<double> cpu_percent{0.0};
    std::atomic<uint64_t> total_time_us{0};
    std::atomic<uint64_t> idle_time_us{0};
  };

  void startMonitoring() {
    if (monitoring_) {
      return;
    }
    monitoring_ = true;
    monitor_thread_ = std::thread([this]() { this->monitorLoop(); });
  }

  void stopMonitoring() {
    monitoring_ = false;
    if (monitor_thread_.joinable()) {
      monitor_thread_.join();
    }
  }

  CPUStats getStats(const std::string &thread_name) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = stats_.find(thread_name);
    if (it != stats_.end()) {
      return it->second;
    }
    return CPUStats{};
  }

  std::unordered_map<std::string, CPUStats> getAllStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
  }

private:
  void monitorLoop() {
    while (monitoring_) {
      // Read /proc/self/stat to get CPU usage
      // This is a simplified version - in production, use proper CPU monitoring
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  std::atomic<bool> monitoring_{false};
  std::thread monitor_thread_;
  mutable std::mutex stats_mutex_;
  mutable std::unordered_map<std::string, CPUStats> stats_;
};

/**
 * @brief Main profiler class - tổng hợp tất cả profilers
 */
class PerformanceProfiler {
public:
  static PerformanceProfiler &getInstance() {
    static PerformanceProfiler instance;
    return instance;
  }

  LockProfiler &getLockProfiler() { return lock_profiler_; }
  MemoryProfiler &getMemoryProfiler() { return memory_profiler_; }
  FrameProfiler &getFrameProfiler() { return frame_profiler_; }
  IOProfiler &getIOProfiler() { return io_profiler_; }
  CPUProfiler &getCPUProfiler() { return cpu_profiler_; }

  void start() { cpu_profiler_.startMonitoring(); }

  void stop() { cpu_profiler_.stopMonitoring(); }

  /**
   * @brief Generate báo cáo bottleneck
   */
  std::string generateReport() const {
    std::ostringstream oss;
    oss << "========================================\n";
    oss << "PERFORMANCE BOTTLENECK ANALYSIS REPORT\n";
    oss << "========================================\n\n";

    // 1. Lock Contention Analysis
    oss << "1. LOCK CONTENTION ANALYSIS\n";
    oss << "----------------------------\n";
    auto lock_stats = lock_profiler_.getAllStats();
    if (lock_stats.empty()) {
      oss << "No lock contention data collected.\n";
    } else {
      for (const auto &[name, stats] : lock_stats) {
        uint64_t total_waits = stats.total_waits.load();
        uint64_t total_wait_time = stats.total_wait_time_us.load();
        uint64_t max_wait = stats.max_wait_time_us.load();
        uint64_t contention = stats.contention_count.load();

        if (total_waits > 0) {
          double avg_wait_ms = total_wait_time / 1000.0 / total_waits;
          double max_wait_ms = max_wait / 1000.0;

          oss << "  Lock: " << name << "\n";
          oss << "    Total waits: " << total_waits << "\n";
          oss << "    Avg wait time: " << std::fixed << std::setprecision(2)
              << avg_wait_ms << " ms\n";
          oss << "    Max wait time: " << max_wait_ms << " ms\n";
          oss << "    Contention count: " << contention << "\n";

          if (avg_wait_ms > 1.0 || max_wait_ms > 10.0) {
            oss << "    ⚠️  BOTTLENECK: High lock contention detected!\n";
          }
          oss << "\n";
        }
      }
    }

    // 2. Memory Analysis
    oss << "2. MEMORY ALLOCATION/COPY ANALYSIS\n";
    oss << "-----------------------------------\n";
    auto mem_stats = memory_profiler_.getAllStats();
    if (mem_stats.empty()) {
      oss << "No memory data collected.\n";
    } else {
      for (const auto &[tag, stats] : mem_stats) {
        uint64_t total_allocs = stats.total_allocations.load();
        uint64_t total_bytes = stats.total_bytes_allocated.load();
        uint64_t total_copies = stats.total_copies.load();
        uint64_t total_bytes_copied = stats.total_bytes_copied.load();

        if (total_allocs > 0 || total_copies > 0) {
          oss << "  Tag: " << tag << "\n";
          if (total_allocs > 0) {
            oss << "    Allocations: " << total_allocs << " ("
                << (total_bytes / 1024.0 / 1024.0) << " MB)\n";
          }
          if (total_copies > 0) {
            oss << "    Copies: " << total_copies << " ("
                << (total_bytes_copied / 1024.0 / 1024.0) << " MB)\n";
            if (total_bytes_copied > 100 * 1024 * 1024) { // > 100MB
              oss << "    ⚠️  BOTTLENECK: Excessive memory copying detected!\n";
            }
          }
          oss << "\n";
        }
      }
    }

    // 3. Frame Processing Analysis
    oss << "3. FRAME PROCESSING ANALYSIS\n";
    oss << "---------------------------\n";
    auto frame_stats = frame_profiler_.getAllStats();
    if (frame_stats.empty()) {
      oss << "No frame processing data collected.\n";
    } else {
      for (const auto &[stage, stats] : frame_stats) {
        uint64_t frames = stats.frames_processed.load();
        uint64_t total_time = stats.total_processing_time_us.load();
        uint64_t max_time = stats.max_processing_time_us.load();
        uint64_t min_time = stats.min_processing_time_us.load();
        uint64_t dropped = stats.dropped_frames.load();

        if (frames > 0) {
          double avg_time_ms = total_time / 1000.0 / frames;
          double max_time_ms = max_time / 1000.0;
          double min_time_ms = (min_time == UINT64_MAX) ? 0 : min_time / 1000.0;
          double fps = (avg_time_ms > 0) ? 1000.0 / avg_time_ms : 0;

          oss << "  Stage: " << stage << "\n";
          oss << "    Frames processed: " << frames << "\n";
          oss << "    Avg processing time: " << std::fixed
              << std::setprecision(2) << avg_time_ms << " ms (" << fps
              << " FPS)\n";
          oss << "    Min/Max: " << min_time_ms << " / " << max_time_ms
              << " ms\n";
          oss << "    Dropped frames: " << dropped << "\n";

          if (avg_time_ms > 33.0) { // > 30 FPS threshold
            oss << "    ⚠️  BOTTLENECK: Slow frame processing (target: < 33ms "
                   "for 30 FPS)\n";
          }
          if (dropped > frames * 0.1) { // > 10% dropped
            oss << "    ⚠️  BOTTLENECK: High frame drop rate!\n";
          }
          oss << "\n";
        }
      }
    }

    // 4. I/O Analysis
    oss << "4. I/O OPERATIONS ANALYSIS\n";
    oss << "--------------------------\n";
    auto io_stats = io_profiler_.getAllStats();
    if (io_stats.empty()) {
      oss << "No I/O data collected.\n";
    } else {
      for (const auto &[io_name, stats] : io_stats) {
        uint64_t ops = stats.total_operations.load();
        uint64_t total_time = stats.total_time_us.load();
        uint64_t max_time = stats.max_time_us.load();
        uint64_t bytes = stats.bytes_transferred.load();
        uint64_t errors = stats.errors.load();

        if (ops > 0) {
          double avg_time_ms = total_time / 1000.0 / ops;
          double max_time_ms = max_time / 1000.0;
          double throughput_mbps =
              (bytes * 8.0) / (total_time / 1000000.0) / 1000000.0;

          oss << "  I/O: " << io_name << "\n";
          oss << "    Operations: " << ops << "\n";
          oss << "    Avg time: " << std::fixed << std::setprecision(2)
              << avg_time_ms << " ms\n";
          oss << "    Max time: " << max_time_ms << " ms\n";
          oss << "    Throughput: " << throughput_mbps << " Mbps\n";
          oss << "    Errors: " << errors << "\n";

          if (avg_time_ms > 100.0) {
            oss << "    ⚠️  BOTTLENECK: Slow I/O operations detected!\n";
          }
          if (errors > ops * 0.05) { // > 5% error rate
            oss << "    ⚠️  BOTTLENECK: High I/O error rate!\n";
          }
          oss << "\n";
        }
      }
    }

    oss << "========================================\n";
    oss << "END OF REPORT\n";
    oss << "========================================\n";

    return oss.str();
  }

  void saveReportToFile(const std::string &filename) const {
    std::ofstream file(filename);
    if (file.is_open()) {
      file << generateReport();
      file.close();
    }
  }

private:
  LockProfiler lock_profiler_;
  MemoryProfiler memory_profiler_;
  FrameProfiler frame_profiler_;
  IOProfiler io_profiler_;
  CPUProfiler cpu_profiler_;
};

} // namespace PerformanceProfiler
