#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <string>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdexcept>
#include <vector>
#include <memory>
#include <unordered_set>
#include <poll.h>

#define MAX_BUFFER_SIZE 1024
#define MAX_LISTEN 20

#define ACTIONS "pick one:  rock, paper, scissors\n"

#define END_GAME "end_game"

#define MAX_BUFFER_SIZE 1024
#define INVALID_ROOM "invalid_room"
#define VALID_ROOM "ok"

#define ROCK "rock"
#define PAPER "paper"
#define SCISSORS "scissors"

#define WON "You Won!"
#define LOST "You Lost!"

#define ROOM "Room"
#define SERVER "Server"

#define START 1
#define GET_NAME 2
#define ROOM_CHECK 3
#define IN_ROOM 4

using namespace std;

int setupServer_tcp(int port, char* ip, string mode);
int setupServer_udp(string mode);

int state = START;


class Client
{
private:
    int fd_server;  // when accepted by the Server it returns a fd
    int fd_room;  // when accepted by the Room it returns a fd
    int state = GET_NAME;
    string name;
    int wins;
    int loses;
    int ties;
public:
    Client(int fd);
    ~Client();
    int get_state() {return state;}
    int get_fd_server() {return fd_server;}
    int get_fd_room() {return fd_room;}
    string get_name() {return name;}
    void set_fd_room(int fd) {this->fd_room = fd;}
    void set_state(int state){this->state = state;}
    void set_name(string namee) {this->name = namee;}
    void increment_wins(){this->wins++;}
    void increment_loses(){this->loses++;}
    void increment_ties(){this->ties++;}
    int get_wins() {return wins;}
    int get_loses() {return loses;}
    int get_ties() {return ties;}
};

Client::Client(int fdd)
{
    this->fd_server = fdd;
}

Client::~Client()
{
}


class SubServer {
private:
    static int instanceCount;
    int fd_tcp; 
    int fd_udp;
    vector<int> fd_players;  // the client-room fd
    int port_tcp;
    int port_udp = 8080;  // hard-wired
    int id_room;
    int num_of_players_in = 0;
    const int MAX_PLAYERS = 2;
    int status = GET_NAME;
    int number_of_actions_played = 0;

    string player1_action;
    string player2_action;

public:
    SubServer(int fd_tcpp, int fd_udpp, int port);
    ~SubServer();
    bool is_full();
    void add_player(int fd);
    int get_port(); 
    int get_fd_tcp(){return fd_tcp;}
    static int getInstanceCount() { return instanceCount; }
    vector<int> get_fds(){return fd_players;}
    void send_players_actions();
    void add_action(string action, int fd);
    bool did_all_players_make_action();
    int check_match_result();  // rerturns the fds of the winner - if equal return -1
    void send_end_match_msg(int fd_won, vector<Client>& client_objs);  // sends to both winner and loser
    void clear_room();  /////////////////////////////////////////
    void clear_one_hand();  /////////////////////////////////////////
};

void endEntireGameMessage(int fd_server_udp, const std::vector<Client>& client_objs, const char* buffer);
void sendBroadcastMessage(int fd_udp, char* message, int port);
Client* getClientByFdRoom(int fd, std::vector<Client>& client_objs);

int SubServer::instanceCount = 0;

SubServer::SubServer(int fd_tcpp, int fd_udpp, int port) : fd_tcp(fd_tcpp), fd_udp(fd_udpp), port_tcp(port), id_room(this->instanceCount) {
    instanceCount++;
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "SubServer created with fd_tcp: %d\nSubServer created with fd_udp: %d\n", fd_tcp, fd_udp);
    write(1, buffer, strlen(buffer)); 
}

SubServer::~SubServer() {
    //instanceCount--;
    if (fd_tcp >= 0) {
        close(fd_tcp);
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "SubServer with fd: %d closed.\n", fd_tcp);
        write(1, buffer, strlen(buffer)); 
    }
    if (fd_udp >= 0) {
        close(fd_udp);
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "SubServer with fd: %d closed.\n", fd_udp);
        write(1, buffer, strlen(buffer)); 
    }
}

int SubServer::get_port() {
    return this->port_tcp;
}

bool SubServer::is_full() {
    return this->num_of_players_in >= MAX_PLAYERS;
}

void SubServer::add_player(int fd) {
    if (!is_full()) {
        num_of_players_in++;
        fd_players.push_back(fd);
        // write(1, "Player added to SubServer.\n", strlen("Player added to SubServer.\n"));/////////////////
    }
}

void SubServer::send_players_actions() {

    // char msg_success[MAX_BUFFER_SIZE];
    // snprintf(msg_success, sizeof(msg_success), "number of fd_players: %d\n", fd_players.size());
    // write(1, msg_success, strlen(msg_success)); 


    for (int fd: fd_players) {

        // send the actions(rock, paper, scissors) to the two players 
        send(fd, ACTIONS, strlen(ACTIONS), 0);
        // write(1, ACTIONS, strlen(ACTIONS));
        // snprintf(msg_success, sizeof(msg_success), "fd:  %d\n", fd);
        // write(1, msg_success, strlen(msg_success));

        
    }
}

bool SubServer::did_all_players_make_action() {
    return (player1_action != "" && player2_action != "") ? true: false;
}

void SubServer::add_action(string action, int fd) {
    this->number_of_actions_played ++;
    if (fd == fd_players[0]) {
        player1_action = action;
    }
    else if(fd == fd_players[1]) {
        player2_action = action;
    }
    else {
        write(2, "something went horibbly wrong in add action\n", strlen("something went horibbly wrong in add action\n"));
    }
}

int SubServer::check_match_result() {
    if (player1_action == player2_action) {
        return -1;
    }
    else if (player1_action == ROCK && player2_action == SCISSORS) {
        return fd_players[0];
    }
    else if (player1_action == ROCK && player2_action == PAPER) {
        return fd_players[1];
    }
    else if (player1_action == PAPER && player2_action == SCISSORS) {
        return fd_players[1];
    }
    else if (player1_action == PAPER && player2_action == ROCK) {
        return fd_players[0];
    }
    else if (player1_action == SCISSORS && player2_action == ROCK) {
        return fd_players[1];
    }
    else if (player1_action == SCISSORS && player2_action == PAPER) {
        return fd_players[0];
    }
    else {
        write(2, "something wierd has happened in check_match_result()\n", strlen("something wierd has happened in check_match_result()\n"));
        return -1;
    }
}

void SubServer::clear_room() {

    num_of_players_in = 0;
    number_of_actions_played = 0;

    fd_players.clear();

    string player1_action = "";
    string player2_action = "";
}

void SubServer::clear_one_hand() {

    string player1_action = "";
    string player2_action = "";
}

void SubServer::send_end_match_msg(int fd_won, vector<Client>& client_objs) {
    string message;
    
    try {
        // Retrieve the names of the players using fd_players vector and getClientByFdRoom
        Client* player1 = getClientByFdRoom(fd_players[0], client_objs);
        Client* player2 = getClientByFdRoom(fd_players[1], client_objs);
        
            if (fd_won == -1) {
                // No winner (draw scenario)
                message = "The game in room " + to_string(port_tcp) + " has concluded in an epic draw!\n";
                message += "Both players showed exceptional skill, making it a match to remember.\n";
                message += "Congratulations to " + player1->get_name() + " and " + player2->get_name() + " for a fantastic game!";

                player1->increment_ties();
                player2->increment_ties();
            }

        else if (fd_won == fd_players[0]) {
            // Player 1 won
            message = "The game in room " + to_string(port_tcp) + " has finished.\n";
            message += player1->get_name() + " won the game.\n";
            message += player2->get_name() + " lost the game.\n";
            player1->increment_wins();
            player2->increment_loses();
        } 
        else if (fd_won == fd_players[1]) {
            // Player 2 won
            message = "The game in room " + to_string(port_tcp) + " has finished.\n";
            message += player2->get_name() + " won the game.\n";
            message += player1->get_name() + " lost the game.\n";
            player2->increment_wins();
            player1->increment_loses();
        } 
        else {
            // Invalid fd_won value
            write(2, "something went terribly wrong\n", strlen("something went terribly wrong\n"));
            return;
        }

        // Convert message to C-string and send it using the UDP broadcast
        write(1, message.c_str(), strlen(message.c_str()));////////////////////////////////////
        sendBroadcastMessage(this->fd_udp, const_cast<char*>(message.c_str()), port_tcp);
    } 
    catch (const std::runtime_error& e) {
        write(2, "Error retrieving client: ", strlen("Error retrieving client: "));
        write(2, e.what(), strlen(e.what()));
        write(2, "\n", 1);
    }
}


void sendBroadcastMessage(int fd_udp, char* message, int port) {
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));

    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(port);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);  // Set to broadcast address

    // Set socket options to allow broadcasting
    int broadcastEnable = 1;
    if (setsockopt(fd_udp, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("setsockopt failed");
        return;
    }

    // Send the broadcast message
    ssize_t bytes_sent = sendto(fd_udp, message, strlen(message), 0,
                                (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
            char bytes_str[20];
        int len = snprintf(bytes_str, sizeof(bytes_str), "%zd\n", bytes_sent);
        write(1, bytes_str, len);
    write(1, "\nheyimBored\n", strlen("heyimBored\n"));
    // Prepare the feedback message
    char feedback_msg[256];
    if (bytes_sent < 0) {
        snprintf(feedback_msg, sizeof(feedback_msg), "Broadcast message failed to send: %s\n", strerror(errno));
    } else {
        snprintf(feedback_msg, sizeof(feedback_msg), "Broadcast message sent: %s\n", message);
    }

    // // Optionally, you can send feedback message back
    // sendto(fd_udp, feedback_msg, strlen(feedback_msg), 0,
    //        (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
}

void endEntireGameMessage(int fd_server_udp, vector<Client>& client_objs, char* buffer) {
    char end_msg[MAX_BUFFER_SIZE];
    
    // Construct the ending message
    snprintf(end_msg, sizeof(end_msg),
             "Game has ended with the command: %c.\n"
             "Players' results:\n",
             buffer[0]);

    // Add each player's results
    for (auto client : client_objs) {
        snprintf(end_msg + strlen(end_msg), sizeof(end_msg) - strlen(end_msg),
                 "%s - Wins: %d, Losses: %d, Ties: %d\n",
                 client.get_name().c_str(), client.get_wins(), client.get_loses(), client.get_ties());
    }

    // Send the message using the write system call
    write(fd_server_udp, end_msg, strlen(end_msg));
}

void close_all_fds(vector<pollfd>& pfds) {
    for(auto& pollfd: pfds) {
        if (pollfd.fd != 0) 
            close(pollfd.fd);
    }
}

char* getBroadcastAddress() {
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;
    char* broadcastIp = nullptr;  // Ensure broadcastIp is initialized to nullptr

    if (getifaddrs(&ifap) == -1) {
        perror("getifaddrs");
        return nullptr;
    }

    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && ifa->ifa_broadaddr) {  // Check if ifa_broadaddr is not nullptr
            sa = (struct sockaddr_in *) ifa->ifa_broadaddr;
            if (sa) {
                broadcastIp = strdup(inet_ntoa(sa->sin_addr));  // Duplicate IP as char*
                break;
            }
        }
    }

    freeifaddrs(ifap);
    return broadcastIp; // Caller is responsible for freeing this memory
}

Client* getClientByName(string name, vector<Client>& client_objs) {
    for (auto& client : client_objs) {
        if (client.get_name() == name) {
            return &client; // Return pointer to the found Client object
        }
    }
    return nullptr; // Return nullptr if no match is found
}

char* getAvailablePortsString(const std::vector<SubServer*>& emptySubServers) {
    std::vector<int> availablePorts;

    for (const auto& subServer : emptySubServers) {
        availablePorts.push_back(subServer->get_port());
    }

    // Calculate required buffer size
    size_t bufferSize = 0;
    for (int port : availablePorts) {
        bufferSize += std::to_string(port).length() + 1; // Add 1 for space or null terminator
    }

    // Allocate memory for the result string
    char* portList = new char[bufferSize];
    portList[0] = '\0'; // Initialize to empty string

    // Concatenate ports into portList
    for (size_t i = 0; i < availablePorts.size(); ++i) {
        strcat(portList, std::to_string(availablePorts[i]).c_str());
        if (i < availablePorts.size() - 1) {
            strcat(portList, " ");
        }
    }

    return portList;
}

Client* getClientByFdRoom(int fd, std::vector<Client>& client_objs) {
    for (auto& client : client_objs) {
        if (client.get_fd_room() == fd) {
            return &client;
        }
    }
    throw std::runtime_error("Client with the specified fd not found.");
}

Client* getClientByFdServer(int fd, std::vector<Client>& client_objs) {
    for (auto& client : client_objs) {
        if (client.get_fd_server() == fd) {
            return &client;
        }
    }
    throw std::runtime_error("Client with the specified fd not found.");
}

SubServer* findSubServerByFdTcp(int fd_tcp, const vector<unique_ptr<SubServer>>& subServers) {
    for (const auto& subServer : subServers) {
        if (subServer->get_fd_tcp() == fd_tcp) {
            return subServer.get();
        }
    }
    return nullptr;  // Return nullptr if no matching SubServer is found
}

SubServer* findSubServerByFdPlayer(int fd_player, const vector<unique_ptr<SubServer>>& subServers) {
    for (const auto& subServer : subServers) {
        if (subServer->get_fds()[0] == fd_player || subServer->get_fds()[1] == fd_player) {
            return subServer.get();
        }
    }
    return nullptr;  // Return nullptr if no matching SubServer is found
}

void setupSubServers(vector<unique_ptr<SubServer>>& subServers, int num_of_rooms, int base_port, char* ip, vector<pollfd> &pfds, unordered_set<int>& subserver_fds, int fd_udp) {
    for (int i = 0; i < num_of_rooms; i++) {
        int room_port = base_port + i + 1;
        int fd_tcp = setupServer_tcp(room_port, ip, ROOM);
        subserver_fds.insert(fd_tcp);
        subServers.emplace_back(make_unique<SubServer>(fd_tcp, fd_udp, room_port));
        pfds.push_back(pollfd{fd_tcp, POLLIN, 0});
        pfds.push_back(pollfd{fd_udp, POLLIN, 0});

        char buffer[100];
        snprintf(buffer, sizeof(buffer), "Subserver for room %d is running on port %d\n\n", i + 1, room_port);
        write(1, buffer, strlen(buffer));          
    }
}

vector<SubServer*> findEmptySubServers(const vector<unique_ptr<SubServer>>& subServers) {
    vector<SubServer*> emptySubServers;
    for (const auto& subServer : subServers) {
        if (!subServer->is_full()) {
            emptySubServers.push_back(subServer.get());
        }
    }
   return emptySubServers;
}

void error_handler_setup(const char* error_msg, int fd) {
    write(2, error_msg, strlen(error_msg));
    close(fd);
    exit(EXIT_FAILURE);
}

int setupServer_tcp(int port, char* ip, string mode) {
    struct sockaddr_in address;
    const char* error_msg;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);  // creation of socket
    if (server_fd < 0) {
        error_msg = "socket creation didnt work\n";
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {  // for configuration of socket
        error_msg = "setsockopt didnt work\n";
        error_handler_setup(error_msg, server_fd);
    }  
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {  // for configuration of socket
        error_msg = "setsockopt didnt work\n";
        error_handler_setup(error_msg, server_fd);
    }  

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
        write(2, "inet_pton in setupServer_tcp Invalid address\n", strlen("inet_pton in setupServer_tcp Invalid address\n"));
        exit(EXIT_FAILURE);
    }

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {  // binding
        error_msg = "setsockopt didnt work\n";
        error_handler_setup(error_msg, server_fd);
    }

    if (listen(server_fd, MAX_LISTEN) < 0) {  // listening
        error_msg = "setsockopt didnt work\n";
        error_handler_setup(error_msg, server_fd);
    }

    char msg_success[MAX_BUFFER_SIZE];
    snprintf(msg_success, sizeof(msg_success), "TCP fd setup at %s : %d\n%s is now listening\n", ip, port, mode.c_str());
    write(1, msg_success, strlen(msg_success));

    return server_fd;
}

int setupServer_udp(string mode) {
    int server_fd;
    struct sockaddr_in address;

    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        write(2, "in setupServer_udp Socket creation failed\n", strlen("in setupServer_udp Socket creation failed\n"));
        return -1;
    }

    int opt = 1;
    int broadcast = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        write(2, "Failed to set SO_BROADCAST\n", strlen("Failed to set SO_BROADCAST\n"));
        close(server_fd);
        return -1;
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        write(2, "Failed to set SO_REUSEPORT\n", strlen("Failed to set SO_REUSEPORT\n"));
        close(server_fd);
        return -1;
    }


    memset(&address, 0, sizeof(address));

    char* ip = (char*)"255.255.255.255"  /*getBroadcastAddress()*/;
    int port = 8080;

    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
        write(2, "Invalid address or not supported\n", strlen("Invalid address or not supported\n"));
        close(server_fd);
        return -1;
    }

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        write(2, "in setupServer_udp Bind failed\n", strlen("in setupServer_udp Bind failed\n"));
        close(server_fd);
        return -1;
    }

    char msg_success[MAX_BUFFER_SIZE];
    snprintf(msg_success, sizeof(msg_success), "UDP fd setup at %s : %d\n%s is now Binded\n", ip, port, mode.c_str());
    write(1, msg_success, strlen(msg_success));

    return server_fd; 
}

int acceptClient_tcp(int server_fd) {
    int client_fd;
    struct sockaddr_in client_address;
    socklen_t address_len = sizeof(client_address);

    // Clear the client_address structure
    memset(&client_address, 0, sizeof(client_address));

    client_fd = accept(server_fd, (struct sockaddr *)&client_address, &address_len);
    if (client_fd < 0) {
        write(2, "acceptClient_tcp didnt work\n", strlen("acceptClient_tcp didnt work\n"));
        exit(EXIT_FAILURE);
    }

    return client_fd; // Return the client file descriptor
}

int main(int argc, char* argv[]) {
    cout << argc << endl;
    if (argc != 4) {
        write(2, "Args not valid\n", strlen("Args not valid\n"));
        exit(EXIT_FAILURE);
    }

    char* ip = argv[1];
    int port_server = atoi(argv[2]);
    char* char_num_of_rooms = argv[3];
    int num_of_rooms = atoi(char_num_of_rooms);
    int port_udp = 8080;

    int fd_server_tcp, fd_server_udp, fd_new_client_tcp, fd_new_player_tcp;
    char buffer[MAX_BUFFER_SIZE] = {0};
    vector<pollfd> pfds;

    fd_server_tcp = setupServer_tcp(port_server, ip, SERVER);  // Main Server Setup
    fd_server_udp = setupServer_udp(SERVER);
    write(1, "Main Server has been setup.\n\n", strlen("Main Server has been setup.\n\n"));

    pfds.push_back(pollfd{fd_server_tcp, POLLIN, 0});
    pfds.push_back(pollfd{fd_server_udp, POLLIN, 0});
    pfds.push_back(pollfd{0, POLLIN, 0});  // add the stdin so that we could get the 


    unordered_set<int> client_server_fds;
    unordered_set<int> subserver_fds;
    unordered_set<int> client_room_fds;
    unordered_set<int> queue_temprory;

    vector<unique_ptr<SubServer>> subServers; 
    vector<Client> client_objs;
    SubServer* subServer_obj;
    Client* client_obj;

    setupSubServers(subServers, num_of_rooms, port_server, ip, pfds, subserver_fds, fd_server_udp);  // SubServers setup

    write(1, "Server is running...\n\n", strlen("Server is running...\n\n"));

    ssize_t bytes_received;
    struct sockaddr_in server_address;  // for udp connection
    socklen_t server_address_len = sizeof(server_address);  // for udp connection



    while (true) {
        if (poll(pfds.data(), (nfds_t)(pfds.size()), -1) == -1) 
            write(2, "FAILED: Poll in client\n", strlen("FAILED: Poll in client\n"));
        
        for (int i = 0; i < pfds.size(); i++) {
            if(pfds[i].revents & POLLIN) {
                if(pfds[i].fd == fd_server_tcp) { // add new user

                    // accept the icnoming client without any exceptions
                    fd_new_client_tcp = acceptClient_tcp(fd_server_tcp);

                    // add the new client to the client_objs/pfds/client_server_fds
                    client_objs.push_back(Client(fd_new_client_tcp));
                    pfds.push_back(pollfd{fd_new_client_tcp, POLLIN, 0});
                    client_server_fds.insert(fd_new_client_tcp);

                    //send the client the welcome message
                    send(fd_new_client_tcp, "Welcome to the game!\nPlease write your name: ", strlen("Welcome to the game!\nPlease write your name: "), 0);

                }

                else if(pfds[i].fd == fd_server_udp) {
                    // receive redundunt msg
                    write(1, "we have come to this part that the udp is reiceved by myself\n", strlen("we have come to this part that the udp is reiceved by myself\n"));
                    bytes_received = recvfrom(fd_server_udp, buffer, sizeof(buffer) - 1, 0,
                                            (struct sockaddr *)&server_address, &server_address_len);
                    if(bytes_received > 0) {
                        buffer[bytes_received] = '\0';  
                    } 
                    write(1, buffer, strlen(buffer));
                    memset(buffer, 0, sizeof(buffer));
                }

                else if(client_server_fds.find(pfds[i].fd) != client_server_fds.end()) {  // recieve the name of client and send client the list of rooms
                    
                    // The name of the client is recieved
                    bytes_received = recv(pfds[i].fd, buffer, sizeof(buffer) - 1, 0);
                    if (bytes_received > 0) {
                        buffer[bytes_received] = '\0'; 
                    }

                    // set the name of the client in its object for later identifying
                    client_obj = getClientByFdServer(pfds[i].fd, client_objs);
                    client_obj->set_name(buffer);

                    write(1, buffer, strlen(buffer)); 
                    write(1, " is added to the game.\n", strlen(" is added to the game.\n"));
                    write(1, "\n", strlen("\n\n"));


                    // send the ports available to the client
                    char* msg = getAvailablePortsString(findEmptySubServers(subServers));
                    send(pfds[i].fd, msg, strlen(msg), 0);
                }
                
                else if(subserver_fds.find(pfds[i].fd) != subserver_fds.end()) {  // new player wants to get added to a room    ////////////////////

                    // accept the icnoming client in room with condition
                    fd_new_player_tcp = acceptClient_tcp(pfds[i].fd);  // correct

                    // find the Room object the player want to go play in
                    subServer_obj = findSubServerByFdTcp(pfds[i].fd, subServers);  // correct

                    // check if the room is full or not
                    if (subServer_obj->is_full()) {

                        // invalid room
                        send(pfds[i].fd, INVALID_ROOM, strlen(INVALID_ROOM), 0);    ////////////////////////// bayad ye kari koni
                        close(fd_new_player_tcp);
                    }

                    // room is not full
                    else {
                        // write(1, "not full\n", strlen("not full\n"));

                        send(fd_new_player_tcp, VALID_ROOM, strlen(VALID_ROOM), 0);

                        // fd_client_room was added to the room object
                        subServer_obj->add_player(fd_new_player_tcp);  // correct

                        // add fd_new_player to the temprory queue until it send its name so we could identify it correctly
                        queue_temprory.insert(fd_new_player_tcp);

                        pfds.push_back(pollfd{fd_new_player_tcp, POLLIN, 0});
                    }

                }

                else if(queue_temprory.find(pfds[i].fd) != queue_temprory.end()) {  // The name will be recieved so that the client object structure gets completed

                    // The name of the client is recieved
                    bytes_received = recv(pfds[i].fd, buffer, sizeof(buffer) - 1, 0);
                    if (bytes_received > 0) {
                        buffer[bytes_received] = '\0'; 
                    }


                    client_obj = getClientByName(std::string(buffer), client_objs);
                    client_obj->set_fd_room(pfds[i].fd);
                    client_obj->set_state(ROOM_CHECK);

                    queue_temprory.erase(pfds[i].fd);
                    client_room_fds.insert(pfds[i].fd);


                    // check if the room is now full send actions to both of them
                    if (subServer_obj->is_full()) {
                        // write(1, "queue_temprory\n", strlen("queue_temprory\n"));
                        subServer_obj->send_players_actions();
                    }
                }

                else if(client_room_fds.find(pfds[i].fd) != client_room_fds.end()) {  // The player has chosen a move
                    subServer_obj = findSubServerByFdPlayer(pfds[i].fd, subServers);

                    bytes_received = recv(pfds[i].fd, buffer, sizeof(buffer) - 1, 0);
                    if (bytes_received > 0) {
                        buffer[bytes_received] = '\0';
                    }

                    subServer_obj->add_action((string)buffer, pfds[i].fd);

                    // when both the players made their action
                    if(subServer_obj->did_all_players_make_action()) {
                        write(1, "doneYO\n", strlen("doneYO\n"));
                        int result = subServer_obj->check_match_result();
                        
                        subServer_obj->send_end_match_msg(result, client_objs);///////////////////////////////

                        subServer_obj->clear_room();
                    }
                }

                else if(pfds[i].fd == 0) {  // for stdin(0) terminal input

                    bytes_received =  read(0, buffer, sizeof(buffer) - 1);
                    if (bytes_received > 0) {
                        buffer[bytes_received] = '\0'; 
                        if (buffer[bytes_received - 1] == '\n') {
                            buffer[bytes_received - 1] = '\0';
                        }
                    }       

                    if (string(buffer) == END_GAME) {
                        endEntireGameMessage(fd_server_udp, client_objs, buffer);
                        close_all_fds(pfds);
                        exit(EXIT_SUCCESS);
                    }      
                    else {
                        continue;
                    }   
                }

                else {

                    // not usual
                    cout << pfds[i].fd << endl;
                    write(2, "something unusual has happend in {else} while true Server\n", strlen("something unusual has happend in else while true Server\n"));
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    return 0;

}
