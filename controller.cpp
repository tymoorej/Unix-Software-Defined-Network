// the controller file has all of the methods and data needed for the controller

#include "controller.h"
#include "shared.h"

// these structs are needed to process SIGUSR1 signals
struct sigaction controller_oldAct, controller_newAct;

// declaring a map which will map switch numbers to all the data the controller has on them
map<int, struct controller_known_switch_data> controller_switch_map;

// used to keep track of all the signals the controller can send or recieve
struct controller_signal_count{
    int open;
    int ack;
    int query;
    int add;

    controller_signal_count(){
        this->open=0;
        this->ack=0;
        this->query=0;
        this->add=0;
    };
    void print_receiving(){
        cout << "OPEN: " << this->open << ",   ";
        cout << "QUERY: " << this->query << endl;
    };
    void print_transmitting(){
        cout << "ACK: " << this->ack << ",   ";
        cout << "ADD: " << this->add << endl;
    };

};

// also used to keep track of all the signals the controller can send or recieve
// instantiates two controller_signal_count structs. One for recieving and one
// for terminating
struct controller_total_signals{
    struct controller_signal_count recieved;
    struct controller_signal_count transmitted;

    // print all recieved and transmitted signals
    void print(){
        cout << "Packet Stats:" << endl;
        cout << "\tRecieved: ";
        this->recieved.print_receiving();
        cout << "\tTransmitted: ";
        this->transmitted.print_transmitting();
    }
};

// declaring an instance of controller_total_signals
struct controller_total_signals controller_signals;

// This struct holds all the data a controller has on the switches
struct controller_known_switch_data{
    int switch_number;
    int read_fd;
    int write_fd;
    int left;
    int right;
    int ip_lo;
    int ip_hi;
    bool active;
    controller_known_switch_data(int switch_number){
        this->switch_number = switch_number;
        this->active = false;
        this->read_fd = -1;
        this->write_fd = -1;

    };
    // Print the switches data
    void print(){
        if (this->active){
            printf("[sw%d] port1= %d, port2= %d, port3= %d-%d\n", \
            this->switch_number, this->left, this->right, this->ip_lo, this->ip_hi);
        }
    }
};

// Print all of the known data for each switch
void controller_print_switch_info(){
    for (map<int, struct controller_known_switch_data>::iterator it = controller_switch_map.begin(); it != controller_switch_map.end(); ++it){
        it->second.print();
    }
}

// Print switch data and all signals revcieved by or transmitted by the controller
void controller_list(){
    controller_print_switch_info();
    controller_signals.print();
}

// This function is called when the process receives the SIGUSR1 signal
// it calls the list function
void controller_handle_signal_USR1(int sigNo){
    controller_list();
}

// Create all fifos needed for communication to and from the controller
// opens all fifos the controller reads from but not fifos the controller 
// writes to. Stores these read fds in the switch data structure
void controller_create_fifos(int num_of_switches, struct pollfd fdarray[]){
    string first, second;
    int fd;
    // for each switch, create 2 fifos, one for reading, one for writing,
    // open and store the fds for all reading fifos
    // do not open writing fifos
    for (int i = 1; i <= num_of_switches; i++){
        first = "fifo-0-" + int_to_string(i);
        second = "fifo-" + int_to_string(i) + "-0";

        mkfifo(first.c_str(), 0666);
        mkfifo(second.c_str(), 0666);

        struct controller_known_switch_data switch_data = controller_known_switch_data(i);
        fd = open(second.c_str(), O_RDWR | O_NONBLOCK);
        switch_data.read_fd = fd;

        controller_switch_map.insert(map<int, struct controller_known_switch_data>::value_type(i, switch_data));
    }
}

// Remove all fifos created. Called on startup and shutdown of controller
void controller_remove_fifos(){
    system("rm fifo-* -f");
}

// builds the pollfd array which will hold all of the fds and thier events
void controller_build_poll_array(struct pollfd fdarray[]){
    fdarray[0].fd = STDIN_FILENO;
    fdarray[0].events = POLLIN;
	fdarray[0].revents = 0;

    for (map<int, struct controller_known_switch_data>::iterator it = controller_switch_map.begin(); it != controller_switch_map.end(); ++it){
        fdarray[it->first].fd = it->second.read_fd;
        fdarray[it->first].events= POLLIN;
	    fdarray[it->first].revents= 0;
    }

}

// Setup the controller, do this by setting the signal handler,
// removing all pre-existing fifos, creating all needed fifos,
// and then creating the poll array
void controller_setup(int num_of_switches, struct pollfd fdarray[]){
    controller_newAct.sa_handler = controller_handle_signal_USR1;
    sigaction(SIGUSR1, &controller_newAct, &controller_oldAct);

    cout << "Starting Controller. Controller pid: " << getpid() << endl;

    controller_remove_fifos();
    controller_create_fifos(num_of_switches, fdarray);
    controller_build_poll_array(fdarray);
}

// Shutdown the controller, restore signal behaviour, and remove all fifos
void controller_shutdown(){
    cout << "Controller Shutting Down" << endl;
    sigaction(SIGUSR1,&controller_oldAct,&controller_newAct);
    controller_remove_fifos();
}

// Exit the controller, first call list to do all needed printing
// then shutdown the controller and exit the process
void controller_exit(){
    controller_list();
    controller_shutdown();
    exit(0);
}

// For a certain switch, get the cooresponding reading fd
int controller_get_fd_read(int target){
    for (map<int, struct controller_known_switch_data>::iterator it = controller_switch_map.begin(); it != controller_switch_map.end(); ++it){
        if (it->second.switch_number == target){
            return it->second.write_fd;
        }
    }
    return -1;
}

// For a certain switch, get the cooresponding writing fd
int controller_get_fd_write(int target){
    for (map<int, struct controller_known_switch_data>::iterator it = controller_switch_map.begin(); it != controller_switch_map.end(); ++it){
        if (it->second.switch_number == target){
            return it->second.write_fd;
        }
    }
    return -1;
}

// Set the writing fd for a specific switch
void controller_set_fd_write(int target, int fd){
    for (map<int, struct controller_known_switch_data>::iterator it = controller_switch_map.begin(); it != controller_switch_map.end(); ++it){
        if (it->second.switch_number == target){
            it->second.write_fd=fd;
        }
    }
}

// Send a message to a switch from the controller
// also opens the write fifo if not already open
void controller_send_message(struct message m, int target){
    cout << "Transmitting " << msg_type_to_string(m.type) << " to Switch " << target << endl;

    int fd = controller_get_fd_write(target);
    if (fd == -1){
        string ds = "fifo-0-" + int_to_string(target);
        fd  = open(ds.c_str(), O_WRONLY | O_NONBLOCK);
        controller_set_fd_write(target, fd);
    }

    write(fd, (char*)&m, sizeof(m));
}

// Construct and send the ACK message also incriment the 
// ack transmitted signal counter
void controller_send_ack_message(int switch_number){
    struct message m;
    struct Ack_Data data;
    memset((char *) &m, 0, sizeof(m));

    m.type = ACK;
    m.data.ack_data = data;
    controller_send_message(m, switch_number);
    controller_signals.transmitted.ack++;
}

// Set all the known data of a switch after receiving an open signal
void controller_set_open_data(int target, struct Open_Data data){
    for (map<int, struct controller_known_switch_data>::iterator it = controller_switch_map.begin(); it != controller_switch_map.end(); ++it){
        if (it->second.switch_number == target){
            it->second.left = data.left;
            it->second.right = data.right;
            it->second.ip_lo = data.ip_range_lo;
            it->second.ip_hi = data.ip_range_hi;
            it->second.active = true;
        }
    }
}

// Determines if the dest ip is an ip address that the controller does not know about
bool controller_ip_not_known(int dest_ip){
    for (map<int, struct controller_known_switch_data>::iterator it = controller_switch_map.begin(); it != controller_switch_map.end(); ++it){
        if ((dest_ip >= it->second.ip_lo) && (dest_ip <= it->second.ip_hi)){
            return false;
        }
    }
    return true;
}

// Constructs and sends an add-drop message from the controller to a specific switch
void controller_send_add_drop_message(int dest_ip_lo, int dest_ip_hi, int switch_number){
    struct message m;
    struct Add_Data data;
    struct flow_element element;
    memset((char *) &m, 0, sizeof(m));


    element.scrIP_lo = 0;
    element.scrIP_hi = MAXIP;
    element.destIP_lo = dest_ip_lo;
    element.destIP_hi = dest_ip_hi;
    element.actionType = DROP;
    element.actionVal = 0;
    element.pri = 4;
    element.pktCount = 0;

    m.type = ADD;
    data.rule = element;
    m.data.add_data = data;
    controller_send_message(m, switch_number);
    controller_signals.transmitted.add++;
}

// For a specific switch get all the data the controller has about it
struct controller_known_switch_data controller_switch_number_to_data(int switch_number){
    for (map<int, struct controller_known_switch_data>::iterator it = controller_switch_map.begin(); it != controller_switch_map.end(); ++it){
        if (it->second.switch_number == switch_number){
            return it->second;
        }
    }
}

// Constructs and sends an add-forward message from the controller to a specific switch
void controller_send_add_forward_message(int dest_ip_lo, int dest_ip_hi, int switch_number, int port){
    struct message m;
    struct Add_Data data;
    struct flow_element element;
    memset((char *) &m, 0, sizeof(m));


    element.scrIP_lo = 0;
    element.scrIP_hi = MAXIP;
    element.destIP_lo = dest_ip_lo;
    element.destIP_hi = dest_ip_hi;
    element.actionType = FORWARD;
    element.actionVal = port;
    element.pri = 4;
    element.pktCount = 0;

    m.type = ADD;
    data.rule = element;
    m.data.add_data = data;
    controller_send_message(m, switch_number);
    controller_signals.transmitted.add++;
}

// Recursive function which determines if a switch is able to relay a message to the right
bool controller_possible_to_reach_right(int switch_number, int dest_ip){
    if (switch_number == -1){
         return false;
    }
    struct controller_known_switch_data current_switch = controller_switch_number_to_data(switch_number);

    if (!current_switch.active){
        return false;
    }

    if (dest_ip >= current_switch.ip_lo && dest_ip <= current_switch.ip_hi){
        return true;
    }
    if (current_switch.right == -1){
        return false;
    }
    return controller_possible_to_reach_right(current_switch.right, dest_ip);
}

// Recursive function which determines if a switch is able to relay a message to the left
bool controller_possible_to_reach_left(int switch_number, int dest_ip){
    if (switch_number == -1){
         return false;
    }
    struct controller_known_switch_data current_switch = controller_switch_number_to_data(switch_number);

    if (!current_switch.active){
        return false;
    }

    if (dest_ip >= current_switch.ip_lo && dest_ip <= current_switch.ip_hi){
        return true;
    }
    if (current_switch.left == -1){
        return false;
    }
    return controller_possible_to_reach_left(current_switch.left, dest_ip);
}

// Given a destination ip, get the ip range it belongs to
void controller_dest_ip_to_ip_range(int dest_ip, int dest_ip_range[]){
    for (map<int, struct controller_known_switch_data>::iterator it = controller_switch_map.begin(); it != controller_switch_map.end(); ++it){
        if (dest_ip >= it->second.ip_lo && dest_ip <= it->second.ip_hi){
            dest_ip_range[0] = it->second.ip_lo;
            dest_ip_range[1] = it->second.ip_hi;
            return;
        }
    }
} 

// Given a switch number and destination ip, this function either sends
// an add-forward message to the proper switch and specifies which port to use
// or sends an add-drop message if the destination ip is not reachable from the source
void controller_path_to_target(int switch_number, int dest_ip){
    struct controller_known_switch_data current_switch = controller_switch_number_to_data(switch_number);
    if (dest_ip >= current_switch.ip_lo && dest_ip <= current_switch.ip_hi){
        controller_send_add_forward_message(current_switch.ip_lo, current_switch.ip_hi, switch_number, 3);
        return;
    }

    int dest_ip_range[2];
    controller_dest_ip_to_ip_range(dest_ip, dest_ip_range);

    if (controller_possible_to_reach_right(current_switch.right, dest_ip)){
        controller_send_add_forward_message(dest_ip_range[0], dest_ip_range[1], switch_number, 2);
        return;
    }
    if (controller_possible_to_reach_left(current_switch.left, dest_ip)){
        controller_send_add_forward_message(dest_ip_range[0], dest_ip_range[1], switch_number, 1);
        return;
    }
    else{
        controller_send_add_drop_message(dest_ip_range[0], dest_ip_range[1], switch_number);
        return;
    }
    
}

// Given a rule and a switch number, sends the correct rule to the switch
// which sent the query
void controller_send_add_message(struct Query_Data rule, int switch_number){
    if (controller_ip_not_known(rule.dest_ip)){
        controller_send_add_drop_message(rule.dest_ip, rule.dest_ip, switch_number);
        return;
    }
    controller_path_to_target(switch_number, rule.dest_ip);
}

// Handles the messages a controller can receive from a switch
void controller_recieve_message(message m, int switch_number){
    cout << "Recieved " << msg_type_to_string(m.type) << " From Switch " << switch_number << endl;
    if (m.type == OPEN){
        controller_signals.recieved.open++;
        controller_set_open_data(switch_number, m.data.open_data);
        controller_send_ack_message(switch_number);
        return;
    }
    if (m.type == QUERY){
        controller_signals.recieved.query++;
        controller_send_add_message(m.data.query_data, switch_number);
    }
}

// The main controller loop. Polls all file descriptors to detect any incomming
// messages and handles them accordingly
void controller_main(struct pollfd fdarray[], int num_of_switches){
    int rval;
    string line;
    while (true){
        rval = poll(fdarray, num_of_switches + 1 , 10);
        if (rval > 0){
            for (int i=0; i <= num_of_switches; i++){
                if (fdarray[i].revents & POLLIN){
                    // stdin (keyboard)
                    if (i == 0){
                        cin >> line;
                        cout << "Recieved " << line << " From Keyboard" << endl;
                        if (line == "exit"){
                            controller_exit();
                        }
                        if (line == "list"){
                            controller_list();
                        }
                    }
                    // switch
                    else{
                        struct message m;
                        memset( (char *) &m, 0, sizeof(m) );
                        read(fdarray[i].fd, (char*)&m, sizeof(m));
                        controller_recieve_message(m, i);
                    }
                }
            }
        }
    }
}

// The controller initialization function, setup the controller, run the main loop
// when done shutdown the controller.
void controller_init(int num_of_switches){
    struct pollfd fdarray[num_of_switches + 1];
    controller_setup(num_of_switches, fdarray);
    controller_main(fdarray, num_of_switches);
    controller_shutdown();
}