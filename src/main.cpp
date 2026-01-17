#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctime>
#include "constants.h"

// Zmienne globalne
int shmid;
BusState* bus = nullptr;
pid_t bus_pid = 0;
pid_t gen_pid = 0;

void cleanup(int sig) {
    std::cout << "\n[Dyspozytor] Zamykanie systemu (Sygnał: " << sig << ")..." << std::endl;
    if (bus_pid > 0) kill(bus_pid, SIGTERM);
    if (gen_pid > 0) kill(gen_pid, SIGTERM);
    if (bus != nullptr && bus != (void*)-1) shmdt(bus);
    shmctl(shmid, IPC_RMID, NULL);
    std::cout << "[Dyspozytor] Zasoby IPC usunięte. Koniec." << std::endl;
    exit(0);
}

void run_bus() {
    std::cout << "[Autobus] Startuję (PID: " << getpid() << ")" << std::endl;
    while (true) {
        std::cout << "[Autobus] Na przystanku: " << bus->current_passengers << "/" << P_CAPACITY 
                  << " (Rowery: " << bus->current_bikes << "/" << R_BIKES << ")" << std::endl;
        sleep(5);
    }
}

void run_generator() {
    std::cout << "[Generator] Startuję (PID: " << getpid() << ")" << std::endl;
    // Lepsze ziarno dla losowości (PID + czas)
    srand(time(NULL) ^ (getpid() << 16));
    
    const char* type_names[] = {"ZWYKLY", "ROWERZYSTA", "VIP", "DZIECKO"};

    while (true) {
        sleep(rand() % 4 + 1); // Nowy pasażer co 1-4 sekundy
        
        if (bus->current_passengers < P_CAPACITY) {
            // Losowanie atrybutów pasażera
            PassengerType type = static_cast<PassengerType>(rand() % 4);
            int has_ticket = rand() % 10 < 8; // 80% szans na posiadanie biletu

            // Aktualizacja stanu w pamięci dzielonej
            bus->current_passengers++;
            if (type == BIKER) {
                bus->current_bikes++;
            }

            std::cout << "[Generator] Nowy pasażer: " << type_names[type] 
                      << " (Bilet: " << (has_ticket ? "TAK" : "NIE") << "). "
                      << "Łącznie na przystanku: " << bus->current_passengers << std::endl;
        } else {
            std::cout << "[Generator] Przystanek pełny. Oczekiwanie..." << std::endl;
            sleep(5);
        }
    }
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    shmid = shmget(SHM_KEY, sizeof(BusState), IPC_CREAT | 0666);
    check_error(shmid, "shmget");
    bus = (BusState*)shmat(shmid, NULL, 0);
    
    bus->current_passengers = 0;
    bus->current_bikes = 0;
    bus->is_at_station = 1;

    bus_pid = fork();
    if (bus_pid == 0) {
        run_bus();
        exit(0);
    }

    gen_pid = fork();
    if (gen_pid == 0) {
        run_generator();
        exit(0);
    }

    std::cout << "[Dyspozytor] System aktywny. Monitorowanie..." << std::endl;
    while (true) {
        pause();
    }
    return 0;
}
