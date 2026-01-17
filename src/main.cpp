#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "constants.h"

// Zmienne globalne
int shmid;
BusState* bus = nullptr;
pid_t bus_pid = 0; 

void cleanup(int sig) {
    std::cout << "\n[Dyspozytor] Otrzymano sygnał (" << sig << "). Zamykanie systemu..." << std::endl;
    
    // Zabijamy proces dziecka (autobusu) przed zamknięciem
    if (bus_pid > 0) {
        std::cout << "[Dyspozytor] Kończenie pracy autobusu (PID: " << bus_pid << ")..." << std::endl;
        kill(bus_pid, SIGTERM);
    }

    if (bus != nullptr && bus != (void*)-1) shmdt(bus);
    shmctl(shmid, IPC_RMID, NULL);
    
    std::cout << "[Dyspozytor] Zasoby IPC usunięte. Koniec." << std::endl;
    exit(0);
}

// Logika procesu AUTOBUSU
void run_bus() {
    std::cout << "[Autobus] Startuję! Mój PID to: " << getpid() << std::endl;
    while (true) {
        // Autobus odczytuje stan z pamięci dzielonej
        std::cout << "[Autobus] Na pokładzie: " << bus->current_passengers << " pasażerów." << std::endl;
        sleep(5);
    }
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    shmid = shmget(SHM_KEY, sizeof(BusState), IPC_CREAT | 0666);
    check_error(shmid, "shmget");
    bus = (BusState*)shmat(shmid, NULL, 0);
    
    // Inicjalizacja stanu
    bus->current_passengers = 0;
    bus->current_bikes = 0;
    bus->is_at_station = 1;

    std::cout << "[Dyspozytor] Tworzę proces autobusu (fork)..." << std::endl;

    bus_pid = fork();

    if (bus_pid == -1) {
        check_error(-1, "fork failed");
    } else if (bus_pid == 0) {
        // KOD DZIECKA
        run_bus();
        exit(0);
    } else {
        // KOD RODZICA
        std::cout << "[Dyspozytor] Autobus wysłany (PID dziecka: " << bus_pid << ")." << std::endl;
        while (true) {
            pause(); 
        }
    }
    return 0;
}
