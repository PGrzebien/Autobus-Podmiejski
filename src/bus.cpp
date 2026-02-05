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

int shmid, semid;
BusState* bus = nullptr;
volatile sig_atomic_t force_departure = 0;

union semun { int val; struct semid_ds *buf; unsigned short *array; };

void init_resources() {
    shmid = shmget(SHM_KEY, sizeof(BusState), 0666);
    bus = (BusState*)shmat(shmid, NULL, 0);
    semid = semget(SEM_KEY, 5, 0666); // Pamiętaj: 5 semaforów
}

// Naprawiona obsługa sygnału - sprawdza czy jest na stacji
void handle_signal(int sig) {
    if (sig == SIGUSR1 && bus != nullptr && bus->is_at_station) {
        force_departure = 1;
    }
}

// Funkcja pomocnicza - czyści kod w main
void set_doors(int open) {
    union semun arg;
    // Drzwi 1: Pojemność
    arg.val = open ? P_CAPACITY : 0;
    semctl(semid, SEM_DOOR_1, SETVAL, arg);
    
    // Drzwi 2: Rowery
    arg.val = open ? R_BIKES : 0;
    semctl(semid, SEM_DOOR_2, SETVAL, arg);

    if (open) log_action("[Drzwi] OTWARTE.");
    else log_action("[Drzwi] ZAMKNIĘTE.");
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

        // 1. OCZEKIWANIE NA PERON (Szybkie sprawdzanie)
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
                // Zwolnij peron i poczekaj chwilę
                struct sembuf v_platform = {SEM_PLATFORM, 1, 0};
                semop(semid, &v_platform, 1);
                usleep(100000); // 0.1s
            }
        }

        log_action(C_BOLD C_YELLOW "[Autobus %d] >>> PODSTAWIONO NA PERON <<<" C_RESET, bus_num);
        
        // Otwarcie drzwi
        set_doors(1);
        log_action("[Autobus %d] Zbieram pasażerów...", bus_num);

        // 2. OCZEKIWANIE (Szybka pętla zamiast sleep)
        for (int i = 0; i < T_WAIT; i++) {
            // Sprawdzamy sygnał co 0.1s (10 razy na sekundę)
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

        // Zamknięcie drzwi
        set_doors(0);

        // 3. ODJAZD
        semaphore_p(semid, SEM_MUTEX);
        bus->is_at_station = 0; 
        bus->bus_at_station_pid = 0;
        log_action("[Autobus %d] ODJAZD! Pasażerów: %d/%d.", bus_num, bus->current_passengers, P_CAPACITY);
        bus->total_travels++;
        semaphore_v(semid, SEM_MUTEX);

        // Zwolnienie peronu
        struct sembuf v_platform = {SEM_PLATFORM, 1, 0};
        semop(semid, &v_platform, 1);

        // 4. TRASA (Tutaj sleep jest OK, bo to symulacja jazdy)
        int drive_time = (rand() % 6) + 5;
        log_action("[Autobus %d] W trasie... (czas: %ds)", bus_num, drive_time);
        sleep(drive_time);
    }
    return 0;
}
