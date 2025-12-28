#ifndef MP4_DIRECTORY_WATCHER_H
#define MP4_DIRECTORY_WATCHER_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <set>
#include <filesystem>

namespace fs = std::filesystem;

namespace MP4Finalizer {

/**
 * @brief Watches a directory for new MP4 files and automatically converts them
 * 
 * This class monitors a directory for new MP4 files. When a file is created
 * and stabilized (not being written anymore), it automatically converts it
 * to a compatible format, overwriting the original file.
 */
class MP4DirectoryWatcher {
public:
  MP4DirectoryWatcher(const std::string &watchDirectory);
  ~MP4DirectoryWatcher();

  /**
   * @brief Start watching the directory
   */
  void start();

  /**
   * @brief Stop watching the directory
   */
  void stop();

  /**
   * @brief Check if watcher is running
   */
  bool isRunning() const { return running_; }

private:
  void watchLoop();
  void watchLoopInotify(); // Use inotify for efficient file system monitoring
  void watchLoopPolling(); // Fallback to polling if inotify not available
  void processNewFile(const std::string &filePath);
  bool isFileStable(const std::string &filePath);

  std::string watchDirectory_;
  std::thread watchThread_;
  std::atomic<bool> running_;
  std::atomic<bool> shouldStop_;
  std::mutex processedFilesMutex_;
  std::set<std::string> processedFiles_; // Track processed files to avoid duplicates
  int inotifyFd_; // File descriptor for inotify
  int watchDescriptor_; // Watch descriptor for the directory
};

} // namespace MP4Finalizer

#endif // MP4_DIRECTORY_WATCHER_H

