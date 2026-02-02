#include <iostream>
#include <cstdio>
#include "../include/timetree_utils.h"
#include "../include/tt_config.h"
#include <string>
using namespace std;

const int test_var = 1;

int main() {
    char const * sessionToken = getTTSessionToken(TT_USERNAME, TT_PASSWORD);
    return 0;
}