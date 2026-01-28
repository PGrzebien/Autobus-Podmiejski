#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "../include/common.h"
#include "../include/utils.h"

// Zmienne globalne
int shmid, semid;
BusState* bus = nullptr;

// Inicjalizacja zasobów IPC
void init_resources() {
    shmid = shmget(SHM_KEY, sizeof(BusState), 0666);
    check_error(shmid, "[Pasażer] Błąd shmget");
    
    bus = (BusState*)shmat(shmid, NULL, 0);
    check_error(bus == (void*)-1 ? -1 : 0, "[Pasażer] Błąd shmat");
    
    semid = semget(SEM_KEY, 3, 0666);
    check_error(semid, "[Pasażer] Błąd semget");
}

// Główna logika pasażera
void board_bus(PassengerType type, pid_t pid, int age) {
    // 1. Walidacja wieku (Wymóg: < 8 lat tylko z opiekunem)
    if (age < 8) {
        // Symulacja: 10% szans, że dziecko jest samo (brak opiekuna)
        if (rand() % 100 < 10) {
            log_action("[Kontrola] Dziecko (%d lat, PID: %d) bez opiekuna! Odmowa wstępu.", age, pid);
            return; // Dziecko odchodzi, proces kończy działanie
        }
    }

    // 2. Obsługa biletu (Wymóg: VIP wchodzi bez kolejki)
    if (type == VIP) {
        log_action("[Pasażer %d] VIP (%d lat) - mam bilet, wchodzę bez kolejki.", pid, age);
    } else {
        // Symulacja stania w kolejce do kasy (zakup biletu)
        usleep((rand() % 500) * 1000); 
        log_action("[Kasa] Pasażer %d (%d lat) kupił bilet.", pid, age);
    }

    // 3. Sprawdź czy dworzec jest otwarty (Obsługa sygnału 2 od Dyspozytora)
    semaphore_p(semid, SEM_MUTEX);
    if (!bus->is_station_open) {
        log_action("[Pasażer %d] Dworzec ZAMKNIĘTY (Sygnał 2). Rezygnuję.", pid);
        semaphore_v(semid, SEM_MUTEX);
        exit(0);
    }
    semaphore_v(semid, SEM_MUTEX);

    // 4. Wybór odpowiednich drzwi
    int door_sem = (type == BIKER) ? SEM_DOOR_2 : SEM_DOOR_1;
    
    // --- OCZEKIWANIE NA WEJŚCIE ---
    // Tutaj proces czeka, aż kierowca otworzy drzwi (podniesie semafor)
    semaphore_p(semid, door_sem);

    // 5. Wsiadanie (Sekcja Krytyczna)
    semaphore_p(semid, SEM_MUTEX);
    
    bool can_enter = true;
    
    // Kierowca pilnuje limitów P (pasażerowie) i R (rowery)
    if (bus->current_passengers >= P_CAPACITY) {
        can_enter = false; 
    } 
    else if (type == BIKER && bus->current_bikes >= R_BIKES) {
        can_enter = false;
    }

    if (can_enter) {
        bus->current_passengers++;
        if (type == BIKER) bus->current_bikes++;
        
        log_action("[Wejście] %d (%d lat) wchodzi. Stan: %d/%d (Rowery: %d/%d)", 
                   pid, age, bus->current_passengers, P_CAPACITY, bus->current_bikes, R_BIKES);
    } else {
        // Sytuacja rzadka (wyścig), ale możliwa - brak miejsca
        log_action("[Pasażer %d] Brak miejsca w środku! Rezygnuję.", pid);
    }

    semaphore_v(semid, SEM_MUTEX);
    
    // Jeśli nie udało się wejść, proces musi zniknąć
    if (!can_enter) exit(0);
}

int main(int argc, char* argv[]) {
    // Walidacja argumentów
    if (argc < 2) {
        fprintf(stderr, "Użycie: %s [typ 0-3]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int type_id = atoi(argv[1]);
    PassengerType type = static_cast<PassengerType>(type_id);
    pid_t my_pid = getpid();
    
    // Unikalny seed dla liczb losowych
    srand(my_pid * time(NULL));

    // Podłączenie do systemu
    init_resources();

    // Generowanie WIEKU (Wymóg: Dziecko < 8 lat, reszta starsza)
    int age;
    if (type == CHILD_WITH_GUARDIAN) {
        age = rand() % 8;        // Wiek 0-7 lat
    } else {
        age = 8 + (rand() % 75); // Wiek 8-82 lat
    }

    // Próba wejścia do autobusu
    board_bus(type, my_pid, age);

    // Sprzątanie pamięci lokalnej
    shmdt(bus);
    return 0;
}
