/*
a2sdn is the file that the process begins executing in. It determines if the process is a switch
or a controller, does some input checking and then passes the inputs along to either the 
controller file or the switch file 
*/

#include "includes.h"
#include "controller.h"
#include "switch.h"
#include "shared.h"

// Set the CPU limit in seconds
void setCPU_limit(int seconds){
    struct rlimit cpu_limiter;     
    cpu_limiter.rlim_cur = seconds;
    cpu_limiter.rlim_max = seconds;
    setrlimit (RLIMIT_CPU, &cpu_limiter); 
}

// Given a string of a switch name convert it to an int
// Null converted to -1
int get_sw(string sw){
    // Switch must have length 3 and a digit at position 2
    if ((sw.length() == 3 && isdigit(sw.at(2)))){
        return sw.at(2) - '0';
    }
    // Null switches are converted to -1
    else if(sw == "null" || sw == "Null" || sw == "NULL"){
        return -1;
    }
    // Otherwise we have invalid syntax
    else{
        cout << "Invalid Syntax" << endl;
        exit(0);
    }
}

// get the switch's low ip address
int get_lo_ip(string ip_range){
    // The low ip address is the number before the -
    if (ip_range.find("-") < (ip_range.length() - 1) && ip_range.find("-") > 0){
        return atoi(ip_range.substr(0,ip_range.find("-")).c_str());
    }
    else{
        cout << "Invalid Syntax" << endl;
        exit(0);
    }
}

// get the switch's high ip address
int get_hi_ip(string ip_range){
    // The high ip address is the number after the -    
    if (ip_range.find("-") < (ip_range.length() - 1) && ip_range.find("-") > 0){
        return atoi(ip_range.substr(ip_range.find("-")+1).c_str());
    }
    else{
        cout << "Invalid Syntax" << endl;
        exit(0);
    }
}

// main function for a2sdn, does error checking,
// determines if the process is a switch or 
// a controller, then runs the respective init function
int main(int argc, char *argv[]){
    // set CPU limit to 10 minutes
    setCPU_limit(600);

    // if we have 3 arguments and the second is cont then we need 
    // to initialize the controller
    if (argc == 3 && strcmp(argv[1], "cont") == 0){
        // get the number of switches and perform some error testing
        int num_of_switches = atoi(argv[2]); 
        if (num_of_switches > 0 && num_of_switches <= MAX_NSW){
            // Initialize the controller found in controller.cpp
            controller_init(num_of_switches);
        }
        else{
            cout << "Invalid number of switches" << endl;
        }
    }
    // If we have 6 arguments then there is a possibility we have a switch
    else if(argc == 6){
        // get all inputs from command line
        string sw = argv[1];
        string traffic_file_name = argv[2];
        string swj = argv[3];
        string swk = argv[4];
        string ip_range = argv[5];

        // convert inputs to thier proper type
        int switch_number = get_sw(sw);
        int left = get_sw(swj);
        int right = get_sw(swk);
        int lo_ip = get_lo_ip(ip_range);
        int hi_ip = get_hi_ip(ip_range);

        // Verify the switches internal order in maintained
        if ((left >= switch_number) || (right <= switch_number && right != -1)){
            cout << "Invalid Syntax" << endl;
            exit(0);
        }
        if (left <= 0 && left!=-1){
            cout << "Invalid Syntax" << endl;
            exit(0);
        }
        if ((switch_number > MAX_NSW) || (switch_number <= 0)){
            cout << "Invalid Syntax" << endl;
            exit(0);
        }
        if (right > MAX_NSW){
            cout << "Invalid Syntax" << endl;
            exit(0);
        }
        if (hi_ip > MAXIP || lo_ip >= hi_ip){
            cout << "Invalid Syntax" << endl;
            exit(0);
        }

        // Initialize the switch found in switch.cpp
        switch_init(switch_number, traffic_file_name, left, right, lo_ip, hi_ip);
    }
    // Otherwise we have invalid syntax
    else{
        cout << "Invalid syntax" << endl;
    }
    
    return 0;
}