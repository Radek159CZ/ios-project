#ifndef utilities_h
#define utilities_h

#include <semaphore.h>
#include <stdio.h>

// struktura sdilene pameti pro komunikaci mezi procesy
typedef struct {
    int counter;        // a: poradove cislo radku ve vystupu
    int visitors_left;  // pocet navstevniku, kteri jeste nedojeli
    int closed;         // priznak ukonceni atrakce dispecerem
} shared_data_t;

// prototypy pomocnych funkci
shared_data_t* init_shared_mem();
void cleanup_shared_mem(shared_data_t* sd);
void log_action(shared_data_t* sd, sem_t* mutex, FILE* out, const char* fmt, ...);

#endif