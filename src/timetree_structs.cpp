#include <string>
#include <format>
#include <vector>
#include <map>
#include <ctime>
#include <chrono>
using namespace std;
namespace chr = std::chrono;

#include "timetree_structs.h"

// Defining TTTime functions
TTTime::TTTime(long long ms_since_e) {
    chr::milliseconds msDuration{ms_since_e};
    time = chr::sys_time<chr::milliseconds>(msDuration);
};

void TTTime::apply_delta(int years, int months, int weeks, int days, int hours, int minutes, int seconds) {
    // Apply the given delta to the time. The alters the internal state of the TTTime objject.
    // The order of these helpers is very important. The year and month helpers must be applied 
    // before the day, week and hour helpers to ensure that the varying number of days in each 
    // month is handled correctly.
    if (years != 0) {
        _add_years(years);
    }
    
    if (months != 0) {
        _add_months(months);
    }

    // The week, day and hour functions simply add milliseconds to the time so the order of these 
    // does not matter. No helpers are required as they're simple to calculate.
    if (weeks != 0) {
        time += chr::hours{24*7*weeks};
    }

    if (days != 0) {
        time += chr::hours{24*days};
    }

    if (hours != 0) {
        time += chr::hours{hours};
    }

    if (minutes != 0) {
        time += chr::minutes{minutes};
    }

    if (seconds != 0) {
        time += chr::seconds{seconds};
    }
};

void TTTime::_add_months(int months) {
    // Add the given number of months to the time. This is a helper function for apply_delta.
    // This is more complicated than adding days or weeks because of the varying number of days in each month.
    auto day_midnight = floor<chr::days>(time);
    auto time_of_day = time - day_midnight;

    // Convert the date portion to a year_month_day struct
    chr::year_month_day ymd{day_midnight};

    // Add only the year and month first
    auto ym = ymd.year() / ymd.month();
    // Then add months such that any year rollover etc can be handled
    ym += chr::months{months};
    ymd = ym / ymd.day();

    // Performing a check to see if the date is valid.
    // If the date is not valid then rebuild it with the last day of the month.
    if (!ymd.ok()) {
        ymd = ymd.year() / ymd.month() / chr::last;
        }

    // Convert the new date back to midnight, then add the original time back
    time = chr::sys_days{ymd} + chr::duration_cast<chr::milliseconds>(time_of_day);
};

void TTTime::_add_years(int years) {
    // Add the given number of years to the time. This is a helper function for apply_delta.
    // Simply wraps the _add_months function as adding years is the same as adding 12 months.
    _add_months(years*12);
};

std::string TTTime::to_string() {
    // Convert the time to a string in the format "YYYY-MM-DD HH:MM:SS.mmm". 
    time_t tt = chr::system_clock::to_time_t(time);

    std::string s = std::format("{:%Y-%m-%d %H:%M:%S}", tt);
    return s;
}

// Defining TTEvent functions
TTEvent::TTEvent(map<std::string, std::string> event_def) {
    parent_id = stoi(event_def["calendar_id"]);
    recurs = false;
    deleted = false;
    deleted_date = TTTime(0);
    _extract_event_info(event_def);
};

void TTEvent::_extract_event_info(map<string, string> event_def) {
    id = stoi(event_def["id"]);
    author_id = stoi(event_def["author_id"]);
    title = event_def["title"];
    updated = TTTime(stoi(event_def["updated_at"]));
    start = TTTime(stoi(event_def["start_at"]));
    // All Day events are considered to end on a day, but last for all of it.
    // A day worth of milliseconds therefore needs to be added.
    if (event_def["all_day"] == "true" || event_def["all_day"] == "True" || event_def["all_day"] == "1") {
        end = TTTime(stoi(event_def["end_at"]) + DAY_MS - 1000);
    } else {
        end = TTTime(stoi(event_def["end_at"]));
    }
    // TODO deal withh the subtraction below by adding to the TTTime class
    duration = chr::milliseconds(end.as_ms() - start.as_ms());
    label_id = stoi(event_def["label_id"]);
    if (event_def["recurrences"].length() > 0) {
        _store_recurrence(event_def["recurrences"]);
    }
};

void TTEvent::_store_recurrence(map<string, string> recur_map) {
    // A helper function to extract the recurrence information from the API response and store it in the event object.
    // The recurrence information is stored as a string in the format "FREQ=DAILY;INTERVAL=1;WKST=MO;BYDAY=MO,TU,WE,TH,FR;UNTIL=20231231T235959Z"
    // This function parses this string and stores the information in the recur_rules struct.
    exceptions = [x.split("EXDATE:")[1] for x in rule_list[1:]];
    // TODO make the exception dates into milliseconds since epoch so it can be TTTimed
    exceptions_tt = [TTTime(dt_object=dt.datetime.strptime(exp, EXCEPTION_DATE_FMT)) for exp in exceptions];

    // TODO this is now how it will arrive
    _unpack_rules(recur_map["rules"]);

    if (exceptions_tt.length() > 0) {
        recur_rules["EXDATE"] = exceptions_dt
        recur_exceptions = 1
    }

    recurs = true
};

void TTEvent::_unpack_rules(vector<std::string> rule_list) {
    map<std::string, std::string> _tmp_rule_map;

    for (const std::string& rule : FULL_RULE_LIST) {
        if (std::find(rule_list.begin(), rule_list.end(), rule) != rule_list.end()) {
            // TODO deal with this nightmare of string manipulation below.
            _tmp_rule_dict[rule] = rule_list.split(rule)[1].split("=")[1].split(";")[0];
        }
    }
    // TODO work out if making a temp dict like this is a good idea in c++
    recur_rules = _tmp_rule_dict;
};

vector<TTEventRecur> TTEvent::recur_within_dates(TTTime start_date, TTTime end_date) {
    // A function to generate the instances of a recurring event that fall within the given date range.
    int t = 1;
    return vector<TTEventRecur>();
};

// Defining TTEventRecur functions
TTEventRecur::TTEventRecur(TTEvent parent, TTTime instance_start, TTTime instance_end) {
    parent_id = parent.id;
    start_time = instance_start;
    end_time = instance_end;
    title = parent.title;
};

// Defining TTCalendar functions
TTCalendar::TTCalendar(string sessionID, map<string, string> calender_def, vector<string> login) {
    login_info = login;
    s_id = sessionID;
    _extract_calendar_info(calender_def);
};

void TTCalendar::_extract_calendar_info(map<string, string> definition) {
    // Called as part of the calendar initialisation to unpack the API response.
    auto name = definition["name"];
    auto alias = definition["alias_code"];
    string unique_id = definition["id"];
    
    created = TTTime(stoi(definition["created_at"]));
};