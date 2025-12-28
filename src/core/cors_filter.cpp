#include "core/cors_filter.h"
#include <drogon/HttpResponse.h>
#include <iostream>

void CorsFilter::doFilter(const drogon::HttpRequestPtr &req,
                          drogon::FilterCallback &&fcb,
                          drogon::FilterChainCallback &&fccb) {
  // Handle OPTIONS preflight request - ALLOW ALL
  if (req->method() == drogon::Options) {
    std::cerr << "[CorsFilter] OPTIONS preflight: " << req->path() << std::endl;

    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);

    // ALLOW ALL - Simple CORS configuration
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods",
                    "GET, POST, PUT, DELETE, PATCH, OPTIONS, HEAD");
    resp->addHeader("Access-Control-Allow-Headers", "*");
    resp->addHeader("Access-Control-Expose-Headers", "*");
    resp->addHeader("Access-Control-Max-Age", "86400"); // 24 hours

    std::cerr << "[CorsFilter] OPTIONS response sent with ALLOW ALL headers"
              << std::endl;
    fcb(resp);
    return;
  }

  // For all other requests, continue to handler and add CORS headers to
  // response Wrap the callback to add CORS headers
  auto wrapped_fcb = [fcb =
                          std::move(fcb)](const drogon::HttpResponsePtr &resp) {
    // ALLOW ALL - Add CORS headers to all responses
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods",
                    "GET, POST, PUT, DELETE, PATCH, OPTIONS, HEAD");
    resp->addHeader("Access-Control-Allow-Headers", "*");
    resp->addHeader("Access-Control-Expose-Headers", "*");

    fcb(resp);
  };

  // Continue to next filter/handler
  // Note: We need to wrap the callback, but Drogon's filter chain doesn't
  // support this directly So we'll let handlers add CORS headers themselves,
  // and this filter handles OPTIONS
  fccb();
}
