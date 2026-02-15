#include <string>
#include <vector>
#include <map>
using namespace std;

class TTTime{
    public:
        int t = 1;
        TTTime(int ms_since_e) {

        };
};

class TTEvent{
    public:
        int t = 1;
        TTEvent(vector<string> event_def) {

        };
};

class TTCalendar{
    public:
        // Need to redefine the TTEvent and TTTime types so that we can have vecotrs of them.
        vector<int> events;
        vector<int> recur_events;
        vector<int> bounds;
        vector<int> deleted_events;
        string s_id;
        vector<string> login_info;
        char* name;
        char* alias;
        int unique_id;
        map<int, string> known_users;
        TTTime created;

        TTCalendar(string sessionID, vector<string> calender_def, vector<string> login) {
            login_info = login;
            s_id = sessionID;

        };

        int _extract_calendar_info(vector<string> definition) {
            // Called as part of the calendar initialisation to unpack the API response.
            char* name = definition["name"];
            char* alias = definition["alias_code"];
            int unique_id = definition["id"];
            known_users = map([[user["user_id"], user["name"]] for user in resp["calendar_users"]])
            
            created = TTTime(ms_since_e=resp["created_at"])
        }
};