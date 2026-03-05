#ifndef TIMETREE_STRUCTS_H
#define TIMETREE_STRUCTS_H
#include <string>
#include <vector>
#include <chrono>
#include <map>
using namespace std;
namespace chr = std::chrono;

int const DAY_MS = 24*60*60*1000;

vector<string> const FULL_RULE_LIST = {"FREQ", "INTERVAL", "WKST", "BYDAY", "UNTIL"};

struct RecurringEventInfo {
    // A struct to hold the information about a recurring event. This is used to generate the TTEventRecur objects.
    string frequency;
    int interval;
    string wkst;
    string byday;
    string until;
};

class TTTime{
    // Holds a time in the format milliseconds since Epoch. This is the format used by the TimeTree API.
    // Most important functionality is to handle the addition and subtraction of days, weeks etc across
    // month and year boundaries.
    // All times are assumed to be UTC.
    public:
        chr::sys_time<chr::milliseconds> time;
        chr::milliseconds msDuration;

        TTTime(long long ms_since_e=0);

        void apply_delta(int years=0, int months=0, int weeks=0, int days=0, int hours=0 , int minutes=0, int seconds=0);

        string to_string();

        long long as_ms() {
            return time.time_since_epoch().count();
        }
    
    private:
        void _add_months(int months);

        void _add_years(int years);
};

class TTEvent{
    // Represents a single event as returned by the TimeTree API. This is used to store the events in 
    // the calendar object. Can have recurring information that is then used to generate TTEventRecur 
    // objects.
    public:
        int id;
        string title;
        int parent_id;
        bool deleted;
        TTTime deleted_date;
        int author_id;
        string title;
        TTTime updated;
        TTTime start;
        TTTime end;
        chr::milliseconds duration;
        int label_id;
        bool recurs;
        int recur_exceptions;
        vector<string> exceptions;
        vector<TTTime> exceptions_tt;
        RecurringEventInfo recur_rules;

        TTEvent(map<string, string> event_def);

        vector<TTEventRecur> recur_within_dates(TTTime start_date, TTTime end_date);
        
    private:
        void _extract_event_info(std::map<std::string, std::string> event_def);

        void _store_recurrence(map<string, string> recur_map);

        void _unpack_rules(vector<std::string> rule_list);
};

class TTEventRecur{
    // An additional type of event that represents a single instance of a recurring event. 
    // This is required to handle the fact that the TimeTree API returns recurring events as a 
    // single event definition with a recurrence rule,
    public:
        int parent_id;
        TTTime start_time;
        TTTime end_time;
        string title;
        TTEventRecur(TTEvent parent, TTTime instance_start, TTTime instance_end);
};

class TTCalendar{
    public:
        map<int, TTEvent> events;
        map<int, TTEvent> recur_events;
        TTTime created;
        string name;
        string alias;
        string unique_id;
        string s_id;
        vector<string> login_info;

        TTCalendar(string sessionID, map<string, string> calender_def, vector<string> login);

    private:
        void _extract_calendar_info(map<string, string> definition);
};

#endif