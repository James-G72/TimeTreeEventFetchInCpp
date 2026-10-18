#include "timetree_structs.hpp"
#include <iostream>
#include <stdexcept>

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
TTTime date(const std::string& text) { return TTTime::from_string(text); }
TTEvent event(const std::string& start, const std::vector<std::string>& rules) {
    const auto timestamp = date(start).as_ms();
    return TTEvent({{"id", "series"}, {"calendar_id", 7}, {"title", "Fixture"},
        {"updated_at", timestamp}, {"start_at", timestamp}, {"end_at", timestamp + 3600000},
        {"all_day", false}, {"recurrences", rules}, {"author_id", 1}, {"label_id", 4}});
}
void expect(const TTEvent& source, const std::string& lower, const std::string& upper,
            const std::vector<std::string>& dates) {
    const auto instances = source.recur_within_dates(date(lower), date(upper));
    check(instances.size() == dates.size(), "Unexpected number of occurrences");
    for (size_t i = 0; i < dates.size(); ++i) {
        check(instances[i].start == date(dates[i]), "Incorrect occurrence date");
        check(instances[i].end.as_ms() - instances[i].start.as_ms() == source.duration, "Duration lost");
        check(instances[i].parent_id == source.id && instances[i].label_id == 4, "Parent metadata lost");
    }
}
int main() {
    try {
        check(date("20250101").as_str() == "20250101T000000Z", "Date-only parsing");
        check(round_tttime_to_day(TTTime(-1)).as_ms() == -DAY_MS, "Negative epoch floor");
        check(round_tttime_to_day(date("20250101T123456Z"), true) == date("20250102"), "Full-day upper bound");
        expect(event("20250101T090000Z", {"RRULE:FREQ=DAILY;INTERVAL=2"}),
               "20250102", "20250105T090000Z", {"20250103T090000Z", "20250105T090000Z"});
        expect(event("20250101T090000Z", {"RRULE:FREQ=WEEKLY;WKST=MO;BYDAY=WE", "EXDATE:20250108T090000Z,20250122T090000Z"}),
               "20250101", "20250131", {"20250101T090000Z", "20250115T090000Z", "20250129T090000Z"});
        expect(event("20250131T090000Z", {"RRULE:FREQ=MONTHLY"}),
               "20250101", "20250401", {"20250131T090000Z", "20250228T090000Z", "20250328T090000Z"});
        expect(event("20240229T090000Z", {"RRULE:FREQ=YEARLY"}),
               "20240101", "20270301", {"20240229T090000Z", "20250228T090000Z", "20260228T090000Z", "20270228T090000Z"});
        expect(event("20250101T090000Z", {"RRULE:FREQ=DAILY;UNTIL=20250103T090000Z"}),
               "20250101", "20250110", {"20250101T090000Z", "20250102T090000Z", "20250103T090000Z"});
        expect(event("20250101T090000Z", {"RRULE:FREQ=DAILY;UNTIL=20250102"}),
               "20250101", "20250110", {"20250101T090000Z", "20250102T090000Z"});
        expect(event("20250101T090000Z", {"RRULE:FREQ=DAILY;UNTIL=20250102"}), "20250201", "20250301", {});
        expect(event("20250101T090000Z", {}), "20250101", "20250201", {});
        for (const auto& rule : {"FREQ=DAILY;INTERVAL=0", "FREQ=DAILY;INTERVAL=-1", "FREQ=HOURLY"}) {
            bool failed = false;
            try { event("20250101", {rule}).recur_within_dates(date("20250101"), date("20250102")); }
            catch (const std::exception&) { failed = true; }
            check(failed, "Invalid recurrence must fail instead of looping");
        }
        bool invalid_date = false;
        try { date("20250230"); } catch (const std::exception&) { invalid_date = true; }
        check(invalid_date, "Invalid date must fail");
        TTCalendar calendar({{"id", 7}, {"calendar_users", {{{"user_id", 1}, {"name", "User"}}}},
            {"calendar_labels", {{{"id", 4}, {"name", "Label"}, {"color", 42}}}}});
        check(calendar.known_users.at(1) == "User" && calendar.label_data.at(4).colour == 42, "User and label lookup");
        auto ordinary = event("20250102T130000Z", {});
        calendar.events.emplace(ordinary.id, ordinary);
        auto recurring = event("20250101T090000Z", {"FREQ=DAILY"});
        calendar.recur_events.emplace(recurring.id, recurring);
        auto window = calendar.events_between_dates(date("20250102T120000Z"), date("20250102T120000Z"), true);
        check(window.size() == 2 && window[0].start == date("20250102T090000Z")
              && window[1].start == date("20250102T130000Z"), "Combined window must be chronological and cover whole day");
        auto exact = calendar.events_between_dates(ordinary.start, ordinary.start);
        check(exact.size() == 1 && !exact[0].recurring, "Ordinary event included at exact boundary");
        std::cout << "All recurrence checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
