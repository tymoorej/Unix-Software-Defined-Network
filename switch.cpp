// the switch file has all of the methods and data needed for the switch

#include "switch.h"
#include "shared.h"

// these structs are needed to process SIGUSR1 signals
struct sigaction switch_oldAct, switch_newAct;

// contains all of the information the switch contains on either the
// controller, keyboard, or on another switch
struct switch_fd{
    int switch_number; // 0 for cont, -1 for keyboard
    int read_fd;
    int write_fd;
    switch_fd(){
        this->read_fd = -1;
        this->write_fd = -1;
    }
};

// used to keep track of all the signals a switch can send or recieve
struct switch_signal_count{
    int admit;
    int ack;
    int add;
    int relay;
    int open;
    int query;

    switch_signal_count(){
        this->open=0;
        this->ack=0;
        this->query=0;
        this->add=0;
    };
    void print_receiving(){
        cout << "ADMIT: " << this->admit << ",   ";
        cout << "ACK: " << this->ack << ",   ";
        cout << "ADD: " << this->add << ",   ";
        cout << "RELAY: " << this->relay << endl;
    };
    void print_transmitting(){
        cout << "OPEN: " << this->open << ",   ";
        cout << "QUERY: " << this->query << ",   ";
        cout << "RELAY: " << this->relay << endl;
    };

};

// also used to keep track of all the signals a switch can send or recieve
// instantiates two switch_signal_count structs. One for recieving and one
// for terminating
struct switch_total_signals{
    struct switch_signal_count recieved;
    struct switch_signal_count transmitted;
    // print all received and transmittes signals
    void print(){
        cout << "Packet Stats:" << endl;
        cout << "\tRecieved: ";
        this->recieved.print_receiving();
        cout << "\tTransmitted: ";
        this->transmitted.print_transmitting();
    }
};

// list which contains all the fds a switch has on it's neighboring switches
// and the controller
vector<struct switch_fd> switch_fd_list;

// The switch flow table, contains all rules the switch knows
vector<struct flow_element> switch_flow_table;

// declaring an instance of switch_total_signals
struct switch_total_signals switch_signals;

// Waiting queue that holds all instructions we are waiting to execute
// they are either on this queue because the switch is waiting on the
// controller rule for that insruction or because they just got sent
// via relay
vector<struct instruction> instructions_waiting_queue;

// Waiting queue that holds all instructions read from the traffic file
// This allows the traffic file to be read all at once and then only fed
// to the switch one line at a time
vector<struct instruction> traffic_file_queue;

// A list of IP addresses already sent to the controller, this prevents the
// switch from querying the controller twice for the same rule
vector<int> sent_dest_ips;

// Prints the switch flow table
void switch_print_flow_table(){
    cout << "Flow Table:" << endl;
    int i = 0;
    for (vector<struct flow_element>::iterator it = switch_flow_table.begin(); it != switch_flow_table.end(); ++it){
        cout << "[" << i << "] ";
        it->print();
        i++;
    }
}

// Prints the switch flow table and all recieved signals
void switch_list(){
    switch_print_flow_table();
    switch_signals.print();
}

// This function is called when the process receives the SIGUSR1 signal
// it calls the list function
void switch_handle_signal_USR1(int sigNo){
    switch_list();
}

// Create all fifos needed for communication to and from other switches
// opens all fifos the switch reads from but not fifos the switch 
// writes to. Stores these read fds in the switch_fd data structure
void switch_create_fifos(int switch_number, int left, int right){
    string first, second, third;
    int fd;

    if (left != -1){
        first = "fifo-" + int_to_string(switch_number) + "-" + int_to_string(left);
        second = "fifo-" + int_to_string(left) + "-" + int_to_string(switch_number);
        mkfifo(first.c_str(), 0666);
        mkfifo(second.c_str(), 0666);

        struct switch_fd fds = switch_fd();
        fd = open(second.c_str(), O_RDWR | O_NONBLOCK);
        fds.read_fd = fd;
        fds.switch_number = left;
        switch_fd_list.push_back(fds);

    }
    if (right != -1){
        first = "fifo-" + int_to_string(switch_number) + "-" + int_to_string(right);
        second = "fifo-" + int_to_string(right) + "-" + int_to_string(switch_number);
        mkfifo(first.c_str(), 0666);
        mkfifo(second.c_str(), 0666);

        struct switch_fd fds = switch_fd();
        fd = open(second.c_str(), O_RDWR | O_NONBLOCK);
        fds.read_fd = fd;
        fds.switch_number = right;
        switch_fd_list.push_back(fds);
    }

    // controller
    third = "fifo-0-" + int_to_string(switch_number);
    struct switch_fd fds = switch_fd();
    fd = open(third.c_str(), O_RDWR | O_NONBLOCK);
    fds.read_fd = fd;
    fds.switch_number = 0;
    switch_fd_list.push_back(fds);

    // keyboard
    fds = switch_fd();
    fds.read_fd = STDIN_FILENO;
    fds.switch_number = -1;
    switch_fd_list.push_back(fds);
    
}

// Shutdown the switch, restore signal behaviour
void switch_shutdown(){
    cout << "Switch Shutting Down" << endl;
    sigaction(SIGUSR1,&switch_oldAct,&switch_newAct);
}

// Gets the write_fd of a specific switch attached to the current switch
int switch_get_fd_write(int target){
    for (vector<struct switch_fd>::iterator it = switch_fd_list.begin(); it != switch_fd_list.end(); ++it){
        if (it->switch_number == target){
            return it->write_fd;
        }
    }
    return -1;
}

// Sets the write_fd of a specific switch attached to the current switch
void switch_set_fd_write(int target, int fd){
    for (vector<struct switch_fd>::iterator it = switch_fd_list.begin(); it != switch_fd_list.end(); ++it){
        if (it->switch_number == target){
            it->write_fd = fd;
            return;
        }
    }
}

// Given a message and target switch, send a message to that switch
// also opens the write fifo if not already open
void switch_send_message(struct message m, int switch_number, int target){
    cout << "Transmitting " << msg_type_to_string(m.type) << " to ";
    if (target == 0){
        cout << "Controller" << endl;
    }
    else{
        cout << "Switch " << target << endl;
    }

    int fd = switch_get_fd_write(target);
    if (fd == -1){
        string ds = "fifo-" + int_to_string(switch_number) + "-" + int_to_string(target);
        fd  = open(ds.c_str(), O_WRONLY | O_NONBLOCK);
        switch_set_fd_write(target, fd);
    }
    write(fd, (char*)&m, sizeof(m));
}

// Construct and send the OPEN message also incriment the 
// OPEN transmitted signal counter
void switch_send_open_message(int switch_number, int left, int right, int ip_range_lo, int ip_range_hi){
    struct message m;
    struct Open_Data data;
    memset((char *) &m, 0, sizeof(m));

    m.type = OPEN;
    data.switch_number = switch_number;
    data.left = left;
    data.right = right;
    data.ip_range_lo = ip_range_lo;
    data.ip_range_hi = ip_range_hi;
    m.data.open_data = data;
    switch_send_message(m, switch_number, 0);
    switch_signals.transmitted.open++;
}

// builds the pollfd array which will hold all of the fds and thier events
void switch_build_poll_array(struct pollfd fdarray[]){
    int i = 0;

    for (vector<struct switch_fd>::iterator it = switch_fd_list.begin(); it != switch_fd_list.end(); ++it){
        fdarray[i].fd = it->read_fd;
        fdarray[i].events= POLLIN;
	    fdarray[i].revents= 0;
        i++;
    }

}

// Initialize the switch's flow table by adding the default entry to the table
void switch_initialize_flow_table(int left, int right, int ip_range_lo, int ip_range_hi){
    struct flow_element element;
    element.scrIP_lo = 0;
    element.scrIP_hi = MAXIP;
    element.destIP_lo = ip_range_lo;
    element.destIP_hi = ip_range_hi;
    element.actionType = FORWARD;
    element.actionVal = 3;
    element.pri = MINPRI;
    element.pktCount = 0;
    switch_flow_table.push_back(element);
}

// Get the sw# from the transfer file
int switch_tf_get_sw(string sw){
    if ((sw.length() == 3 && isdigit(sw.at(2)))){
        return sw.at(2) - '0';
    }
    else if(sw == "null"){
        return -1;
    }
    else{
        cout << "Invalid Field in Traffic file" << endl;
        exit(0);
    }
}

// Get all lines from traffic file, performs error handling
// stores all lines in a data structure so it can be easily
// fed to switch line by line in each iteration
void switch_get_lines_from_traffic_file(string traffic_file_name, int switch_number){
    string line;
    int tf_sw_num, tf_source, tf_dest;
    vector<string> split_line;

    ifstream traffic_file;
    traffic_file.open(traffic_file_name.c_str());

    int error_checker = -1;

    while(getline(traffic_file, line)) {
        error_checker++;
        if (line.at(0) == '#'){
            continue;
        }
        get_vector_input(&split_line, line);

        if (split_line.size()!=3){
            continue;
        }

        tf_sw_num = switch_tf_get_sw(split_line[0]);
        tf_source = atoi(split_line[1].c_str());
        tf_dest = atoi(split_line[2].c_str());

        if(tf_sw_num != switch_number){
            continue;
        }

        struct instruction ins= instruction();
        ins.source_ip = tf_source;
        ins.dest_ip = tf_dest;
        traffic_file_queue.push_back(ins);
    }

    if (error_checker == -1){
        cout << "Invalid or empty traffic_file" << endl;
        exit(0);
    }
}

// Sets up the switch, changes the behaviour of the SIGUSR1 signal,
// initialize the flow table, create all needed fifos, build the poll array,
// send the open message to the controller, and load in the traffic file
void switch_setup(int switch_number, int left, int right, int ip_range_lo, int ip_range_hi, struct pollfd fdarray[], string traffic_file_name){
    switch_newAct.sa_handler = switch_handle_signal_USR1;
    sigaction(SIGUSR1, &switch_newAct, &switch_oldAct);
    cout << "Starting Switch " << switch_number << ". Switch pid: " << getpid() << endl;
    switch_initialize_flow_table(left, right, ip_range_lo, ip_range_hi);
    switch_create_fifos(switch_number, left, right);
    switch_build_poll_array(fdarray);
    switch_send_open_message(switch_number, left, right, ip_range_lo, ip_range_hi);
    switch_get_lines_from_traffic_file(traffic_file_name, switch_number);
}

// Calls list and then shuts down
void switch_exit(){
    switch_list();
    switch_shutdown();
    exit(0);
}

// Searches to see if a instruction is in the flow table, if so then it returns the element
// also passed in a found pointer, the passed pointer is set to the index of the rule if 
// the rule is found, -1 otherwise
struct flow_element switch_instruction_in_flow_table(struct instruction ins, int *found){
    bool source_ip_range_ok, dest_ip_range_ok;
    struct flow_element element;
    for (int i=0; i < switch_flow_table.size(); i++){
        element = switch_flow_table[i];
        source_ip_range_ok = (ins.source_ip >= element.scrIP_lo) && (ins.source_ip <= element.scrIP_hi);
        dest_ip_range_ok = (ins.dest_ip >= element.destIP_lo) && (ins.dest_ip <= element.destIP_hi);
        if (source_ip_range_ok && dest_ip_range_ok){
            (*found) = i;
            return element;
        }
    }
    (*found) = -1;
}

// Takes in a reverse-sorted array of indexes and removes those indexes from the instruction
// waiting queue
void switch_clean_up_waiting_queue(vector<int> *indexes_to_remove){
    for (int i=0; i < (*indexes_to_remove).size(); i++){
        instructions_waiting_queue.erase(instructions_waiting_queue.begin() + (*indexes_to_remove)[i]);
    }
}

// Construct and send the RELAY message to a neigboring switch, also incriment the 
// RELAY transmitted signal counter
void switch_send_relay_message(int switch_number, int target, int source_ip, int dest_ip){
    struct message m;
    struct Relay_Data data;
    struct instruction ins;
    memset((char *) &m, 0, sizeof(m));

    ins.source_ip = source_ip;
    ins.dest_ip = dest_ip;
    data.ins=ins;

    m.type = RELAY;
    m.data.relay_data = data;
    switch_send_message(m, switch_number, target);
    switch_signals.transmitted.relay++;
}

// Iterate through the waiting queue and for each instruction determine if it can
// be executed, if it can then execute it (either just incriment it or incriment it
// and relay it) then remove it from the queue. Otherwise keep it in the queue
void switch_process_waiting_queue(int switch_number, int left, int right){
    vector<int> indexes_to_remove;
    for (int i=0; i < instructions_waiting_queue.size(); i++){
        struct instruction ins = instructions_waiting_queue[i];
        int found = -1;
        struct flow_element element = switch_instruction_in_flow_table(ins, &found);
        if (found == -1){
            continue;
        }
        else{
            indexes_to_remove.push_back(i);
            element.pktCount++;
            switch_flow_table[found] = element;
            if (element.actionVal == 1){
                switch_send_relay_message(switch_number, left, ins.source_ip, ins.dest_ip);
            }
            if (element.actionVal == 2){
                switch_send_relay_message(switch_number, right, ins.source_ip, ins.dest_ip);
            }
        }
    }

    sort(indexes_to_remove.begin(), indexes_to_remove.end(), greater<int>());
    switch_clean_up_waiting_queue(&indexes_to_remove);
}

// Determine if a destination ip has already been sent to the controller
// in a query
bool switch_dest_ip_not_yet_sent(int dest_ip){
    for (int i=0; i < sent_dest_ips.size(); i++){
        if (sent_dest_ips[i] == dest_ip){
            return false;
        }
    }
    return true;
}

// Construct and send the QUERY message to the controller, also incriment the 
// QUERY transmitted signal counter
void switch_send_query_message(struct instruction ins, int switch_number){
    struct message m;
    struct Query_Data data;
    memset((char *) &m, 0, sizeof(m));

    m.type = QUERY;
    data.source_ip = ins.source_ip;
    data.dest_ip = ins.dest_ip;
    m.data.query_data = data;
    switch_send_message(m, switch_number, 0);
    switch_signals.transmitted.query++;
}

// Handle Received messages from other switches, add the new instruction to the waiting queue
// after sending out a query if the switch does not know how to handle the instruction.
// If the switch knows how to handle the instruction, handle it immediatley.
void switch_recieve_message_from_switch(message m, int sw_rf, int switch_number, int left, int right){
    cout << "Recieved " << msg_type_to_string(m.type) << " From Switch " << sw_rf << endl;
    if(m.type == RELAY){
        switch_signals.recieved.relay++;

        struct instruction ins = m.data.relay_data.ins;
        int found = -1;
        struct flow_element element = switch_instruction_in_flow_table(ins, &found);

        if (found == -1){
            if(switch_dest_ip_not_yet_sent(ins.dest_ip)){
                switch_send_query_message(ins, switch_number);
                sent_dest_ips.push_back(ins.dest_ip);
            }
            instructions_waiting_queue.push_back(ins);
            return;
        }
        else{
            element.pktCount++;
            switch_flow_table[found] = element;
            if (element.actionVal == 3){
                return;
            }
            if (element.actionVal == 1){
                switch_send_relay_message(switch_number, left, ins.source_ip, ins.dest_ip);
                return;
            }
            if (element.actionVal == 2){
                switch_send_relay_message(switch_number, right, ins.source_ip, ins.dest_ip);
                return;
            }
            
        }
    }
}

// Determins if a rule is safe to add to the flow table (does not overlap with another rule)
bool rule_safe_to_add(struct flow_element rule){
    int hi = rule.destIP_hi;
    int lo = rule.destIP_lo;
    struct flow_element element;
    for (int i=0; i < switch_flow_table.size(); i++){
        element = switch_flow_table[i];
        if (lo >= element.destIP_lo && lo <= element.destIP_hi){
            return false;
        }
        if (hi >= element.destIP_lo && hi <= element.destIP_hi){
            return false;
        }
    }
    return true;
}
 
// Handle Received messages from controller, if type ACK only incriment counter,
// if of type add, add the rule to the flow table if the rule is safe (does not 
// conflict with a pre-existing rule)
void switch_recieve_message_from_controller(message m){
    cout << "Recieved " << msg_type_to_string(m.type) << " From Controller " << endl;
    if (m.type == ACK){
        switch_signals.recieved.ack++;
        return;
    }
    if (m.type == ADD && rule_safe_to_add(m.data.add_data.rule)){
        switch_signals.recieved.add++;
        switch_flow_table.push_back(m.data.add_data.rule);
        return;
    }
}

// Process a current line from the traffic file, admit the line, query the controller if nessesary
// if we can't resolve it immideatly we must add it to the waiting queue, otherwise resolve it
void switch_process_current_line_from_traffic_file(int tf_index, int switch_number, int left, int right){
    if (tf_index >= traffic_file_queue.size()){
        return;
    }

    switch_signals.recieved.admit++;

    struct instruction ins = traffic_file_queue[tf_index];
    int found = -1;
    struct flow_element element = switch_instruction_in_flow_table(ins, &found);

    if (found == -1){
        if(switch_dest_ip_not_yet_sent(ins.dest_ip)){
            switch_send_query_message(ins, switch_number);
            sent_dest_ips.push_back(ins.dest_ip);
        }
        instructions_waiting_queue.push_back(ins);
        return;
    }
    else{
        element.pktCount++;
        switch_flow_table[found] = element;
        if (element.actionVal == 3){
            return;
        }
        if (element.actionVal == 1){
            switch_send_relay_message(switch_number, left, ins.source_ip, ins.dest_ip);
            return;
        }
        if (element.actionVal == 2){
            switch_send_relay_message(switch_number, right, ins.source_ip, ins.dest_ip);
            return;
        }
        
    }
}

// The main switch loop. Polls all file descriptors to detect any incomming
// messages and handles them accordingly
void switch_main(struct pollfd fdarray[], int number_of_fds, int switch_number, int left, int right){
    int rval;
    string line;

    int tf_index = 0;

    while (true){
        switch_process_current_line_from_traffic_file(tf_index, switch_number, left, right);
        tf_index++;

        rval = poll(fdarray, number_of_fds, 10);
        if (rval > 0){
            for (int i=0; i <= number_of_fds; i++){
                if (fdarray[i].revents & POLLIN){
                    // stdin (keyboard)
                    if (switch_fd_list[i].switch_number == -1){
                        cin >> line;
                        cout << "Recieved " << line << " From Keyboard" << endl;
                        if (line == "exit"){
                            switch_exit();
                        }
                        if (line == "list"){
                            switch_list();
                        }
                    }
                    // switch or controller
                    else{
                        struct message m;
                        memset( (char *) &m, 0, sizeof(m) );
                        read(fdarray[i].fd, (char*)&m, sizeof(m));

                        if (switch_fd_list[i].switch_number == 0){
                            switch_recieve_message_from_controller(m);
                        }
                        else{
                            switch_recieve_message_from_switch(m, switch_fd_list[i].switch_number, switch_number, left, right);
                        }
                        switch_process_waiting_queue(switch_number, left, right);
                    }
                }
            }
        }
    }
}

// The switch initialization function, setup the switch, run the main loop
// when done shutdown the switch.
void switch_init(int switch_number, string traffic_file_name, int left, int right, int ip_range_lo, int ip_range_hi){
    int number_of_fds = 2; // controller + keyboard
    if(left !=-1){
        number_of_fds++;
    }
    if(right !=-1){
        number_of_fds++;
    }
    struct pollfd fdarray[number_of_fds];
    switch_setup(switch_number, left, right, ip_range_lo, ip_range_hi, fdarray, traffic_file_name);
    switch_main(fdarray, number_of_fds, switch_number, left, right);
    switch_shutdown();
}