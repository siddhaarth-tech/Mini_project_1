#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <time.h>

#define PORT "8080"
#define BUF_SIZE 1024
#define BACKLOG 10
// GLOBAL message counter
static int global_msg_count = 0;

/*
 * Create and return a listening socket
 */
int get_listener_socket(void)
{
    int listener, yes = 1, rv;
    struct addrinfo hints, *ai, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "%s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) continue;

        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }
        break;
    }

    freeaddrinfo(ai);

    if (p == NULL) return -1;
    if (listen(listener,BACKLOG) == -1) return -1;

    return listener;
}

/*
 * Add a new fd to poll list
 */
void add_to_pfds(struct pollfd **pfds, int newfd,
                 int *fd_count, int *fd_size)
{
    if (*fd_count == *fd_size) {
        *fd_size *= 2;
        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN;
    (*pfds)[*fd_count].revents = 0;
    (*fd_count)++;
}

/*
 * Remove fd from poll list (O(1))
 */
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    pfds[i] = pfds[*fd_count - 1];
    (*fd_count)--;
}

/*
 * Handle client data: ECHO + TIME + GLOBAL COUNT
 */
void handle_client_data(struct pollfd *pfds, int *fd_count, int *i)
{
    char buf[BUF_SIZE];
    char reply[BUF_SIZE * 2];
    time_t now;

    int nbytes = recv(pfds[*i].fd, buf, sizeof buf - 1, 0);

    if (nbytes <= 0) {
        // Client disconnected or error
        close(pfds[*i].fd);
        del_from_pfds(pfds, *i, fd_count);
        (*i)--;   // re-check swapped fd
        return;
    }

    buf[nbytes] = '\0';

    // Increment GLOBAL counter
    global_msg_count++;

    time(&now);

    snprintf(reply, sizeof reply,
             "Echo: %s"
             "Time: %s"
             "Total echo messages (global): %d\n",
             buf, ctime(&now), global_msg_count);

    send(pfds[*i].fd, reply, strlen(reply), 0);
}


int main(void)
{
    int listener;
    int fd_count = 0;
    int fd_size = 5;

    struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

    listener = get_listener_socket();
    if (listener == -1) {
        fprintf(stderr, "error getting listener socket\n");
        exit(1);
    }

    // Add listener to poll list
    pfds[0].fd = listener;
    pfds[0].events = POLLIN;
    fd_count = 1;

    printf("Poll echo server running on port %s\n", PORT);

    while (1) {
        poll(pfds, fd_count, -1);

        for (int i = 0; i < fd_count; i++) {
            if (pfds[i].revents & (POLLIN | POLLHUP)) {
                if (pfds[i].fd == listener) {
                    // New client
                    int newfd = accept(listener, NULL, NULL);
                    add_to_pfds(&pfds, newfd, &fd_count, &fd_size);
                    printf("New client connected (fd=%d)\n", newfd);
                } else {
                    // Existing client
                    handle_client_data(pfds, &fd_count, &i);
                }
            }
        }
    }

    free(pfds);
    close(listener);
    return 0;
}
