#include "timetree_structs.hpp"
#include <iostream>
#include <stdexcept>
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    try {
        check(argc == 2, "Expected local server URL");
        const std::string base = argv[1];
        check(json::parse(contactAPI(base + "/ping"))["ok"] == true, "GET response");
        const auto session = getTTSessionToken("test-user", "test-password");
        check(session == "test-session", "Login cookie extraction");
        auto response = requestCalendars(session);
        TTCalendar calendar(session, response.at("calendars").at(0), {"test-user", "test-password"});
        check(calendar.s_id == session && calendar.login_info.username == "test-user", "Constructor members");
        check(calendar.created.as_ms() == 1735689600123LL, "Created milliseconds");
        check(calendar.updated.as_ms() == 1735776000456LL, "Updated milliseconds");
        check(calendar.event_labels.at(0).name == "Work", "Calendar label");
        calendar.fetch_events();
        check(calendar.events.size() == 2 && calendar.recur_events.size() == 1, "All event pages populated");
        const auto& event = calendar.events.at("5000000001");
        check(event.title == "Updated title", "Duplicate IDs use latest page");
        check(event.parent_id == 7 && !event.label_id && event.author_id == 1, "Event metadata and null label");
        check(event.duration == 3600000 && event.start.as_ms() == 1735689600000LL, "Event timing");
        check(event.definition.at("note") == "Preserve extra API fields", "Original JSON retained");
        const auto& recurring = calendar.recur_events.at("2");
        check(recurring.all_day && recurring.recurs && recurring.recurrences.size() == 1, "Recurring metadata");
        check(recurring.end.as_ms() == 1735775999000LL, "Inclusive all-day end matches Python");
        calendar.fetch_events();
        check(calendar.events.size() == 2, "Repeated full fetch replaces snapshot");
        for (const auto& badSession : {"bad-page", "loop"}) {
            calendar.s_id = badSession;
            bool failed = false;
            try { calendar.fetch_events(); }
            catch (const std::runtime_error&) { failed = true; }
            check(failed, "Malformed pages and repeated cursors must fail");
            check(calendar.events.size() == 2 && calendar.recur_events.size() == 1
                  && calendar.events.at("5000000001").title == "Updated title", "Failed fetch retains prior snapshot");
        }
        calendar.s_id = session;
        calendar.s_id = "expired";
        calendar.refresh_events();
        check(calendar.s_id == session, "Expired session must be replaced for retries");
        calendar.s_id = "changed";
        calendar.refresh_events();
        check(calendar.events.size() == 2 && calendar.recur_events.empty(), "Refresh reclassifies series as ordinary event");
        check(calendar.events.at("5000000001").title == "Refreshed title", "Refresh catches edits with unchanged timestamp");
        check(calendar.deleted_events.at("3").deleted && calendar.deleted_events.at("3").deleted_time.has_value(), "Missing event tracked as deleted");
        calendar.s_id = session;
        calendar.refresh_events();
        check(calendar.deleted_events.empty() && calendar.recur_events.size() == 1, "Reappearing event removes deletion marker");
        calendar.fetch_events(TTTime::from_string("20250108"), TTTime::from_string("20250109"));
        check(calendar.events.empty() && calendar.recur_events.size() == 1, "Date filter retains earlier series seeds");
        check(calendar.deleted_events.empty(), "Date filtering must not mark events deleted");
        check(calendar.events_between_dates(TTTime::from_string("20250108"), TTTime::from_string("20250108T235959Z")).size() == 1,
              "Series seed before fetch bounds must still expand");
        calendar.refresh_events();
        check(calendar.events.empty() && calendar.bounds->first == TTTime::from_string("20250108"), "Refresh retains fetch bounds");
        bool rejected = false;
        try { contactAPI(base + "/unauthorized"); }
        catch (const TTHttpError& e) {
            rejected = e.status == 401 && json::parse(e.response_body).at("error") == "unauthorized"
                       && std::string(e.what()).find("GET") != std::string::npos;
        }
        check(rejected, "HTTP errors must throw");
        rejected = false;
        try { requestCalendars("invalid-json"); }
        catch (const json::parse_error&) { rejected = true; }
        check(rejected, "Invalid JSON must throw");
        TTTime date(1738281600123LL); // 2025-01-31 00:00:00.123 UTC
        date.apply_delta(0, 1);
        check(date.as_ms() == 1740700800123LL, "Month clamping and subsecond preservation");
        std::cout << "All local integration checks passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
