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

// Zmienne globalne do komunikacji
int shmid;
int semid;
BusState* bus = nullptr;

// Funkcja łącząca się z zasobami (Pamięć + Semafory)
void init_resources() {
    shmid = shmget(SHM_KEY, sizeof(BusState), 0666);
    check_error(shmid, "[Pasażer] Błąd shmget - brak pamięci (czy dyspozytor działa?)");

    // 2. Podłączenie pamięci do przestrzeni adresowej
    bus = (BusState*)shmat(shmid, NULL, 0);
    check_error(bus == (void*)-1 ? -1 : 0, "[Pasażer] Błąd shmat");

    // 3. Pobranie ID semaforów
    semid = semget(SEM_KEY, 3, 0666);
    check_error(semid, "[Pasażer] Błąd semget - brak semaforów");
}

// Funkcja symulująca wizytę w Kasie
void buy_ticket(PassengerType type, pid_t pid) {
    srand(pid * time(NULL));
    usleep((rand() % 500) * 1000); // Czekaj 0-500ms

    if (type == VIP) {
        log_action("[Pasażer %d] Jestem VIP - wchodzę bez kolejki do kasy.", pid);
    } else {
        log_action("[Kasa] Sprzedano bilet dla pasażera %d (Typ: %d).", pid, type);
    }
}

// Logika Dziecka - 10% szans, że zgubiło opiekuna
bool check_guardian(pid_t pid) {
    if (rand() % 100 < 10) {
        log_action("[Kontrola] Dziecko %d bez opiekuna! Odmowa wstępu.", pid);
        return false;
    }
    return true;
}

// Funkcja próbująca wejść do autobusu
void board_bus(PassengerType type, pid_t pid) {
    // 0. Dziecko musi mieć opiekuna
    if (type == CHILD_WITH_GUARDIAN) {
        if (!check_guardian(pid)) return; // Rezygnacja
    }

    // 1. Sprawdź czy dworzec otwarty
    semaphore_p(semid, SEM_MUTEX);
    if (!bus->is_station_open) {
        log_action("[Pasażer %d] Dworzec ZAMKNIĘTY (Sygnał 2). Rezygnuję.", pid);
        semaphore_v(semid, SEM_MUTEX);
        exit(0);
    }
    semaphore_v(semid, SEM_MUTEX);

    // 2. Wybierz drzwi
    int door_sem = (type == BIKER) ? SEM_DOOR_2 : SEM_DOOR_1;
    log_action("[Pasażer %d] Czekam na otwarcie drzwi %d...", pid, door_sem == SEM_DOOR_1 ? 1 : 2);

    // 3. Czekaj na wejście (Tu proces stoi w kolejce)
    semaphore_p(semid, door_sem);

    // 4. Wsiadanie - KRYTYCZNE SPRAWDZENIE MIEJSCA
    semaphore_p(semid, SEM_MUTEX);
    
    bool can_enter = true;
    
    // Sprawdzamy, czy wchodząc nie przekroczymy limitu
    if (bus->current_passengers >= P_CAPACITY) {
        log_action("[Pasażer %d] Autobus pełny! (%d/%d). Rezygnuję.", pid, bus->current_passengers, P_CAPACITY);
        can_enter = false;
    } 
    else if (type == BIKER && bus->current_bikes >= R_BIKES) {
        log_action("[Pasażer %d] Brak miejsca na rower! (%d/%d). Rezygnuję.", pid, bus->current_bikes, R_BIKES);
        can_enter = false;
    }

    if (can_enter) {
        bus->current_passengers++;
        if (type == BIKER) bus->current_bikes++;
        
        log_action("[Wejście] %d wchodzi. Stan: %d/%d (R: %d/%d)", 
                   pid, bus->current_passengers, P_CAPACITY, bus->current_bikes, R_BIKES);
    }

    semaphore_v(semid, SEM_MUTEX);
    
    // Jeśli nie udało się wejść, proces musi zniknąć
    if (!can_enter) exit(0);
}

int main(int argc, char* argv[]) {
    // 1. Walidacja argumentów
    if (argc < 2) {
        fprintf(stderr, "Użycie: %s [typ 0-3]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int type_id = atoi(argv[1]);
    if (type_id < 0 || type_id > 3) exit(EXIT_FAILURE);

    PassengerType type = static_cast<PassengerType>(type_id);
    pid_t my_pid = getpid();

    // 2. Inicjalizacja zasobów IPC
    init_resources();

    // 3. Kupno biletu
    buy_ticket(type, my_pid);

    // 4. Próba wejścia
    board_bus(type, my_pid);

    // 5. Odłączenie od pamięci przed wyjściem
    shmdt(bus);
    return 0;
}
