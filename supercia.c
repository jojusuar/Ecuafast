#include "common.h"
#include <pthread.h>
#include "boat.h"

void *workerThread(void *arg);

int main(int argc, char* argv[]){
    int listenfd;
    unsigned int clientlen;
    struct sockaddr_in clientaddr;
    char *port = "8082";
    listenfd = open_listenfd(port);

    if (listenfd < 0){
		connection_error(listenfd);
    }
    printf("Server listening on port %s.\n", port);
    pthread_t tid;
    while(true){
        clientlen = sizeof(clientaddr);
        int *connfd = (int *)malloc(sizeof(int));
		*connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, workerThread, (void *)connfd);
    }
    return 0;
}

void *workerThread(void *arg){
    int connfd_ptr = *(int *)arg;
    free(arg);
}