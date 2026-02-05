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

// --- KOLORY ---
#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_YELLOW  "\033[33m"
#define C_GREEN   "\033[32m"
#define C_RED     "\033[31m"
#define C_MAGENTA "\033[35m"

int shmid, semid;
BusState* bus = nullptr;
volatile sig_atomic_t force_departure = 0;

union semun { int val; struct semid_ds *buf; unsigned short *array; };

void init_resources() {
    shmid = shmget(SHM_KEY, sizeof(BusState), 0666);
    bus = (BusState*)shmat(shmid, NULL, 0);
    semid = semget(SEM_KEY, 5, 0666); 
}

void handle_signal(int sig) {
    // Jeśli dostaniemy SIGUSR1 (od Dyspozytora '1'), ustawiamy flagę
    if (sig == SIGUSR1 && bus != nullptr && bus->is_at_station) {
        force_departure = 1;
    }
}

// Funkcja techniczna - tylko ustawia semafory
void set_doors(int open) {
    union semun arg;
    // Drzwi 1: Pojemność
    arg.val = open ? P_CAPACITY : 0;
    semctl(semid, SEM_DOOR_1, SETVAL, arg);
    
    // Drzwi 2: Rowery
    arg.val = open ? R_BIKES : 0;
    semctl(semid, SEM_DOOR_2, SETVAL, arg);
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
                bus->force_departure_signal = 0; // Reset flagi sygnału
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
        log_action("[Autobus %d] Zbieram pasażerów... (Postój: %ds)", bus_num, T_WAIT);

        // Zmienna lokalna do pilnowania logów drzwi
        int local_last_state = -1; 
        
        // Pętla oczekiwania - T_WAIT sekund
        int max_checks = T_WAIT * 10;
        
        for (int i = 0; i < max_checks; i++) {
            
            // A. Sygnał ODJAZD
            if (force_departure) {
                log_action(C_BOLD C_MAGENTA "[Autobus %d] >>> WYMUSZONO ODJAZD! <<<" C_RESET, bus_num);
                force_departure = 0;
                break; // Przerywamy czekanie i odjeżdżamy natychmiast
            }

            // B. Obsługa DRZWI (Priorytet wysoki)
            int current_state = bus->is_station_open;
            set_doors(current_state);

            if (current_state != local_last_state) {
                if (current_state) log_action(C_BOLD C_GREEN "[Drzwi] OTWARTE." C_RESET);
                else log_action(C_BOLD C_RED "[Drzwi] ZAMKNIĘTE." C_RESET);
                local_last_state = current_state;
            }

            // IPC_NOWAIT zapobiega blokowaniu procesu w kolejce pasażerów.
            struct sembuf sb_check = {(unsigned short)SEM_MUTEX, -1, IPC_NOWAIT};
            
            if (semop(semid, &sb_check, 1) == 0) {
                semaphore_v(semid, SEM_MUTEX);
            }

            usleep(100000); // 0.1s przerwy (zegar pętli)
        }

        // Zamknięcie drzwi i czyszczenie
        set_doors(0);
        if (local_last_state != 0) {
             log_action(C_BOLD C_RED "[Drzwi] ZAMKNIĘTE." C_RESET);
        }

        // 3. ODJAZD
        semaphore_p(semid, SEM_MUTEX);
        bus->is_at_station = 0;
        bus->bus_at_station_pid = 0;
        bus->force_departure_signal = 0; // Reset flagi
        log_action("[Autobus %d] ODJAZD! Pasażerów: %d/%d.", bus_num, bus->current_passengers, P_CAPACITY);
        bus->total_travels++;
        semaphore_v(semid, SEM_MUTEX);

        // Zwalniamy peron dla innego autobusu
        struct sembuf v_platform = {SEM_PLATFORM, 1, 0};
        semop(semid, &v_platform, 1);

        // 4. TRASA
        int drive_time = (rand() % 6) + 5;
        log_action("[Autobus %d] W trasie... (czas: %ds)", bus_num, drive_time);
        sleep(drive_time);
    }
    return 0;
}
