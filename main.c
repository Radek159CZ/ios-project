#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include "utilities.h"

// globalni ukazatele na zdroje
shared_data_t *sd = NULL;
sem_t *mutex_write, *sem_dispatch, *sem_boarding, *sem_boarded, *sem_leaving, *sem_finished, *sem_turn;
FILE *out_file = NULL;

// funkce pro uklid semaforu a souboru
void cleanup() {
    if (mutex_write) sem_close(mutex_write);
    if (sem_dispatch) sem_close(sem_dispatch);
    if (sem_boarding) sem_close(sem_boarding);
    if (sem_boarded) sem_close(sem_boarded);
    if (sem_leaving) sem_close(sem_leaving);
    if (sem_finished) sem_close(sem_finished);
    if (sem_turn) sem_close(sem_turn);

    sem_unlink("/x_write_r"); sem_unlink("/x_dispatch_r");
    sem_unlink("/x_boarding_r"); sem_unlink("/x_boarded_r");
    sem_unlink("/x_leaving_r"); sem_unlink("/x_finished_r");
    sem_unlink("/x_turn_r");

    if (sd) cleanup_shared_mem(sd);
    if (out_file) fclose(out_file);
}

// proces navstevnika
void visitor(int id, int tn) {
    log_action(sd, mutex_write, out_file, "n %d: started", id);
    if (tn > 0) usleep(rand() % (tn + 1)); // simulace prichodu
    log_action(sd, mutex_write, out_file, "n %d: queue", id);

    sem_wait(sem_boarding); // ceka na pokyn k nastupu od voziku
    
    // pojistka pro navstevniky, na ktere uz nezbylo misto pri zavirani
    if (sd->closed && sd->visitors_left == 0) {
        log_action(sd, mutex_write, out_file, "n %d: finished", id);
        exit(0);
    }

    log_action(sd, mutex_write, out_file, "n %d: boarding", id);
    sem_post(sem_boarded); // potvrdi voziku ze nastoupil

    sem_wait(sem_leaving); // ceka na dokonceni jizdy
    log_action(sd, mutex_write, out_file, "n %d: leaving", id);
    sem_post(sem_finished); // potvrdi voziku ze vystoupil
    exit(0);
}

// proces voziku
void cart(int id, int v_total, int k_cap, int tv) {
    (void)v_total; // zamezeni unused parameter warningu
    log_action(sd, mutex_write, out_file, "v %d: started", id);

    while (1) {
        sem_wait(sem_turn); // poradi v nastupni stanici
        sem_wait(sem_dispatch); // ceka na pokyn dispecera

        // kontrola zda atrakce jiz nekonci
        if (sd->closed && sd->visitors_left <= 0) {
            sem_post(sem_turn); // uvolneni dalsiho voziku v poradi pro ukonceni
            break;
        }

        log_action(sd, mutex_write, out_file, "v %d: boarding started", id);
        
        // urceni poctu lidi pro tuto jizdu (plna kapacita nebo zbytek)
        int to_load = (sd->visitors_left < k_cap) ? sd->visitors_left : k_cap;
        
        for (int i = 0; i < to_load; i++) {
            sem_post(sem_boarding);
            sem_wait(sem_boarded);
        }

        // aktualizace poctu lidi v parku
        sem_wait(mutex_write);
        sd->visitors_left -= to_load;
        sem_post(mutex_write);

        log_action(sd, mutex_write, out_file, "v %d: boarding complete", id);
        sem_post(sem_turn); // uvolneni stanice pro dalsi vozik

        if (tv > 0) usleep(rand() % (tv + 1)); // simulace jizdy

        sem_wait(sem_turn); // poradi ve vystupni stanici
        log_action(sd, mutex_write, out_file, "v %d: leaving started", id);
        for (int i = 0; i < to_load; i++) {
            sem_post(sem_leaving);
            sem_wait(sem_finished);
        }
        log_action(sd, mutex_write, out_file, "v %d: leaving complete", id);
        sem_post(sem_turn); // uvolneni stanice pro dalsi vozik
    }
    log_action(sd, mutex_write, out_file, "v %d: closed", id);
    exit(0);
}

// proces dispecera
void dispatcher(int n_visitors, int k_cap, int o_time, int v_total) {
    log_action(sd, mutex_write, out_file, "d: started");
    
    // vypocet celkoveho poctu jizd (včetně te neulne)
    int total_rides = (n_visitors + k_cap - 1) / k_cap;

    for (int i = 0; i < total_rides; i++) {
        log_action(sd, mutex_write, out_file, "d: next cart");
        sem_post(sem_dispatch); // signal pro vozik k zahajeni boarding
        
        if (o_time > 0) usleep(rand() % (o_time + 1)); // bezpecna pauza
    }

    // ukonceni atrakce
    sd->closed = 1;
    // probuzeni vsech voziku aby zjistily ze je konec
    for (int i = 0; i < v_total; i++) {
        sem_post(sem_dispatch);
    }

    log_action(sd, mutex_write, out_file, "d: closing");
    exit(0);
}

int main(int argc, char *argv[]) {
    // kontrola argumentu
    if (argc != 7) return 1;
    int v = atoi(argv[1]), n = atoi(argv[2]), k = atoi(argv[3]);
    int tv = atoi(argv[4]), tn = atoi(argv[5]), o = atoi(argv[6]);

    // priprava vystupu
    out_file = fopen("proj2.out", "w");
    if (!out_file) return 1;
    setlinebuf(out_file);
    
    // inicializace sdilene pameti
    sd = init_shared_mem();
    if (!sd) return 1;
    sd->visitors_left = n; 
    srand(time(NULL) ^ getpid());

    // smazani starych semaforu
    sem_unlink("/x_write_r"); sem_unlink("/x_dispatch_r"); sem_unlink("/x_boarding_r");
    sem_unlink("/x_boarded_r"); sem_unlink("/x_leaving_r"); sem_unlink("/x_finished_r");
    sem_unlink("/x_turn_r");

    // otevreni novych semaforu
    mutex_write = sem_open("/x_write_r", O_CREAT, 0666, 1);
    sem_dispatch = sem_open("/x_dispatch_r", O_CREAT, 0666, 0);
    sem_boarding = sem_open("/x_boarding_r", O_CREAT, 0666, 0);
    sem_boarded = sem_open("/x_boarded_r", O_CREAT, 0666, 0);
    sem_leaving = sem_open("/x_leaving_r", O_CREAT, 0666, 0);
    sem_finished = sem_open("/x_finished_r", O_CREAT, 0666, 0);
    sem_turn = sem_open("/x_turn_r", O_CREAT, 0666, 1);

    // spusteni procesu
    if (fork() == 0) { dispatcher(n, k, o, v); exit(0); }
    for (int i = 1; i <= v; i++) if (fork() == 0) { cart(i, v, k, tv); exit(0); }
    for (int i = 1; i <= n; i++) if (fork() == 0) { visitor(i, tn); exit(0); }

    // cekani na vsechny potomky
    while (wait(NULL) > 0);
    cleanup();
    return 0;
}