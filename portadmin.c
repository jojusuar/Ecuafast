#include "boat.h"
#include "common.h"
#include "list.h"
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_STR_SIZE 8192
#define MAX_BOAT_ENTRY_SIZE 2000

typedef struct
{
    Boat **docks;
    sem_t mutex;
    sem_t full;
    sem_t empty;
} Port;

typedef struct
{
    List *lowPriorityQueue;
    List *highPriorityQueue;
    char *queue_str;
    int str_capacity;
    sem_t rw_mutex;
    sem_t mutex;
    int read_count;
    int next_id;
    int version;
    pthread_cond_t cond;
    pthread_mutex_t cond_mutex;
} DockingOrder;

void *workerThread(void *);
void *unloaderThread(void *);
void *listenerThread(void *);
void sigint_handler(int);
void serialize_queue(char *);
Boat *popBoat();

Port *port;
DockingOrder *order;
sem_t docksSemaphore;
int docks_number = 5;

int main()
{
    signal(SIGPIPE, SIG_IGN);
    port = (Port *)malloc(sizeof(Port));
    port->docks = (Boat **)malloc(docks_number * sizeof(Boat *));
    sem_init(&(port->empty), 0, 0);
    sem_init(&(port->full), 0, 1);

    sem_init(&docksSemaphore, 0, 0);

    order = (DockingOrder *)malloc(sizeof(DockingOrder));
    order->lowPriorityQueue = newList();
    order->highPriorityQueue = newList();
    order->str_capacity = INITIAL_STR_SIZE;
    order->queue_str = (char *)malloc(order->str_capacity * sizeof(char));
    order->queue_str[0] = '\0';
    order->read_count = 0;
    order->version = 0;
    sem_init(&(order->mutex), 0, 1);
    sem_init(&(order->rw_mutex), 0, 1);
    order->version = 0;
    pthread_cond_init(&order->cond, NULL);
    pthread_mutex_init(&order->cond_mutex, NULL);

    struct sigaction sigintAction;
    sigintAction.sa_handler = sigint_handler;
    sigintAction.sa_flags = 0;
    sigemptyset(&sigintAction.sa_mask);
    if (sigaction(SIGINT, &sigintAction, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    int listenfd;
    unsigned int clientlen;
    struct sockaddr_in clientaddr;
    char *port = "8083";
    listenfd = open_listenfd(port);

    if (listenfd < 0)
    {
        connection_error(listenfd);
    }
    printf("Server listening on port %s.\n  Press Ctrl+C to quit safely.\n",
           port);
    pthread_t unloader_tid;
    for (int i = 0; i < docks_number; i++)
    {
        pthread_create(&unloader_tid, NULL, unloaderThread, NULL);
    }
    pthread_t tid;
    while (true)
    {
        clientlen = sizeof(clientaddr);
        int *connfd = (int *)malloc(sizeof(int));
        *connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, workerThread, (void *)connfd);
    }
    return 0;
}

void *workerThread(void *arg)
{
    int connfd = *(int *)arg;
    free(arg);
    Boat *currentBoat = (Boat *)malloc(sizeof(Boat));
    currentBoat->toCheck = false;
    currentBoat->unloading_time = 0;
    currentBoat->connfd = connfd;
    size_t dest_length;
    sem_wait(&(order->rw_mutex));
    currentBoat->id = order->next_id;
    order->next_id++;
    sem_post(&(order->rw_mutex));

    if(write(connfd, &currentBoat->id, sizeof(int)) == -1){
        printf("A boat broke before establishing connection.\n");
        free(currentBoat);
        pthread_exit(NULL);
    }
    if(read(connfd, &(currentBoat->type), sizeof(BoatType)) == 0){
        printf("A boat broke before establishing connection.\n");
        free(currentBoat);
        pthread_exit(NULL);
    }
    if(read(connfd, &(currentBoat->avg_weight), sizeof(double)) == 0){
        printf("A boat broke before establishing connection.\n");
        free(currentBoat);
        pthread_exit(NULL);
    }
    if(read(connfd, &dest_length, sizeof(size_t)) == 0){
        printf("A boat broke before establishing connection.\n");
        free(currentBoat);
        pthread_exit(NULL);
    }
    currentBoat->destination = (char *)malloc(dest_length + 1);
    if(read(connfd, currentBoat->destination, dest_length) == -1){
        printf("A boat broke before establishing connection.\n");
        free(currentBoat->destination);
        free(currentBoat);
        pthread_exit(NULL);
    }
    currentBoat->destination[dest_length] = '\0';
    printf("\n*******************************************************\n");
    printf("waiting for check\n");
    if(read(connfd, &(currentBoat->toCheck), sizeof(bool)) == 0){
        printf("A boat broke before establishing connection.\n");
        free(currentBoat->destination);
        free(currentBoat);
        pthread_exit(NULL);
    }
    if(read(connfd, &(currentBoat->unloading_time), sizeof(double)) == 0){
        printf("A boat broke before establishing connection.\n");
        free(currentBoat->destination);
        free(currentBoat);
        pthread_exit(NULL);
    }
    printf("\n. Boat ID: %d, class: %d, avg weight: %.2f, destination: "
           "%s, to "
           "check: %d, cargo unloading time: %.2f hours\n",
           currentBoat->id, currentBoat->type, currentBoat->avg_weight,
           currentBoat->destination, currentBoat->toCheck,
           currentBoat->unloading_time);

    // WRITER BEHAVIOR START
    sem_wait(&(order->rw_mutex));
    if (currentBoat->toCheck &&
        strcmp(currentBoat->destination, "ecuador") != 0)
    {
        tailInsert(order->highPriorityQueue, (void *)currentBoat);
    }
    else
    {
        tailInsert(order->lowPriorityQueue, (void *)currentBoat);
    }
    if (MAX_BOAT_ENTRY_SIZE * (order->highPriorityQueue->length +
                               order->lowPriorityQueue->length) >=
        order->str_capacity)
    {
        size_t new_capacity = order->str_capacity * 2;
        char *new_queue_str = (char *)realloc(order->queue_str, new_capacity);
        if (new_queue_str == NULL)
        {
            fprintf(stderr, "Error: Memory allocation failed.\n");
            exit(EXIT_FAILURE);
        }
        order->queue_str = new_queue_str;
        order->str_capacity = new_capacity;
    }
    serialize_queue(order->queue_str);
    printf("\nINTEGRITY CHECK Boat ID: %d, class: %d, avg weight: %.2f, destination: "
           "%s, to "
           "check: %d, cargo unloading time: %.2f hours\n",
           currentBoat->id, currentBoat->type, currentBoat->avg_weight,
           currentBoat->destination, currentBoat->toCheck,
           currentBoat->unloading_time);
    printf("serialized. entering pthread mutex\n");
    pthread_mutex_lock(&order->cond_mutex);
    order->version++;
    pthread_cond_broadcast(&order->cond);
    pthread_mutex_unlock(&order->cond_mutex);
    sem_post(&(order->rw_mutex));
    sem_post(&docksSemaphore);
    /// WRITER BEHAVIOR END

    int mytid = pthread_self();
    pthread_t listener_tid;
    pthread_create(&listener_tid, NULL, listenerThread, (void *)currentBoat);

    // READER BEHAVIOR START
    int last_version = -1;
    while (1)
    {
        pthread_mutex_lock(&order->cond_mutex);
        while (order->version == last_version)
        {
            pthread_cond_wait(&order->cond, &order->cond_mutex);
        }
        last_version = order->version;
        pthread_mutex_unlock(&order->cond_mutex);

        // READER START
        sem_wait(&order->mutex);
        order->read_count++;
        if (order->read_count == 1)
        {
            sem_wait(&order->rw_mutex);
        }
        sem_post(&order->mutex);
        size_t list_length = strlen(order->queue_str);
        if (write(connfd, &list_length, sizeof(size_t)) == -1)
        {
            sem_wait(&order->mutex);
            order->read_count--;
            if (order->read_count == 0)
            {
                sem_post(&order->rw_mutex);
            }
            sem_post(&order->mutex);
            pthread_exit(NULL);
        }
        if (write(connfd, order->queue_str, strlen(order->queue_str)) == -1)
        {
            sem_wait(&order->mutex);
            order->read_count--;
            if (order->read_count == 0)
            {
                sem_post(&order->rw_mutex);
            }
            sem_post(&order->mutex);
            pthread_exit(NULL);
        }
        sem_wait(&order->mutex);
        order->read_count--;
        if (order->read_count == 0)
        {
            sem_post(&order->rw_mutex);
        }
        sem_post(&order->mutex);
    }
    // READER BEHAVIOR END
}

void sigint_handler(int signum)
{
    // recorrer la lista haciendo close a todos los connfd, liberar las
    // estructuras y chao
    exit(0);
}

void serialize_queue(char *target)
{
    target[0] = '\0';
    if (order->highPriorityQueue->head == NULL &&
        order->lowPriorityQueue->head == NULL)
    {
        strcat(target, "The queue is clear!\n");
        return;
    }
    strcat(target, "PORT DOCKING QUEUE");
    char formatted[MAX_BOAT_ENTRY_SIZE];
    int turn = 1;
    Node *current = order->highPriorityQueue->head;
    Boat *currentBoat;
    while (current != NULL)
    {
        currentBoat = (Boat *)current->n;
        sprintf(formatted,
                "\n%d. Boat ID: %d, class: %d, avg weight: %.2f, destination: "
                "%s, to "
                "check: %d, cargo unloading time: %.2f hours\n",
                turn, currentBoat->id, currentBoat->type,
                currentBoat->avg_weight, currentBoat->destination,
                currentBoat->toCheck, currentBoat->unloading_time);
        strcat(target, formatted);
        turn++;
        current = current->next;
    }
    current = order->lowPriorityQueue->head;
    while (current != NULL)
    {
        currentBoat = (Boat *)current->n;
        sprintf(formatted,
                "\n%d. Boat ID: %d, class: %d, avg weight: %.2f, destination: "
                "%s, to "
                "check: %d, cargo unloading time: %.2f hours\n",
                turn, currentBoat->id, currentBoat->type,
                currentBoat->avg_weight, currentBoat->destination,
                currentBoat->toCheck, currentBoat->unloading_time);
        strcat(target, formatted);
        turn++;
        current = current->next;
    }
}

void *listenerThread(void *arg)
{
    Boat *myBoat = (Boat *)arg;
    char message[4];
    if(read(myBoat->connfd, message, 3) <= 0){
        printf("Cerrando listener, cliente chao\n"); //POR ALGUNA RAZON EL CONNFD SE ESTA CERRANDO
        pthread_exit(NULL);
    }
    message[4] = '\0';
    if (strcmp(message, "DMG") == 0)
    {
        printf("Boat with ID: %d has broken! removing from list...\n",
               myBoat->id);
        close(myBoat->connfd);
        sem_wait(&(order->rw_mutex));
        if (!deleteBoat(order->highPriorityQueue, myBoat->id))
        {
            deleteBoat(order->lowPriorityQueue, myBoat->id);
        }
        serialize_queue(order->queue_str);
        pthread_mutex_lock(&order->cond_mutex);
        order->version++;
        pthread_cond_broadcast(&order->cond);
        pthread_mutex_unlock(&order->cond_mutex);
        sem_post(&(order->rw_mutex));
        sem_wait(&docksSemaphore);
        pthread_exit(NULL);
    }
    return NULL;
}

void *unloaderThread(void *arg)
{
    Boat *currentBoat = NULL;
    char message[7] = "DOCKED";
    int current_id;
    size_t msg_length = strlen(message);
    while (true)
    {
        sem_wait(&docksSemaphore);
        sem_wait(&(order->rw_mutex));
        currentBoat = popBoat();
        current_id = currentBoat->id;
        write(currentBoat->connfd, &msg_length, sizeof(size_t));
        write(currentBoat->connfd, message, msg_length);
        close(currentBoat->connfd);
        serialize_queue(order->queue_str);
        pthread_mutex_lock(&order->cond_mutex);
        order->version++;
        pthread_cond_broadcast(&order->cond);
        pthread_mutex_unlock(&order->cond_mutex);
        sem_post(&(order->rw_mutex));
        printf("Unloading boat with ID: %d... Hours remaining: %.2f\n",
               currentBoat->id, currentBoat->unloading_time);
        sleep(currentBoat->unloading_time);
        printf(
            "Boat with ID: %d has finished unloading and has left the port.\n",
            current_id);
        free(currentBoat->destination);
        free(currentBoat);
    }
}

Boat *popBoat()
{
    Boat *boat = NULL;
    if (order->highPriorityQueue->head != NULL)
    {
        boat = (Boat *)pop(order->highPriorityQueue, 0);
    }
    else if (order->lowPriorityQueue->head != NULL)
    {
        boat = (Boat *)pop(order->lowPriorityQueue, 0);
    }
    return boat;
}