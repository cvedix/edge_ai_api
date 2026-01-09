#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>

namespace worker {

/**
 * @brief Watches instance config file for changes and automatically reloads
 *
 * This class monitors a config file for changes. When the file is modified,
 * it automatically loads the new config and triggers a callback to apply it.
 * Uses inotify for efficient file system monitoring.
 */
class ConfigFileWatcher {
public:
  /**
   * @brief Callback type for config changes
   * @param newConfig The new configuration JSON
   */
  using ConfigChangeCallback =
      std::function<void(const std::string &configPath)>;

  /**
   * @brief Constructor
   * @param configPath Path to the config file to watch
   * @param callback Callback to call when config changes
   */
  ConfigFileWatcher(const std::string &configPath,
                    ConfigChangeCallback callback);

  ~ConfigFileWatcher();

  // Non-copyable
  ConfigFileWatcher(const ConfigFileWatcher &) = delete;
  ConfigFileWatcher &operator=(const ConfigFileWatcher &) = delete;

  /**
   * @brief Start watching the config file
   */
  void start();

  /**
   * @brief Stop watching the config file
   */
  void stop();

  /**
   * @brief Check if watcher is running
   */
  bool isRunning() const { return running_; }

  /**
   * @brief Get the config file path being watched
   */
  std::string getConfigPath() const { return config_path_; }

private:
  void watchLoop();
  void watchLoopInotify(); // Use inotify for efficient monitoring
  void watchLoopPolling(); // Fallback to polling if inotify not available
  void onConfigChanged();
  bool isFileStable(const std::string &filePath);
  std::string getLastModifiedTime(const std::string &filePath);

  std::string config_path_;
  std::string config_dir_;
  std::string config_filename_;
  ConfigChangeCallback callback_;
  std::thread watch_thread_;
  std::atomic<bool> running_;
  std::atomic<bool> should_stop_;

  // inotify
  int inotify_fd_;
  int watch_descriptor_;

  // File stability check
  std::string last_modified_time_;
  std::chrono::steady_clock::time_point last_change_time_;
  static constexpr int STABILITY_CHECK_MS =
      100; // Wait 100ms for file to stabilize
};

} // namespace worker
