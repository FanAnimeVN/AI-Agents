#pragma once

#include "core/result.h"

#include <map>
#include <string>

namespace oop {

struct HttpResponse {
    long status_code = 0;
    std::string body;
};

class HttpClient {
public:
    virtual ~HttpClient() = default;
    virtual Result<HttpResponse> post_json(
        const std::string& url,
        const std::string& body,
        const std::map<std::string, std::string>& headers,
        int timeout_seconds) = 0;

    virtual Result<HttpResponse> get(
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        int timeout_seconds) = 0;
};

class DefaultHttpClient final : public HttpClient {
public:
    Result<HttpResponse> post_json(
        const std::string& url,
        const std::string& body,
        const std::map<std::string, std::string>& headers,
        int timeout_seconds) override;

    Result<HttpResponse> get(
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        int timeout_seconds) override;
};

}  // namespace oop
