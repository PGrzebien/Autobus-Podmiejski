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

    // 3. Odłączenie od pamięci przed wyjściem
    shmdt(bus);
    return 0;
}
