#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <chrono>
#include <iomanip> // Required for std::put_time
#include <sstream>
using namespace std;
namespace chr = std::chrono;

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
    // Most important functionality is to handle the addition and subtraction of days, weeks etc accross
    // month and year boundaries.
    // All times are assumed to be UTC.
    public:
        chr::sys_time<chr::milliseconds> time;
        TTTime(long long ms_since_e) {
            chr::milliseconds msDuration{ms_since_e};
            chr::sys_time<chr::milliseconds> time{msDuration};
        };

        void apply_delta(int years=0, int months=0, int weeks=0, int days=0, int hours=0 , int minutes=0, int seconds=0) {
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

        void _add_months(int months) {
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

        void _add_years(int years) {
            // Add the given number of years to the time. This is a helper function for apply_delta.
            // Simply wraps the _add_months function as adding years is the same as adding 12 months.
            _add_months(years*12);
        };

        std::string to_string() const {
            // Convert the time to a string in the format "YYYY-MM-DD HH:MM:SS.mmm". 
            time_t tt = chr::system_clock::to_time_t(time);

            tm* tm_ptr = gmtime(&tt);

            stringstream ss;
            ss << put_time(tm_ptr, "%Y-%m-%d %H:%M:%S");

            return ss.str();
        }
};

class TTEvent{
    // Represents a single event as returned by the TimeTree API. This is used to store the events in 
    // the calendar object. Can have recurring information that is then used to generate TTEventRecur 
    // objects.
    public:
        int id;
        string title;
        int parennt_id;
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
        RecurringEventInfo recur_rules;

        TTEvent(vector<string> event_def) {

        };
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
        TTEventRecur(TTEvent parent, TTTime instance_start, TTTime instance_end) {
            parent_id = parent.id;
            start_time = instance_start;
            end_time = instance_end;
            title = parent.title;
        };
};

class TTCalendar{
    public:
        // Need to redefine the TTEvent and TTTime types so that we can have vecotrs of them.
        map<int, TTEvent> events;
        map<int, TTEventRecur> recur_events;
        vector<int> bounds;
        vector<int> deleted_events;
        string s_id;
        vector<string> login_info;
        char* name;
        char* alias;
        string unique_id;
        map<int, string> known_users;
        TTTime created;

        TTCalendar(string sessionID, map<string, string> calender_def, vector<string> login) {
            login_info = login;
            s_id = sessionID;
            _extract_calendar_info(calender_def);
        };

        int _extract_calendar_info(map<string, string> definition) {
            // Called as part of the calendar initialisation to unpack the API response.
            auto name = definition["name"];
            auto alias = definition["alias_code"];
            string unique_id = definition["id"];
            
            created = TTTime(stoi(definition["created_at"]));
        };
};