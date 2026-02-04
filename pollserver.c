#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <time.h>

#define PORT "8080"          // Port number used by server
#define BUF_SIZE 1024        // Buffer size for send and receive
#define BACKLOG 10           // Max pending connections

// Global counter to track total messages echoed by server
static int global_msg_count = 0;

/*
  Create, bind, and return a listening socket
  Supports both IPv4 and IPv6 using a dual-stack IPv6 socket
 */
int get_listener_socket(void)
{
    int listener, yes = 1, rv;
    struct addrinfo hints, *ai, *p;

    // Clear hints structure
    memset(&hints, 0, sizeof hints);

    // Use IPv6 (can also accept IPv4 via mapping)
    hints.ai_family = AF_INET6;

    // TCP socket
    hints.ai_socktype = SOCK_STREAM;

    // Bind to local machine
    hints.ai_flags = AI_PASSIVE;

    /*
       getaddrinfo():
       node    -> NULL means local host
       PORT    -> port number as string
       hints   -> address selection criteria
       ai      -> list of address results
    */
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "%s\n", gai_strerror(rv));
        exit(1);
    }

    // Try each address until bind succeeds
    for (p = ai; p != NULL; p = p->ai_next) {

        /*
           socket():
           p->ai_family   -> address family
           p->ai_socktype -> socket type
           p->ai_protocol -> protocol
        */
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) continue;

        // Allow port reuse
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        // Disable IPv6-only mode to allow IPv4 connections
        int no = 0;
        setsockopt(listener, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(int));

        /*
           bind():
           listener      -> socket descriptor
           p->ai_addr    -> local address
           p->ai_addrlen -> address length
        */
        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        // Successfully bound
        break;
    }

    // Free address list
    freeaddrinfo(ai);

    // If no address worked
    if (p == NULL) return -1;

    /*
       listen():
       listener -> bound socket
       BACKLOG  -> max pending connections
    */
    if (listen(listener, BACKLOG) == -1) return -1;

    return listener;
}

/*
  Add a new file descriptor to the poll list
 */
void add_to_pfds(struct pollfd **pfds, int newfd,
                 int *fd_count, int *fd_size)
{
    // Resize poll array if full
    if (*fd_count == *fd_size) {
        *fd_size *= 2;
        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    // Add new fd to poll list
    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN;
    (*pfds)[*fd_count].revents = 0;
    (*fd_count)++;
}

/*
  Remove a file descriptor from poll list in O(1) time
 */
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    // Replace removed fd with last fd
    pfds[i] = pfds[*fd_count - 1];
    (*fd_count)--;
}

/*
  Handle client data:
  - receive message
  - echo message
  - add server time
  - add global message count
 */
void handle_client_data(struct pollfd *pfds, int *fd_count, int *i)
{
    char buf[BUF_SIZE];
    char reply[BUF_SIZE * 2];
    time_t now;

    /*
       recv():
       pfds[*i].fd -> client socket
       buf         -> receive buffer
       size        -> max bytes to read
    */
    int nbytes = recv(pfds[*i].fd, buf, sizeof buf - 1, 0);

    // Client disconnected or error
    if (nbytes <= 0) {
        close(pfds[*i].fd);
        del_from_pfds(pfds, *i, fd_count);
        (*i)--;   // adjust index after removal
        return;
    }

    // Null terminate received data
    buf[nbytes] = '\0';

    // Increment global message counter
    global_msg_count++;

    // Get current server time
    time(&now);

    /*
       Build response containing:
       - echoed message
       - server time
       - global message count
    */
    snprintf(reply, sizeof reply,
             "Echo: %s"
             "Time: %s"
             "Total echo messages (global): %d\n",
             buf, ctime(&now), global_msg_count);

    // Send response back to client
    send(pfds[*i].fd, reply, strlen(reply), 0);
}

int main(void)
{
    int listener;
    int fd_count = 0;     // number of active file descriptors
    int fd_size = 5;      // initial size of poll array

    // Allocate pollfd array
    struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

    // Create server listening socket
    listener = get_listener_socket();
    if (listener == -1) {
        fprintf(stderr, "error getting listener socket\n");
        exit(1);
    }

    // Add listener socket to poll list
    pfds[0].fd = listener;
    pfds[0].events = POLLIN;
    fd_count = 1;

    printf("Poll echo server running on port %s\n", PORT);

    while (1) {

        /*
           poll():
           pfds     -> list of file descriptors
           fd_count -> number of fds
           -1       -> wait indefinitely
        */
        poll(pfds, fd_count, -1);

        // Loop through active file descriptors
        for (int i = 0; i < fd_count; i++) {

            // Check if fd is ready for reading or closed
            if (pfds[i].revents & (POLLIN | POLLHUP)) {

                if (pfds[i].fd == listener) {
                    // Accept new client connection
                    int newfd = accept(listener, NULL, NULL);
                    add_to_pfds(&pfds, newfd, &fd_count, &fd_size);
                    printf("New client connected (fd=%d)\n", newfd);
                } else {
                    // Handle data from existing client
                    handle_client_data(pfds, &fd_count, &i);
                }

                // Clear event flags
                pfds[i].revents = 0;
            }
        }
    }

    // Cleanup (normally not reached)
    free(pfds);
    close(listener);
    return 0;
}
