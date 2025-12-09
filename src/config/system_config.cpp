#include "config/system_config.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

SystemConfig& SystemConfig::getInstance() {
    static SystemConfig instance;
    return instance;
}

bool SystemConfig::loadConfig(const std::string& configPath) {
    config_path_ = configPath;
    
    try {
        if (!std::filesystem::exists(configPath)) {
            std::cerr << "[SystemConfig] Config file not found: " << configPath << std::endl;
            std::cerr << "[SystemConfig] Initializing with default configuration" << std::endl;
            
            {
                std::lock_guard<std::mutex> lock(mutex_);
                initializeDefaults();
                loaded_ = true;
            }
            
            // Save default config to file (outside lock to avoid deadlock)
            saveConfig(configPath);
            return true;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ifstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "[SystemConfig] Error: Failed to open config file: " << configPath << std::endl;
            initializeDefaults();
            loaded_ = false;
            return false;
        }
        
        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, file, &config_json_, &errors)) {
            std::cerr << "[SystemConfig] Failed to parse config file: " << errors << std::endl;
            initializeDefaults();
            loaded_ = false;
            return false;
        }
        
        if (!validateConfig(config_json_)) {
            std::cerr << "[SystemConfig] Invalid config structure, using defaults" << std::endl;
            initializeDefaults();
            loaded_ = false;
            return false;
        }
        
        loaded_ = true;
        std::cerr << "[SystemConfig] Successfully loaded config from: " << configPath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SystemConfig] Exception loading config: " << e.what() << std::endl;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            initializeDefaults();
            loaded_ = false;
        }
        return false;
    }
}

bool SystemConfig::saveConfig(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string path = configPath.empty() ? config_path_ : configPath;
    if (path.empty()) {
        std::cerr << "[SystemConfig] Error: No config path specified" << std::endl;
        return false;
    }
    
    try {
        // Create parent directory if needed
        std::filesystem::path filePath(path);
        if (filePath.has_parent_path()) {
            std::filesystem::create_directories(filePath.parent_path());
        }
        
        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "[SystemConfig] Error: Failed to open file for writing: " << path << std::endl;
            return false;
        }
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  "; // 2 spaces for indentation
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(config_json_, &file);
        file.close();
        
        if (configPath.empty()) {
            config_path_ = path;
        }
        
        std::cerr << "[SystemConfig] Successfully saved config to: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SystemConfig] Exception saving config: " << e.what() << std::endl;
        return false;
    }
}

void SystemConfig::initializeDefaults() {
    config_json_ = Json::Value(Json::objectValue);
    
    // auto_device_list
    Json::Value autoDeviceList(Json::arrayValue);
    autoDeviceList.append("hailo.auto");
    autoDeviceList.append("blaize.auto");
    autoDeviceList.append("tensorrt.1");
    autoDeviceList.append("rknn.auto");
    autoDeviceList.append("tensorrt.2");
    autoDeviceList.append("cavalry");
    autoDeviceList.append("openvino.VPU");
    autoDeviceList.append("openvino.GPU");
    autoDeviceList.append("openvino.CPU");
    autoDeviceList.append("snpe.dsp");
    autoDeviceList.append("snpe.aip");
    autoDeviceList.append("mnn.auto");
    autoDeviceList.append("armnn.GpuAcc");
    autoDeviceList.append("armnn.CpuAcc");
    autoDeviceList.append("armnn.CpuRef");
    autoDeviceList.append("memx.memx");
    autoDeviceList.append("memx.cpu");
    config_json_["auto_device_list"] = autoDeviceList;
    
    // decoder_priority_list
    Json::Value decoderPriorityList(Json::arrayValue);
    decoderPriorityList.append("blaize.auto");
    decoderPriorityList.append("rockchip");
    decoderPriorityList.append("nvidia.1");
    decoderPriorityList.append("intel.1");
    decoderPriorityList.append("software");
    config_json_["decoder_priority_list"] = decoderPriorityList;
    
    // gstreamer
    Json::Value gstreamer(Json::objectValue);
    
    // decode_pipelines
    Json::Value decodePipelines(Json::objectValue);
    
    Json::Value autoPipeline(Json::objectValue);
    autoPipeline["pipeline"] = "decodebin ! videoconvert";
    Json::Value autoCaps(Json::arrayValue);
    autoCaps.append("H264");
    autoCaps.append("HEVC");
    autoCaps.append("VP9");
    autoCaps.append("VC1");
    autoCaps.append("AV1");
    autoCaps.append("MJPEG");
    autoPipeline["capabilities"] = autoCaps;
    decodePipelines["auto"] = autoPipeline;
    
    Json::Value jetsonPipeline(Json::objectValue);
    jetsonPipeline["pipeline"] = "parsebin ! nvv4l2decoder ! nvvidconv";
    Json::Value jetsonCaps(Json::arrayValue);
    jetsonCaps.append("H264");
    jetsonCaps.append("HEVC");
    jetsonPipeline["capabilities"] = jetsonCaps;
    decodePipelines["jetson"] = jetsonPipeline;
    
    Json::Value nvidiaPipeline(Json::objectValue);
    nvidiaPipeline["pipeline"] = "decodebin ! nvvideoconvert ! videoconvert";
    Json::Value nvidiaCaps(Json::arrayValue);
    nvidiaCaps.append("H264");
    nvidiaCaps.append("HEVC");
    nvidiaCaps.append("VP9");
    nvidiaCaps.append("AV1");
    nvidiaCaps.append("MJPEG");
    nvidiaPipeline["capabilities"] = nvidiaCaps;
    decodePipelines["nvidia"] = nvidiaPipeline;
    
    Json::Value msdkPipeline(Json::objectValue);
    msdkPipeline["pipeline"] = "decodebin ! msdkvpp ! videoconvert";
    Json::Value msdkCaps(Json::arrayValue);
    msdkCaps.append("H264");
    msdkCaps.append("HEVC");
    msdkCaps.append("VP9");
    msdkCaps.append("VC1");
    msdkPipeline["capabilities"] = msdkCaps;
    decodePipelines["msdk"] = msdkPipeline;
    
    Json::Value vaapiPipeline(Json::objectValue);
    vaapiPipeline["pipeline"] = "decodebin ! vaapipostproc ! videoconvert";
    Json::Value vaapiCaps(Json::arrayValue);
    vaapiCaps.append("H264");
    vaapiCaps.append("HEVC");
    vaapiCaps.append("VP9");
    vaapiCaps.append("AV1");
    vaapiPipeline["capabilities"] = vaapiCaps;
    decodePipelines["vaapi"] = vaapiPipeline;
    
    gstreamer["decode_pipelines"] = decodePipelines;
    
    // plugin_rank
    Json::Value pluginRank(Json::objectValue);
    pluginRank["nvv4l2decoder"] = "257";
    pluginRank["nvjpegdec"] = "257";
    pluginRank["nvjpegenc"] = "257";
    pluginRank["nvvidconv"] = "257";
    pluginRank["msdkvpp"] = "257";
    pluginRank["vaapipostproc"] = "257";
    pluginRank["vpldec"] = "257";
    pluginRank["qsv"] = "300";
    pluginRank["qsvh265dec"] = "300";
    pluginRank["qsvh264dec"] = "300";
    pluginRank["qsvh265enc"] = "300";
    pluginRank["qsvh264enc"] = "300";
    pluginRank["amfh264dec"] = "300";
    pluginRank["amfh265dec"] = "300";
    pluginRank["amfhvp9dec"] = "300";
    pluginRank["amfhav1dec"] = "300";
    pluginRank["nvh264dec"] = "257";
    pluginRank["nvh265dec"] = "257";
    pluginRank["nvh264enc"] = "257";
    pluginRank["nvh265enc"] = "257";
    pluginRank["nvvp9dec"] = "257";
    pluginRank["nvvp9enc"] = "257";
    pluginRank["nvmpeg4videodec"] = "257";
    pluginRank["nvmpeg2videodec"] = "257";
    pluginRank["nvmpegvideodec"] = "257";
    pluginRank["mpph264enc"] = "256";
    pluginRank["mpph265enc"] = "256";
    pluginRank["mppvp8enc"] = "256";
    pluginRank["mppjpegenc"] = "256";
    pluginRank["mppvideodec"] = "256";
    pluginRank["mppjpegdec"] = "256";
    gstreamer["plugin_rank"] = pluginRank;
    
    config_json_["gstreamer"] = gstreamer;
    
    // system
    Json::Value system(Json::objectValue);
    
    // web_server
    Json::Value webServer(Json::objectValue);
    webServer["enabled"] = true;
    webServer["ip_address"] = "0.0.0.0";
    webServer["port"] = 3546;
    webServer["name"] = "default";
    Json::Value cors(Json::objectValue);
    cors["enabled"] = false;
    webServer["cors"] = cors;
    system["web_server"] = webServer;
    
    // logging
    Json::Value logging(Json::objectValue);
    logging["log_file"] = "logs/api.log";
    logging["log_level"] = "debug";
    logging["max_log_file_size"] = 52428800;
    logging["max_log_files"] = 3;
    system["logging"] = logging;
    
    system["max_running_instances"] = 0; // 0 = unlimited
    system["modelforge_permissive"] = false;
    
    config_json_["system"] = system;
}

bool SystemConfig::validateConfig(const Json::Value& json) const {
    // Basic validation - check if it's an object
    if (!json.isObject()) {
        return false;
    }
    
    // Check for required top-level keys (optional validation)
    // We allow partial configs, so we don't require all keys
    
    return true;
}

int SystemConfig::getMaxRunningInstances() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (config_json_.isMember("system") && 
        config_json_["system"].isMember("max_running_instances") &&
        config_json_["system"]["max_running_instances"].isInt()) {
        return config_json_["system"]["max_running_instances"].asInt();
    }
    
    return 0; // Default: unlimited
}

void SystemConfig::setMaxRunningInstances(int maxInstances) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!config_json_.isMember("system")) {
        config_json_["system"] = Json::Value(Json::objectValue);
    }
    
    config_json_["system"]["max_running_instances"] = maxInstances;
}

std::vector<std::string> SystemConfig::getAutoDeviceList() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    
    if (config_json_.isMember("auto_device_list") && 
        config_json_["auto_device_list"].isArray()) {
        for (const auto& item : config_json_["auto_device_list"]) {
            if (item.isString()) {
                result.push_back(item.asString());
            }
        }
    }
    
    return result;
}

void SystemConfig::setAutoDeviceList(const std::vector<std::string>& devices) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Json::Value array(Json::arrayValue);
    for (const auto& device : devices) {
        array.append(device);
    }
    config_json_["auto_device_list"] = array;
}

std::vector<std::string> SystemConfig::getDecoderPriorityList() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    
    if (config_json_.isMember("decoder_priority_list") && 
        config_json_["decoder_priority_list"].isArray()) {
        for (const auto& item : config_json_["decoder_priority_list"]) {
            if (item.isString()) {
                result.push_back(item.asString());
            }
        }
    }
    
    return result;
}

void SystemConfig::setDecoderPriorityList(const std::vector<std::string>& decoders) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Json::Value array(Json::arrayValue);
    for (const auto& decoder : decoders) {
        array.append(decoder);
    }
    config_json_["decoder_priority_list"] = array;
}

SystemConfig::WebServerConfig SystemConfig::getWebServerConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    WebServerConfig config;
    
    if (config_json_.isMember("system") && 
        config_json_["system"].isMember("web_server")) {
        const auto& ws = config_json_["system"]["web_server"];
        
        if (ws.isMember("enabled") && ws["enabled"].isBool()) {
            config.enabled = ws["enabled"].asBool();
        }
        if (ws.isMember("ip_address") && ws["ip_address"].isString()) {
            config.ipAddress = ws["ip_address"].asString();
        }
        if (ws.isMember("port") && ws["port"].isInt()) {
            config.port = static_cast<uint16_t>(ws["port"].asInt());
        }
        if (ws.isMember("name") && ws["name"].isString()) {
            config.name = ws["name"].asString();
        }
        if (ws.isMember("cors") && ws["cors"].isMember("enabled") && 
            ws["cors"]["enabled"].isBool()) {
            config.corsEnabled = ws["cors"]["enabled"].asBool();
        }
    }
    
    return config;
}

void SystemConfig::setWebServerConfig(const WebServerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!config_json_.isMember("system")) {
        config_json_["system"] = Json::Value(Json::objectValue);
    }
    
    Json::Value webServer(Json::objectValue);
    webServer["enabled"] = config.enabled;
    webServer["ip_address"] = config.ipAddress;
    webServer["port"] = config.port;
    webServer["name"] = config.name;
    
    Json::Value cors(Json::objectValue);
    cors["enabled"] = config.corsEnabled;
    webServer["cors"] = cors;
    
    config_json_["system"]["web_server"] = webServer;
}

SystemConfig::LoggingConfig SystemConfig::getLoggingConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    LoggingConfig config;
    
    if (config_json_.isMember("system") && 
        config_json_["system"].isMember("logging")) {
        const auto& log = config_json_["system"]["logging"];
        
        if (log.isMember("log_file") && log["log_file"].isString()) {
            config.logFile = log["log_file"].asString();
        }
        if (log.isMember("log_level") && log["log_level"].isString()) {
            config.logLevel = log["log_level"].asString();
        }
        if (log.isMember("max_log_file_size") && log["max_log_file_size"].isInt()) {
            config.maxLogFileSize = static_cast<size_t>(log["max_log_file_size"].asInt());
        }
        if (log.isMember("max_log_files") && log["max_log_files"].isInt()) {
            config.maxLogFiles = log["max_log_files"].asInt();
        }
    }
    
    return config;
}

void SystemConfig::setLoggingConfig(const LoggingConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!config_json_.isMember("system")) {
        config_json_["system"] = Json::Value(Json::objectValue);
    }
    
    Json::Value logging(Json::objectValue);
    logging["log_file"] = config.logFile;
    logging["log_level"] = config.logLevel;
    logging["max_log_file_size"] = static_cast<Json::Int64>(config.maxLogFileSize);
    logging["max_log_files"] = config.maxLogFiles;
    
    config_json_["system"]["logging"] = logging;
}

bool SystemConfig::getModelforgePermissive() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (config_json_.isMember("system") && 
        config_json_["system"].isMember("modelforge_permissive") &&
        config_json_["system"]["modelforge_permissive"].isBool()) {
        return config_json_["system"]["modelforge_permissive"].asBool();
    }
    
    return false;
}

void SystemConfig::setModelforgePermissive(bool permissive) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!config_json_.isMember("system")) {
        config_json_["system"] = Json::Value(Json::objectValue);
    }
    
    config_json_["system"]["modelforge_permissive"] = permissive;
}

std::string SystemConfig::getGStreamerPipeline(const std::string& platform) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (config_json_.isMember("gstreamer") && 
        config_json_["gstreamer"].isMember("decode_pipelines") &&
        config_json_["gstreamer"]["decode_pipelines"].isMember(platform) &&
        config_json_["gstreamer"]["decode_pipelines"][platform].isMember("pipeline") &&
        config_json_["gstreamer"]["decode_pipelines"][platform]["pipeline"].isString()) {
        return config_json_["gstreamer"]["decode_pipelines"][platform]["pipeline"].asString();
    }
    
    return "";
}

void SystemConfig::setGStreamerPipeline(const std::string& platform, const std::string& pipeline) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!config_json_.isMember("gstreamer")) {
        config_json_["gstreamer"] = Json::Value(Json::objectValue);
    }
    if (!config_json_["gstreamer"].isMember("decode_pipelines")) {
        config_json_["gstreamer"]["decode_pipelines"] = Json::Value(Json::objectValue);
    }
    if (!config_json_["gstreamer"]["decode_pipelines"].isMember(platform)) {
        config_json_["gstreamer"]["decode_pipelines"][platform] = Json::Value(Json::objectValue);
    }
    
    config_json_["gstreamer"]["decode_pipelines"][platform]["pipeline"] = pipeline;
}

std::vector<std::string> SystemConfig::getGStreamerCapabilities(const std::string& platform) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    
    if (config_json_.isMember("gstreamer") && 
        config_json_["gstreamer"].isMember("decode_pipelines") &&
        config_json_["gstreamer"]["decode_pipelines"].isMember(platform) &&
        config_json_["gstreamer"]["decode_pipelines"][platform].isMember("capabilities") &&
        config_json_["gstreamer"]["decode_pipelines"][platform]["capabilities"].isArray()) {
        for (const auto& cap : config_json_["gstreamer"]["decode_pipelines"][platform]["capabilities"]) {
            if (cap.isString()) {
                result.push_back(cap.asString());
            }
        }
    }
    
    return result;
}

void SystemConfig::setGStreamerCapabilities(const std::string& platform, const std::vector<std::string>& capabilities) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!config_json_.isMember("gstreamer")) {
        config_json_["gstreamer"] = Json::Value(Json::objectValue);
    }
    if (!config_json_["gstreamer"].isMember("decode_pipelines")) {
        config_json_["gstreamer"]["decode_pipelines"] = Json::Value(Json::objectValue);
    }
    if (!config_json_["gstreamer"]["decode_pipelines"].isMember(platform)) {
        config_json_["gstreamer"]["decode_pipelines"][platform] = Json::Value(Json::objectValue);
    }
    
    Json::Value caps(Json::arrayValue);
    for (const auto& cap : capabilities) {
        caps.append(cap);
    }
    config_json_["gstreamer"]["decode_pipelines"][platform]["capabilities"] = caps;
}

std::string SystemConfig::getGStreamerPluginRank(const std::string& pluginName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (config_json_.isMember("gstreamer") && 
        config_json_["gstreamer"].isMember("plugin_rank") &&
        config_json_["gstreamer"]["plugin_rank"].isMember(pluginName) &&
        config_json_["gstreamer"]["plugin_rank"][pluginName].isString()) {
        return config_json_["gstreamer"]["plugin_rank"][pluginName].asString();
    }
    
    return "";
}

void SystemConfig::setGStreamerPluginRank(const std::string& pluginName, const std::string& rank) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!config_json_.isMember("gstreamer")) {
        config_json_["gstreamer"] = Json::Value(Json::objectValue);
    }
    if (!config_json_["gstreamer"].isMember("plugin_rank")) {
        config_json_["gstreamer"]["plugin_rank"] = Json::Value(Json::objectValue);
    }
    
    config_json_["gstreamer"]["plugin_rank"][pluginName] = rank;
}

Json::Value SystemConfig::getConfigJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_json_;
}

Json::Value SystemConfig::getConfigSection(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto [parent, key] = parsePath(path);
    if (!parent || !parent->isMember(key)) {
        return Json::Value(Json::nullValue);
    }
    
    return (*parent)[key];
}

bool SystemConfig::updateConfig(const Json::Value& json) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!json.isObject()) {
        return false;
    }
    
    // Merge JSON objects recursively
    for (const auto& key : json.getMemberNames()) {
        config_json_[key] = json[key];
    }
    
    return true;
}

bool SystemConfig::replaceConfig(const Json::Value& json) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!validateConfig(json)) {
        return false;
    }
    
    config_json_ = json;
    return true;
}

bool SystemConfig::updateConfigSection(const std::string& path, const Json::Value& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto [parent, key] = parsePath(path);
    if (!parent) {
        return false;
    }
    
    (*parent)[key] = value;
    return true;
}

bool SystemConfig::deleteConfigSection(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto [parent, key] = parsePath(path);
    if (!parent || !parent->isMember(key)) {
        return false;
    }
    
    parent->removeMember(key);
    return true;
}

std::string SystemConfig::getConfigPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_path_;
}

bool SystemConfig::reloadConfig() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (config_path_.empty()) {
        return false;
    }
    
    // Release lock before calling loadConfig (which will acquire it)
    std::string path = config_path_;
    mutex_.unlock();
    
    bool result = loadConfig(path);
    
    mutex_.lock();
    return result;
}

bool SystemConfig::resetToDefaults() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Initialize default configuration
    initializeDefaults();
    loaded_ = true;
    
    // Save to file if config path is set
    if (!config_path_.empty()) {
        std::string path = config_path_;
        mutex_.unlock();
        
        bool saved = saveConfig(path);
        
        mutex_.lock();
        return saved;
    }
    
    return true;
}

bool SystemConfig::isLoaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loaded_;
}

std::pair<Json::Value*, std::string> SystemConfig::parsePath(const std::string& path) const {
    if (path.empty()) {
        return {nullptr, ""};
    }
    
    // Split path by dots or forward slashes (support both formats)
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string item;
    
    // Check if path contains forward slashes
    bool useSlash = (path.find('/') != std::string::npos);
    char delimiter = useSlash ? '/' : '.';
    
    while (std::getline(ss, item, delimiter)) {
        if (!item.empty()) {
            parts.push_back(item);
        }
    }
    
    if (parts.empty()) {
        return {nullptr, ""};
    }
    
    // Navigate to parent
    Json::Value* current = const_cast<Json::Value*>(&config_json_);
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        if (!current->isMember(parts[i]) || !(*current)[parts[i]].isObject()) {
            return {nullptr, ""};
        }
        current = &((*current)[parts[i]]);
    }
    
    return {current, parts.back()};
}

