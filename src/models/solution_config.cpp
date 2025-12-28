#include "models/solution_config.h"
#include <regex>
#include <sstream>

std::string SolutionConfig::getNodeName(const std::string &templateName,
                                        const std::string &instanceId) const {
  std::string result = templateName;

  // Replace {instanceId} placeholder
  std::regex pattern("\\{instanceId\\}");
  result = std::regex_replace(result, pattern, instanceId);

  return result;
}

std::string SolutionConfig::getParameter(
    const std::string &key,
    const std::map<std::string, std::string> &requestParams,
    const std::string &instanceId) const {
  // First check if parameter exists in defaults
  auto it = defaults.find(key);
  if (it != defaults.end()) {
    std::string value = it->second;

    // Replace placeholders
    std::regex instanceId_pattern("\\{instanceId\\}");
    value = std::regex_replace(value, instanceId_pattern, instanceId);

    // Replace request parameter placeholders
    for (const auto &param : requestParams) {
      std::regex param_pattern("\\$\\{" + param.first + "\\}");
      value = std::regex_replace(value, param_pattern, param.second);
    }

    return value;
  }

  // Check request parameters
  auto reqIt = requestParams.find(key);
  if (reqIt != requestParams.end()) {
    return reqIt->second;
  }

  return "";
}
