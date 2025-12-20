#include "utils/mp4_directory_watcher.h"
#include "utils/mp4_finalizer.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <sys/inotify.h>
#include <unistd.h>
#include <limits.h>
#include <poll.h>
#include <cstring> // For strerror
#include <cerrno>  // For errno

namespace fs = std::filesystem;

namespace MP4Finalizer {

MP4DirectoryWatcher::MP4DirectoryWatcher(const std::string &watchDirectory)
    : watchDirectory_(watchDirectory), running_(false), shouldStop_(false),
      inotifyFd_(-1), watchDescriptor_(-1) {
  // Ensure directory exists
  if (!fs::exists(watchDirectory_)) {
    fs::create_directories(watchDirectory_);
  }
}

MP4DirectoryWatcher::~MP4DirectoryWatcher() {
  stop();
  // Cleanup inotify
  if (watchDescriptor_ >= 0 && inotifyFd_ >= 0) {
    inotify_rm_watch(inotifyFd_, watchDescriptor_);
  }
  if (inotifyFd_ >= 0) {
    close(inotifyFd_);
  }
}

void MP4DirectoryWatcher::start() {
  if (running_) {
    return;
  }

  running_ = true;
  shouldStop_ = false;
  watchThread_ = std::thread(&MP4DirectoryWatcher::watchLoop, this);

  std::cerr << "[MP4DirectoryWatcher] Started watching directory: "
            << watchDirectory_ << std::endl;
}

void MP4DirectoryWatcher::stop() {
  if (!running_) {
    return;
  }

  shouldStop_ = true;
  if (watchThread_.joinable()) {
    watchThread_.join();
  }
  running_ = false;

  std::cerr << "[MP4DirectoryWatcher] Stopped watching directory: "
            << watchDirectory_ << std::endl;
}

bool MP4DirectoryWatcher::isFileStable(const std::string &filePath) {
  if (!fs::exists(filePath)) {
    return false;
  }

  // Check if file is being written to
  if (MP4Finalizer::MP4Finalizer::isFileBeingWritten(filePath)) {
    return false;
  }

  // Check file size stability
  uintmax_t prevSize = fs::file_size(filePath);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  
  if (!fs::exists(filePath)) {
    return false;
  }

  uintmax_t currSize = fs::file_size(filePath);
  if (currSize != prevSize) {
    return false;
  }

  // Wait a bit more to be sure
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  if (!fs::exists(filePath)) {
    return false;
  }

  currSize = fs::file_size(filePath);
  return (currSize == prevSize);
}

void MP4DirectoryWatcher::processNewFile(const std::string &filePath) {
  // Check if already processed
  {
    std::lock_guard<std::mutex> lock(processedFilesMutex_);
    if (processedFiles_.find(filePath) != processedFiles_.end()) {
      return; // Already processed
    }
  }

  // Wait for file to stabilize (file_des_node may still be writing)
  // But don't wait too long - we want to convert as soon as possible
  int retries = 0;
  const int maxRetries = 10; // Wait up to 5 seconds (10 * 500ms)
  
  while (retries < maxRetries && !shouldStop_) {
    if (isFileStable(filePath)) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    retries++;
  }

  if (shouldStop_ || !fs::exists(filePath)) {
    return;
  }

  // Even if file is not completely stable, try to process it
  // This allows conversion to happen during recording
  // The conversion process will handle partial files gracefully

  // Mark as processed
  {
    std::lock_guard<std::mutex> lock(processedFilesMutex_);
    processedFiles_.insert(filePath);
  }

  std::cerr << "[MP4DirectoryWatcher] Processing file: " << filePath
            << std::endl;

  // Finalize and convert file (overwrite original)
  // This will:
  // 1. Try faststart first (if file is complete)
  // 2. If file needs conversion, convert and overwrite original
  // Note: If file is still being written, conversion may fail, but we'll retry
  // when file is closed (IN_CLOSE_WRITE event)
  if (MP4Finalizer::MP4Finalizer::finalizeFile(filePath, true)) {
    std::cerr << "[MP4DirectoryWatcher] ✓ File converted successfully: "
              << filePath << std::endl;
  } else {
    // Conversion failed, likely because file is still being written
    // Remove from processed list so we can retry when file is closed
    std::lock_guard<std::mutex> lock(processedFilesMutex_);
    processedFiles_.erase(filePath);
  }
}

void MP4DirectoryWatcher::watchLoop() {
  // Try to use inotify first (more efficient)
  inotifyFd_ = inotify_init1(IN_NONBLOCK);
  if (inotifyFd_ >= 0) {
    watchDescriptor_ = inotify_add_watch(
        inotifyFd_, watchDirectory_.c_str(),
        IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);
    if (watchDescriptor_ >= 0) {
      std::cerr << "[MP4DirectoryWatcher] Using inotify for efficient file monitoring"
                << std::endl;
      watchLoopInotify();
      return;
    } else {
      // inotify_add_watch failed, close fd and fallback to polling
      close(inotifyFd_);
      inotifyFd_ = -1;
    }
  }

  // Fallback to polling if inotify not available
  std::cerr << "[MP4DirectoryWatcher] Falling back to polling mode (less efficient)"
            << std::endl;
  watchLoopPolling();
}

void MP4DirectoryWatcher::watchLoopInotify() {
  const size_t BUF_LEN = 1024 * (sizeof(struct inotify_event) + NAME_MAX + 1);
  char buffer[BUF_LEN];

  struct pollfd pfd;
  pfd.fd = inotifyFd_;
  pfd.events = POLLIN;

  while (!shouldStop_) {
    // Poll with timeout (1000ms) to allow checking shouldStop_
    int pollResult = poll(&pfd, 1, 1000);

    if (pollResult < 0) {
      if (errno == EINTR) {
        continue; // Interrupted by signal, continue
      }
      std::cerr << "[MP4DirectoryWatcher] Poll error: " << strerror(errno)
                << std::endl;
      break;
    }

    if (pollResult == 0) {
      // Timeout, check shouldStop_ and continue
      continue;
    }

    if (pfd.revents & POLLIN) {
      ssize_t length = read(inotifyFd_, buffer, BUF_LEN);
      if (length < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          continue; // No data available
        }
        std::cerr << "[MP4DirectoryWatcher] Read error: " << strerror(errno)
                  << std::endl;
        break;
      }

      // Process inotify events
      int i = 0;
      while (i < length) {
        struct inotify_event *event =
            reinterpret_cast<struct inotify_event *>(&buffer[i]);

        if (event->len > 0) {
          std::string fileName(event->name);
          if (fileName.length() >= 4 &&
              fileName.substr(fileName.length() - 4) == ".mp4") {
            std::string filePath = watchDirectory_ + "/" + fileName;

            // Check if file is being created or closed
            if (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) {
              // File was closed after writing or moved to directory
              // Process it in background
              std::thread([this, filePath]() { processNewFile(filePath); })
                  .detach();
            } else if (event->mask & IN_CREATE) {
              // File created, but may still be written to
              // We'll process it when it's closed (IN_CLOSE_WRITE)
            }
          }
        }

        i += sizeof(struct inotify_event) + event->len;
      }
    }
  }
}

void MP4DirectoryWatcher::watchLoopPolling() {
  std::set<std::string> lastFiles;

  while (!shouldStop_) {
    try {
      if (!fs::exists(watchDirectory_) || !fs::is_directory(watchDirectory_)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        continue;
      }

      std::set<std::string> currentFiles;

      // Scan directory for MP4 files
      for (const auto &entry : fs::directory_iterator(watchDirectory_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".mp4") {
          std::string filePath = entry.path().string();
          currentFiles.insert(filePath);

          // Check if this is a new file
          if (lastFiles.find(filePath) == lastFiles.end()) {
            // New file detected, process it in background
            std::thread([this, filePath]() { processNewFile(filePath); })
                .detach();
          }
        }
      }

      lastFiles = currentFiles;

    } catch (const fs::filesystem_error &e) {
      std::cerr << "[MP4DirectoryWatcher] Error scanning directory: "
                << e.what() << std::endl;
    } catch (...) {
      std::cerr << "[MP4DirectoryWatcher] Unknown error in watch loop"
                << std::endl;
    }

    // Sleep before next scan (longer interval for polling mode)
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  }
}

} // namespace MP4Finalizer

