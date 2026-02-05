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
    if (sig == SIGUSR1 && bus != nullptr && bus->is_at_station) {
        force_departure = 1;
    }
}

// Funkcja techniczna - tylko ustawia semafory (bez logowania)
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
        log_action("[Autobus %d] Zbieram pasażerów...", bus_num);

        // Zmienna lokalna do pilnowania logów drzwi
        int local_last_state = -1; 
        
        // Pętla oczekiwania
        int max_checks = T_WAIT * 10;
        
        // 2. PĘTLA OCZEKIWANIA (SUPER-SZYBKA REAKCJA - IPC_NOWAIT)
        for (int i = 0; i < max_checks; i++) {
            
            // A. Sygnał ODJAZDU (Priorytet najwyższy - sprawdzamy ZAWSZE)
            if (force_departure) {
                log_action(C_BOLD C_MAGENTA "[Autobus %d] >>> WYMUSZONO ODJAZD! <<<" C_RESET, bus_num);
                force_departure = 0;
                break; 
            }

            // B. Obsługa DRZWI (Priorytet wysoki - sprawdzamy ZAWSZE)
            int current_state = bus->is_station_open;
            set_doors(current_state);

            // Logujemy tylko zmianę stanu
            if (current_state != local_last_state) {
                if (current_state) log_action(C_BOLD C_GREEN "[Drzwi] OTWARTE." C_RESET);
                else log_action(C_BOLD C_RED "[Drzwi] ZAMKNIĘTE." C_RESET);
                local_last_state = current_state;
            }

            // C. Sprawdzanie POJEMNOŚCI (Wersja NON-BLOCKING)
            // Używamy IPC_NOWAIT, żeby autobus nie utknął w kolejce 5000 pasażerów
            struct sembuf sb_check = {(unsigned short)SEM_MUTEX, -1, IPC_NOWAIT};
            
            // Próbujemy wziąć semafor bez czekania
            if (semop(semid, &sb_check, 1) == 0) {
                // Udało się! Mamy dostęp do pamięci (nikt nie wchodzi w tej nanosekundzie)
                bool is_full = (bus->current_passengers >= P_CAPACITY);
                semaphore_v(semid, SEM_MUTEX); // Zwalniamy normalnie

                if (is_full) {
                    log_action(C_BOLD C_GREEN "[Autobus %d] Komplet! Odjeżdżam." C_RESET, bus_num);
                    break;
                }
            }
            // Jeśli semop zwrócił -1 (zajęte przez pasażera), to TRUDNO.
            // Ignorujemy to i lecimy dalej pętlą, żeby sprawdzić sygnał force_departure.
            // Dzięki temu autobus nigdy się nie zawiesi.

            usleep(100000); // 0.1s przerwy
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
