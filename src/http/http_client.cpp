#include "http/http_client.h"

#ifdef OOP_HAVE_CURL
#include <curl/curl.h>
#endif

#include <sstream>

namespace oop {
namespace {

#ifdef OOP_HAVE_CURL
size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    body->append(ptr, size * nmemb);
    return size * nmemb;
}

Result<HttpResponse> perform_curl(
    const std::string& url,
    const std::string* post_body,
    const std::map<std::string, std::string>& headers,
    int timeout_seconds) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return fail<HttpResponse>("curl_easy_init failed");
    }

    std::string response_body;
    struct curl_slist* header_list = nullptr;
    for (const auto& [key, value] : headers) {
        const std::string header = key + ": " + value;
        header_list = curl_slist_append(header_list, header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if (post_body != nullptr) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body->c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(post_body->size()));
    }

    const CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        return fail<HttpResponse>(std::string("HTTP request failed: ") + curl_easy_strerror(code));
    }
    return HttpResponse{status, std::move(response_body)};
}
#endif

Result<HttpResponse> curl_missing() {
    return fail<HttpResponse>(
        "HTTP support is disabled because libcurl was not found at build time. "
        "Install libcurl development headers and rebuild with OOP_ENABLE_CURL=ON.");
}

}  // namespace

Result<HttpResponse> DefaultHttpClient::post_json(
    const std::string& url,
    const std::string& body,
    const std::map<std::string, std::string>& headers,
    int timeout_seconds) {
#ifdef OOP_HAVE_CURL
    auto merged = headers;
    merged.emplace("Content-Type", "application/json");
    return perform_curl(url, &body, merged, timeout_seconds);
#else
    (void)url;
    (void)body;
    (void)headers;
    (void)timeout_seconds;
    return curl_missing();
#endif
}

Result<HttpResponse> DefaultHttpClient::get(
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    int timeout_seconds) {
#ifdef OOP_HAVE_CURL
    return perform_curl(url, nullptr, headers, timeout_seconds);
#else
    (void)url;
    (void)headers;
    (void)timeout_seconds;
    return curl_missing();
#endif
}

}  // namespace oop
