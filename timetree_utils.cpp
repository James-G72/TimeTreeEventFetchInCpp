#include "timetree_utils.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <memory>
#include <random>
#include <stdexcept>

const char* const TT_API_BASE_URL = "https://timetreeapp.com/api/v1";
const char* const TT_API_LOGIN_ENDPOINT = "/auth/email/signin";
const char* const TT_API_VERSION_HEADER = "web/2.1.0/en";

namespace {
struct CurlRuntime {
    CurlRuntime() {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
            throw std::runtime_error("Unable to initialize libcurl");
    }
    ~CurlRuntime() { curl_global_cleanup(); }
};
std::string baseUrl() {
    const char* overrideUrl = std::getenv("TIMETREE_BASE_URL");
    std::string url = overrideUrl ? overrideUrl : TT_API_BASE_URL;
    while (!url.empty() && url.back() == '/') url.pop_back();
    return url;
}
size_t appendBody(char* data, size_t size, size_t count, void* output) noexcept {
    try {
        static_cast<std::string*>(output)->append(data, size * count);
        return size * count;
    } catch (...) { return 0; }
}
struct Response { std::string body, session; };
Response perform(const std::string& url, const std::string& session,
                 const std::string& method, const std::string& payload) {
    static CurlRuntime runtime;
    if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0)
        throw std::invalid_argument("Expected an HTTP or HTTPS URL");
    if (method != "GET" && method != "PUT" && method != "POST" && method != "DELETE")
        throw std::invalid_argument("Unsupported HTTP method");
    if (session.find_first_of("\r\n;") != std::string::npos)
        throw std::invalid_argument("Invalid session cookie");
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(curl_easy_init(), curl_easy_cleanup);
    if (!handle) throw std::runtime_error("Unable to create curl handle");
    Response response;
    char error[CURL_ERROR_SIZE] = {};
    auto set = [&](CURLoption option, auto value) {
        auto code = curl_easy_setopt(handle.get(), option, value);
        if (code != CURLE_OK) throw std::runtime_error(curl_easy_strerror(code));
    };
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(nullptr, curl_slist_free_all);
    auto addHeader = [&](const std::string& value) {
        auto next = curl_slist_append(headers.get(), value.c_str());
        if (!next) throw std::bad_alloc();
        headers.release();
        headers.reset(next);
    };
    addHeader("Accept: application/json");
    addHeader("Content-Type: application/json");
    addHeader(std::string("X-Timetreea: ") + TT_API_VERSION_HEADER);
    set(CURLOPT_URL, url.c_str());
    set(CURLOPT_USERAGENT, "TimeTreeHandlerCpp/0.1");
    set(CURLOPT_HTTPHEADER, headers.get());
    set(CURLOPT_ERRORBUFFER, error);
    set(CURLOPT_CONNECTTIMEOUT, 10L);
    set(CURLOPT_TIMEOUT, 30L);
    set(CURLOPT_NOSIGNAL, 1L);
    set(CURLOPT_WRITEFUNCTION, &appendBody);
    set(CURLOPT_WRITEDATA, &response.body);
    set(CURLOPT_COOKIEFILE, "");
    const auto cookie = "_session_id=" + session;
    if (!session.empty()) set(CURLOPT_COOKIE, cookie.c_str());
    // Keep TLS verification enabled. Do not forward credentials through redirects.
    if (method != "GET") {
        set(CURLOPT_POSTFIELDS, payload.c_str());
        set(CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(payload.size()));
    }
    set(CURLOPT_CUSTOMREQUEST, method.c_str());
    auto result = curl_easy_perform(handle.get());
    if (result != CURLE_OK)
        throw std::runtime_error(std::string("HTTP transport failed: ") +
                                 (error[0] ? error : curl_easy_strerror(result)));
    long status = 0;
    curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &status);
    if (status < 200 || status >= 300)
        throw TTHttpError(status, method, response.body);
    curl_slist* rawCookies = nullptr;
    if (curl_easy_getinfo(handle.get(), CURLINFO_COOKIELIST, &rawCookies) != CURLE_OK)
        throw std::runtime_error("Unable to read session cookie");
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> cookies(rawCookies, curl_slist_free_all);
    for (auto entry = cookies.get(); entry; entry = entry->next) {
        // Netscape format: domain, flag, path, secure, expiry, name, value.
        std::string line(entry->data);
        auto end = line.rfind('\t');
        if (end != std::string::npos) {
            auto start = line.rfind('\t', end - 1);
            if (start != std::string::npos && line.substr(start + 1, end - start - 1) == "_session_id")
                response.session = line.substr(end + 1);
        }
    }
    return response;
}
}
std::string generateSimpleUUID() {
    std::random_device random;
    std::uniform_int_distribution<int> digit(0, 15);
    std::string uuid;
    for (int i = 0; i < 32; ++i) uuid += "0123456789abcdef"[digit(random)];
    // Match Python uuid.uuid4().hex, including version and variant bits.
    uuid[12] = '4';
    uuid[16] = "89ab"[digit(random) & 3];
    return uuid;
}
std::string contactAPI(const std::string& url, const std::string& session_id,
                       std::string request, std::string payload_string) {
    return perform(url, session_id, request, payload_string).body;
}
std::string getTTSessionToken(const std::string& username, const std::string& password) {
    json payload = {{"uid", username}, {"password", password}, {"uuid", generateSimpleUUID()}};
    auto response = perform(baseUrl() + TT_API_LOGIN_ENDPOINT, "", "PUT", payload.dump());
    if (response.session.empty()) throw std::runtime_error("Login did not set _session_id");
    return response.session;
}
json requestCalendars(const std::string& session_id) {
    if (session_id.empty()) throw std::invalid_argument("A session ID is required");
    return json::parse(contactAPI(baseUrl() + "/calendars?since=0", session_id));
}

json requestCalendarEvents(const std::string& session_id, long long calendar_id, long long since) {
    if (session_id.empty()) throw std::invalid_argument("A session ID is required");
    auto url = baseUrl() + "/calendar/" + std::to_string(calendar_id) + "/events/sync";
    if (since >= 0) url += "?since=" + std::to_string(since);
    return json::parse(contactAPI(url, session_id));
}
