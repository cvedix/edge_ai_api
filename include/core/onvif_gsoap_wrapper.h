#pragma once

#ifdef USE_GSOAP
#include "core/onvif_camera_registry.h"
#include <string>
#include <vector>

/**
 * @brief gSOAP wrapper for ONVIF operations
 * 
 * This class provides a wrapper around gSOAP for ONVIF SOAP operations.
 * It can be used alongside or instead of the manual SOAP implementation.
 * 
 * Note: To use gSOAP with ONVIF, you need to:
 * 1. Generate C++ code from ONVIF WSDL files using wsdl2h and soapcpp2
 * 2. Include the generated headers
 * 3. Link against gSOAP libraries
 * 
 * For now, this is a placeholder that can be extended when gSOAP ONVIF
 * code generation is set up.
 */
class ONVIFGSoapWrapper {
public:
  ONVIFGSoapWrapper();
  ~ONVIFGSoapWrapper();

  /**
   * @brief Check if gSOAP is available and properly configured
   */
  static bool isAvailable();

  /**
   * @brief Get device information using gSOAP
   */
  bool getDeviceInformation(const std::string &endpoint,
                            const std::string &username,
                            const std::string &password,
                            ONVIFCamera &camera);

  /**
   * @brief Get capabilities using gSOAP
   */
  bool getCapabilities(const std::string &endpoint,
                       const std::string &username,
                       const std::string &password,
                       std::string &deviceServiceUrl,
                       std::string &mediaServiceUrl,
                       std::string &ptzServiceUrl);

  /**
   * @brief Get profiles using gSOAP
   */
  std::vector<std::string> getProfiles(const std::string &endpoint,
                                        const std::string &username,
                                        const std::string &password);

  /**
   * @brief Get stream URI using gSOAP
   */
  std::string getStreamUri(const std::string &endpoint,
                           const std::string &profileToken,
                           const std::string &username,
                           const std::string &password);

  /**
   * @brief Get video encoder configuration using gSOAP
   */
  bool getVideoEncoderConfiguration(const std::string &endpoint,
                                     const std::string &configurationToken,
                                     const std::string &username,
                                     const std::string &password,
                                     int &width, int &height, int &fps);

private:
  // gSOAP context (will be added when gSOAP code is generated)
  // struct soap *soap_;
};

#else // !USE_GSOAP

// Stub implementation when gSOAP is not available
class ONVIFGSoapWrapper {
public:
  static bool isAvailable() { return false; }
  
  bool getDeviceInformation(const std::string &, const std::string &,
                            const std::string &, ONVIFCamera &) { return false; }
  bool getCapabilities(const std::string &, const std::string &,
                       const std::string &, std::string &, std::string &, std::string &) { return false; }
  std::vector<std::string> getProfiles(const std::string &, const std::string &, const std::string &) { return {}; }
  std::string getStreamUri(const std::string &, const std::string &, const std::string &, const std::string &) { return ""; }
  bool getVideoEncoderConfiguration(const std::string &, const std::string &,
                                     const std::string &, const std::string &, int &, int &, int &) { return false; }
};

#endif // USE_GSOAP

