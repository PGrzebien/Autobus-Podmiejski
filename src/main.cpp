#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctime>
#include "constants.h"

// Zmienne globalne
int shmid;
int semid; // ID zestawu semaforów
BusState* bus = nullptr;
pid_t bus_pid = 0;
pid_t gen_pid = 0;

// Funkcja sprzątająca zasoby
void cleanup(int sig) {
    std::cout << "\n[Dyspozytor] Zamykanie systemu (Sygnał: " << sig << ")..." << std::endl;
    
    // Kończenie procesów potomnych
    if (bus_pid > 0) kill(bus_pid, SIGTERM);
    if (gen_pid > 0) kill(gen_pid, SIGTERM);

    // Odłączanie i usuwanie pamięci dzielonej
    if (bus != nullptr && bus != (void*)-1) shmdt(bus);
    shmctl(shmid, IPC_RMID, NULL);
    
    // USUWANIE SEMAFORÓW
    if (semid >= 0) {
        if (semctl(semid, 0, IPC_RMID) == -1) {
            perror("semctl (IPC_RMID)");
        } else {
            std::cout << "[Dyspozytor] Zestaw semaforów usunięty poprawnie." << std::endl;
        }
    }

    std::cout << "[Dyspozytor] Zasoby IPC wyczyszczone. Koniec pracy." << std::endl;
    exit(0);
}

void run_bus() {
    std::cout << "[Autobus] Startuję (PID: " << getpid() << ")" << std::endl;
    while (true) {
        std::cout << "[Autobus] Stan: " << bus->current_passengers << "/" << P_CAPACITY << " pasażerów." << std::endl;
        sleep(5);
    }
}

void run_generator() {
    std::cout << "[Generator] Startuję (PID: " << getpid() << ")" << std::endl;
    srand(time(NULL) ^ (getpid() << 16));
    while (true) {
        sleep(rand() % 4 + 1);
        if (bus->current_passengers < P_CAPACITY) {
            bus->current_passengers++;
            std::cout << "[Generator] Nowy pasażer. Kolejka: " << bus->current_passengers << std::endl;
        }
    }
}

int main() {
    // Rejestracja sygnałów
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    // 1. Inicjalizacja Pamięci Dzielonej (SHM)
    shmid = shmget(SHM_KEY, sizeof(BusState), IPC_CREAT | 0666);
    check_error(shmid, "shmget (SHM)");
    bus = (BusState*)shmat(shmid, NULL, 0);
    check_error(bus == (void*)-1 ? -1 : 0, "shmat");

    // 2. TWORZENIE ZESTAWU SEMAFORÓW (3 semafory: Mutex, Drzwi1, Drzwi2)
    semid = semget(SEM_KEY, 3, IPC_CREAT | 0666);
    check_error(semid, "semget (SEM)");

    // Inicjalizacja stanu początkowego w SHM
    bus->current_passengers = 0;
    bus->current_bikes = 0;
    bus->is_at_station = 1;

    std::cout << "[Dyspozytor] Zasoby zainicjalizowane. SEM_ID: " << semid << std::endl;

    // Tworzenie procesów
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

    std::cout << "[Dyspozytor] System działa. Monitorowanie... (Ctrl+C kończy)" << std::endl;
    while (true) {
        pause();
    }

    return 0;
}
