#pragma once

#include <map>
#include <string>
#include <vector>

namespace GStreamerChecker {

// Structure to hold plugin information
struct PluginInfo {
  std::string name;
  std::string description;
  std::string package; // Package name to install
  bool required;       // Is this plugin required for basic functionality
  bool available;      // Is this plugin available
};

// Check if a specific GStreamer plugin/element is available
bool checkPlugin(const std::string &pluginName);

// Check all required plugins for the application
std::map<std::string, PluginInfo> checkRequiredPlugins();

// Get installation command for missing plugins
std::string
getInstallationCommand(const std::vector<std::string> &missingPlugins);

// Print plugin status report
void printPluginStatus(bool verbose = false);

// Check and report missing plugins (returns true if all required plugins are
// available)
bool validatePlugins(bool autoSuggest = true);
} // namespace GStreamerChecker
