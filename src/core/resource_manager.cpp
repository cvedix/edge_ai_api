#include "core/resource_manager.h"
#include <iostream>
#include <algorithm>
#include <cmath>

void ResourceManager::initialize(size_t max_concurrent) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_concurrent_per_device_ = max_concurrent;
    detectGPUs();
}

void ResourceManager::detectGPUs() {
    // Placeholder for GPU detection
    // In real implementation, this would use CUDA, OpenCL, or vendor-specific APIs
    
    // Example: Detect NVIDIA GPUs via nvidia-smi or CUDA
    // For now, create a dummy GPU for testing
    #ifdef ENABLE_GPU_DETECTION
    // Real GPU detection code would go here
    #else
    // Dummy GPU for development
    auto gpu = std::make_shared<GPUInfo>();
    gpu->device_id = 0;
    gpu->name = "Dummy GPU";
    gpu->total_memory_mb = 8192; // 8GB
    gpu->free_memory_mb = 8192;
    gpu->used_memory_mb = 0;
    gpu->utilization_percent = 0.0;
    gpus_.push_back(gpu);
    #endif
}

std::shared_ptr<ResourceManager::Allocation> ResourceManager::allocateGPU(size_t memory_mb, int preferred_device) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Find best GPU
    std::shared_ptr<GPUInfo> selected_gpu;
    
    if (preferred_device >= 0 && preferred_device < static_cast<int>(gpus_.size())) {
        selected_gpu = gpus_[preferred_device];
    } else {
        selected_gpu = getBestGPU();
    }
    
    if (!selected_gpu || selected_gpu->free_memory_mb < memory_mb) {
        return nullptr;
    }
    
    // Check concurrent allocation limit
    size_t current_allocations = 0;
    for (const auto& alloc : allocations_) {
        if (alloc.second->device_id == selected_gpu->device_id) {
            current_allocations++;
        }
    }
    
    if (current_allocations >= max_concurrent_per_device_) {
        return nullptr;
    }
    
    // Create allocation
    auto allocation = std::make_shared<Allocation>();
    allocation->device_id = selected_gpu->device_id;
    allocation->memory_mb = memory_mb;
    allocation->resource_id = "gpu_" + std::to_string(allocation_counter_++);
    allocation->allocated_at = std::chrono::steady_clock::now();
    
    // Update GPU stats
    selected_gpu->free_memory_mb -= memory_mb;
    selected_gpu->used_memory_mb += memory_mb;
    selected_gpu->in_use = true;
    selected_gpu->last_used = std::chrono::steady_clock::now();
    updateGPUStats(selected_gpu->device_id);
    
    allocations_[allocation->resource_id] = allocation;
    
    return allocation;
}

void ResourceManager::releaseGPU(std::shared_ptr<Allocation> allocation) {
    if (!allocation) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = allocations_.find(allocation->resource_id);
    if (it == allocations_.end()) {
        return;
    }
    
    // Find GPU and update stats
    for (auto& gpu : gpus_) {
        if (gpu->device_id == allocation->device_id) {
            gpu->free_memory_mb += allocation->memory_mb;
            gpu->used_memory_mb -= allocation->memory_mb;
            updateGPUStats(gpu->device_id);
            
            // Check if GPU is still in use
            bool still_in_use = false;
            for (const auto& alloc : allocations_) {
                if (alloc.second->device_id == gpu->device_id && 
                    alloc.first != allocation->resource_id) {
                    still_in_use = true;
                    break;
                }
            }
            gpu->in_use = still_in_use;
            break;
        }
    }
    
    allocations_.erase(it);
}

std::shared_ptr<ResourceManager::GPUInfo> ResourceManager::getGPUInfo(int device_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& gpu : gpus_) {
        if (gpu->device_id == device_id) {
            return gpu;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<ResourceManager::GPUInfo>> ResourceManager::getAllGPUs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return gpus_;
}

std::shared_ptr<ResourceManager::GPUInfo> ResourceManager::getBestGPU() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (gpus_.empty()) {
        return nullptr;
    }
    
    // Find GPU with lowest utilization and enough free memory
    std::shared_ptr<GPUInfo> best = nullptr;
    double best_score = std::numeric_limits<double>::max();
    
    for (const auto& gpu : gpus_) {
        if (gpu->free_memory_mb > 0) {
            // Score based on utilization and free memory
            double score = gpu->utilization_percent - 
                          (static_cast<double>(gpu->free_memory_mb) / gpu->total_memory_mb) * 100.0;
            
            if (score < best_score) {
                best_score = score;
                best = gpu;
            }
        }
    }
    
    return best ? best : gpus_[0];
}

void ResourceManager::updateGPUStats(int device_id) {
    // In real implementation, this would query GPU stats
    // For now, calculate based on memory usage
    for (auto& gpu : gpus_) {
        if (gpu->device_id == device_id) {
            if (gpu->total_memory_mb > 0) {
                gpu->utilization_percent = 
                    (static_cast<double>(gpu->used_memory_mb) / gpu->total_memory_mb) * 100.0;
            }
            break;
        }
    }
}

ResourceManager::Stats ResourceManager::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Stats stats;
    stats.total_gpus = gpus_.size();
    stats.total_allocations = allocations_.size();
    stats.total_memory_mb = 0;
    stats.used_memory_mb = 0;
    
    for (const auto& gpu : gpus_) {
        if (!gpu->in_use) {
            stats.available_gpus++;
        }
        stats.total_memory_mb += gpu->total_memory_mb;
        stats.used_memory_mb += gpu->used_memory_mb;
    }
    
    return stats;
}

