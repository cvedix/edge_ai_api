#pragma once

#include <json/json.h>
#include <string>
#include <vector>

/**
 * @brief SecuRT Line Types
 *
 * Defines base classes and specific line types for SecuRT analytics.
 */

/**
 * @brief Direction enum for line crossing detection
 */
enum class LineDirection {
  Up,
  Down,
  Both
};

/**
 * @brief Convert direction string to enum
 */
inline LineDirection directionFromString(const std::string &dir) {
  if (dir == "Up") return LineDirection::Up;
  if (dir == "Down") return LineDirection::Down;
  return LineDirection::Both;
}

/**
 * @brief Convert direction enum to string
 */
inline std::string directionToString(LineDirection dir) {
  switch (dir) {
    case LineDirection::Up:
      return "Up";
    case LineDirection::Down:
      return "Down";
    case LineDirection::Both:
      return "Both";
    default:
      return "Both";
  }
}

/**
 * @brief Coordinate point
 */
struct Coordinate {
  double x;
  double y;

  /**
   * @brief Create from JSON
   */
  static Coordinate fromJson(const Json::Value &json) {
    Coordinate coord;
    if (json.isMember("x") && json["x"].isNumeric()) {
      coord.x = json["x"].asDouble();
    }
    if (json.isMember("y") && json["y"].isNumeric()) {
      coord.y = json["y"].asDouble();
    }
    return coord;
  }

  /**
   * @brief Convert to JSON
   */
  Json::Value toJson() const {
    Json::Value json;
    json["x"] = x;
    json["y"] = y;
    return json;
  }
};

/**
 * @brief Color RGBA (0.0-1.0 range)
 */
struct ColorRGBA {
  double r;
  double g;
  double b;
  double a;

  /**
   * @brief Create from JSON array [r, g, b, a]
   */
  static ColorRGBA fromJson(const Json::Value &json) {
    ColorRGBA color{0.0, 0.0, 0.0, 1.0};
    if (json.isArray() && json.size() >= 4) {
      if (json[0].isNumeric()) color.r = json[0].asDouble();
      if (json[1].isNumeric()) color.g = json[1].asDouble();
      if (json[2].isNumeric()) color.b = json[2].asDouble();
      if (json[3].isNumeric()) color.a = json[3].asDouble();
    }
    return color;
  }

  /**
   * @brief Convert to JSON array
   */
  Json::Value toJson() const {
    Json::Value json(Json::arrayValue);
    json.append(r);
    json.append(g);
    json.append(b);
    json.append(a);
    return json;
  }
};

/**
 * @brief Class enum for object types
 */
enum class ObjectClass {
  Person,
  Animal,
  Vehicle,
  Face,
  Unknown
};

/**
 * @brief Convert class string to enum
 */
inline ObjectClass classFromString(const std::string &cls) {
  if (cls == "Person") return ObjectClass::Person;
  if (cls == "Animal") return ObjectClass::Animal;
  if (cls == "Vehicle") return ObjectClass::Vehicle;
  if (cls == "Face") return ObjectClass::Face;
  return ObjectClass::Unknown;
}

/**
 * @brief Convert class enum to string
 */
inline std::string classToString(ObjectClass cls) {
  switch (cls) {
    case ObjectClass::Person:
      return "Person";
    case ObjectClass::Animal:
      return "Animal";
    case ObjectClass::Vehicle:
      return "Vehicle";
    case ObjectClass::Face:
      return "Face";
    case ObjectClass::Unknown:
      return "Unknown";
    default:
      return "Unknown";
  }
}

/**
 * @brief Base line structure
 */
struct LineBase {
  std::string id;
  std::string name;
  std::vector<Coordinate> coordinates;  // Exactly 2 points for line
  std::vector<ObjectClass> classes;
  LineDirection direction;
  ColorRGBA color;

  /**
   * @brief Convert to JSON
   */
  Json::Value toJson() const {
    Json::Value json;
    json["id"] = id;
    json["name"] = name;
    Json::Value coords(Json::arrayValue);
    for (const auto &coord : coordinates) {
      coords.append(coord.toJson());
    }
    json["coordinates"] = coords;
    Json::Value classesArray(Json::arrayValue);
    for (const auto &cls : classes) {
      classesArray.append(classToString(cls));
    }
    json["classes"] = classesArray;
    json["direction"] = directionToString(direction);
    json["color"] = color.toJson();
    return json;
  }
};

/**
 * @brief Counting Line
 * Counts objects crossing line
 */
struct CountingLine : LineBase {
  /**
   * @brief Create from JSON (LineWrite schema)
   */
  static CountingLine fromJson(const Json::Value &json, const std::string &lineId = "") {
    CountingLine line;
    if (!lineId.empty()) {
      line.id = lineId;
    }
    if (json.isMember("name") && json["name"].isString()) {
      line.name = json["name"].asString();
    }
    if (json.isMember("coordinates") && json["coordinates"].isArray()) {
      for (const auto &coord : json["coordinates"]) {
        line.coordinates.push_back(Coordinate::fromJson(coord));
      }
    }
    if (json.isMember("classes") && json["classes"].isArray()) {
      for (const auto &cls : json["classes"]) {
        if (cls.isString()) {
          line.classes.push_back(classFromString(cls.asString()));
        }
      }
    }
    if (json.isMember("direction") && json["direction"].isString()) {
      line.direction = directionFromString(json["direction"].asString());
    } else {
      line.direction = LineDirection::Both;
    }
    if (json.isMember("color") && json["color"].isArray()) {
      line.color = ColorRGBA::fromJson(json["color"]);
    } else {
      line.color = ColorRGBA{0.0, 0.0, 0.0, 1.0};
    }
    return line;
  }
};

/**
 * @brief Crossing Line
 * Detects objects crossing line by direction
 */
struct CrossingLine : LineBase {
  /**
   * @brief Create from JSON (LineWrite schema)
   */
  static CrossingLine fromJson(const Json::Value &json, const std::string &lineId = "") {
    CrossingLine line;
    if (!lineId.empty()) {
      line.id = lineId;
    }
    if (json.isMember("name") && json["name"].isString()) {
      line.name = json["name"].asString();
    }
    if (json.isMember("coordinates") && json["coordinates"].isArray()) {
      for (const auto &coord : json["coordinates"]) {
        line.coordinates.push_back(Coordinate::fromJson(coord));
      }
    }
    if (json.isMember("classes") && json["classes"].isArray()) {
      for (const auto &cls : json["classes"]) {
        if (cls.isString()) {
          line.classes.push_back(classFromString(cls.asString()));
        }
      }
    }
    if (json.isMember("direction") && json["direction"].isString()) {
      line.direction = directionFromString(json["direction"].asString());
    } else {
      line.direction = LineDirection::Both;
    }
    if (json.isMember("color") && json["color"].isArray()) {
      line.color = ColorRGBA::fromJson(json["color"]);
    } else {
      line.color = ColorRGBA{0.0, 0.0, 0.0, 1.0};
    }
    return line;
  }
};

/**
 * @brief Tailgating Line
 * Detects multiple objects crossing simultaneously within time window
 */
struct TailgatingLine : LineBase {
  int seconds;  // Time window for tailgating detection

  /**
   * @brief Create from JSON (LineWrite schema)
   */
  static TailgatingLine fromJson(const Json::Value &json, const std::string &lineId = "") {
    TailgatingLine line;
    if (!lineId.empty()) {
      line.id = lineId;
    }
    if (json.isMember("name") && json["name"].isString()) {
      line.name = json["name"].asString();
    }
    if (json.isMember("coordinates") && json["coordinates"].isArray()) {
      for (const auto &coord : json["coordinates"]) {
        line.coordinates.push_back(Coordinate::fromJson(coord));
      }
    }
    if (json.isMember("classes") && json["classes"].isArray()) {
      for (const auto &cls : json["classes"]) {
        if (cls.isString()) {
          line.classes.push_back(classFromString(cls.asString()));
        }
      }
    }
    if (json.isMember("direction") && json["direction"].isString()) {
      line.direction = directionFromString(json["direction"].asString());
    } else {
      line.direction = LineDirection::Both;
    }
    if (json.isMember("color") && json["color"].isArray()) {
      line.color = ColorRGBA::fromJson(json["color"]);
    } else {
      line.color = ColorRGBA{0.0, 0.0, 0.0, 1.0};
    }
    if (json.isMember("seconds") && json["seconds"].isInt()) {
      line.seconds = json["seconds"].asInt();
    } else {
      line.seconds = 1;  // Default 1 second
    }
    return line;
  }

  /**
   * @brief Convert to JSON
   */
  Json::Value toJson() const {
    Json::Value json = LineBase::toJson();
    json["seconds"] = seconds;
    return json;
  }
};

/**
 * @brief Line type enum
 */
enum class LineType {
  Counting,
  Crossing,
  Tailgating
};

/**
 * @brief Convert line type string to enum
 */
inline LineType lineTypeFromString(const std::string &type) {
  if (type == "counting") return LineType::Counting;
  if (type == "crossing") return LineType::Crossing;
  if (type == "tailgating") return LineType::Tailgating;
  return LineType::Counting;
}

/**
 * @brief Convert line type enum to string
 */
inline std::string lineTypeToString(LineType type) {
  switch (type) {
    case LineType::Counting:
      return "counting";
    case LineType::Crossing:
      return "crossing";
    case LineType::Tailgating:
      return "tailgating";
    default:
      return "counting";
  }
}

