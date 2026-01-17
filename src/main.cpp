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
pid_t gen_pid = 0; // PID generatora pasażerów

void cleanup(int sig) {
    std::cout << "\n[Dyspozytor] Zamykanie systemu (Sygnał: " << sig << ")..." << std::endl;
    
    if (bus_pid > 0) kill(bus_pid, SIGTERM);
    if (gen_pid > 0) kill(gen_pid, SIGTERM);

    if (bus != nullptr && bus != (void*)-1) shmdt(bus);
    shmctl(shmid, IPC_RMID, NULL);
    
    std::cout << "[Dyspozytor] Wszystkie procesy zakończone, zasoby IPC usunięte." << std::endl;
    exit(0);
}

void run_bus() {
    std::cout << "[Autobus] Startuję (PID: " << getpid() << ")" << std::endl;
    while (true) {
        std::cout << "[Autobus] Przystanek: " << bus->current_passengers << "/" << P_CAPACITY << " pasażerów." << std::endl;
        sleep(3);
    }
}

void run_generator() {
    std::cout << "[Generator] Startuję (PID: " << getpid() << ")" << std::endl;
    srand(time(NULL));
    while (true) {
        sleep(rand() % 5 + 1); // Czekaj od 1 do 5 sekund
        if (bus->current_passengers < P_CAPACITY) {
            bus->current_passengers++;
            std::cout << "[Generator] Nowy pasażer! Kolejka: " << bus->current_passengers << std::endl;
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
    bus->is_at_station = 1;

    // Tworzenie Autobusu
    bus_pid = fork();
    if (bus_pid == 0) {
        run_bus();
        exit(0);
    }

    // Tworzenie Generatora
    gen_pid = fork();
    if (gen_pid == 0) {
        run_generator();
        exit(0);
    }

    std::cout << "[Dyspozytor] System działa. Monitorowanie... (Ctrl+C przerywa)" << std::endl;
    while (true) {
        pause();
    }

    return 0;
}
