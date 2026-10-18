#include "timetree_structs.hpp"
#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
json fixture_event(const string& id, const string& title, bool recurring = false) {
    return {{"id", id}, {"calendar_id", 7}, {"title", title}, {"author_id", 1}, {"label_id", nullptr},
            {"updated_at", 1735776000000LL}, {"start_at", 1735689600000LL},
            {"end_at", recurring ? 1735689600000LL : 1735693200000LL}, {"all_day", recurring},
            {"recurrences", recurring ? json::array({"RRULE:FREQ=WEEKLY"}) : json::array()},
            {"note", "Preserve extra API fields"}};
}
int main() {
    try {
        const json response = {{"calendars", json::array({
            {{"id", 8}, {"name", "Other"}},
            {{"id", 7}, {"name", "Ruth"}, {"created_at", 1735689600123LL}, {"updated_at", 1735776000456LL},
             {"calendar_labels", {{{"id", 4}, {"name", "Work"}, {"color", 42}}}}}
        })}};
        const auto definition = selectCalendarDefinition(response, "Ruth");
        check(definition.at("id") == 7, "Select calendar by name");
        bool failed = false;
        try { selectCalendarDefinition(response, "Missing"); } catch (const std::runtime_error&) { failed = true; }
        check(failed, "Missing calendar must fail");
        TTCalendar offline(definition);
        failed = false;
        try { offline.fetch_events(); } catch (const std::logic_error&) { failed = true; }
        check(failed, "Missing page callback must fail clearly");

        string mode = "normal";
        vector<long long> cursors;
        const TTEventFetcher fetch_page = [&](long long calendar_id, long long since) -> json {
            check(calendar_id == 7, "Callback receives calendar ID");
            cursors.push_back(since);
            if (mode == "transport-failure") throw std::runtime_error("Application request failed");
            if (mode == "empty") return {{"events", json::array()}, {"chunk", false}};
            if (mode == "changed")
                return {{"events", json::array({fixture_event("5000000001", "Refreshed title"),
                                                fixture_event("2", "No longer recurring")})}, {"chunk", false}};
            if (since == -1)
                return {{"events", json::array({fixture_event("5000000001", "Old title"),
                                               fixture_event("2", "Recurring", true)})},
                        {"chunk", true}, {"since", 1000}};
            check(since == 1000, "Callback receives server cursor");
            if (mode == "bad-page") return {{"events", json::object()}, {"chunk", false}};
            return {{"events", json::array({fixture_event("5000000001", "Updated title"), fixture_event("3", "New event")})},
                    {"chunk", mode == "loop"}, {"since", 1000}};
        };
        TTCalendar calendar(definition, fetch_page);
        calendar.fetch_events();
        check(cursors == vector<long long>({-1, 1000}), "Follow every response chunk");
        check(calendar.created.as_ms() == 1735689600123LL && calendar.updated.as_ms() == 1735776000456LL, "Calendar timestamps");
        check(calendar.label_data.at(4).name == "Work", "Label lookup");
        check(calendar.events.size() == 2 && calendar.recur_events.size() == 1, "All pages populated");
        const auto& event = calendar.events.at("5000000001");
        check(event.title == "Updated title" && event.parent_id == 7, "Duplicate IDs use latest page");
        check(!event.label_id && event.author_id == 1 && event.duration == 3600000, "Event metadata");
        check(event.definition.at("note") == "Preserve extra API fields", "Original JSON retained");
        check(calendar.recur_events.at("2").end.as_ms() == 1735775999000LL, "All-day timing");
        for (const auto& bad_mode : {"bad-page", "loop", "transport-failure"}) {
            mode = bad_mode;
            failed = false;
            try { calendar.fetch_events(); } catch (const std::runtime_error&) { failed = true; }
            check(failed, "Bad page or callback failure must fail");
            check(calendar.events.size() == 2 && calendar.recur_events.size() == 1
                  && calendar.deleted_events.empty(), "Failed fetch retains snapshot");
        }
        mode = "changed";
        calendar.refresh_events();
        check(calendar.events.size() == 2 && calendar.recur_events.empty(), "Refresh reclassifies series");
        check(calendar.events.at("5000000001").title == "Refreshed title", "Refresh catches edits");
        check(calendar.deleted_events.at("3").deleted && calendar.deleted_events.at("3").deleted_time.has_value(), "Deletion history");
        mode = "normal";
        calendar.refresh_events();
        check(calendar.deleted_events.empty() && calendar.recur_events.size() == 1, "Reappearance removes deletion marker");
        calendar.fetch_events(TTTime::from_string("20250108"), TTTime::from_string("20250109"));
        check(calendar.events.empty() && calendar.recur_events.size() == 1 && calendar.deleted_events.empty(), "Filter retains earlier recurring seed");
        check(calendar.events_between_dates(TTTime::from_string("20250108"), TTTime::from_string("20250108T235959Z")).size() == 1, "Expand earlier seed");
        calendar.refresh_events();
        check(calendar.events.empty() && calendar.bounds->first == TTTime::from_string("20250108"), "Refresh preserves bounds");
        mode = "empty";
        calendar.refresh_events();
        check(calendar.events.empty() && calendar.recur_events.empty() && calendar.deleted_events.size() == 3, "Empty snapshot deletes prior events");
        offline.set_event_fetcher(fetch_page);
        offline.fetch_events();
        check(offline.events.empty(), "Offline calendar accepts a page callback later");
        std::cout << "All offline integration checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
