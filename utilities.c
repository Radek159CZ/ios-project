#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include "utilities.h"

// vytvoreni sdilene pameti pomocm mmap
shared_data_t* init_shared_mem() {
    shared_data_t* sd = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sd == MAP_FAILED) return NULL;
    sd->counter = 1;
    sd->visitors_left = 0;
    sd->closed = 0;
    return sd;
}

// uvolneni sdilene pameti
void cleanup_shared_mem(shared_data_t* sd) {
    munmap(sd, sizeof(shared_data_t));
}

// formatovany zapis do souboru s inkrementaci citace
void log_action(shared_data_t* sd, sem_t* mutex, FILE* out, const char* fmt, ...) {
    sem_wait(mutex); // ochrana pristupu k citaci a souboru
    fprintf(out, "%d: ", sd->counter++);
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
    fprintf(out, "\n");
    fflush(out); // zajisteni okamziteho zapisu
    sem_post(mutex);
}