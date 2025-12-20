#include "utils/mp4_finalizer.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace fs = std::filesystem;

namespace MP4Finalizer {

bool MP4Finalizer::isFileBeingWritten(const std::string &filePath) {
  // Check if file is open by another process using lsof
  std::string command = "lsof \"" + filePath + "\" >/dev/null 2>&1";
  int result = std::system(command.c_str());
  return (result == 0);
}

bool MP4Finalizer::waitForFileStable(const std::string &filePath,
                                      int maxWaitMs) {
  if (!fs::exists(filePath)) {
    return false;
  }

  auto startTime = std::chrono::steady_clock::now();
  uintmax_t prevSize = fs::file_size(filePath);

  while (true) {
    // Check timeout
    auto elapsed = std::chrono::steady_clock::now() - startTime;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >
        maxWaitMs) {
      return false;
    }

    // Check if file is being written
    if (isFileBeingWritten(filePath)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      continue;
    }

    // Check file size stability
    if (!fs::exists(filePath)) {
      return false;
    }

    uintmax_t currSize = fs::file_size(filePath);
    if (currSize == prevSize) {
      // File size is stable, wait a bit more to be sure
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      currSize = fs::file_size(filePath);
      if (currSize == prevSize) {
        return true;
      }
    }

    prevSize = currSize;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

std::string MP4Finalizer::getFileProfile(const std::string &filePath) {
  std::string command =
      "ffprobe -v quiet -select_streams v:0 -show_entries stream=profile "
      "-of default=noprint_wrappers=1:nokey=1 \"" +
      filePath + "\" 2>/dev/null";

  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe) {
    return "";
  }

  char buffer[128];
  std::string result;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }
  pclose(pipe);

  // Remove trailing newline
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }

  return result;
}

std::string MP4Finalizer::getPixelFormat(const std::string &filePath) {
  std::string command =
      "ffprobe -v quiet -select_streams v:0 -show_entries stream=pix_fmt "
      "-of default=noprint_wrappers=1:nokey=1 \"" +
      filePath + "\" 2>/dev/null";

  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe) {
    return "";
  }

  char buffer[128];
  std::string result;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }
  pclose(pipe);

  // Remove trailing newline
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }

  return result;
}

bool MP4Finalizer::needsConversion(const std::string &filePath) {
  if (!fs::exists(filePath)) {
    return false;
  }

  std::string profile = getFileProfile(filePath);
  std::string pixFmt = getPixelFormat(filePath);

  // Check if profile contains "High" (High profile)
  bool hasHighProfile = (profile.find("High") != std::string::npos ||
                         profile.find("high") != std::string::npos);

  // Check if pixel format is not yuv420p
  bool needsPixelFormatConversion = (pixFmt != "yuv420p" && !pixFmt.empty());

  return hasHighProfile || needsPixelFormatConversion;
}

bool MP4Finalizer::executeFFmpeg(const std::string &command) {
  // Capture stderr to help debug conversion failures
  std::string fullCommand = command + " 2>&1";
  FILE *pipe = popen(fullCommand.c_str(), "r");
  if (!pipe) {
    std::cerr << "[MP4Finalizer] Failed to execute ffmpeg command" << std::endl;
    return false;
  }
  
  char buffer[128];
  std::string output;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }
  int result = pclose(pipe);
  
  if (result != 0) {
    // Log error output for debugging
    if (!output.empty()) {
      // Log full output if it contains errors
      if (output.find("error") != std::string::npos || 
          output.find("Error") != std::string::npos ||
          output.find("ERROR") != std::string::npos ||
          output.find("Invalid") != std::string::npos) {
        std::cerr << "[MP4Finalizer] FFmpeg error output (full):" << std::endl;
        // Print in chunks to avoid truncation
        size_t pos = 0;
        const size_t chunkSize = 1000;
        while (pos < output.length()) {
          size_t endPos = std::min(pos + chunkSize, output.length());
          std::cerr << output.substr(pos, endPos - pos);
          pos = endPos;
        }
        std::cerr << std::endl;
      } else {
        // Even if no explicit error keyword, log if command failed
        std::cerr << "[MP4Finalizer] FFmpeg command failed. Output:" << std::endl;
        std::cerr << output.substr(0, 2000) << std::endl;
      }
    } else {
      std::cerr << "[MP4Finalizer] FFmpeg command failed with exit code: " 
                << result << " (no output captured)" << std::endl;
    }
    return false;
  }
  
  return true;
}

bool MP4Finalizer::finalizeFile(const std::string &filePath,
                                 bool createCompatible) {
  if (!fs::exists(filePath)) {
    std::cerr << "[MP4Finalizer] File not found: " << filePath << std::endl;
    return false;
  }

  // Wait for file to stabilize (increased timeout for stop instance scenario)
  // When instance stops, file_des_node may take longer to close the file
  if (!waitForFileStable(filePath, 10000)) { // Increased from 5s to 10s
    std::cerr << "[MP4Finalizer] File is still being written or timeout: "
              << filePath << std::endl;
    std::cerr << "[MP4Finalizer] Will retry conversion after additional wait..."
              << std::endl;
    
    // Retry after additional wait (file might still be closing)
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    if (!waitForFileStable(filePath, 10000)) {
      std::cerr << "[MP4Finalizer] File still not stable after retry: "
                << filePath << std::endl;
      return false;
    }
    std::cerr << "[MP4Finalizer] File is now stable, proceeding with conversion..."
              << std::endl;
  }

  std::string tempFile = filePath + ".tmp.mp4"; // Use .mp4 extension so ffmpeg recognizes format

  // Try faststart first (fastest, preserves quality)
  // Note: If file is still being written or incomplete, faststart may fail
  std::string faststartCmd = "ffmpeg -i \"" + filePath + "\" -c copy -movflags +faststart -f mp4 \"" +
                              tempFile + "\" -y";
  
  std::cerr << "[MP4Finalizer] Attempting faststart on: " << filePath << std::endl;
  if (executeFFmpeg(faststartCmd)) {
    // Replace original with finalized version
    try {
      fs::remove(filePath);
      fs::rename(tempFile, filePath);
      std::cerr << "[MP4Finalizer] ✓ Finalized: " << filePath << std::endl;
      
      // If file needs conversion, convert it now (overwrite original)
      if (createCompatible && needsConversion(filePath)) {
        std::cerr << "[MP4Finalizer] File uses incompatible encoding, converting to compatible format..."
                  << std::endl;
        if (convertToCompatible(filePath, "")) { // Empty string = overwrite original
          return true;
        }
      }
      return true;
    } catch (const fs::filesystem_error &e) {
      std::cerr << "[MP4Finalizer] ✗ Failed to rename file: " << e.what()
                << std::endl;
      fs::remove(tempFile);
      return false;
    }
  }

  // Faststart failed, check if conversion is needed
  fs::remove(tempFile);
  std::cerr << "[MP4Finalizer] Faststart failed, will try conversion if needed..."
            << std::endl;

  // Check if file is still being written (might be the issue)
  if (isFileBeingWritten(filePath)) {
    std::cerr << "[MP4Finalizer] ⚠ File is still being written: " << filePath << std::endl;
    std::cerr << "[MP4Finalizer] ⚠ Will skip conversion - file_des_node may not have closed file yet"
              << std::endl;
    std::cerr << "[MP4Finalizer] ⚠ MP4DirectoryWatcher will convert this file when it's closed"
              << std::endl;
    return false;
  }

  if (createCompatible && needsConversion(filePath)) {
    std::cerr << "[MP4Finalizer] File uses incompatible encoding, converting to compatible format..."
              << std::endl;
    // Convert and overwrite original file
    if (convertToCompatible(filePath, "")) { // Empty string = overwrite original
      return true;
    }
    std::cerr << "[MP4Finalizer] Conversion also failed for: " << filePath << std::endl;
  } else if (createCompatible) {
    std::cerr << "[MP4Finalizer] File encoding is compatible, but faststart failed. "
              << "File may be incomplete or corrupted." << std::endl;
    // Even if encoding is compatible, try to fix the file with conversion
    // This can help with incomplete files
    std::cerr << "[MP4Finalizer] Attempting conversion anyway to fix file structure..."
              << std::endl;
    if (convertToCompatible(filePath, "")) {
      return true;
    }
  }

  std::cerr << "[MP4Finalizer] ✗ Failed to finalize: " << filePath
            << std::endl;
  return false;
}

bool MP4Finalizer::convertToCompatible(const std::string &inputPath,
                                       const std::string &outputPath) {
  if (!fs::exists(inputPath)) {
    return false;
  }

  std::string outPath = outputPath;
  bool overwriteOriginal = outputPath.empty(); // If no output specified, overwrite original
  
  if (outPath.empty()) {
    // Use temp file for conversion, then replace original
    // Use .mp4 extension so ffmpeg recognizes format
    outPath = inputPath + ".tmp_convert.mp4";
  }

  // Convert with compatible settings
  // Use -ignore_unknown to handle incomplete files gracefully
  // Use -fflags +genpts to generate PTS if missing (for incomplete files)
  // Use -f mp4 to explicitly specify output format
  std::string convertCmd =
      "ffmpeg -fflags +genpts -ignore_unknown -i \"" + inputPath + "\" "
      "-c:v libx264 "
      "-profile:v baseline "
      "-level 3.1 "
      "-preset medium "
      "-crf 23 "
      "-pix_fmt yuv420p "
      "-c:a aac "
      "-b:a 128k "
      "-ar 44100 "
      "-movflags +faststart "
      "-f mp4 "
      "\"" + outPath + "\" -y";
  
  std::cerr << "[MP4Finalizer] Converting file: " << inputPath << std::endl;

  if (executeFFmpeg(convertCmd)) {
    // If overwriting original, replace file
    if (overwriteOriginal) {
      try {
        fs::remove(inputPath);
        fs::rename(outPath, inputPath);
        std::cerr << "[MP4Finalizer] ✓ Converted and replaced original file: "
                  << inputPath << std::endl;
        return true;
      } catch (const fs::filesystem_error &e) {
        std::cerr << "[MP4Finalizer] ✗ Failed to replace original file: "
                  << e.what() << std::endl;
        fs::remove(outPath);
        return false;
      }
    }
    return true;
  }

  // Clean up temp file if conversion failed
  if (overwriteOriginal) {
    fs::remove(outPath);
  }

  return false;
}

int MP4Finalizer::finalizeDirectory(const std::string &directory,
                                    bool createCompatible) {
  if (!fs::exists(directory) || !fs::is_directory(directory)) {
    std::cerr << "[MP4Finalizer] Directory not found: " << directory
              << std::endl;
    return 0;
  }

  int successCount = 0;
  int totalCount = 0;

  try {
    for (const auto &entry : fs::directory_iterator(directory)) {
      if (entry.is_regular_file() && entry.path().extension() == ".mp4") {
        totalCount++;
        if (finalizeFile(entry.path().string(), createCompatible)) {
          successCount++;
        }
      }
    }
  } catch (const fs::filesystem_error &e) {
    std::cerr << "[MP4Finalizer] Error reading directory: " << e.what()
              << std::endl;
  }

  std::cerr << "[MP4Finalizer] Processed " << successCount << "/" << totalCount
            << " files" << std::endl;

  return successCount;
}

// BackgroundMP4Processor implementation
BackgroundMP4Processor::BackgroundMP4Processor()
    : running_(false), shouldStop_(false) {}

BackgroundMP4Processor::~BackgroundMP4Processor() { stop(); }

void BackgroundMP4Processor::queueFile(const std::string &filePath,
                                       bool createCompatible) {
  std::lock_guard<std::mutex> lock(queueMutex_);
  queue_.push_back({filePath, createCompatible});

  // Start processing thread if not running
  if (!running_) {
    running_ = true;
    shouldStop_ = false;
    processorThread_ = std::thread(&BackgroundMP4Processor::processLoop, this);
  }
}

void BackgroundMP4Processor::queueDirectory(const std::string &directory,
                                             bool createCompatible) {
  if (!fs::exists(directory) || !fs::is_directory(directory)) {
    return;
  }

  try {
    for (const auto &entry : fs::directory_iterator(directory)) {
      if (entry.is_regular_file() && entry.path().extension() == ".mp4") {
        queueFile(entry.path().string(), createCompatible);
      }
    }
  } catch (const fs::filesystem_error &e) {
    std::cerr << "[MP4Finalizer] Error reading directory: " << e.what()
              << std::endl;
  }
}

void BackgroundMP4Processor::stop() {
  shouldStop_ = true;
  if (processorThread_.joinable()) {
    processorThread_.join();
  }
  running_ = false;
}

void BackgroundMP4Processor::processLoop() {
  while (!shouldStop_ || !queue_.empty()) {
    ProcessingTask task;

    // Get next task from queue
    {
      std::lock_guard<std::mutex> lock(queueMutex_);
      if (queue_.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
      task = queue_.front();
      queue_.erase(queue_.begin());
    }

    // Process the task
    processFile(task);
  }

  running_ = false;
}

void BackgroundMP4Processor::processFile(const ProcessingTask &task) {
  MP4Finalizer::finalizeFile(task.filePath, task.createCompatible);
}

} // namespace MP4Finalizer

