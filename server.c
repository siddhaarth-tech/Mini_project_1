#include <stdio.h>      // Provides standard IO functions like printf(), scanf()
#include <stdlib.h>     // Contains functions such as malloc(), free(), atoi()
#include <string.h>     // Used for string handling functions like strcpy(), strcmp(), strlen(),
#include <unistd.h>     // Provides POSIX operating system APIs like read(), write(), close(), fork(), sleep()
#include <pthread.h>    // Supports multithreading using POSIX threads (pthread_create, pthread_join, mutexes)
#include <sys/types.h>  // Defines data types used in system calls
#include <sys/socket.h> // Provides socket programming functions like socket(), bind(), listen(), accept(), send(), recv()
#include <netdb.h>      // Used for network database operations such as getaddrinfo()
#include <time.h>       // Provides date and time functions like time(), localtime()


// Port number used by getaddrinfo for server/client communication 
#define PORT "8080"
// Maximum buffer size for sending and receiving data over the socket 
#define BUFFER_SIZE 1024
// Number of successfull connections waiting in the queue to get accept by server limit
#define BACKLOG 10 

/* Global counter to track total number of messages handled by the server */
int message_count = 0;

/* Mutex used to protect the shared message counter from race conditions */
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

//This function is used to setup the socket information of the server to connect with clients 
int setup_server_socket(void)
{
    struct addrinfo hints, *servinfo, *p;
    int sockfd, rv;
    int yes = 1;

    // Clear the hints structure to avoid garbage values 
    memset(&hints, 0, sizeof hints);

    /*
       Force IPv6 socket creation.
       This allows us to create a dual-stack server that can
       accept both IPv6 and IPv4 clients.
    */
    hints.ai_family = AF_INET6;

    // Use TCP (stream socket)
    hints.ai_socktype = SOCK_STREAM;

    /*
       AI_PASSIVE:
       NULL node means bind to all local interfaces (::)
    */
    hints.ai_flags = AI_PASSIVE;

    /*
       getaddrinfo():
       node    - NULL means bind to local host
       PORT    - port number as a string
       hints   - criteria for selecting socket addresses
       servinfo- linked list of possible local addresses returned by the OS
    */
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    /*
       Loop through returned addresses and attempt
       to create and bind a socket.
    */
        for (p = servinfo; p != NULL; p = p->ai_next) {

        /*
           socket():
           p->ai_family   - address family (IPv4 or IPv6)
           p->ai_socktype - socket type (TCP)
           p->ai_protocol - protocol selected by getaddrinfo
        */
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
            continue;

        /*
           setsockopt():
           SO_REUSEADDR allows the server to reuse the port immediately
           after the program restarts.
        */
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        /*
           Disable IPV6_V6ONLY so that this IPv6 socket
           also accepts IPv4 connections as IPv4-mapped IPv6 addresses
           This is the KEY setting that allows:
           - IPv4-only clients
           - IPv6-only clients
           to connect to the same server.
        */
        int no = 0;
        setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));

        /*
           bind():
           sockfd         - socket descriptor
           p->ai_addr     - local address to bind
           p->ai_addrlen  - size of the address structure
        */
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        // Successful bind
        break;
    }

    // If no address was successfully bound, exit with error
    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(2);
    }

    // Free memory allocated by getaddrinfo 
    freeaddrinfo(servinfo);

    /*
       listen():
       sockfd   - bound socket descriptor
       BACKLOG  - maximum number of pending client connections
    */
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    // Return listening socket descriptor 
    return sockfd;
}

// Handles each connected client using a separate thread and mutex
// to avoid race conditions while accessing shared data
// Void function and parameters are used because threads are allowed to accept any type of pointers
void *handle_client(void *arg)
{
    // Retrieve client socket descriptor passed from main thread
    int client_fd = *(int *)arg;

    // Free dynamically allocated memory used to pass the socket
    free(arg);

    // Buffer to store messages received from the client
    char buffer[BUFFER_SIZE];

    while (1) {

        /*
           recv():
           client_fd        - socket descriptor for the connected client
           buffer           - buffer to store received data
           BUFFER_SIZE - 1  - maximum number of bytes to receive
           flags (0)        - no special options
        */
        int bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

        // If client closes the connection or an error occurs
        if (bytes <= 0)
            break;

        // Null-terminate the received data
        buffer[bytes] = '\0';

        // Get current server time
        time_t now = time(NULL);
        char *timestamp = ctime(&now);

        // Remove newline character added by ctime
        timestamp[strlen(timestamp) - 1] = '\0';

        /*
           Protect shared message counter using mutex to
           prevent race conditions between multiple threads
        */
        pthread_mutex_lock(&count_mutex);
        message_count++;
        int current_count = message_count;
        pthread_mutex_unlock(&count_mutex);

        // Buffer to store formatted server response
        char response[BUFFER_SIZE];

        /*
           snprintf():
           response       - destination buffer
           BUFFER_SIZE    - maximum size of the buffer
           %.400s         - limits client message length to prevent overflow
        */
       //Used the function to format the string to send it to the client
        snprintf(response, BUFFER_SIZE,
                 "Echo: %.400s | Time: %s | Total messages: %d\n",
                 buffer, timestamp, current_count);

        /*
           send():
           client_fd       - socket descriptor
           response        - data to send to the client
           strlen(response)- number of bytes to send
           flags (0)       - no special options
        */
        send(client_fd, response, strlen(response), 0);
    }

    // Close client socket when communication ends
    close(client_fd);

    // Terminate the client handler thread
    return NULL;
}


int main()
{
    // Create, bind, and start listening on the server socket
    int server_fd = setup_server_socket();
    printf("Server listening on port %s\n", PORT);

    while (1) {

        // Structure to store client address information
        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof client_addr;

        /*
           accept():
           server_fd       -listening socket descriptor
           client_addr     - stores client address information
           addr_size       - size of the address structure
        */
        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &addr_size);

        // If accept fails, continue to next iteration
        if (client_fd == -1)
            continue;

        // Thread identifier for the client handler
        pthread_t tid;

        // Dynamically allocate memory to pass client socket to thread 
        // reusing same address resukt in accessing same client so we provide new memory each time for new clients
        int *pclient = malloc(sizeof(int));
        *pclient = client_fd;

        /*
           pthread_create():
           tid          - thread identifier
           NULL         - default thread attributes
           handle_client- function executed by the new thread
           pclient      - argument passed to the thread function
        */
        pthread_create(&tid, NULL, handle_client, pclient);

        // Detach thread so its resources are released automatically
        pthread_detach(tid);
    }

    // Close server socket (normally unreachable)
    close(server_fd);
    return 0;
}
