// Justin Casali
// CSCI 3753 PA3

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "queue.h"
#include "util.h"

#define MAX_INPUT_FILES 10
#define MAX_RESOLVER_THREADS 10
#define MIN_RESOLVER_THREADS 2
#define MAX_NAME_LENGTH 1025
#define MAX_IP_LENGTH 64

#define OUTPUT_FILE "output.txt"
#define QUEUE_SIZE 32
#define SLEEP_TIME 10

queue domain; // Queue for domain names
pthread_mutex_t qLock = PTHREAD_MUTEX_INITIALIZER;  // Mutex Lock for Queue
pthread_mutex_t fLock = PTHREAD_MUTEX_INITIALIZER;  // Mutex Lock for Output File

int inputComplete = 0;  // Int-Boolean, true when all request threads are complete

void * request(void * fileName) {
    // Open file
    // Read one line at a time
    // Store line in heap
    // Mutex lock
    // Push pointer on Queue
    // Mutex unlock

    // Locals
    char * name;
    int full;
    int count = 0;

    // Opens file for reading
    FILE * text = fopen((char *) fileName, "r");
    if (text == NULL) {
        printf("Error: input file \"%s\" does not exist; will ignore.\n", (char *) fileName);
        return NULL;
    }

    // Runs loop until end-of-file
    while (1) {
        // Allocates string and fills with next line
        name = (char *) malloc(MAX_NAME_LENGTH);
        fscanf(text, "%s", name);

        // Exit if end-of-file
        if (feof(text)) {
            free(name);
            fclose(text);
            printf("Requestor thread added %d hostnames to queue.\n", count);
            return NULL;
        }

        // Check if full & sleeps if true
        do {
            pthread_mutex_lock(&qLock);
            full = queue_is_full(&domain);
            if (!full) break;
            pthread_mutex_unlock(&qLock);
            usleep(SLEEP_TIME);
        } while (full);

        // Add name to queue & release lock
        queue_push(&domain, (void *) name);
        pthread_mutex_unlock(&qLock);

        // Increments counter
        count++;
    }

}

void * resolve() {
    // If queue is empty wait a bit
    // free string memory off the heap
    // Exit thread if queue is empty and inputComplete is true;

    // Locals
    char * name;
    char * address;
    int empty;
    int count = 0;

    // Loops until queue is empty and inputs are done (function returns)
    while (1) {
        // Checks if empty & sleeps if true
        do {
            pthread_mutex_lock(&qLock);
            empty = queue_is_empty(&domain);
            if (!empty) break;
            pthread_mutex_unlock(&qLock);
            if (inputComplete) {
                printf("Resolver thread processed %d hostnames from queue.\n", count);
                return NULL; // Exit thread
            }
            usleep(SLEEP_TIME);
        } while (empty);

        // Pop domain name off queue & release lock
        name = queue_pop(&domain);
        pthread_mutex_unlock(&qLock);

        // DNS Lookup
        address = (char *) malloc(MAX_IP_LENGTH);
        if (dnslookup(name, address, MAX_IP_LENGTH) == UTIL_FAILURE) address[0] = '\0';

        // Open & Write address to files
        pthread_mutex_lock(&fLock);
        FILE * text = fopen(OUTPUT_FILE, "a");
        fprintf(text, "%s, %s\n", name, address);
        fclose(text);
        pthread_mutex_unlock(&fLock);

        // Increments counter
        count++;

        // Free memory
        free(name);
        free(address);
    }
}

int main(int argc, char *argv[]) {
    // Initialization
    // Create requestor threads
    // Create resolver threads
    // Join request threads & Join resolver threads
    // return 0

    // Locals
    int rv, index;

    // Return if too many input files
    if (argc - 1 > MAX_INPUT_FILES) {
        printf("Error: input files exceed maximum of %d.\n", MAX_INPUT_FILES);
        return -1;
    }

    // Creates Queue and returns on failure
    rv = queue_init(&domain, QUEUE_SIZE);
    if (rv == QUEUE_FAILURE) {
        printf("Error: queue failed to initalize.\n");
        return -1;
    }

    // Determine number of cores within bounds
    int cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (cores > MAX_RESOLVER_THREADS) cores = MAX_RESOLVER_THREADS;
    if (cores < MIN_RESOLVER_THREADS) cores = MIN_RESOLVER_THREADS;

    // Remove output file to clear previous values
    remove(OUTPUT_FILE);

    // Creates requestor threads
    pthread_t requestors[argc - 1];
    for (index = 0; index < argc - 1; index++) pthread_create(&(requestors[index]), NULL, request, (void *) argv[index + 1]);

    // Creates resolver threads based off number of cores
    pthread_t resolvers[cores];
    for (index = 0; index < cores; index++) pthread_create(&(resolvers[index]), NULL, resolve, NULL);

    // Waits for requestor threads to terminate
    for (index = 0; index < argc - 1; index++) pthread_join(requestors[index], NULL);
    inputComplete = 1;

    // Waits for resolvers threads to terminate & exits
    for (index = 0; index < cores; index++) pthread_join(resolvers[index], NULL);
    return 0;
}
