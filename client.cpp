#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <sys/time.h>
#include <vector>
#include <poll.h>

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
#define GET_LIST_EMPTY_ROOMS 2
#define IN_ROOM 3 
#define END_GAME 4
#define WAIT_FOR_RESPONSE_VALID_ROOM 5
#define SEE_RESULTS 6

using namespace std;

int state = START;

char* getBroadcastAddress() {
        struct ifaddrs *ifap, *ifa;
        struct sockaddr_in *sa;

        if (getifaddrs(&ifap) == -1) {
            perror("getifaddrs");
            return nullptr;
        }

        char* broadcastIp = nullptr;

        for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                sa = (struct sockaddr_in *) ifa->ifa_broadaddr;
                broadcastIp = strdup(inet_ntoa(sa->sin_addr)); // Duplicate IP as char*
                break;
            }
        }

        freeifaddrs(ifap);
        return broadcastIp; // Caller is responsible for freeing this memory
    }

int connectToServer_tcp(int port, char* ip, string mode) {
    int fd;
    struct sockaddr_in server_address;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        write(2, "couldn't create socket for client\n", strlen("couldn't create socket for client\n"));
        exit(EXIT_FAILURE);
    }
    
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &server_address.sin_addr) <= 0) {
        write(2, "Invalid address or address not supported\n", strlen("Invalid address or address not supported\n"));
        exit(EXIT_FAILURE);
    }

    if (connect(fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) { 
        write(2, "Error in connecting to server\n", strlen("Error in connecting to server\n"));
        close(fd); 
        return -1; 
    }

    char msg_success[MAX_BUFFER_SIZE];
    snprintf(msg_success, sizeof(msg_success), "TCP fd setup at %s : %d with fd_server: %d\nConnection stablished with %s\n", ip, port, fd, mode.c_str());
    write(1, msg_success, strlen(msg_success)); 

    return fd; // Successful connection
}

int setupServer_udp(string mode) {
    int server_fd;
    struct sockaddr_in address;

    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        write(2, "client Socket creation failed\n", strlen("client Socket creation failed\n"));
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
    int port;
    port = 8080;

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
        write(2, "Invalid address or address not supported\n", strlen("Invalid address or address not supported\n"));
        exit(EXIT_FAILURE);
    }

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        write(2, "UDP Bind failed in client side\n", strlen("UDP Bind failed in client side\n"));
        close(server_fd);
        return -1;
    }

    char msg_success[MAX_BUFFER_SIZE];
    snprintf(msg_success, sizeof(msg_success), "client UDP fd set up at %s : %d\n\n", ip, port);
    write(1, msg_success, strlen(msg_success)); 

    return server_fd;  // successful connection
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        write(2, "Args not valid\n", strlen("Args not valid\n"));
        return -1;
    }

    char* ip = argv[1];
    int port = atoi(argv[2]);

    int fd_server_tcp, fd_server_udp, fd_room_tcp;
    char buffer[MAX_BUFFER_SIZE] = {0};
    char buffer_name[MAX_BUFFER_SIZE] = {0};
    ssize_t bytes_received;
    int port_room;
    struct sockaddr_in server_address;  // for udp connection
    socklen_t server_address_len = sizeof(server_address);  // for udp connection
    int wons, losts;

    memset(buffer, 0, MAX_BUFFER_SIZE);

    fd_server_tcp = connectToServer_tcp(port, ip, SERVER);
    fd_server_udp = setupServer_udp(SERVER);

    vector<pollfd> pfds;
    pfds.push_back(pollfd{fd_server_tcp, POLLIN, 0});
    pfds.push_back(pollfd{fd_server_udp, POLLIN, 0});

    /*
    Room id is the PortNum of the room 
    */
     
    while (true) {
        
        // a system call that waits until some event accurs on a fd in pfds
        if(poll(pfds.data(), (nfds_t)(pfds.size()), -1) == -1)
            write(2, "FAILED: Poll in client\n", strlen("FAILED: Poll in client\n"));

        for(int i = 0; i < pfds.size(); i++) {
            if(pfds[i].revents & POLLIN) {

                if(pfds[i].fd == fd_server_tcp) {
                    switch (state) {
                    case START:

                        // get wellcome message
                        bytes_received = recv(fd_server_tcp, buffer, sizeof(buffer) - 1, 0);
                        if (bytes_received > 0) {
                            buffer[bytes_received] = '\0';
                        }

                        // write server message in terminal
                        write(1, buffer, bytes_received); 
                        write(1, "\n\n", strlen("\n\n"));
                        
                        // get name from user via terminal
                        bytes_received =  read(0, buffer_name, sizeof(buffer_name) - 1);
                        if (bytes_received > 0) {
                            buffer_name[bytes_received] = '\0'; 
                            if (buffer_name[bytes_received - 1] == '\n') {
                                buffer_name[bytes_received - 1] = '\0';
                            }
                        }

                        // send name to server
                        send(fd_server_tcp, buffer_name, bytes_received, 0);

                        // removing the \n of the nameBuffer
                        
                        state = GET_LIST_EMPTY_ROOMS;
                        break;

                    case GET_LIST_EMPTY_ROOMS:

                        // get the empty rooms in a string
                        bytes_received = recv(fd_server_tcp, buffer, sizeof(buffer) - 1, 0);
                        if (bytes_received > 0) {
                            buffer[bytes_received] = '\0';
                        }

                        // write the empty rooms in the terminal
                        write(1, "Which room you want to chose??\n", strlen("Which room you want to chose??\n"));
                        write(1, buffer, bytes_received); 
                        write(1, "\n", 1);

                        // get room port from user via terminal 
                        bytes_received =  read(0, buffer, sizeof(buffer) - 1);
                        if (bytes_received > 0) {
                            buffer[bytes_received] = '\0'; 
                        }
                        port_room = atoi(buffer);
                        fd_room_tcp = connectToServer_tcp(port_room, ip, ROOM);

                        pfds.push_back(pollfd{fd_room_tcp, POLLIN, 0});

                        state = WAIT_FOR_RESPONSE_VALID_ROOM;

                        break;
                    }
                }
                    
                else if(pfds[i].fd == fd_room_tcp) { 
                    switch (state) {
                    case WAIT_FOR_RESPONSE_VALID_ROOM:

                        // get the status of if the room was valid or not
                        bytes_received =  recv(fd_room_tcp, buffer, sizeof(buffer) - 1, 0);
                        if (bytes_received > 0) {
                            buffer[bytes_received] = '\0'; 
                        }

                        // write(1, "omad\n", strlen("omad\n"));
                      

                        // The room was invalid so we close the connection
                        if (std::string(buffer) == INVALID_ROOM) {/////////////////////////////////////bayad ye kari koni
                            // write(1, "invalid room omad\n", strlen("invalid room omad\n"));
                            pfds.pop_back();
                            pfds.pop_back();
                            close(fd_room_tcp);
                            state = GET_LIST_EMPTY_ROOMS;
                        }

                        // We enter the game in the room
                        else if(std::string(buffer) == VALID_ROOM){
                            // write(1, "valid room omad\n", strlen("valid room omad\n"));

                            // send the name of the client so the configuraton in the client side gets handled (user wont notice anything)
                            send(fd_room_tcp, buffer_name, strlen(buffer_name), 0);
                            state = IN_ROOM;
                        }
                        else {
                            write(2, buffer, strlen(buffer));
                            write(2, "\n", 1);
                            write(2, "valid or invalid that was sent from server wasnt the correct define format\n", strlen("valid or invalid that was sent from server wasnt the correct define format\n"));
                            exit(EXIT_FAILURE);
                        }

                        state = IN_ROOM;

                        break;

                    case IN_ROOM:

                        // get menu message (rock, paper, scissors)
                        bytes_received = recv(fd_room_tcp, buffer, sizeof(buffer) - 1, 0);
                        if (bytes_received > 0) {
                            buffer[bytes_received] = '\0';
                        }

                        // write server message in terminal
                        write(1, buffer, bytes_received); 
                        
                        // get action from user via terminal
                        while (true) { // We are here until the user writes a correct action
                        
                            // user action recieved
                            bytes_received =  read(0, buffer, sizeof(buffer) - 1);
                            if (bytes_received > 0) {
                                buffer[bytes_received] = '\0'; 
                                if (buffer[bytes_received - 1] == '\n') {
                                    buffer[bytes_received - 1] = '\0';
                                }

                            }
                            if (string(buffer) == ROCK || string(buffer) == PAPER || string(buffer) == SCISSORS) {
                                break;
                            }
                            write(1, "invalid choise, pick again!!\n", strlen("invalid choise, pick again!!\n"));
                        }

                        // send action to server
                        send(fd_room_tcp, buffer, bytes_received, 0);
                        
                        state = SEE_RESULTS;
                        break;

                    default:
                        break;
                    }
                }

                else if(pfds[i].fd == fd_server_udp) {  // Anouncing the Results which we got from the server

                    write(1, "it came to client udp side to recvfrom()\n", strlen("it came to client upd side to recvfrom()\n"));

                    // receive msg
                    bytes_received = recvfrom(fd_server_udp, buffer, sizeof(buffer) - 1, 0,
                                            (struct sockaddr *)&server_address, &server_address_len);
                    if(bytes_received > 0) {
                        buffer[bytes_received] = '\0';  
                        write(1, "\n", 1);
                    } 

                    if (buffer[0] == 'e' && buffer[1] == 'n' && buffer[2] == 'd') {  // The end_game (entire game has ended)

                        write(1, "We were glad to have you! End of tournament: \n\n", strlen("We were glad to have you! End of tournament: \n"));
                        write(1, buffer, bytes_received); 

                        close(fd_room_tcp);
                        close(fd_server_tcp);
                        close(fd_server_udp);
                    }

                    else {  // The single hand match has ended

                        write(1, "End of Game in Room: \n", strlen("End of Game in Room: \n"));
                        write(1, buffer, bytes_received); 
                        write(1, "\n", 1);

                        close(fd_room_tcp);

                        state = GET_LIST_EMPTY_ROOMS;
                    }

      
                }

                else {
                    cout << pfds[i].fd << endl;
                    write(2, "ERROR: what just happend?!!\n", strlen("ERROR: what just happend?!!\n"));
                }
            }
        }
    }
    


    close(fd_server_tcp);
    return 0;
}