#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "../include/common.h"
#include "../include/utils.h"

// Zmienne globalne
int shmid;
int semid;
BusState* bus = nullptr;
// Definicja unii dla semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};


// Flaga przerwania pętli
volatile sig_atomic_t force_departure = 0;

// Funkcja inicjalizująca zasoby (podłączenie do systemu)
void init_resources() {
    // 1. Pobranie ID pamięci dzielonej
    shmid = shmget(SHM_KEY, sizeof(BusState), 0666);
    check_error(shmid, "[Kierowca] Błąd shmget (uruchom najpierw main!)");

    // 2. Podłączenie do pamięci
    bus = (BusState*)shmat(shmid, NULL, 0);
    check_error(bus == (void*)-1 ? -1 : 0, "[Kierowca] Błąd shmat");

    // 3. Pobranie ID semaforów
    semid = semget(SEM_KEY, 3, 0666);
    check_error(semid, "[Kierowca] Błąd semget");

    log_action("[Kierowca] Zameldowałem się w systemie (PID: %d).", getpid());
}

// Handler sygnału SIGUSR1 - Wymuszony odjazd
void handle_signal(int sig) {
    if (sig == SIGUSR1) {
        force_departure = 1;
        log_action("[Kierowca] Otrzymano rozkaz odjazdu (SIGUSR1)!");
    }
}

// Funkcja sterująca drzwiami (Otwórz/Zamknij)
void set_doors(int open) {
    union semun arg;
    if (open) {
        // Otwieramy: Ustawiamy wartość semafora na limit miejsc (wpuszczamy tłum)
        arg.val = P_CAPACITY;
        if (semctl(semid, SEM_DOOR_1, SETVAL, arg) == -1) perror("Otwieranie D1");
        
        arg.val = R_BIKES;
        if (semctl(semid, SEM_DOOR_2, SETVAL, arg) == -1) perror("Otwieranie D2");
        
        log_action("[Drzwi] OTWARTE. Zapraszam!");
    } else {
        // Zamykamy: Ustawiamy semafory na 0 (blokada)
        arg.val = 0;
        if (semctl(semid, SEM_DOOR_1, SETVAL, arg) == -1) perror("Zamykanie D1");
        if (semctl(semid, SEM_DOOR_2, SETVAL, arg) == -1) perror("Zamykanie D2");
        
        log_action("[Drzwi] ZAMKNIĘTE.");
    }
}


int main() {
    // 1. Podłącz zasoby
    init_resources();
    // 2. Rejestracja sygnałów
    signal(SIGUSR1, handle_signal);

while (bus->total_travels < N_BUSES) {
        // 1. Podjazd na stanowisko
        semaphore_p(semid, SEM_MUTEX);
        bus->is_at_station = 1;
        bus->current_passengers = 0;
        bus->current_bikes = 0;
        log_action("[Autobus] Podstawiono nowy autobus. Kurs nr: %d", bus->total_travels + 1);
        semaphore_v(semid, SEM_MUTEX);

        // 2. Otwarcie drzwi
        set_doors(1); // Otwórz

        // 3. Oczekiwanie (Czas T lub sygnał)
        log_action("[Autobus] Oczekiwanie na pasażerów (%d s)...", T_WAIT);
        for (int i = 0; i < T_WAIT; i++) {
            if (force_departure) {
                log_action("[Autobus] ! WYMUSZONY ODJAZD (Sygnał 1) !");
                force_departure = 0; // Reset flagi
                break;
            }
            sleep(1);
        }

        // 4. Zamknięcie drzwi
        set_doors(0); // Zamknij

        // 5. Odjazd i trasa
        semaphore_p(semid, SEM_MUTEX);
        bus->is_at_station = 0;
        log_action("[Autobus] ODJAZD! Pasażerów: %d/%d (Rowery: %d).", 
                   bus->current_passengers, P_CAPACITY, bus->current_bikes);
        bus->total_travels++;
        semaphore_v(semid, SEM_MUTEX);

        sleep(3); // Symulacja jazdy (powrót na pętlę)
    }    
  
    log_action("[System] Koniec zmiany. Wykorzystano limit %d autobusów.", N_BUSES);

    // Sprzątanie przy wyjściu
    shmdt(bus);
    return 0;
}
