#pragma once

#include <string>

/**
 * @brief UUID Generator utility
 * Generates UUID v4 format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
 */
class UUIDGenerator {
public:
  /**
   * @brief Generate a new UUID v4
   * @return UUID string in format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
   */
  static std::string generateUUID();

  /**
   * @brief Validate UUID format
   * @param uuid UUID string to validate
   * @return true if valid UUID format
   */
  static bool isValidUUID(const std::string &uuid);

private:
  /**
   * @brief Generate random hex character
   */
  static char randomHexChar();

  /**
   * @brief Generate random number in range
   */
  static int randomInt(int min, int max);
};
