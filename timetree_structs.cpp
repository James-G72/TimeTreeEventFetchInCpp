#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <chrono>
#include <algorithm>
#include <set>
#include <sstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <utility>
using namespace std;
namespace chr = std::chrono;
#include "vendor/json.hpp"
using json = nlohmann::json;

#include "timetree_structs.hpp"

const int DAY_MS = 24*60*60*1000;

namespace {
std::tm utc_tm(std::time_t value) {
    std::tm result{};
#ifdef _WIN32
    if (gmtime_s(&result, &value) != 0)
#else
    if (!gmtime_r(&value, &result))
#endif
        throw std::runtime_error("Timestamp is outside the supported UTC range");
    return result;
}
std::time_t utc_time(std::tm& value) {
#ifdef _WIN32
    return _mkgmtime(&value);
#else
    return timegm(&value);
#endif
}
vector<string> split(const string& value, char delimiter) {
    vector<string> parts;
    std::istringstream input(value);
    string part;
    while (std::getline(input, part, delimiter)) parts.push_back(part);
    return parts;
}
int recurrence_interval(const map<string, string>& rules) {
    const auto found = rules.find("INTERVAL");
    if (found == rules.end()) return 1;
    size_t used = 0;
    const int interval = std::stoi(found->second, &used);
    if (used != found->second.size() || interval <= 0)
        throw std::runtime_error("Recurrence INTERVAL must be a positive integer");
    return interval;
}
}

// Defining TTTime functions
TTTime::TTTime(long long ms_since_e) {
    const auto maximum = chr::duration_cast<chr::milliseconds>(chr::system_clock::duration::max()).count();
    const auto minimum = chr::duration_cast<chr::milliseconds>(chr::system_clock::duration::min()).count();
    if (ms_since_e > maximum || ms_since_e < minimum)
        throw std::out_of_range("Timestamp is outside the system clock range");
    msDuration = chr::milliseconds{ms_since_e};
    // Initialize the time_point to the epoch and then add the milliseconds duration
    time = chr::system_clock::from_time_t(0) + msDuration;
};

void TTTime::apply_delta(int years, int months, int weeks, int days, int hours, int minutes, int seconds) {
    // Apply the given delta to the time. This alters the internal state of the TTTime objject.
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
        time += chr::hours{24LL*7*weeks};
    }

    if (days != 0) {
        time += chr::hours{24LL*days};
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
    // Using std::time_t and std::tm to handle the month and year rollovers correctly.
    std::time_t t = std::chrono::system_clock::to_time_t(time);

    // The ctime tm structure stores the time_point as a broken-down time in UTC.
    std::tm timeInfo = utc_tm(t);

    const auto remainder = time - chr::system_clock::from_time_t(t);
    const long long total = static_cast<long long>(timeInfo.tm_year + 1900) * 12
                          + timeInfo.tm_mon + months;
    long long year = total / 12;
    int month = static_cast<int>(total % 12);
    if (month < 0) { month += 12; --year; }
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    const int monthDays[] = {31, leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    timeInfo.tm_year = static_cast<int>(year - 1900);
    timeInfo.tm_mon = month;
    timeInfo.tm_mday = std::min(timeInfo.tm_mday, monthDays[month]);
    timeInfo.tm_isdst = 0;
    // mktime interprets tm as local time; these values are UTC.
    std::time_t new_t = utc_time(timeInfo);

    // Recreating the time_point from the time_t using chr::system_clock::from_time_t.
    time = TTTime(static_cast<long long>(new_t) * 1000
                  + chr::duration_cast<chr::milliseconds>(remainder).count()).time;
};

void TTTime::_add_years(int years) {
    // Add the given number of years to the time. This is a helper function for apply_delta.
    // Theres no need to create a seperate function as 12 monmths will be evaludated identically.
    if (years > std::numeric_limits<int>::max() / 12 || years < std::numeric_limits<int>::min() / 12)
        throw std::out_of_range("Year delta is too large");
    _add_months(years*12);
};

std::string TTTime::to_string() const {
    // Convert the time to a string in the format "YYYY-MM-DD HH:MM:SS.mmm".
    time_t tt = chr::system_clock::to_time_t(time);

    std::tm tm_buf = utc_tm(tt);

    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S%z", &tm_buf);

    return std::string(buffer);
};

TTTime TTTime::now() {
    return TTTime(chr::duration_cast<chr::milliseconds>(chr::system_clock::now().time_since_epoch()).count());
}

string TTTime::as_str() const {
    auto value = chr::system_clock::to_time_t(time);
    auto tm = utc_tm(value);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y%m%dT%H%M%SZ", &tm);
    return buffer;
}

TTTime TTTime::from_string(const string& value) {
    const bool date_only = value.size() == 8;
    if (!date_only && value.size() != 16)
        throw std::invalid_argument("Expected YYYYMMDD or YYYYMMDDTHHMMSSZ in UTC");
    std::tm tm{};
    std::istringstream input(value);
    input >> std::get_time(&tm, date_only ? "%Y%m%d" : "%Y%m%dT%H%M%SZ");
    if (input.fail()) throw std::invalid_argument("Invalid recurrence date: " + value);
    const auto timestamp = utc_time(tm);
    TTTime result(static_cast<long long>(timestamp) * 1000);
    const auto canonical = result.as_str();
    if ((date_only ? canonical.substr(0, 8) : canonical) != value)
        throw std::invalid_argument("Invalid recurrence date: " + value);
    return result;
}

TTTime round_tttime_to_day(const TTTime& value, bool up) {
    auto days = value.as_ms() / DAY_MS;
    if (value.as_ms() % DAY_MS < 0) --days;
    return TTTime((days + (up ? 1 : 0)) * DAY_MS);
}

TTEvent::TTEvent(const json& event_definition)
    : id(event_definition.at("id").get<string>()),
      parent_id(event_definition.at("calendar_id").get<long long>()),
      title(event_definition.at("title").get<string>()),
      updated(event_definition.at("updated_at").get<long long>()),
      start(event_definition.at("start_at").get<long long>()),
      end(event_definition.at("end_at").get<long long>()),
      all_day(event_definition.at("all_day").get<bool>()),
      recurrences(event_definition.value("recurrences", vector<string>{})),
      definition(event_definition) {
    if (event_definition.contains("author_id") && !event_definition.at("author_id").is_null())
        author_id = event_definition.at("author_id").get<long long>();
    if (event_definition.contains("label_id") && !event_definition.at("label_id").is_null())
        label_id = event_definition.at("label_id").get<long long>();
    // Match the Python model's inclusive all-day end (23:59:59).
    if (all_day) end.time += chr::milliseconds{DAY_MS - 1000};
    duration = end.as_ms() - start.as_ms();
    recurs = !recurrences.empty();
    if (recurs) _store_recurrence();
}

void TTEvent::_store_recurrence() {
    for (auto rule : recurrences) {
        if (rule.rfind("RRULE:", 0) == 0) rule.erase(0, 6);
        if (rule.rfind("EXDATE:", 0) == 0) {
            for (const auto& date : split(rule.substr(7), ','))
                recur_exceptions.push_back(TTTime::from_string(date));
            continue;
        }
        for (const auto& item : split(rule, ';')) {
            const auto equals = item.find('=');
            if (equals == string::npos || equals == 0 || equals + 1 == item.size())
                throw std::runtime_error("Malformed recurrence rule");
            recur_rules[item.substr(0, equals)] = item.substr(equals + 1);
        }
    }
}

TTEventRecur::TTEventRecur(const TTEvent& parent, TTTime instance_start, TTTime instance_end)
    : parent_id(parent.id), title(parent.title), start(instance_start), end(instance_end),
      author_id(parent.author_id), label_id(parent.label_id), recurring(parent.recurs) {}

vector<TTEventRecur> TTEvent::recur_within_dates(TTTime start_date, TTTime end_date) const {
    if (end_date < start_date) throw std::invalid_argument("End date precedes start date");
    vector<TTEventRecur> result;
    if (!recurs) return result;
    const auto frequency = recur_rules.at("FREQ");
    if (frequency != "DAILY" && frequency != "WEEKLY" && frequency != "MONTHLY" && frequency != "YEARLY")
        throw std::runtime_error("Unsupported recurrence FREQ: " + frequency);
    const int interval = recurrence_interval(recur_rules);
    const auto until = recur_rules.find("UNTIL");
    if (until != recur_rules.end()) {
        auto finish = TTTime::from_string(until->second);
        // Date-only UNTIL includes the entire last day.
        if (until->second.size() == 8) finish = finish + chr::milliseconds{DAY_MS - 1};
        if (finish < end_date) end_date = finish;
    }
    if (end_date < start_date || end_date < start) return result;

    set<long long> exceptions;
    for (const auto& date : recur_exceptions) exceptions.insert(date.as_ms());
    TTTime current = start;
    const long long step = (frequency == "WEEKLY" ? 7LL : 1LL) * interval * DAY_MS;
    if ((frequency == "DAILY" || frequency == "WEEKLY") && current < start_date) {
        const auto difference = start_date.as_ms() - current.as_ms();
        const auto jumps = difference / step + (difference % step != 0);
        current = current + chr::milliseconds{jumps * step};
    }
    while (current <= end_date) {
        if (start_date <= current && exceptions.count(current.as_ms()) == 0)
            result.emplace_back(*this, current, current + chr::milliseconds{duration});
        if (current == end_date) break;
        TTTime next = current;
        if (frequency == "MONTHLY") next.apply_delta(0, interval);
        else if (frequency == "YEARLY") next.apply_delta(interval);
        else next = current + chr::milliseconds{step};
        if (next <= current) throw std::runtime_error("Recurrence did not advance");
        current = next;
    }
    return result;
}

// Defining TTCalendar functions
TTCalendar::TTCalendar(json calendar_def, TTEventFetcher fetch_page)
    : _fetch_page(std::move(fetch_page)) {
    _extract_calendar_info(calendar_def);
}

void TTCalendar::set_event_fetcher(TTEventFetcher fetch_page) {
    _fetch_page = std::move(fetch_page);
}

void TTCalendar::fetch_events(std::optional<TTTime> since_date, std::optional<TTTime> until_date) {
    if (!_fetch_page) throw std::logic_error("Set an event-page callback before fetching events");
    TTTime lower = since_date.value_or(created - chr::hours{24 * 7 * 52});
    TTTime upper = until_date.value_or(TTTime::now() + chr::hours{24 * 7 * 1000});
    if (upper < lower) throw std::invalid_argument("End date precedes start date");
    map<string, TTEvent> fetched;
    set<long long> cursors;
    long long since = -1;
    while (true) {
        const json response = _fetch_page(unique_id, since);
        const auto& definitions = response.at("events");
        if (!definitions.is_array())
            throw std::runtime_error("Expected 'events' to be a JSON array");
        for (const auto& definition : definitions) {
            TTEvent event(definition);
            if (event.parent_id != unique_id)
                throw std::runtime_error("Event belongs to a different calendar");
            fetched.insert_or_assign(event.id, std::move(event));
        }
        if (!response.at("chunk").get<bool>()) break;
        since = response.at("since").get<long long>();
        if (since < 0 || !cursors.insert(since).second)
            throw std::runtime_error("Event sync returned a repeated or invalid cursor");
    }

    map<string, TTEvent> new_events, new_recur_events;
    for (const auto& entry : fetched) {
        // Keep earlier recurring seeds so a window can include later occurrences.
        if (entry.second.start > upper || (!entry.second.recurs && entry.second.start < lower)) continue;
        auto& destination = entry.second.recurs ? new_recur_events : new_events;
        destination.emplace(entry.first, entry.second);
    }
    auto new_deleted = deleted_events;
    const auto deleted_at = TTTime::now();
    for (const auto& entry : _snapshot) {
        if (fetched.count(entry.first)) continue;
        auto event = entry.second;
        event.deleted = true;
        event.deleted_time = deleted_at;
        new_deleted.insert_or_assign(entry.first, std::move(event));
    }
    for (const auto& entry : fetched) new_deleted.erase(entry.first);
    events.swap(new_events);
    recur_events.swap(new_recur_events);
    deleted_events.swap(new_deleted);
    _snapshot.swap(fetched);
    bounds = std::make_pair(lower, upper);
}

void TTCalendar::refresh_events() {
    if (bounds) fetch_events(bounds->first, bounds->second);
    else fetch_events();
}

vector<TTEventRecur> TTCalendar::events_between_dates(TTTime start_date, TTTime end_date, bool full_day) const {
    if (end_date < start_date) throw std::invalid_argument("End date precedes start date");
    if (full_day) {
        start_date = round_tttime_to_day(start_date);
        end_date = round_tttime_to_day(end_date, true) - chr::milliseconds{1};
    }
    vector<TTEventRecur> result;
    for (const auto& entry : events) {
        const auto& event = entry.second;
        if (start_date <= event.start && event.start <= end_date)
            result.emplace_back(event, event.start, event.end);
    }
    for (const auto& entry : recur_events) {
        auto instances = entry.second.recur_within_dates(start_date, end_date);
        result.insert(result.end(), instances.begin(), instances.end());
    }
    std::sort(result.begin(), result.end(), [](const TTEventRecur& a, const TTEventRecur& b) {
        if (a.start != b.start) return a.start < b.start;
        return a.parent_id < b.parent_id;
    });
    return result;
}

void TTCalendar::_extract_calendar_info(json definition) {
    // Called as part of the calendar initialisation to unpack the API response.
    unique_id = definition.value("id", 0);
    name = definition.value("name", "Unnamed Calendar").c_str();
    alias = definition.value("alias_code", "").c_str();
    created = TTTime(definition.value("created_at", 0LL));
    updated = TTTime(definition.value("updated_at", 0LL));
    _extract_event_labels(definition);
    for (const auto& user : definition.value("calendar_users", json::array()))
        known_users[user.at("user_id").get<long long>()] = user.at("name").get<string>();
};

void TTCalendar::_extract_event_labels(json definition) {
    // The labels that can be assigned to an event are defined per calendar.
    event_labels.clear();
    label_data.clear();
    json label_list = definition.value("calendar_labels", json::array());

    for (json label : label_list) {
        EventLabels event_label;
        event_label.name = label.value("name", "").c_str();
        event_label.colour = label.value("color", 0L);
        event_label.id = label.value("id", 0LL);
        event_labels.push_back(event_label);
        if (label.contains("id")) label_data[event_label.id] = event_label;
    }
}
