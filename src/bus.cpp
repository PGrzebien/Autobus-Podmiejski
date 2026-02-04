#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "../include/common.h"
#include "../include/utils.h"

#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_YELLOW  "\033[33m"
#define C_GREEN   "\033[32m"

int shmid, semid;
BusState* bus = nullptr;
volatile sig_atomic_t force_departure = 0;

union semun { int val; struct semid_ds *buf; unsigned short *array; };

void init_resources() {
    shmid = shmget(SHM_KEY, sizeof(BusState), 0666);
    bus = (BusState*)shmat(shmid, NULL, 0);
    semid = semget(SEM_KEY, 4, 0666);
}

void handle_signal(int sig) {
    if (sig == SIGUSR1 && bus != nullptr && bus->is_at_station) {
        force_departure = 1;
    }
}

int main(int argc, char* argv[]) {
    int bus_num = (argc > 1) ? atoi(argv[1]) : 1;
    srand(time(NULL) ^ getpid() ^ bus_num);
    init_resources();
    signal(SIGUSR1, handle_signal);

    log_action("[Kierowca %d] Gotowy do pracy.", bus_num);

    while (true) {
        bool entered = false;
        struct sembuf p_platform = {SEM_PLATFORM, -1, 0};

        // 1. OCZEKIWANIE NA PERON
        while (!entered) {
            if (semop(semid, &p_platform, 1) == -1) {
                if (errno == EINTR) continue;
                exit(1);
            }

            semaphore_p(semid, SEM_MUTEX);
            if (bus->is_at_station == 0) {
                bus->is_at_station = 1;
                bus->bus_at_station_pid = getpid();
                bus->current_passengers = 0;
                bus->current_bikes = 0;
                entered = true;
            }
            semaphore_v(semid, SEM_MUTEX);

            if (!entered) {
                struct sembuf v_platform = {SEM_PLATFORM, 1, 0};
                semop(semid, &v_platform, 1);
                usleep(100000); 
            }
        }

        log_action(C_BOLD C_YELLOW "[Autobus %d] >>> PODSTAWIONO NA PERON <<<" C_RESET, bus_num);
        
        // OTWIERANIE DRZWI
        union semun arg;
        arg.val = P_CAPACITY; 
        semctl(semid, SEM_DOOR_1, SETVAL, arg);
        arg.val = R_BIKES;
        semctl(semid, SEM_DOOR_2, SETVAL, arg);
        
        log_action("[Drzwi] OTWARTE.");
        log_action("[Autobus %d] Zbieram pasażerów...", bus_num);

        // 2. OCZEKIWANIE (Szybka reakcja na '1')
        for (int i = 0; i < T_WAIT; i++) {
            // Sprawdzamy sygnał co 0.1s
            for (int k = 0; k < 10; k++) {
                if (force_departure) break;
                usleep(100000);
            }

            if (force_departure) {
                log_action("[Autobus %d] Otrzymano sygnał odjazdu!", bus_num);
                force_departure = 0;
                break;
            }

            semaphore_p(semid, SEM_MUTEX);
            bool is_full = (bus->current_passengers >= P_CAPACITY);
            semaphore_v(semid, SEM_MUTEX);

            if (is_full) {
                 log_action(C_BOLD C_GREEN "[Autobus %d] Komplet pasażerów! Odjeżdżam wcześniej." C_RESET, bus_num);
                 break;
            }
        }

        // ZAMYKANIE DRZWI
        arg.val = 0;
        semctl(semid, SEM_DOOR_1, SETVAL, arg);
        semctl(semid, SEM_DOOR_2, SETVAL, arg);
        log_action("[Drzwi] ZAMKNIĘTE.");

        // 3. ODJAZD
        semaphore_p(semid, SEM_MUTEX);
        bus->is_at_station = 0; 
        bus->bus_at_station_pid = 0;
        log_action("[Autobus %d] ODJAZD! Pasażerów: %d/%d.", bus_num, bus->current_passengers, P_CAPACITY);
        bus->total_travels++;
        semaphore_v(semid, SEM_MUTEX);

        struct sembuf v_platform = {SEM_PLATFORM, 1, 0};
        semop(semid, &v_platform, 1);

        // 4. TRASA
        int drive_time = (rand() % 6) + 5;
        log_action("[Autobus %d] W trasie... (czas: %ds)", bus_num, drive_time);
        sleep(drive_time);
    }
    return 0;
}
