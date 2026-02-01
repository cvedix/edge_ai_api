#include "core/pipeline_builder_other_nodes.h"
#include "core/pipeline_builder_model_resolver.h"
#include <iostream>
#include <stdexcept>
#include <cvedix/nodes/track/cvedix_sort_track_node.h>
#include <cvedix/nodes/osd/cvedix_face_osd_node_v2.h>
#include <cvedix/nodes/osd/cvedix_osd_node_v3.h>
#include <filesystem>

namespace fs = std::filesystem;



std::shared_ptr<cvedix_nodes::cvedix_node>
PipelineBuilderOtherNodes::createSORTTrackNode(
    const std::string &nodeName,
    const std::map<std::string, std::string> &params) {

  try {
    if (nodeName.empty()) {
      throw std::invalid_argument("Node name cannot be empty");
    }

    std::cerr << "[PipelineBuilderOtherNodes] Creating SORT tracker node:" << std::endl;
    std::cerr << "  Name: '" << nodeName << "'" << std::endl;

    auto node =
        std::make_shared<cvedix_nodes::cvedix_sort_track_node>(nodeName);

    std::cerr << "[PipelineBuilderOtherNodes] ✓ SORT tracker node created successfully"
              << std::endl;
    return node;
  } catch (const std::exception &e) {
    std::cerr << "[PipelineBuilderOtherNodes] Exception in createSORTTrackNode: "
              << e.what() << std::endl;
    throw;
  }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilderOtherNodes::createFaceOSDNode(
    const std::string &nodeName,
    const std::map<std::string, std::string> &params) {

  try {
    // Validate node name
    if (nodeName.empty()) {
      throw std::invalid_argument("Node name cannot be empty");
    }

    std::cerr << "[PipelineBuilderOtherNodes] Creating face OSD v2 node:" << std::endl;
    std::cerr << "  Name: '" << nodeName << "'" << std::endl;

    auto node =
        std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>(nodeName);

    std::cerr << "[PipelineBuilderOtherNodes] ✓ Face OSD v2 node created successfully"
              << std::endl;
    return node;
  } catch (const std::exception &e) {
    std::cerr << "[PipelineBuilderOtherNodes] Exception in createFaceOSDNode: "
              << e.what() << std::endl;
    throw;
  }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilderOtherNodes::createOSDv3Node(
    const std::string &nodeName,
    const std::map<std::string, std::string> &params,
    const CreateInstanceRequest &req) {

  try {
    if (nodeName.empty()) {
      throw std::invalid_argument("Node name cannot be empty");
    }

    // Priority 1: Check additionalParams for FONT_PATH (highest priority -
    // allows runtime override)
    std::string fontPath = "";
    auto it = req.additionalParams.find("FONT_PATH");
    if (it != req.additionalParams.end() && !it->second.empty()) {
      fontPath = it->second;
      std::cerr << "[PipelineBuilderOtherNodes] Using FONT_PATH from additionalParams: "
                << fontPath << std::endl;
    }

    // Priority 2: Get font_path from params if not in additionalParams
    if (fontPath.empty() && params.count("font_path") &&
        !params.at("font_path").empty()) {
      fontPath = params.at("font_path");

      // Check if font file exists, try to resolve path
      fs::path fontFilePath(fontPath);

      // If relative path, try to resolve it
      if (!fontFilePath.is_absolute()) {
        // Try current directory first
        if (fs::exists(fontPath)) {
          fontPath = fs::absolute(fontPath).string();
        } else {
          // Try with CVEDIX_DATA_ROOT or CVEDIX_SDK_ROOT
          const char *dataRoot = std::getenv("CVEDIX_DATA_ROOT");
          if (dataRoot && strlen(dataRoot) > 0) {
            std::string resolvedPath = std::string(dataRoot);
            if (resolvedPath.back() != '/')
              resolvedPath += '/';
            resolvedPath += fontPath;
            if (fs::exists(resolvedPath)) {
              fontPath = resolvedPath;
            }
          }

          // Try CVEDIX_SDK_ROOT
          if (!fs::exists(fontPath)) {
            const char *sdkRoot = std::getenv("CVEDIX_SDK_ROOT");
            if (sdkRoot && strlen(sdkRoot) > 0) {
              std::string resolvedPath = std::string(sdkRoot);
              if (resolvedPath.back() != '/')
                resolvedPath += '/';
              resolvedPath += "cvedix_data/" + fontPath;
              if (fs::exists(resolvedPath)) {
                fontPath = resolvedPath;
              }
            }
          }
        }
      }

      // Check if font file exists after resolution
      if (!fs::exists(fontPath)) {
        std::cerr << "[PipelineBuilderOtherNodes] ⚠ WARNING: Font file not found: '"
                  << params.at("font_path") << "'" << std::endl;
        std::cerr << "[PipelineBuilderOtherNodes] ⚠ Resolved path: '" << fontPath << "'"
                  << std::endl;
        std::cerr
            << "[PipelineBuilderOtherNodes] ⚠ Trying default font from environment..."
            << std::endl;
        fontPath = ""; // Will try default font below
      }
    }

    // Priority 3: If no font_path in params/additionalParams or font file not
    // found, try default font
    if (fontPath.empty()) {
      // Try default font from /opt/edge_ai_api/fonts/ first
      std::string defaultFontPath =
          "/opt/edge_ai_api/fonts/NotoSansCJKsc-Medium.otf";
      if (fs::exists(defaultFontPath)) {
        fontPath = defaultFontPath;
        std::cerr << "[PipelineBuilderOtherNodes] Using default font from "
                     "/opt/edge_ai_api/fonts/"
                  << std::endl;
      } else {
        // Fallback to environment variable resolution
        fontPath = EnvConfig::resolveDefaultFontPath();
      }
    }

    std::cerr << "[PipelineBuilderOtherNodes] Creating OSD v3 node:" << std::endl;
    std::cerr << "  Name: '" << nodeName << "'" << std::endl;
    if (!fontPath.empty()) {
      std::cerr << "  Font path: '" << fontPath << "'" << std::endl;
    } else {
      std::cerr << "  Font: Using default font" << std::endl;
    }

    // Try to create node with font path, if it fails, fallback to default font
    std::shared_ptr<cvedix_nodes::cvedix_node> node;
    try {
      node = std::make_shared<cvedix_nodes::cvedix_osd_node_v3>(nodeName,
                                                                fontPath);
      std::cerr << "[PipelineBuilderOtherNodes] ✓ OSD v3 node created successfully"
                << std::endl;
      return node;
    } catch (const cv::Exception &e) {
      // OpenCV exception (likely font loading failed)
      if (!fontPath.empty()) {
        std::cerr << "[PipelineBuilderOtherNodes] ⚠ WARNING: Failed to load font from '"
                  << fontPath << "': " << e.what() << std::endl;
        std::cerr << "[PipelineBuilderOtherNodes] ⚠ Falling back to default font (no "
                     "Chinese/Unicode support)"
                  << std::endl;
        // Try again with empty font path (default font)
        try {
          node =
              std::make_shared<cvedix_nodes::cvedix_osd_node_v3>(nodeName, "");
          std::cerr << "[PipelineBuilderOtherNodes] ✓ OSD v3 node created successfully "
                       "with default font"
                    << std::endl;
          return node;
        } catch (const std::exception &e2) {
          std::cerr << "[PipelineBuilderOtherNodes] ✗ ERROR: Failed to create OSD v3 "
                       "node even with default font: "
                    << e2.what() << std::endl;
          throw;
        }
      } else {
        // Already using default font, rethrow
        throw;
      }
    } catch (const std::exception &e) {
      // Other exceptions
      std::cerr << "[PipelineBuilderOtherNodes] Exception in createOSDv3Node: "
                << e.what() << std::endl;
      throw;
    }
  } catch (const std::exception &e) {
    std::cerr << "[PipelineBuilderOtherNodes] Exception in createOSDv3Node: " << e.what()
              << std::endl;
    throw;
  }
}