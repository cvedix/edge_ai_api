#include "worker/config_file_watcher.h"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <poll.h>
#include <sys/inotify.h>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem;

namespace worker {

ConfigFileWatcher::ConfigFileWatcher(const std::string &configPath,
                                     ConfigChangeCallback callback)
    : config_path_(configPath), callback_(callback), running_(false),
      should_stop_(false), inotify_fd_(-1), watch_descriptor_(-1) {

  // Extract directory and filename
  fs::path path(configPath);
  config_dir_ = path.parent_path().string();
  config_filename_ = path.filename().string();

  if (config_dir_.empty()) {
    config_dir_ = ".";
  }

  // Ensure directory exists
  if (!fs::exists(config_dir_)) {
    fs::create_directories(config_dir_);
  }
}

ConfigFileWatcher::~ConfigFileWatcher() {
  stop();

  // Cleanup inotify
  if (watch_descriptor_ >= 0 && inotify_fd_ >= 0) {
    inotify_rm_watch(inotify_fd_, watch_descriptor_);
  }
  if (inotify_fd_ >= 0) {
    close(inotify_fd_);
  }
}

void ConfigFileWatcher::start() {
  if (running_) {
    return;
  }

  running_ = true;
  should_stop_ = false;
  watch_thread_ = std::thread(&ConfigFileWatcher::watchLoop, this);

  std::cout << "[ConfigFileWatcher] Started watching config file: "
            << config_path_ << std::endl;
}

void ConfigFileWatcher::stop() {
  if (!running_) {
    return;
  }

  should_stop_ = true;
  if (watch_thread_.joinable()) {
    watch_thread_.join();
  }
  running_ = false;

  std::cout << "[ConfigFileWatcher] Stopped watching config file: "
            << config_path_ << std::endl;
}

bool ConfigFileWatcher::isFileStable(const std::string &filePath) {
  if (!fs::exists(filePath)) {
    return false;
  }

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now - last_change_time_)
                     .count();

  // Check if file modification time has changed
  std::string currentModified = getLastModifiedTime(filePath);
  if (currentModified != last_modified_time_) {
    last_modified_time_ = currentModified;
    last_change_time_ = now;
    return false; // File is still changing
  }

  // File hasn't changed for at least STABILITY_CHECK_MS
  return elapsed >= STABILITY_CHECK_MS;
}

std::string
ConfigFileWatcher::getLastModifiedTime(const std::string &filePath) {
  try {
    if (fs::exists(filePath)) {
      auto ftime = fs::last_write_time(filePath);
      auto sctp =
          std::chrono::time_point_cast<std::chrono::system_clock::duration>(
              ftime - fs::file_time_type::clock::now() +
              std::chrono::system_clock::now());
      auto time_t = std::chrono::system_clock::to_time_t(sctp);
      return std::to_string(time_t);
    }
  } catch (...) {
    // Ignore errors
  }
  return "";
}

void ConfigFileWatcher::onConfigChanged() {
  if (callback_) {
    std::cout << "[ConfigFileWatcher] Config file changed: " << config_path_
              << ", triggering reload..." << std::endl;
    callback_(config_path_);
  }
}

void ConfigFileWatcher::watchLoop() {
  // Try to use inotify first (more efficient)
  inotify_fd_ = inotify_init1(IN_NONBLOCK);
  if (inotify_fd_ >= 0) {
    watch_descriptor_ =
        inotify_add_watch(inotify_fd_, config_dir_.c_str(),
                          IN_CLOSE_WRITE | IN_MOVED_TO | IN_MODIFY);
    if (watch_descriptor_ >= 0) {
      std::cout
          << "[ConfigFileWatcher] Using inotify for efficient file monitoring"
          << std::endl;
      watchLoopInotify();
      return;
    } else {
      // inotify_add_watch failed, close fd and fallback to polling
      close(inotify_fd_);
      inotify_fd_ = -1;
    }
  }

  // Fallback to polling if inotify not available
  std::cout
      << "[ConfigFileWatcher] Falling back to polling mode (less efficient)"
      << std::endl;
  watchLoopPolling();
}

void ConfigFileWatcher::watchLoopInotify() {
  const size_t BUF_LEN = 1024 * (sizeof(struct inotify_event) + NAME_MAX + 1);
  char buffer[BUF_LEN];

  struct pollfd pfd;
  pfd.fd = inotify_fd_;
  pfd.events = POLLIN;

  // Initialize last modified time
  last_modified_time_ = getLastModifiedTime(config_path_);
  last_change_time_ = std::chrono::steady_clock::now();

  while (!should_stop_) {
    // Poll with timeout (500ms) to allow checking should_stop_
    int pollResult = poll(&pfd, 1, 500);

    if (pollResult < 0) {
      if (errno == EINTR) {
        continue; // Interrupted by signal, continue
      }
      std::cerr << "[ConfigFileWatcher] Poll error: " << strerror(errno)
                << std::endl;
      break;
    }

    if (pollResult == 0) {
      // Timeout, check should_stop_ and continue
      continue;
    }

    if (pfd.revents & POLLIN) {
      ssize_t length = read(inotify_fd_, buffer, BUF_LEN);
      if (length < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          continue; // No data available
        }
        std::cerr << "[ConfigFileWatcher] Read error: " << strerror(errno)
                  << std::endl;
        break;
      }

      // Process inotify events
      int i = 0;
      bool configFileChanged = false;
      while (i < length) {
        struct inotify_event *event =
            reinterpret_cast<struct inotify_event *>(&buffer[i]);

        if (event->len > 0) {
          std::string fileName(event->name);
          if (fileName == config_filename_) {
            // Check if file was modified or closed after writing
            if (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO | IN_MODIFY)) {
              configFileChanged = true;
            }
          }
        }

        i += sizeof(struct inotify_event) + event->len;
      }

      // Wait for file to stabilize before processing
      if (configFileChanged) {
        // Wait a bit for file to stabilize
        std::this_thread::sleep_for(
            std::chrono::milliseconds(STABILITY_CHECK_MS));

        if (isFileStable(config_path_)) {
          onConfigChanged();
        }
      }
    }
  }
}

void ConfigFileWatcher::watchLoopPolling() {
  // Initialize last modified time
  last_modified_time_ = getLastModifiedTime(config_path_);
  last_change_time_ = std::chrono::steady_clock::now();

  while (!should_stop_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (should_stop_) {
      break;
    }

    // Check if file exists and has changed
    if (fs::exists(config_path_)) {
      std::string currentModified = getLastModifiedTime(config_path_);
      if (currentModified != last_modified_time_) {
        // File changed, wait for it to stabilize
        std::this_thread::sleep_for(
            std::chrono::milliseconds(STABILITY_CHECK_MS));

        if (isFileStable(config_path_)) {
          onConfigChanged();
        }
      }
    }
  }
}

} // namespace worker
