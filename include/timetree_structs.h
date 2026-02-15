#ifndef TIMETREE_STRUCTS_H
#define TIMETREE_STRUCTS_H
#include <string>
#include <vector>
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
        int t = 1;
        TTCalendar(std::string sessionID, vector<string> calender_def, vector<string> login) {

        };
};

#endif