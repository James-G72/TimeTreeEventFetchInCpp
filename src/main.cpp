#include <iostream>
#include <cstdio>
#include "timetree_utils.h"
#include "tt_config.h"
#include <string>
using namespace std;

int main(int argc, char* argv[]) {
    char const * sessionToken = getTTSessionToken(TT_USERNAME, TT_PASSWORD);
    cout << "Session Token: " << sessionToken << endl;
    return 0;
}