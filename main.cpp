#include "timetree_structs.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <array>
#include <iomanip>
#include <thread>

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
        const auto session = getTTSessionToken(login.username, login.password);

        stage = "Calendar query";
        // requestCalendars uses json::parse from vendor/json.hpp on the HTTP body.
        const json response = requestCalendars(session);

        stage = "Calendar parsing";
        const json& definitions = response.at("calendars");
        if (!definitions.is_array())
            throw std::runtime_error("Expected 'calendars' to be a JSON array");

        const auto main_calendar = std::find_if(definitions.begin(), definitions.end(),
            [](const json& definition) {
                return definition.value("name", "") == "Ruth";
            });
        if (main_calendar == definitions.end())
            throw std::runtime_error("Calendar 'Ruth' was not found");

        TTCalendar calendar(session, *main_calendar, login);
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
