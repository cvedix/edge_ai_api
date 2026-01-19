#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

/**
 * @brief CVEDIX SDK Validator
 *
 * Utility class for validating CVEDIX SDK dependencies and file access
 * before creating SDK nodes. This helps provide better error messages
 * and fail-fast behavior.
 */
class CVEDIXValidator {
public:
  /**
   * @brief Validate model file access
   * @param modelPath Path to model file
   * @return true if file is accessible, false otherwise
   * @throws std::runtime_error with detailed error message if validation fails
   */
  static bool validateModelFile(const std::string &modelPath);

  /**
   * @brief Check if file exists and is readable
   * @param filePath Path to file
   * @return true if file exists and is readable
   */
  static bool isFileReadable(const fs::path &filePath);

  /**
   * @brief Check if directory exists and is traversable
   * @param dirPath Path to directory
   * @return true if directory exists and is traversable
   */
  static bool isDirectoryTraversable(const fs::path &dirPath);

  /**
   * @brief Validate CVEDIX SDK dependencies
   * Checks for required symlinks and libraries
   * @return true if all dependencies are available
   */
  static bool validateSDKDependencies();

  /**
   * @brief Get detailed error message for permission issues
   * @param filePath Path to file that caused permission error
   * @return Detailed error message with fix instructions
   */
  static std::string getPermissionErrorMessage(const std::string &filePath);

  /**
   * @brief Get detailed error message for missing dependencies
   * @return Detailed error message with fix instructions
   */
  static std::string getDependencyErrorMessage();

  /**
   * @brief Pre-check before creating SDK node
   * Validates file access and dependencies
   * @param modelPath Path to model file
   * @throws std::runtime_error if validation fails
   */
  static void preCheckBeforeNodeCreation(const std::string &modelPath);

private:
  /**
   * @brief Check if libtinyexpr.so is available
   */
  static bool checkTinyExprLibrary();

  /**
   * @brief Check if cereal headers are available
   */
  static bool checkCerealHeaders();

  /**
   * @brief Check if cpp-base64 headers are available
   */
  static bool checkCppBase64Headers();
};

