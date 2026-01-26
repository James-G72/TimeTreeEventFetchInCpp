#include <iostream>
#include <printf.h>
using namespace std;

const int test_var = 1;

int helper(int var_in) {
    var_in += test_var;
    return var_in;
}

int main() {
    int out = helper(2);
    printf("Output: %d\n", out);
    return 0;
}