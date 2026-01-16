#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <unistd.h>
#include "constants.h"

// Zmienne globalne, aby były dostępne dla funkcji obsługi sygnałów
int shmid;
BusState* bus = nullptr;

// Funkcja sprzątająca wywoływana przy zamknięciu programu
void cleanup(int sig) {
    std::cout << "\n[System] Otrzymano sygnał termination (" << sig << "). Sprzątanie zasobów IPC..." << std::endl;
    
    if (bus != nullptr && bus != (void*)-1) {
        shmdt(bus); // Odłączenie pamięci
    }

    // Usunięcie segmentu pamięci dzielonej z systemu
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl (IPC_RMID)");
    } else {
        std::cout << "[System] Pamięć dzielona usunięta poprawnie." << std::endl;
    }

    exit(0);
}

int main() {
    // Rejestracja obsługi sygnałów (Ctrl+C i zakończenie)
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    std::cout << "Inicjalizacja systemu Autobus Podmiejski..." << std::endl;

    // 1. Tworzymy segment pamięci dzielonej
    shmid = shmget(SHM_KEY, sizeof(BusState), IPC_CREAT | 0666);
    check_error(shmid, "shmget (tworzenie pamieci)");

    // 2. Podłączamy pamięć
    bus = (BusState*)shmat(shmid, NULL, 0);
    if (bus == (void*)-1) check_error(-1, "shmat (podlaczanie pamieci)");

    // 3. Inicjalizacja stanu początkowego z użyciem stałych z constants.h
    bus->current_passengers = 0;
    bus->current_bikes = 0;
    bus->is_at_station = 1;

    std::cout << "System gotowy. Pamięć IPC utworzona (Klucz: " << SHM_KEY << ")." << std::endl;
    std::cout << "Naciśnij Ctrl+C, aby bezpiecznie zamknąć symulację i usunąć IPC." << std::endl;

    // Pętla symulacji (na razie pusty proces główny)
    while (true) {
        pause(); // Czekaj na sygnał
    }

    return 0;
}
