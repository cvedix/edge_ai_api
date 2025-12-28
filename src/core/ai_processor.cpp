#include "core/ai_processor.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

AIProcessor::AIProcessor()
    : status_(Status::Stopped), should_stop_(false),
      last_fps_calc_time_(std::chrono::steady_clock::now()),
      frame_count_since_last_calc_(0) {
  metrics_.frames_processed = 0;
  metrics_.frames_dropped = 0;
  metrics_.fps = 0.0;
  metrics_.avg_latency_ms = 0.0;
  metrics_.max_latency_ms = 0.0;
  metrics_.memory_usage_mb = 0;
  metrics_.error_count = 0;
  metrics_.status = Status::Stopped;
}

AIProcessor::~AIProcessor() { stop(true); }

bool AIProcessor::start(const std::string &config, ResultCallback callback) {
  if (status_.load() != Status::Stopped) {
    std::cerr << "[AIProcessor] Already running or starting" << std::endl;
    return false;
  }

  config_ = config;
  result_callback_ = callback;
  status_.store(Status::Starting);
  should_stop_.store(false);

  // Initialize SDK
  if (!initializeSDK(config)) {
    status_.store(Status::Error);
    return false;
  }

  // Start processing thread
  processing_thread_ =
      std::make_unique<std::thread>(&AIProcessor::processingLoop, this);

  // Wait a bit to ensure thread started
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  if (status_.load() == Status::Running) {
    std::cout << "[AIProcessor] Started successfully" << std::endl;
    return true;
  } else {
    std::cerr << "[AIProcessor] Failed to start" << std::endl;
    return false;
  }
}

void AIProcessor::stop(bool wait) {
  if (status_.load() == Status::Stopped) {
    return;
  }

  std::cout << "[AIProcessor] Stopping..." << std::endl;
  status_.store(Status::Stopping);
  should_stop_.store(true);

  if (processing_thread_ && processing_thread_->joinable()) {
    if (wait) {
      processing_thread_->join();
    } else {
      processing_thread_->detach();
    }
  }

  cleanupSDK();
  status_.store(Status::Stopped);
  std::cout << "[AIProcessor] Stopped" << std::endl;
}

void AIProcessor::processingLoop() {
  std::cout << "[AIProcessor] Processing thread started" << std::endl;

  status_.store(Status::Running);
  last_fps_calc_time_ = std::chrono::steady_clock::now();
  frame_count_since_last_calc_ = 0;

  while (!should_stop_.load() && status_.load() == Status::Running) {
    try {
      auto frame_start = std::chrono::steady_clock::now();

      // Process frame
      processFrame();

      auto frame_end = std::chrono::steady_clock::now();
      auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                         frame_end - frame_start)
                         .count();

      // Update metrics
      {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.frames_processed++;
        metrics_.last_frame_time = frame_end;

        // Update latency
        if (metrics_.frames_processed > 0) {
          metrics_.avg_latency_ms =
              (metrics_.avg_latency_ms * (metrics_.frames_processed - 1) +
               latency) /
              metrics_.frames_processed;
        }

        if (latency > metrics_.max_latency_ms) {
          metrics_.max_latency_ms = latency;
        }

        // Calculate FPS
        frame_count_since_last_calc_++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                           now - last_fps_calc_time_)
                           .count();

        if (elapsed >= 1) {
          double fps =
              static_cast<double>(frame_count_since_last_calc_) / elapsed;
          metrics_.fps = std::round(fps);
          frame_count_since_last_calc_ = 0;
          last_fps_calc_time_ = now;
        }
      }

      // Small sleep to prevent 100% CPU usage
      // Adjust based on your frame rate requirements
      std::this_thread::sleep_for(std::chrono::milliseconds(1));

    } catch (const std::exception &e) {
      {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = e.what();
      }
      {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.error_count++;
      }
      std::cerr << "[AIProcessor] Error in processing loop: " << e.what()
                << std::endl;

      // Continue processing despite error
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  std::cout << "[AIProcessor] Processing thread stopped" << std::endl;
}

void AIProcessor::processFrame() {
  // Override this in derived class or use callback
  // For now, just a placeholder
  // In real implementation, this would call SDK processing

  // Example:
  // if (result_callback_) {
  //     std::string result = sdk->process();
  //     result_callback_(result);
  // }
}

bool AIProcessor::initializeSDK(const std::string & /*config*/) {
  // Override this in derived class
  // Initialize your AI SDK here
  // Return true if successful

  // Example:
  // try {
  //     sdk_ = std::make_unique<YourSDK>(config);
  //     return sdk_->initialize();
  // } catch (...) {
  //     return false;
  // }

  return true; // Placeholder
}

void AIProcessor::cleanupSDK() {
  // Override this in derived class
  // Cleanup your AI SDK here

  // Example:
  // if (sdk_) {
  //     sdk_->cleanup();
  //     sdk_.reset();
  // }
}

AIProcessor::Metrics AIProcessor::getMetrics() const {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  Metrics m = metrics_;
  m.status = status_.load();
  return m;
}

std::string AIProcessor::getLastError() const {
  std::lock_guard<std::mutex> lock(error_mutex_);
  return last_error_;
}

bool AIProcessor::isHealthy(uint64_t max_latency_ms, double min_fps) const {
  auto metrics = getMetrics();

  if (metrics.status != Status::Running) {
    return false;
  }

  if (metrics.avg_latency_ms > max_latency_ms) {
    return false;
  }

  if (metrics.fps < min_fps) {
    return false;
  }

  return true;
}
