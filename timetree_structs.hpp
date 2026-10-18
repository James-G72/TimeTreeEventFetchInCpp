#ifndef TIMETREE_STRUCTS_HPP
#define TIMETREE_STRUCTS_HPP

#include <string>
#include <vector>
#include <chrono>
#include <map>
#include <optional>
using namespace std;
namespace chr = std::chrono;
#include "vendor/json.hpp"
using json = nlohmann::json;

#include "timetree_utils.hpp"

extern const int DAY_MS;

// A structure for within TTCalendars for the event label info.
struct EventLabels {
    string name;
    long colour;
    long long id = 0;
};

class TTTime{
    // Holds a time in the format milliseconds since Epoch. This is the format used by the TimeTree API.
    // Most important functionality is to handle the addition and subtraction of days, weeks etc across
    // month and year boundaries.
    // All times are assumed to be UTC.
    public:
        chr::system_clock::time_point time;
        chr::milliseconds msDuration;

        TTTime(long long ms_since_e=0);

        void apply_delta(int years=0, int months=0, int weeks=0, int days=0, int hours=0 , int minutes=0, int seconds=0);

        string to_string() const;
        string as_str() const;
        static TTTime from_string(const string& value);
        static TTTime now();
        bool operator<(const TTTime& other) const { return time < other.time; }
        bool operator<=(const TTTime& other) const { return time <= other.time; }
        bool operator>(const TTTime& other) const { return time > other.time; }
        bool operator>=(const TTTime& other) const { return time >= other.time; }
        bool operator==(const TTTime& other) const { return time == other.time; }
        bool operator!=(const TTTime& other) const { return time != other.time; }
        TTTime operator+(chr::milliseconds delta) const { return TTTime(as_ms() + delta.count()); }
        TTTime operator-(chr::milliseconds delta) const { return TTTime(as_ms() - delta.count()); }

        long long as_ms() const {
            return chr::duration_cast<chr::milliseconds>(time.time_since_epoch()).count();
        }

    private:
        void _add_months(int months);

        void _add_years(int years);
};

TTTime round_tttime_to_day(const TTTime& value, bool up = false);

class TTEvent;
// A value object for a concrete occurrence; parent_id identifies its source event.
class TTEventRecur {
    public:
        string parent_id;
        string title;
        TTTime start;
        TTTime end;
        std::optional<long long> author_id;
        std::optional<long long> label_id;
        bool recurring;
        TTEventRecur(const TTEvent& parent, TTTime instance_start, TTTime instance_end);
};

class TTEvent {
    public:
        string id;
        long long parent_id;
        std::optional<long long> author_id;
        std::optional<long long> label_id;
        string title;
        TTTime updated;
        TTTime start;
        TTTime end;
        long long duration;
        bool all_day;
        bool recurs;
        vector<string> recurrences;
        map<string, string> recur_rules;
        vector<TTTime> recur_exceptions;
        bool deleted = false;
        std::optional<TTTime> deleted_time;
        // Preserve API fields that do not yet have dedicated C++ members.
        json definition;

        explicit TTEvent(const json& event_definition);
        vector<TTEventRecur> recur_within_dates(TTTime start_date, TTTime end_date) const;

    private:
        void _store_recurrence();
};

class TTCalendar{
    public:
        map<string, TTEvent> events;
        map<string, TTEvent> recur_events;
        map<string, TTEvent> deleted_events;
        map<long long, string> known_users;
        map<long long, EventLabels> label_data;
        std::optional<std::pair<TTTime, TTTime>> bounds;
        TTTime created;
        TTTime updated;
        string name;
        string alias;
        int unique_id;
        string s_id;
        UserCredentials login_info;
        vector<EventLabels> event_labels;

        TTCalendar(string sessionID, json calender_def, UserCredentials login);

        // Fetch a full snapshot, following every sync chunk. Replace the maps
        // only after all pages have been fetched and parsed successfully.
        void fetch_events(std::optional<TTTime> since = std::nullopt,
                          std::optional<TTTime> until = std::nullopt);
        void refresh_events();
        // Results include ordinary events and expanded series, sorted by start.
        vector<TTEventRecur> events_between_dates(TTTime start_date, TTTime end_date,
                                                 bool full_day = false) const;

    private:
        void _extract_calendar_info(json definition);
        void _refresh_session();
        void _extract_event_labels(json definition);
        map<string, TTEvent> _snapshot;
};

#endif
