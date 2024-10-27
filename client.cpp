#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <sys/time.h>

#define MAX_BUFFER_SIZE 1024

using namespace std;

int connectToServer(int port, char* ip) {
    int fd;
    struct sockaddr_in server_address;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        write(2, "couldnt create socket for client\n", strlen("couldnt create socket for client\n"));
        exit(EXIT_FAILURE);
    }
    
    server_address.sin_family = AF_INET; 
    server_address.sin_port = htons(port); 
    server_address.sin_addr.s_addr = inet_addr(ip);

    if (connect(fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) { 
        write(2, "Error in connecting to server\n", strlen("Error in connecting to server\n"));
        close(fd); 
        return -1; 
    }

    return fd; // Successful connection
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        write(2, "Args not valid\n", strlen("Args not valid\n"));
        return -1;
    }

    char* ip = argv[1];
    int port = atoi(argv[2]);

    int new_socke,t, fd_client;
    char buffer[MAX_BUFFER_SIZE];

    fd_client = connectToServer(port, ip);

    while (1) {
        read(0, buffer, 1024);
        send(fd_client, buffer, strlen(buffer), 0);
        memset(buffer, 0, MAX_BUFFER_SIZE);
    }
    
}