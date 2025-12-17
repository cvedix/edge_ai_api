#include "core/uuid_generator.h"
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>

std::string UUIDGenerator::generateUUID() {
  // UUID v4 format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
  // where x is any hexadecimal digit and y is one of 8, 9, A, or B

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);
  std::uniform_int_distribution<> dis2(8, 11); // For y position

  std::stringstream ss;

  // xxxxxxxx
  for (int i = 0; i < 8; i++) {
    ss << std::hex << dis(gen);
  }
  ss << "-";

  // xxxx
  for (int i = 0; i < 4; i++) {
    ss << std::hex << dis(gen);
  }
  ss << "-";

  // 4xxx (version 4)
  ss << "4";
  for (int i = 0; i < 3; i++) {
    ss << std::hex << dis(gen);
  }
  ss << "-";

  // yxxx (variant)
  ss << std::hex << dis2(gen);
  for (int i = 0; i < 3; i++) {
    ss << std::hex << dis(gen);
  }
  ss << "-";

  // xxxxxxxxxxxx
  for (int i = 0; i < 12; i++) {
    ss << std::hex << dis(gen);
  }

  return ss.str();
}

bool UUIDGenerator::isValidUUID(const std::string &uuid) {
  // UUID format: 8-4-4-4-12 hex digits
  std::regex uuid_pattern("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-4[0-9a-fA-F]{3}-["
                          "89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}$");
  return std::regex_match(uuid, uuid_pattern);
}

char UUIDGenerator::randomHexChar() {
  static const char hex_chars[] = "0123456789abcdef";
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);
  return hex_chars[dis(gen)];
}

int UUIDGenerator::randomInt(int min, int max) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(min, max);
  return dis(gen);
}
