#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

#define PORT "8080"
#define BACKLOG 10
#define BUFFER_SIZE 1024

/* global message counter */
int message_count = 0;

/* mutex to protect shared counter */
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

/*SERVER SOCKET SETUP */
int setup_server_socket(void)
{
    struct addrinfo hints, *servinfo, *p;
    int sockfd, rv;
    int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;      // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags = AI_PASSIVE;      // use local IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {

        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
            continue;

        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(servinfo);

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    return sockfd;
}

/*CLIENT HANDLER*/
void *handle_client(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];

    while (1) {
        int bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0)
            break;

        buffer[bytes] = '\0';

        /* server timestamp */
        time_t now = time(NULL);
        char *timestamp = ctime(&now);
        timestamp[strlen(timestamp) - 1] = '\0';

        /* update global counter safely */
        pthread_mutex_lock(&count_mutex);
        message_count++;
        int current_count = message_count;
        pthread_mutex_unlock(&count_mutex);

        /* build response (FIXED) */
        char response[BUFFER_SIZE];
        snprintf(response, BUFFER_SIZE,
                 "Echo: %.400s | Time: %s | Total messages: %d\n",
                 buffer, timestamp, current_count);

        send(client_fd, response, strlen(response), 0);
    }

    close(client_fd);
    return NULL;
}

/* MAIN */
int main(void)
{
    int server_fd = setup_server_socket();
    printf("Server listening on port %s\n", PORT);

    while (1) {
        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof client_addr;

        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &addr_size);
        if (client_fd == -1)
            continue;

        pthread_t tid;
        int *pclient = malloc(sizeof(int));
        *pclient = client_fd;

        pthread_create(&tid, NULL, handle_client, pclient);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
