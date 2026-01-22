#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctime>
#include "constants.h"

// Unia wymagana przez semctl zgodnie ze standardem System V
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Zmienne globalne
int shmid;
int semid; // ID zestawu semaforów
BusState* bus = nullptr;
pid_t bus_pid = 0;
pid_t gen_pid = 0;

// Funkcja sprzątająca zasoby
void cleanup(int sig) {
    // Sprawdzamy, czy to proces rodzica (Dyspozytor) powinien sprzątać
    // Aby uniknąć błędów "Invalid argument", sprzątanie wykonuje tylko główny proces
    std::cout << "\n[Dyspozytor] Zamykanie systemu (Sygnał: " << sig << ")..." << std::endl;
    
    if (bus_pid > 0) kill(bus_pid, SIGTERM);
    if (gen_pid > 0) kill(gen_pid, SIGTERM);

    if (bus != nullptr && bus != (void*)-1) shmdt(bus);
    shmctl(shmid, IPC_RMID, NULL);
    
    if (semid >= 0) {
        if (semctl(semid, 0, IPC_RMID) == -1) {
            perror("semctl (IPC_RMID)");
        } else {
            std::cout << "[Dyspozytor] Zestaw semaforów usunięty." << std::endl;
        }
    }

    std::cout << "[Dyspozytor] Zasoby IPC wyczyszczone. Koniec." << std::endl;
    exit(0);
}

void run_bus() {
    log_action("[Autobus] Start serwisu (PID: %d)", getpid());
    while (true) {
        semaphore_p(semid, SEM_MUTEX);
        log_action("[Autobus] Stan: %d/%d pasazerow (w tym %d/%d rowerow)", 
                   bus->current_passengers, P_CAPACITY, 
                   bus->current_bikes, R_BIKES);
        semaphore_v(semid, SEM_MUTEX);

        sleep(5);
    }
}

void run_generator() {
    log_action("[Generator] Start procesu generowania (PID: %d)", getpid());
    srand(time(NULL) ^ (getpid() << 16));

    const char* type_names[] = {"NORMALNY", "ROWERZYSTA", "VIP", "DZIECKO"};

    while (true) {
        sleep(rand() % 3 + 1); // Losowy odstęp 1-3s

        // Losowanie typu pasażera
        PassengerType type = static_cast<PassengerType>(rand() % 4);
        int has_ticket = (rand() % 100 < 80); // 80% ma bilet

        semaphore_p(semid, SEM_MUTEX);

        // Logika wejścia
        bool can_enter = false;

        if (bus->current_passengers < P_CAPACITY) {
            if (type == BIKER) {
                // Rowerzysta musi mieć miejsce na rower ORAZ miejsce w autobusie
                if (bus->current_bikes < R_BIKES) {
                    bus->current_bikes++;
                    bus->current_passengers++;
                    can_enter = true;
                } else {
                    log_action("[Generator] Odmowa: Brak miejsc na rowery (Limit: %d)", R_BIKES);
                }
            } else {
                // Pozostali pasażerowie
                bus->current_passengers++;
                can_enter = true;
            }
        } else {
            log_action("[Generator] Odmowa: Autobus pelny (%d/%d)", P_CAPACITY, P_CAPACITY);
        }

        if (can_enter) {
            log_action("[Generator] Wsiadl: %s (Bilet: %d). Stan: %d/%d (Rowery: %d/%d)", 
                       type_names[type], has_ticket, 
                       bus->current_passengers, P_CAPACITY,
                       bus->current_bikes, R_BIKES);
        }

        semaphore_v(semid, SEM_MUTEX);
    }
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    // 1. Inicjalizacja Pamięci Dzielonej
    shmid = shmget(SHM_KEY, sizeof(BusState), IPC_CREAT | 0666);
    check_error(shmid, "shmget (SHM)");
    bus = (BusState*)shmat(shmid, NULL, 0);
    check_error(bus == (void*)-1 ? -1 : 0, "shmat");

    // 2. Tworzenie zestawu semaforów
    semid = semget(SEM_KEY, 3, IPC_CREAT | 0666);
    check_error(semid, "semget (SEM)");

    // 3. Inicjalizacja wartości semaforów
    union semun arg;
    arg.val = 1; // Wartość 1 = odblokowany / dostępny

    // Ustawiamy Mutex dla pamięci dzielonej
    if (semctl(semid, SEM_MUTEX, SETVAL, arg) == -1) check_error(-1, "semctl (init MUTEX)");
    
    // Ustawiamy dostępność wejść (Drzwi 1 i 2)
    if (semctl(semid, SEM_DOOR_1, SETVAL, arg) == -1) check_error(-1, "semctl (init DOOR 1)");
    if (semctl(semid, SEM_DOOR_2, SETVAL, arg) == -1) check_error(-1, "semctl (init DOOR 2)");

    // Stan początkowy
    bus->current_passengers = 0;
    bus->current_bikes = 0;
    bus->is_at_station = 1;

    std::cout << "[Dyspozytor] Semafory zainicjalizowane wartością 1." << std::endl;

    bus_pid = fork();
    if (bus_pid == 0) { run_bus(); exit(0); }

    gen_pid = fork();
    if (gen_pid == 0) { run_generator(); exit(0); }

    while (true) pause();
    return 0;
}
