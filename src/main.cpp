#include <iostream>
#include <cstdio>
// #include "timetree_utils.h"
#include "timetree_structs.h"
#include "tt_config.h"
#include <string>
using namespace std;


void callTTTime() {
    TTTime time(1000000000);
    cout << "Time: " << time.to_string() << endl;
}


int main(int argc, char* argv[]) {

    callTTTime();

    // char const * sessionToken = getTTSessionToken(TT_USERNAME, TT_PASSWORD);
    // cout << "Session Token: " << sessionToken << endl;
    return 0;
}