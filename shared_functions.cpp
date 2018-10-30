#include "includes.h"
#include "shared_functions.h"

string int_to_string(int i){
    stringstream ss;
    ss << i;
    string str = ss.str();
    return str;
}