// This file contains the definitions of shared methods

#include "includes.h"
#include "shared.h"

// convert an int to a string
string int_to_string(int i){
    stringstream ss;
    ss << i;
    string str = ss.str();
    return str;
}

// convert a msg type to a string
string msg_type_to_string(enum MSG_TYPE type){
    if (type == OPEN){
        return "OPEN";
    }
    if (type == ACK){
        return "ACK";
    }
    if (type == QUERY){
        return "QUERY";
    }
    if (type == ADD){
        return "ADD";
    }
    if (type == RELAY){
        return "RELAY";
    }
    
}

// Convert the string input, into a vector of strings, split on spaces
void get_vector_input(vector<string> *split_input, string input){
    (*split_input).clear();
    istringstream iss(input);
    for(input; iss >> input; ){
        (*split_input).push_back(input);
    }
}

// print a flow element
void flow_element::print(){
    printf("(srcIP= %d-%d, destIP= %d-%d, action= %s:%d, pri= %d, pkCount= %d)\n", \
    scrIP_lo, scrIP_hi, destIP_lo, destIP_hi, actionType_to_string(actionType).c_str(), actionVal, pri, pktCount);
}

// convert a action type to string
string actionType_to_string(enum actionType type){
    if (type == FORWARD){
        return "FORWARD";
    }
    return "DROP";
}