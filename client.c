#include <stdio.h>      // Provides standard IO functions like printf(), scanf()
#include <stdlib.h>     // Contains functions such as malloc(), free(), atoi()
#include <string.h>     // Used for string handling functions like strcpy(), strcmp(), strlen(),
#include <unistd.h>     // Provides POSIX operating system APIs like read(), write(), close(), fork(), sleep()
#include <sys/types.h>  // Defines data types used in system calls
#include <sys/socket.h> // Provides socket programming functions like socket(), bind(), listen(), accept(), send(), recv()
#include <netdb.h>      // Used for network database operations such as getaddrinfo()

/* Port number used by getaddrinfo for server/client communication */
#define PORT "8080"
/* Maximum buffer size for sending and receiving data over the socket */
#define BUFFER_SIZE 1024


//This function will connect the client to the server by setting up the info and then creating the socket and connecting it to the server by the server addrinfo
int connect_to_server(char *hostname)
{
    struct addrinfo hints, *servinfo, *p;
    int sockfd, rv;
    /* 
       Clear the hints structure so that all fields start with
       known zero values before setting required options.
    */
    memset(&hints, 0, sizeof hints);

    /* Allow either IPv4 or IPv6 addresses */
    hints.ai_family = AF_UNSPEC;

    /* Use TCP (stream socket); UDP would use SOCK_DGRAM */
    hints.ai_socktype = SOCK_STREAM;

    /*
       getaddrinfo():
       hostname - server IP address or hostname ("127.0.0.1" or "::1")
       PORT     - service/port number as a string
       hints    - criteria specified for selecting socket addresses
       servinfo - linked list of possible socket addresses returned by the OS
    */
    if ((rv = getaddrinfo(hostname, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    /*
       Loop through each address returned by getaddrinfo and try
       to create a socket and connect until one succeeds.
    */
    for (p = servinfo; p != NULL; p = p->ai_next) {

        /*
           socket():
           p->ai_family   - address family (IPv4 or IPv6)
           p->ai_socktype - socket type (TCP)
           p->ai_protocol - protocol number chosen by getaddrinfo
        */
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
            continue;

        /*
           connect():
           sockfd         → socket file descriptor
           p->ai_addr     → server address structure
           p->ai_addrlen  → size of the address structure
        */
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        /* Successful connection */
        break;
    }

    /* If no address worked, connection failed */
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(2);
    }

    /* Free memory allocated by getaddrinfo */
    freeaddrinfo(servinfo);

    /* Return connected socket descriptor */
    return sockfd;
}

// Main function where the code starts to execute 
int main(int argc, char *argv[])
{
    /* Check if server IP address is provided as a command-line argument */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* 
       connect_to_server():
       argv[1] → IP address or hostname of the server to connect to
       Returns a connected socket file descriptor
    */
    int sockfd = connect_to_server(argv[1]);
    printf("Connection successful\n");

    /* Buffer to store user input message */
    char message[BUFFER_SIZE];

    /* Buffer to store server response */
    char buffer[BUFFER_SIZE];

    while (1) {

        printf("Enter message (type 'exit' to quit): ");

        //Reads a line of input and Stores at most BUFFER_SIZE characters including newline
        fgets(message, BUFFER_SIZE, stdin);

        // Remove trailing newline character added by fgets 
        message[strcspn(message, "\n")] = '\0';
        
        // Exit loop if user types "exit" 
        if (strcmp(message, "exit") == 0)
            break;
        /*
           send():
           sockfd        -connected socket descriptor
           message       - data to be sent to the server
           strlen(message) - length of the data to send
           flags (0)     - no special options
        */
        send(sockfd, message, strlen(message), 0);

        // Receive complete server response 
        while (1) {

            /*
               recv():
               sockfd            - connected socket descriptor
               buffer            - buffer to store received data
               BUFFER_SIZE - 1   - maximum bytes to receive
               flags (0)         - no special options
            */
            int bytes = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
            //If recv returns 0 or negative, server has disconnected 
            if (bytes <= 0) {
                printf("Server disconnected\n");
                close(sockfd);
                return 0;
            }
            //Null-terminate received data 
            buffer[bytes] = '\0';
            // Print server response 
            printf("%s", buffer);
            // Stop reading once newline indicates end of response 
            if (strchr(buffer, '\n') != NULL)
                break;
        }
    }
    // Close the socket before exiting the client 
    close(sockfd);
    return 0;
}
