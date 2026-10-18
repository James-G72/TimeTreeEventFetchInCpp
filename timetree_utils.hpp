#ifndef TIMETREE_UTILS_HPP
#define TIMETREE_UTILS_HPP
#include <string>
#include <stdexcept>
#include "vendor/json.hpp"
using json = nlohmann::json;
// The response body is available for diagnosis, but is not logged automatically.
class TTHttpError : public std::runtime_error {
public:
    long status;
    std::string response_body;
    TTHttpError(long code, const std::string& method, const std::string& body)
        : std::runtime_error("API " + method + " returned HTTP " + std::to_string(code)),
          status(code), response_body(body) {}
};
struct UserCredentials {
    std::string username;
    std::string password;
};
std::string generateSimpleUUID();
std::string getTTSessionToken(const std::string& username, const std::string& password);
json requestCalendars(const std::string& session_id);
// A negative since omits the cursor and starts a full event sync.
json requestCalendarEvents(const std::string& session_id, long long calendar_id,
                           long long since = -1);
// Throws std::runtime_error on transport failures or non-2xx responses.
std::string contactAPI(const std::string& url, const std::string& session_id = "",
                       std::string request = "GET", std::string payload_string = "");
extern const char* const TT_API_BASE_URL;
extern const char* const TT_API_LOGIN_ENDPOINT;
extern const char* const TT_API_VERSION_HEADER;
#endif
