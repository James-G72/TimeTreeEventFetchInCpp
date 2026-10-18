#ifndef ARDUINO
#include "timetree_structs.hpp"
#include <curl/curl.h>
#include <memory>
#include <random>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <array>
#include <iomanip>
#include <thread>

struct UserCredentials {
    std::string username;
    std::string password;
};
class TTHttpError : public std::runtime_error {
public:
    long status;
    std::string response_body;
    TTHttpError(long code, const std::string& method, const std::string& body)
        : std::runtime_error("API " + method + " returned HTTP " + std::to_string(code)),
          status(code), response_body(body) {}
};

std::string generateSimpleUUID();
std::string getTTSessionToken(const std::string& username, const std::string& password);
std::string contactAPI(const std::string& url, const std::string& session_id = "",
                       std::string request = "GET", std::string payload_string = "");

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

json requestCalendarEvents(const std::string& session_id, long long calendar_id, long long since = -1) {
    if (session_id.empty()) throw std::invalid_argument("A session ID is required");
    auto url = baseUrl() + "/calendar/" + std::to_string(calendar_id) + "/events/sync";
    if (since >= 0) url += "?since=" + std::to_string(since);
    return json::parse(contactAPI(url, session_id));
}

namespace {
string single_line(string value) {
    std::replace(value.begin(), value.end(), '\n', ' ');
    std::replace(value.begin(), value.end(), '\r', ' ');
    std::replace(value.begin(), value.end(), '\t', ' ');
    return value;
}

void print_events(const TTCalendar& calendar, size_t update) {
    const auto now = TTTime::now();
    const auto start = round_tttime_to_day(now - chr::hours{24});
    const auto finish = round_tttime_to_day(now + chr::hours{24 * 7}, true) - chr::milliseconds{1};
    const auto events = calendar.events_between_dates(start, finish);

    std::cout << "\n--- " << calendar.name << " | Update " << update
              << " | " << now.to_string() << " UTC ---\n"
              << "Showing " << start.to_string() << " to " << finish.to_string()
              << " UTC (" << events.size() << " events)\n";

    using Row = std::array<string, 6>;
    const Row headings{"Start (UTC)", "End (UTC)", "Title", "Label", "Author", "Repeats"};
    std::array<size_t, 6> widths{};
    for (size_t i = 0; i < widths.size(); ++i) widths[i] = headings[i].size();
    vector<Row> rows;
    for (const auto& event : events) {
        string label = "-", author = "-";
        if (event.label_id) {
            const auto found = calendar.label_data.find(*event.label_id);
            label = found == calendar.label_data.end() ? std::to_string(*event.label_id) : found->second.name;
        }
        if (event.author_id) {
            const auto found = calendar.known_users.find(*event.author_id);
            author = found == calendar.known_users.end() ? std::to_string(*event.author_id) : found->second;
        }
        Row row{event.start.to_string().substr(0, 16), event.end.to_string().substr(0, 16),
                single_line(event.title), single_line(label), single_line(author), event.recurring ? "Yes" : "No"};
        for (size_t i = 0; i < widths.size(); ++i) widths[i] = std::max(widths[i], row[i].size());
        rows.push_back(std::move(row));
    }
    const auto print_row = [&](const Row& row) {
        for (size_t i = 0; i < row.size(); ++i)
            std::cout << std::left << std::setw(static_cast<int>(widths[i])) << row[i] << "  ";
        std::cout << '\n';
    };
    print_row(headings);
    for (const auto width : widths) std::cout << string(width, '-') << "  ";
    std::cout << '\n';
    for (const auto& row : rows) print_row(row);
    if (rows.empty()) std::cout << "No events in this window.\n";

    std::cout << "Deleted since this program started: " << calendar.deleted_events.size() << '\n';
    for (const auto& entry : calendar.deleted_events)
        std::cout << "  " << single_line(entry.second.title) << '\n';
    std::cout << "Next refresh in 10 seconds. Press Ctrl+C to stop.\n" << std::flush;
}
}

int main(int argc, char** argv) {
    const char* stage = "Request";
    try {
        // First test the HTTP transport independently of TimeTree authentication.
        if (argc == 3 && std::string(argv[1]) == "--get") {
            std::cout << contactAPI(argv[2]) << '\n';
            return 0;
        }
        if (argc != 1) {
            std::cerr << "Usage: timetree_example [--get URL]\n";
            return 2;
        }

        const char* username = std::getenv("TIMETREE_USERNAME");
        const char* password = std::getenv("TIMETREE_PASSWORD");
        if (!username || !password)
            throw std::runtime_error("Set TIMETREE_USERNAME and TIMETREE_PASSWORD first");

        stage = "Login";
        const UserCredentials login{username, password};
        auto session = getTTSessionToken(login.username, login.password);

        stage = "Calendar query";
        const json response = requestCalendars(session);

        stage = "Calendar parsing";
        // Only this callback knows how pages are fetched and sessions renewed.
        // The ESP32 sketch can supply the same callback using its HTTP library.
        const TTEventFetcher fetch_page = [&](long long calendar_id, long long since) {
            for (int attempt = 0; ; ++attempt) {
                try {
                    return requestCalendarEvents(session, calendar_id, since);
                } catch (const TTHttpError& error) {
                    if (attempt >= 2 || (error.status != 401 && error.status != 403)) throw;
                    session = getTTSessionToken(login.username, login.password);
                }
            }
        };
        TTCalendar calendar(selectCalendarDefinition(response, "Ruth"), fetch_page);
        stage = "Event fetch";
        calendar.fetch_events();
        std::cout << calendar.name << " (ID: " << calendar.unique_id
                  << ", labels: " << calendar.event_labels.size()
                  << ", events: " << calendar.events.size()
                  << ", recurring series: " << calendar.recur_events.size() << ")\n";

        stage = "Live display";
        size_t update = 0;
        print_events(calendar, update);
        while (true) {
            std::this_thread::sleep_for(chr::seconds{10});
            try {
                calendar.refresh_events();
                print_events(calendar, ++update);
            } catch (const std::exception& error) {
                std::cerr << "Refresh failed: " << error.what()
                          << ". Last successful snapshot retained; retrying in 10 seconds.\n" << std::flush;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << stage << " failed: " << error.what() << '\n';
        return 1;
    }
}

#endif // !ARDUINO
