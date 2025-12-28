#ifndef MP4_FINALIZER_H
#define MP4_FINALIZER_H

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

namespace MP4Finalizer {

/**
 * @brief Utility class to automatically finalize and convert MP4 files
 * 
 * This class provides functionality to:
 * - Finalize MP4 files with faststart (move moov atom to beginning)
 * - Convert files with incompatible encoding settings to compatible format
 * - Process files in background without blocking
 */
class MP4Finalizer {
public:
  /**
   * @brief Finalize a single MP4 file
   * 
   * @param filePath Path to the MP4 file
   * @param createCompatible If true, create a compatible version if needed
   * @return true if successful, false otherwise
   */
  static bool finalizeFile(const std::string &filePath,
                           bool createCompatible = true);

  /**
   * @brief Finalize all MP4 files in a directory
   * 
   * @param directory Path to directory containing MP4 files
   * @param createCompatible If true, create compatible versions if needed
   * @return Number of files successfully processed
   */
  static int finalizeDirectory(const std::string &directory,
                                bool createCompatible = true);

  /**
   * @brief Check if a file is being written to
   * 
   * @param filePath Path to the file
   * @return true if file is being written, false otherwise
   */
  static bool isFileBeingWritten(const std::string &filePath);

  /**
   * @brief Check if file uses incompatible encoding settings
   * 
   * @param filePath Path to the MP4 file
   * @return true if file needs conversion, false otherwise
   */
  static bool needsConversion(const std::string &filePath);

  /**
   * @brief Convert file to compatible format
   * 
   * @param inputPath Input file path
   * @param outputPath Output file path (optional, auto-generated if empty)
   * @return true if successful, false otherwise
   */
  static bool convertToCompatible(const std::string &inputPath,
                                  const std::string &outputPath = "");

private:
  /**
   * @brief Wait for file to stabilize (stop being written)
   * 
   * @param filePath Path to the file
   * @param maxWaitMs Maximum time to wait in milliseconds
   * @return true if file stabilized, false if timeout
   */
  static bool waitForFileStable(const std::string &filePath,
                                 int maxWaitMs = 5000);

  /**
   * @brief Execute ffmpeg command
   * 
   * @param command FFmpeg command to execute
   * @return true if successful, false otherwise
   */
  static bool executeFFmpeg(const std::string &command);

  /**
   * @brief Get file profile using ffprobe
   * 
   * @param filePath Path to the MP4 file
   * @return Profile string (e.g., "High 4:4:4 Predictive")
   */
  static std::string getFileProfile(const std::string &filePath);

  /**
   * @brief Get pixel format using ffprobe
   * 
   * @param filePath Path to the MP4 file
   * @return Pixel format string (e.g., "yuv444p")
   */
  static std::string getPixelFormat(const std::string &filePath);
};

/**
 * @brief Background processor for MP4 files
 * 
 * Processes MP4 files in background thread without blocking
 */
class BackgroundMP4Processor {
public:
  BackgroundMP4Processor();
  ~BackgroundMP4Processor();

  /**
   * @brief Add file to processing queue
   * 
   * @param filePath Path to MP4 file
   * @param createCompatible If true, create compatible version if needed
   */
  void queueFile(const std::string &filePath, bool createCompatible = true);

  /**
   * @brief Process all files in a directory
   * 
   * @param directory Path to directory
   * @param createCompatible If true, create compatible versions if needed
   */
  void queueDirectory(const std::string &directory,
                      bool createCompatible = true);

  /**
   * @brief Stop processing and wait for completion
   */
  void stop();

  /**
   * @brief Check if processor is running
   */
  bool isRunning() const { return running_; }

private:
  struct ProcessingTask {
    std::string filePath;
    bool createCompatible;
  };

  void processLoop();
  void processFile(const ProcessingTask &task);

  std::vector<ProcessingTask> queue_;
  std::mutex queueMutex_;
  std::thread processorThread_;
  std::atomic<bool> running_;
  std::atomic<bool> shouldStop_;
};

} // namespace MP4Finalizer

#endif // MP4_FINALIZER_H


