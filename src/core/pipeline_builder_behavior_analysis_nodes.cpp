#include "core/pipeline_builder_behavior_analysis_nodes.h"
#include "core/pipeline_builder_model_resolver.h"
#include "core/area_manager.h"
#include "core/securt_line_manager.h"
#include <iostream>
#include <stdexcept>
#include <cvedix/nodes/ba/cvedix_ba_crossline_node.h>
#include <cvedix/nodes/ba/cvedix_ba_jam_node.h>
#include <cvedix/nodes/ba/cvedix_ba_stop_node.h>
#include <cvedix/nodes/osd/cvedix_ba_crossline_osd_node.h>
#include <cvedix/nodes/osd/cvedix_ba_jam_osd_node.h>
#include <cvedix/nodes/osd/cvedix_ba_stop_osd_node.h>



std::shared_ptr<cvedix_nodes::cvedix_node>
PipelineBuilderBehaviorAnalysisNodes::createBACrosslineNode(
    const std::string &nodeName,
    const std::map<std::string, std::string> &params,
    const CreateInstanceRequest &req) {

  try {
    if (nodeName.empty()) {
      throw std::invalid_argument("Node name cannot be empty");
    }

    std::map<int, cvedix_objects::cvedix_line> lines;
    bool linesParsed = false;

    // Priority 1: Check CrossingLines from API (stored in additionalParams)
    auto crossingLinesIt = req.additionalParams.find("CrossingLines");
    if (crossingLinesIt != req.additionalParams.end() &&
        !crossingLinesIt->second.empty()) {
      try {
        // Parse JSON string to JSON array
        Json::Reader reader;
        Json::Value parsedLines;
        if (reader.parse(crossingLinesIt->second, parsedLines) &&
            parsedLines.isArray()) {
          // Iterate through lines array
          for (Json::ArrayIndex i = 0; i < parsedLines.size(); ++i) {
            const Json::Value &lineObj = parsedLines[i];

            // Check if line has coordinates
            if (!lineObj.isMember("coordinates") ||
                !lineObj["coordinates"].isArray()) {
              std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Line at index " << i
                        << " missing or invalid 'coordinates' field, skipping"
                        << std::endl;
              continue;
            }

            const Json::Value &coordinates = lineObj["coordinates"];
            if (coordinates.size() < 2) {
              std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Line at index " << i
                        << " has less than 2 coordinates, skipping"
                        << std::endl;
              continue;
            }

            // Get first and last coordinates
            const Json::Value &startCoord = coordinates[0];
            const Json::Value &endCoord = coordinates[coordinates.size() - 1];

            if (!startCoord.isMember("x") || !startCoord.isMember("y") ||
                !endCoord.isMember("x") || !endCoord.isMember("y")) {
              std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Line at index " << i
                        << " has invalid coordinate format, skipping"
                        << std::endl;
              continue;
            }

            if (!startCoord["x"].isNumeric() || !startCoord["y"].isNumeric() ||
                !endCoord["x"].isNumeric() || !endCoord["y"].isNumeric()) {
              std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Line at index " << i
                        << " has non-numeric coordinates, skipping"
                        << std::endl;
              continue;
            }

            // Convert to cvedix_line
            int start_x = startCoord["x"].asInt();
            int start_y = startCoord["y"].asInt();
            int end_x = endCoord["x"].asInt();
            int end_y = endCoord["y"].asInt();

            cvedix_objects::cvedix_point start(start_x, start_y);
            cvedix_objects::cvedix_point end(end_x, end_y);

            // Use array index as channel (0, 1, 2, ...)
            int channel = static_cast<int>(i);
            lines[channel] = cvedix_objects::cvedix_line(start, end);
            linesParsed = true;
          }

          if (linesParsed) {
            std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] ✓ Parsed " << lines.size()
                      << " line(s) from CrossingLines API" << std::endl;
          }
        } else {
          std::cerr
              << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Failed to parse CrossingLines "
                 "JSON or not an array, falling back to solution config"
              << std::endl;
        }
      } catch (const std::exception &e) {
        std::cerr
            << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Exception parsing CrossingLines "
               "JSON: "
            << e.what() << ", falling back to solution config" << std::endl;
      }
    }

    // Priority 2: Fallback to solution config parameters if no lines from API
    if (!linesParsed) {
      // Check if we have line parameters from solution config
      // Also check if values are not placeholders (e.g., ${CROSSLINE_START_X})
      bool hasValidParams =
          params.count("line_channel") && params.count("line_start_x") &&
          params.count("line_start_y") && params.count("line_end_x") &&
          params.count("line_end_y");

      // Check if values are actual numbers (not placeholders)
      if (hasValidParams) {
        bool allValid = true;
        for (const auto &key :
             {"line_start_x", "line_start_y", "line_end_x", "line_end_y"}) {
          if (params.at(key).find("${") != std::string::npos) {
            allValid = false;
            break;
          }
        }

        if (allValid) {
          try {
            int channel = std::stoi(params.at("line_channel"));
            int start_x = std::stoi(params.at("line_start_x"));
            int start_y = std::stoi(params.at("line_start_y"));
            int end_x = std::stoi(params.at("line_end_x"));
            int end_y = std::stoi(params.at("line_end_y"));

            cvedix_objects::cvedix_point start(start_x, start_y);
            cvedix_objects::cvedix_point end(end_x, end_y);
            lines[channel] = cvedix_objects::cvedix_line(start, end);
            std::cerr
                << "[PipelineBuilderBehaviorAnalysisNodes] Using line configuration from solution "
                   "config (channel "
                << channel << ": (" << start_x << "," << start_y << ") -> ("
                << end_x << "," << end_y << "))" << std::endl;
          } catch (const std::exception &e) {
            std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Warning: Failed to parse line "
                         "parameters from solution config: "
                      << e.what() << std::endl;
            hasValidParams = false;
          }
        } else {
          std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Line parameters contain unresolved "
                       "placeholders"
                    << std::endl;
          hasValidParams = false;
        }
      }

      if (!hasValidParams) {
        std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] No valid line configuration found. "
                     "BA crossline node will be created without lines. "
                     "Lines can be added later via API."
                  << std::endl;
      }
    }

    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Creating BA crossline node:" << std::endl;
    std::cerr << "  Name: '" << nodeName << "'" << std::endl;
    std::cerr << "  Lines configured for " << lines.size() << " channel(s)"
              << std::endl;

    auto node = std::make_shared<cvedix_nodes::cvedix_ba_crossline_node>(
        nodeName, lines);

    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] ✓ BA crossline node created successfully"
              << std::endl;
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes]   Lines will be passed to OSD node via "
                 "pipeline metadata"
              << std::endl;
    std::cerr
        << "[PipelineBuilderBehaviorAnalysisNodes]   OSD node will draw these lines on video frames"
        << std::endl;
    return node;
  } catch (const std::exception &e) {
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Exception in createBACrosslineNode: "
              << e.what() << std::endl;
    throw;
  }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilderBehaviorAnalysisNodes::createBAJamNode(
    const std::string &nodeName,
    const std::map<std::string, std::string> &params,
    const CreateInstanceRequest &req) {

  try {
    if (nodeName.empty()) {
      throw std::invalid_argument("Node name cannot be empty");
    }

    std::map<int, std::vector<cvedix_objects::cvedix_point>> jams;
    bool jamsParsed = false;

    // Priority 1: Check Jams from API (stored in additionalParams)
    // Support both "Jams" and "JamZones" for backward compatibility
    auto jamZoneIt = req.additionalParams.find("JamZones");
    if (jamZoneIt == req.additionalParams.end() || jamZoneIt->second.empty()) {
      jamZoneIt = req.additionalParams.find("Jams");
    }
    if (jamZoneIt != req.additionalParams.end() && !jamZoneIt->second.empty()) {
      try {
        // Parse JSON string to JSON array
        Json::Reader reader;
        Json::Value parsedJams;
        if (reader.parse(jamZoneIt->second, parsedJams) &&
            parsedJams.isArray()) {
          // Iterate through jams array
          for (Json::ArrayIndex i = 0; i < parsedJams.size(); ++i) {
            const Json::Value &jamObj = parsedJams[i];
            // Check if jam has coordinates
            if (!jamObj.isMember("roi") || !jamObj["roi"].isArray()) {
              std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Jam at index " << i
                        << " missing or invalid 'coordinates' field, skipping"
                        << std::endl;
              continue;
            }

            const Json::Value &roi = jamObj["roi"];
            if (roi.size() < 3) {
              std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Jam at index " << i
                        << " has less than 3 coordinates, skipping"
                        << std::endl;
              continue;
            }

            for (const auto &pt : roi) {

              if (!pt.isMember("x") || !pt.isMember("y")) {
                std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Point at index " << i
                          << " has invalid coordinate format, skipping"
                          << std::endl;
                continue;
              }

              if (!pt["x"].isNumeric() || !pt["y"].isNumeric()) {
                std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Point at index " << i
                          << " must be number, skipping" << std::endl;
                continue;
              }
            }
            std::vector<cvedix_objects::cvedix_point> roiPoints;
            bool ok = true;
            for (const auto &coord : jamObj["roi"]) {
              if (!coord.isObject() || !coord.isMember("x") ||
                  !coord.isMember("y") || !coord["x"].isNumeric() ||
                  !coord["y"].isNumeric()) {
                ok = false;
                break;
              }
              cvedix_objects::cvedix_point p;
              p.x = static_cast<int>(coord["x"].asDouble());
              p.y = static_cast<int>(coord["y"].asDouble());
              roiPoints.push_back(p);
            }
            if (!ok || roiPoints.empty()) {
              std::cerr << "[API] parseJamsFromJson: Invalid ROI at index " << i
                        << " - skipping";
              continue;
            }
            // Use array index as channel (0, 1, 2, ...)
            int channel = static_cast<int>(i);
            jams[channel] = std::move(roiPoints);
            jamsParsed = true;
          }

          if (jamsParsed) {
            std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] ✓ Parsed " << jams.size()
                      << " jam(s) from Jams API" << std::endl;
          }
        } else {
          std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Failed to parse Jams "
                       "JSON or not an array, falling back to solution config"
                    << std::endl;
        }
      } catch (const std::exception &e) {
        std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Exception parsing Jams "
                     "JSON: "
                  << e.what() << ", falling back to solution config"
                  << std::endl;
      }
    }

    // Priority 2: Fallback to solution config parameters if no jams from API
    if (!jamsParsed) {
      // Check if we have jam parameters from solution config
      // Also check if values are not placeholders (e.g., ${CROSSLINE_START_X})
      bool hasValidParams =
          params.count("line_channel") && params.count("line_start_x") &&
          params.count("line_start_y") && params.count("line_end_x") &&
          params.count("line_end_y");

      // Check if values are actual numbers (not placeholders)
      if (hasValidParams) {
        bool allValid = true;
        for (const auto &key :
             {"line_start_x", "line_start_y", "line_end_x", "line_end_y"}) {
          if (params.at(key).find("${") != std::string::npos) {
            allValid = false;
            break;
          }
        }

        if (allValid) {
          try {
            int channel = std::stoi(params.at("jam_channel"));
            int start_x = std::stoi(params.at("line_start_x"));
            int start_y = std::stoi(params.at("line_start_y"));
            int end_x = std::stoi(params.at("line_end_x"));
            int end_y = std::stoi(params.at("line_end_y"));

            cvedix_objects::cvedix_point start(start_x, start_y);
            cvedix_objects::cvedix_point end(end_x, end_y);
            jams[channel] = {start, end};
            std::cerr
                << "[PipelineBuilderBehaviorAnalysisNodes] Using jam configuration from solution "
                   "config (channel "
                << channel << ": (" << start_x << "," << start_y << ") -> ("
                << end_x << "," << end_y << "))" << std::endl;
          } catch (const std::exception &e) {
            std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Warning: Failed to parse jam "
                         "parameters from solution config: "
                      << e.what() << std::endl;
            hasValidParams = false;
          }
        } else {
          std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Jam parameters contain unresolved "
                       "placeholders"
                    << std::endl;
          hasValidParams = false;
        }
      }

      if (!hasValidParams) {
        std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] No valid jam configuration found. "
                     "BA jam node will be created without jams. "
                     "Jams can be added later via API."
                  << std::endl;
      }
    }

    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Creating BA jam node:" << std::endl;
    std::cerr << "  Name: '" << nodeName << "'" << std::endl;
    std::cerr << "  Jams configured for " << jams.size() << " channel(s)"
              << std::endl;

    auto node =
        std::make_shared<cvedix_nodes::cvedix_ba_jam_node>(nodeName, jams);
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] ✓ BA jam node created successfully"
              << std::endl;
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes]   Jams will be passed to OSD node via "
                 "pipeline metadata"
              << std::endl;
    std::cerr
        << "[PipelineBuilderBehaviorAnalysisNodes]   OSD node will draw these jams on video frames"
        << std::endl;
    return node;
  } catch (const std::exception &e) {
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Exception in createBAJamNode: " << e.what()
              << std::endl;
    throw;
  }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilderBehaviorAnalysisNodes::createBAStopNode(
    const std::string &nodeName,
    const std::map<std::string, std::string> &params,
    const CreateInstanceRequest &req) {

  try {
    if (nodeName.empty()) {
      throw std::invalid_argument("Node name cannot be empty");
    }

    std::map<int, std::vector<cvedix_objects::cvedix_point>> stops;
    bool stopsParsed = false;

    // Priority 1: Check StopZones from API (stored in additionalParams)
    // Support both "Stops" and "StopZones" for backward compatibility
    auto stopZoneIt = req.additionalParams.find("StopZones");
    if (stopZoneIt == req.additionalParams.end() || stopZoneIt->second.empty()) {
      stopZoneIt = req.additionalParams.find("Stops");
    }
    if (stopZoneIt != req.additionalParams.end() && !stopZoneIt->second.empty()) {
      try {
        // Parse JSON string to JSON array
        Json::Reader reader;
        Json::Value parsedStops;
        if (reader.parse(stopZoneIt->second, parsedStops) &&
            parsedStops.isArray()) {
          // Iterate through stops array
          for (Json::ArrayIndex i = 0; i < parsedStops.size(); ++i) {
            const Json::Value &stopObj = parsedStops[i];
            // Check if stop has roi
            if (!stopObj.isMember("roi") || !stopObj["roi"].isArray()) {
              std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Stop at index " << i
                        << " missing or invalid 'roi' field, skipping"
                        << std::endl;
              continue;
            }

            const Json::Value &roi = stopObj["roi"];
            if (roi.size() < 3) {
              std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Stop at index " << i
                        << " has less than 3 coordinates, skipping"
                        << std::endl;
              continue;
            }

            std::vector<cvedix_objects::cvedix_point> roiPoints;
            bool ok = true;
            for (const auto &coord : stopObj["roi"]) {
              if (!coord.isObject() || !coord.isMember("x") ||
                  !coord.isMember("y") || !coord["x"].isNumeric() ||
                  !coord["y"].isNumeric()) {
                ok = false;
                break;
              }
              cvedix_objects::cvedix_point p;
              p.x = static_cast<int>(coord["x"].asDouble());
              p.y = static_cast<int>(coord["y"].asDouble());
              roiPoints.push_back(p);
            }
            if (!ok || roiPoints.empty()) {
              std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Invalid ROI at index " << i
                        << " - skipping" << std::endl;
              continue;
            }
            // Use array index as channel (0, 1, 2, ...)
            // Or use channel from stopObj if provided
            int channel = static_cast<int>(i);
            if (stopObj.isMember("channel") && stopObj["channel"].isNumeric()) {
              channel = stopObj["channel"].asInt();
            }
            stops[channel] = std::move(roiPoints);
            stopsParsed = true;
          }

          if (stopsParsed) {
            std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] ✓ Parsed " << stops.size()
                      << " stop zone(s) from StopZones API" << std::endl;
          }
        } else {
          std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Failed to parse StopZones "
                       "JSON or not an array, falling back to solution config"
                    << std::endl;
        }
      } catch (const std::exception &e) {
        std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] WARNING: Exception parsing StopZones "
                     "JSON: "
                  << e.what() << ", falling back to solution config"
                  << std::endl;
      }
    }

    // Priority 2: Fallback to solution config parameters if no stops from API
    if (!stopsParsed) {
      // Check if we have stop parameters from solution config
      // For ba_stop, we typically use StopZones JSON in params
      if (params.count("StopZones") && !params.at("StopZones").empty()) {
        try {
          Json::Reader reader;
          Json::Value parsedStops;
          if (reader.parse(params.at("StopZones"), parsedStops) &&
              parsedStops.isArray()) {
            for (Json::ArrayIndex i = 0; i < parsedStops.size(); ++i) {
              const Json::Value &stopObj = parsedStops[i];
              if (!stopObj.isMember("roi") || !stopObj["roi"].isArray()) {
                continue;
              }
              std::vector<cvedix_objects::cvedix_point> roiPoints;
              for (const auto &coord : stopObj["roi"]) {
                if (coord.isObject() && coord.isMember("x") &&
                    coord.isMember("y") && coord["x"].isNumeric() &&
                    coord["y"].isNumeric()) {
                  cvedix_objects::cvedix_point p;
                  p.x = static_cast<int>(coord["x"].asDouble());
                  p.y = static_cast<int>(coord["y"].asDouble());
                  roiPoints.push_back(p);
                }
              }
              if (!roiPoints.empty()) {
                int channel = static_cast<int>(i);
                if (stopObj.isMember("channel") && stopObj["channel"].isNumeric()) {
                  channel = stopObj["channel"].asInt();
                }
                stops[channel] = std::move(roiPoints);
                stopsParsed = true;
              }
            }
          }
        } catch (const std::exception &e) {
          std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Warning: Failed to parse StopZones "
                       "from solution config: "
                    << e.what() << std::endl;
        }
      }

      if (!stopsParsed) {
        std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] No valid stop zone configuration found. "
                     "BA stop node will be created without stop zones. "
                     "Stop zones can be added later via API."
                  << std::endl;
      }
    }

    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Creating BA stop node:" << std::endl;
    std::cerr << "  Name: '" << nodeName << "'" << std::endl;
    std::cerr << "  Stop zones configured for " << stops.size() << " channel(s)"
              << std::endl;

    auto node =
        std::make_shared<cvedix_nodes::cvedix_ba_stop_node>(nodeName, stops);
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] ✓ BA stop node created successfully"
              << std::endl;
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes]   Stop zones will be passed to OSD node via "
                 "pipeline metadata"
              << std::endl;
    std::cerr
        << "[PipelineBuilderBehaviorAnalysisNodes]   OSD node will draw these stop zones on video frames"
        << std::endl;
    return node;
  } catch (const std::exception &e) {
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Exception in createBAStopNode: " << e.what()
              << std::endl;
    throw;
  }
}

std::shared_ptr<cvedix_nodes::cvedix_node>
PipelineBuilderBehaviorAnalysisNodes::createBACrosslineOSDNode(
    const std::string &nodeName,
    const std::map<std::string, std::string> &params) {

  try {
    if (nodeName.empty()) {
      throw std::invalid_argument("Node name cannot be empty");
    }

    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Creating BA crossline OSD node:"
              << std::endl;
    std::cerr << "  Name: '" << nodeName << "'" << std::endl;
    std::cerr << "  Note: OSD node will automatically get lines from "
                 "ba_crossline_node via pipeline metadata"
              << std::endl;

    auto node =
        std::make_shared<cvedix_nodes::cvedix_ba_crossline_osd_node>(nodeName);

    std::cerr
        << "[PipelineBuilderBehaviorAnalysisNodes] ✓ BA crossline OSD node created successfully"
        << std::endl;
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes]   OSD node will draw lines on video frames "
                 "from ba_crossline_node"
              << std::endl;
    return node;
  } catch (const std::exception &e) {
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Exception in createBACrosslineOSDNode: "
              << e.what() << std::endl;
    throw;
  }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilderBehaviorAnalysisNodes::createBAJamOSDNode(
    const std::string &nodeName,
    const std::map<std::string, std::string> &params) {

  try {
    if (nodeName.empty()) {
      throw std::invalid_argument("Node name cannot be empty");
    }

    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Creating BA jam OSD node:" << std::endl;
    std::cerr << "  Name: '" << nodeName << "'" << std::endl;
    std::cerr << "  Note: OSD node will automatically get lines from "
                 "ba_jam_node via pipeline metadata"
              << std::endl;

    auto node =
        std::make_shared<cvedix_nodes::cvedix_ba_jam_osd_node>(nodeName);
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] ✓ BA jam OSD node created successfully"
              << std::endl;
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes]   OSD node will draw lines on video frames "
                 "from ba_jam_node"
              << std::endl;
    return node;
  } catch (const std::exception &e) {
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Exception in createBAJamOSDNode: "
              << e.what() << std::endl;
    throw;
  }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilderBehaviorAnalysisNodes::createBAStopOSDNode(
    const std::string &nodeName,
    const std::map<std::string, std::string> &params) {

  try {
    if (nodeName.empty()) {
      throw std::invalid_argument("Node name cannot be empty");
    }

    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Creating BA stop OSD node:" << std::endl;
    std::cerr << "  Name: '" << nodeName << "'" << std::endl;
    std::cerr << "  Note: OSD node will automatically get stop zones from "
                 "ba_stop_node via pipeline metadata"
              << std::endl;

    auto node =
        std::make_shared<cvedix_nodes::cvedix_ba_stop_osd_node>(nodeName);
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] ✓ BA stop OSD node created successfully"
              << std::endl;
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes]   OSD node will draw stop zones on video frames "
                 "from ba_stop_node"
              << std::endl;
    return node;
  } catch (const std::exception &e) {
    std::cerr << "[PipelineBuilderBehaviorAnalysisNodes] Exception in createBAStopOSDNode: "
              << e.what() << std::endl;
    throw;
  }
}