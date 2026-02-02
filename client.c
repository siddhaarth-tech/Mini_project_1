#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define PORT "8080"
#define BUFFER_SIZE 1024

//This function will connect the client to the server by setting up the info and then creating the socket and connecting it to the server by the server addrinfo
int connect_to_server(char *hostname)
{
    struct addrinfo hints, *servinfo, *p;
    int sockfd, rv;

    memset(&hints, 0, sizeof hints);  //clears the data that is already present in that memory to handle new clients
    hints.ai_family = AF_UNSPEC;      // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP also DGRAM for UDP which is less reliable than TCP  

    if ((rv = getaddrinfo(hostname, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {

        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
            continue;

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(2);
    }

    freeaddrinfo(servinfo);
    return sockfd;
}

// Main function where the code starts to execute 
int main(void)
{
    int sockfd = connect_to_server("127.0.0.1");//here we have sent the IP address of the localhost to connect

    char message[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    while (1) {
        printf("Enter message (type 'exit' to quit): ");
        fgets(message, BUFFER_SIZE, stdin);

        message[strcspn(message, "\n")] = '\0'; //replaces the '\n' with '\0'

        //breaks the connection while the client says "exit"
        if (strcmp(message, "exit") == 0)
            break;

        send(sockfd, message, strlen(message), 0);//message from the client is sent to the server after connection established

        // receive full server response
        while (1) {
            int bytes = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
            if (bytes <= 0) {
                printf("Server disconnected\n");
                close(sockfd);//we need to close the fd as the client doesn't get any response
                return 0;
            }

            buffer[bytes] = '\0';//adds the '\0' at the end
            printf("%s", buffer);

            if (strchr(buffer, '\n') != NULL)
                break;
        }
    }

    close(sockfd);
    return 0;
}
