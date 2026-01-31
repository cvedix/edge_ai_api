#pragma once

#include <json/json.h>
#include <string>
#include <vector>

/**
 * @brief Coordinate structure for area points
 */
struct Coordinate {
  double x;
  double y;

  /**
   * @brief Convert to JSON
   */
  Json::Value toJson() const {
    Json::Value json;
    json["x"] = x;
    json["y"] = y;
    return json;
  }

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
};

/**
 * @brief Color RGBA structure
 * Values are in range 0.0-1.0
 */
struct ColorRGBA {
  double r = 0.0;
  double g = 0.0;
  double b = 0.0;
  double a = 1.0;

  /**
   * @brief Convert to JSON array [r, g, b, a]
   */
  Json::Value toJson() const {
    Json::Value json(Json::arrayValue);
    json.append(r);
    json.append(g);
    json.append(b);
    json.append(a);
    return json;
  }

  /**
   * @brief Create from JSON array [r, g, b, a]
   */
  static ColorRGBA fromJson(const Json::Value &json) {
    ColorRGBA color;
    if (json.isArray() && json.size() >= 4) {
      if (json[0].isNumeric()) color.r = json[0].asDouble();
      if (json[1].isNumeric()) color.g = json[1].asDouble();
      if (json[2].isNumeric()) color.b = json[2].asDouble();
      if (json[3].isNumeric()) color.a = json[3].asDouble();
    }
    return color;
  }
};

/**
 * @brief Object class enum
 */
enum class ObjectClass {
  Person,
  Animal,
  Vehicle,
  Face,
  Unknown
};

/**
 * @brief Convert ObjectClass to string
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
 * @brief Convert string to ObjectClass
 */
inline ObjectClass stringToClass(const std::string &str) {
  if (str == "Person")
    return ObjectClass::Person;
  if (str == "Animal")
    return ObjectClass::Animal;
  if (str == "Vehicle")
    return ObjectClass::Vehicle;
  if (str == "Face")
    return ObjectClass::Face;
  return ObjectClass::Unknown;
}

/**
 * @brief Base area structure
 * Contains common fields for all area types
 */
struct AreaBase {
  std::string id;
  std::string name;
  std::vector<Coordinate> coordinates;
  std::vector<ObjectClass> classes;
  ColorRGBA color;

  /**
   * @brief Convert to JSON
   */
  Json::Value toJson() const {
    Json::Value json;
    json["id"] = id;
    json["name"] = name;

    // Convert coordinates
    Json::Value coords(Json::arrayValue);
    for (const auto &coord : coordinates) {
      coords.append(coord.toJson());
    }
    json["coordinates"] = coords;

    // Convert classes
    Json::Value classesJson(Json::arrayValue);
    for (const auto &cls : classes) {
      classesJson.append(classToString(cls));
    }
    json["classes"] = classesJson;

    // Convert color
    json["color"] = color.toJson();

    return json;
  }

  /**
   * @brief Create from JSON (base fields only)
   */
  static AreaBase fromJson(const Json::Value &json) {
    AreaBase area;

    if (json.isMember("id") && json["id"].isString()) {
      area.id = json["id"].asString();
    }

    if (json.isMember("name") && json["name"].isString()) {
      area.name = json["name"].asString();
    }

    // Parse coordinates
    if (json.isMember("coordinates") && json["coordinates"].isArray()) {
      for (const auto &coordJson : json["coordinates"]) {
        area.coordinates.push_back(Coordinate::fromJson(coordJson));
      }
    }

    // Parse classes
    if (json.isMember("classes") && json["classes"].isArray()) {
      for (const auto &classJson : json["classes"]) {
        if (classJson.isString()) {
          area.classes.push_back(stringToClass(classJson.asString()));
        }
      }
    }

    // Parse color
    if (json.isMember("color")) {
      area.color = ColorRGBA::fromJson(json["color"]);
    }

    return area;
  }
};

/**
 * @brief Base area write structure
 * Used for creating/updating areas (without ID)
 */
struct AreaBaseWrite {
  std::string name;
  std::vector<Coordinate> coordinates;
  std::vector<ObjectClass> classes;
  ColorRGBA color;

  /**
   * @brief Create from JSON
   */
  static AreaBaseWrite fromJson(const Json::Value &json) {
    AreaBaseWrite write;

    if (json.isMember("name") && json["name"].isString()) {
      write.name = json["name"].asString();
    }

    // Parse coordinates
    if (json.isMember("coordinates") && json["coordinates"].isArray()) {
      for (const auto &coordJson : json["coordinates"]) {
        write.coordinates.push_back(Coordinate::fromJson(coordJson));
      }
    }

    // Parse classes
    if (json.isMember("classes") && json["classes"].isArray()) {
      for (const auto &classJson : json["classes"]) {
        if (classJson.isString()) {
          write.classes.push_back(stringToClass(classJson.asString()));
        }
      }
    }

    // Parse color
    if (json.isMember("color")) {
      write.color = ColorRGBA::fromJson(json["color"]);
    }

    return write;
  }
};

