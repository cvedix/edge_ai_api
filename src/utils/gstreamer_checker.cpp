#include "gstreamer_checker.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>

namespace GStreamerChecker {

bool checkPlugin(const std::string &pluginName) {
  std::string command = "gst-inspect-1.0 " + pluginName + " >/dev/null 2>&1";
  int status = std::system(command.c_str());
  return status == 0;
}

std::map<std::string, PluginInfo> checkRequiredPlugins() {
  std::map<std::string, PluginInfo> plugins;

  // Required plugins for file source (MP4/H.264)
  // Note: In GStreamer 1.24+, qtdemux is part of isomp4 plugin
  // Check isomp4 plugin which contains qtdemux element
  plugins["isomp4"] = {"isomp4", "ISO MP4 plugin (contains qtdemux)",
                        "gstreamer1.0-plugins-good", true, false};
  plugins["qtdemux"] = {"qtdemux", "MP4 demuxer (for video files) - part of isomp4",
                        "gstreamer1.0-plugins-good", false, false}; // Optional, check via isomp4
  plugins["h264parse"] = {"h264parse", "H.264 parser",
                          "gstreamer1.0-plugins-good", true, false};
  plugins["avdec_h264"] = {"avdec_h264", "H.264 decoder (libav)",
                           "gstreamer1.0-libav", true, false};

  // Required plugins for RTMP output
  plugins["x264enc"] = {"x264enc", "H.264 encoder (x264)",
                        "gstreamer1.0-plugins-ugly", true, false};

  // Alternative encoders (optional but useful)
  plugins["openh264enc"] = {"openh264enc", "H.264 encoder (OpenH264)",
                            "gstreamer1.0-plugins-bad", false, false};

  // RTMP plugins
  plugins["flvmux"] = {"flvmux", "FLV muxer (for RTMP)",
                       "gstreamer1.0-plugins-good", true, false};
  plugins["rtmpsink"] = {"rtmpsink", "RTMP sink", "gstreamer1.0-plugins-bad",
                         true, false};

  // Common plugins
  plugins["filesrc"] = {"filesrc", "File source (for reading video files)",
                        "gstreamer1.0-plugins-base", true, false};
  plugins["videoconvert"] = {"videoconvert", "Video format converter",
                             "gstreamer1.0-plugins-base", true, false};
  plugins["appsink"] = {"appsink", "Application sink",
                        "gstreamer1.0-plugins-base", true, false};
  plugins["appsrc"] = {"appsrc", "Application source",
                       "gstreamer1.0-plugins-base", true, false};

  // Check availability
  for (auto &[name, info] : plugins) {
    info.available = checkPlugin(name);
  }

  return plugins;
}

std::string
getInstallationCommand(const std::vector<std::string> &missingPlugins) {
  std::map<std::string, std::string> packageMap = {
      {"avdec_h264", "gstreamer1.0-libav"},
      {"x264enc", "gstreamer1.0-plugins-ugly"},
      {"openh264enc", "gstreamer1.0-plugins-bad"},
      {"isomp4", "gstreamer1.0-plugins-good"},
      {"qtdemux", "gstreamer1.0-plugins-good"},
      {"h264parse", "gstreamer1.0-plugins-good"},
      {"flvmux", "gstreamer1.0-plugins-good"},
      {"rtmpsink", "gstreamer1.0-plugins-bad"},
      {"filesrc", "gstreamer1.0-plugins-base"},
      {"videoconvert", "gstreamer1.0-plugins-base"},
      {"appsink", "gstreamer1.0-plugins-base"},
      {"appsrc", "gstreamer1.0-plugins-base"}};

  std::set<std::string> packagesToInstall;
  for (const auto &plugin : missingPlugins) {
    auto it = packageMap.find(plugin);
    if (it != packageMap.end()) {
      packagesToInstall.insert(it->second);
    }
  }

  if (packagesToInstall.empty()) {
    return "";
  }

  std::ostringstream cmd;
  cmd << "sudo apt-get update && sudo apt-get install -y";
  for (const auto &pkg : packagesToInstall) {
    cmd << " " << pkg;
  }
  return cmd.str();
}

void printPluginStatus(bool verbose) {
  auto plugins = checkRequiredPlugins();

  std::cerr << "\n[GStreamerChecker] ========================================"
            << std::endl;
  std::cerr << "[GStreamerChecker] GStreamer Plugin Status Check" << std::endl;
  std::cerr << "[GStreamerChecker] ========================================"
            << std::endl;

  std::vector<std::string> missingRequired;
  std::vector<std::string> missingOptional;

  for (const auto &[name, info] : plugins) {
    if (info.required) {
      if (!info.available) {
        missingRequired.push_back(name);
        std::cerr << "[GStreamerChecker] ✗ MISSING (REQUIRED): " << name
                  << " - " << info.description << std::endl;
        std::cerr << "[GStreamerChecker]   Package: " << info.package
                  << std::endl;
      } else if (verbose) {
        std::cerr << "[GStreamerChecker] ✓ Available: " << name << " - "
                  << info.description << std::endl;
      }
    } else {
      if (!info.available && verbose) {
        missingOptional.push_back(name);
        std::cerr << "[GStreamerChecker] ⚠ Missing (optional): " << name
                  << " - " << info.description << std::endl;
      } else if (verbose && info.available) {
        std::cerr << "[GStreamerChecker] ✓ Available: " << name << " - "
                  << info.description << std::endl;
      }
    }
  }

  if (!missingRequired.empty()) {
    std::cerr << "\n[GStreamerChecker] ========================================"
              << std::endl;
    std::cerr << "[GStreamerChecker] ⚠ WARNING: Missing required plugins!"
              << std::endl;
    std::cerr << "[GStreamerChecker] ========================================"
              << std::endl;
    std::cerr << "[GStreamerChecker] Missing plugins: ";
    for (size_t i = 0; i < missingRequired.size(); ++i) {
      std::cerr << missingRequired[i];
      if (i < missingRequired.size() - 1)
        std::cerr << ", ";
    }
    std::cerr << std::endl;

    std::string installCmd = getInstallationCommand(missingRequired);
    if (!installCmd.empty()) {
      std::cerr << "\n[GStreamerChecker] To install missing plugins, run:"
                << std::endl;
      std::cerr << "[GStreamerChecker]   " << installCmd << std::endl;
    }
    std::cerr << "[GStreamerChecker] ========================================\n"
              << std::endl;
  } else {
    std::cerr << "[GStreamerChecker] ✓ All required plugins are available"
              << std::endl;
    std::cerr << "[GStreamerChecker] ========================================\n"
              << std::endl;
  }
}

bool validatePlugins(bool autoSuggest) {
  auto plugins = checkRequiredPlugins();

  std::vector<std::string> missingRequired;
  for (const auto &[name, info] : plugins) {
    if (info.required && !info.available) {
      missingRequired.push_back(name);
    }
  }

  if (!missingRequired.empty()) {
    printPluginStatus(false);

    if (autoSuggest) {
      std::string installCmd = getInstallationCommand(missingRequired);
      if (!installCmd.empty()) {
        std::cerr
            << "[GStreamerChecker] NOTE: You can install missing plugins with:"
            << std::endl;
        std::cerr << "[GStreamerChecker]   " << installCmd << std::endl;
      }
    }

    return false;
  }

  return true;
}
} // namespace GStreamerChecker
