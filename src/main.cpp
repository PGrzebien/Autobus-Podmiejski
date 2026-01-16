#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "constants.h"

int main() {
    std::cout << "Inicjalizacja systemu Autobus Podmiejski..." << std::endl;

    // 1. Tworzymy segment pamięci dzielonej o rozmiarze struktury BusState
    int shmid = shmget(SHM_KEY, sizeof(BusState), IPC_CREAT | 0666);
    check_error(shmid, "shmget (tworzenie pamieci)");

    // 2. Podłączamy pamięć do naszego procesu
    BusState* bus = (BusState*)shmat(shmid, NULL, 0);
    if (bus == (void*)-1) check_error(-1, "shmat (podlaczanie pamieci)");

    // 3. Inicjalizacja stanu początkowego
    bus->current_passengers = 0;
    bus->current_bikes = 0;
    bus->is_at_station = 1;

    std::cout << "Pamięć dzielona IPC utworzona i zainicjalizowana." << std::endl;

    // Odłączamy się na koniec
    shmdt(bus);

    return 0;
}
