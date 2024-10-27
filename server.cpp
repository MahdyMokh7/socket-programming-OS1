#include <iostream>  
#include <cstdlib>  
#include <cstring>
#include <unistd.h> 
#include <fcntl.h>  
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <sys/time.h> 
#include <vector>



#define MAX_BUFFER_SIZE 1024
#define MAX_LISTEN 20

using namespace std;

#include <iostream>
#include <unistd.h> // for close()

#include <iostream>
#include <unistd.h> // for close and write
#include <cstring>  // for strlen
#include <cstdio>   // for snprintf

class SubServer {
private:
    static int instanceCount;
    int fd; 
    int id_room;
    int num_of_players_in;
    const int MAX_PLAYERS = 2;

public:
    SubServer(int fileDescriptor);
    ~SubServer();
    bool is_full();
    void add_player();

    static int getInstanceCount() { return instanceCount; }
};

int SubServer::instanceCount = 0;

SubServer::SubServer(int fileDescriptor) : fd(fileDescriptor) {
    instanceCount++;
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "SubServer created with fd: %d\n", fd);
    write(1, buffer, strlen(buffer)); 
}

SubServer::~SubServer() {
    if (fd >= 0) {
        instanceCount--;
        close(fd);
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "SubServer with fd: %d closed.\n", fd);
        write(1, buffer, strlen(buffer)); 
    }
}

bool SubServer::is_full() {
    return this->num_of_players_in >= MAX_PLAYERS ? true : false;
}


void error_handler_setup(const char* error_msg, int fd) {
    write(2, error_msg, strlen(error_msg));
    close(fd);
    exit(EXIT_FAILURE);
}


int setupServer(int port, char* ip) {
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

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(ip);


    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {  // binding
        error_msg = "setsockopt didnt work\n";
        error_handler_setup(error_msg, server_fd);
    }

    if (listen(server_fd, MAX_LISTEN) < 0) {  // listening
        error_msg = "setsockopt didnt work\n";
        error_handler_setup(error_msg, server_fd);
    }

    return server_fd;
}

void setupSubServers(SubServer* subServers[], int num_of_rooms, int base_port, char* ip) {
    for (int i = 0; i < num_of_rooms; i++) {
        int room_port = base_port + i + 1; // Assign port to each room

        int fd = setupServer(room_port, ip);  // Set up each subserver
        

        char buffer[100];
        snprintf(buffer, sizeof(buffer), "Subserver for room %d is running on port %d\n", i + 1, room_port);
        write(1, buffer, strlen(buffer));  
    }
}

int acceptClient(int server_fd) {
    int client_fd;
    struct sockaddr_in client_address;
    socklen_t address_len = sizeof(client_address);

    // Clear the client_address structure
    memset(&client_address, 0, sizeof(client_address));

    client_fd = accept(server_fd, (struct sockaddr *)&client_address, &address_len);
    if (client_fd < 0) {
        write(2, "Counldnt accept client connection\n", strlen("Counldnt accept client connection\n"));
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
    int port = atoi(argv[2]);
    char* char_num_of_rooms = argv[3];
    int num_of_rooms = atoi(char_num_of_rooms);

    int fd_server, max_sd, new_socket;
    char buffer[MAX_BUFFER_SIZE] = {0};

    fd_server = setupServer(port, ip);  // Main Server Setup
    write(1, "Main Server has been setup.\n", strlen("Main Server has been setup.\n"));

    // int* room_fds = new int[num_of_rooms];
    // int* room_ports = new int[num_of_rooms];
    vector<SubServer> subServers;   

    setupSubServers(subServers, num_of_rooms, port, ip);  // SubServers setup

    fd_set master_set, working_set;
    FD_ZERO(&master_set);
    max_sd = fd_server;
    FD_SET(fd_server, &master_set);
    char buf[MAX_BUFFER_SIZE];  //////////
    snprintf(buf, sizeof(buf), "%s%d\n", "fd_server:  ", fd_server);  ////////////
    write(1, buf, strlen(buf));  /////////////

    write(1, "Server is running\n\n", 18);

    while (true) {
        working_set = master_set;
        select(max_sd + 1, &working_set, NULL, NULL, NULL);

        for (int i = 0; i <= max_sd; i++) {
            if (FD_ISSET(i, &working_set)) {
                
                if (i == fd_server) {  // new client
                    new_socket = acceptClient(fd_server);
                    FD_SET(new_socket, &master_set);
                    if (new_socket > max_sd)
                        max_sd = new_socket;
                    
                    std::string msg = "New client connected. fd = " + std::to_string(new_socket) + "\n";
                    write(1, msg.c_str(), msg.size());
                }
                
                else { // client sending msg
                    int bytes_received;
                    bytes_received = recv(i, buffer, 1024, 0);
                    
                    if (bytes_received == 0) { // EOF
                        std::string msg = "client fd = " + std::to_string(i) + " closed\n";
                        write(1, msg.c_str(), msg.size());
                        // close(i);
                        // FD_CLR(i, &master_set);
                        continue;
                    }

                    std::string client_msg = "client " + std::to_string(i) + ": " + std::string(buffer) + "\n";
                    write(1, client_msg.c_str(), client_msg.size());
                    memset(buffer, 0, 1024);
                }
            }
        }
    }

    close(fd_server);
    return 0;
}