#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>

using namespace drogon;

/**
 * @brief Endpoint statistics handler
 *
 * Endpoint: GET /v1/core/endpoints
 * Returns: JSON with statistics for each endpoint
 */
class EndpointsHandler : public drogon::HttpController<EndpointsHandler> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(EndpointsHandler::getEndpointsStats, "/v1/core/endpoints", Get);
  METHOD_LIST_END

  /**
   * @brief Handle GET /v1/core/endpoints
   */
  void
  getEndpointsStats(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
};
