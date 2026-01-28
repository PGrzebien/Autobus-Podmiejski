#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "../include/common.h"
#include "../include/utils.h"

// Zmienne globalne
int shmid;
int semid;
BusState* bus = nullptr;

// Funkcja inicjalizująca zasoby (podłączenie do systemu)
void init_resources() {
    // 1. Pobranie ID pamięci dzielonej
    shmid = shmget(SHM_KEY, sizeof(BusState), 0666);
    check_error(shmid, "[Kierowca] Błąd shmget (uruchom najpierw main!)");

    // 2. Podłączenie do pamięci
    bus = (BusState*)shmat(shmid, NULL, 0);
    check_error(bus == (void*)-1 ? -1 : 0, "[Kierowca] Błąd shmat");

    // 3. Pobranie ID semaforów
    semid = semget(SEM_KEY, 3, 0666);
    check_error(semid, "[Kierowca] Błąd semget");

    log_action("[Kierowca] Zameldowałem się w systemie (PID: %d).", getpid());
}

int main() {
    // 1. Podłącz zasoby
    init_resources();

    log_action("[Kierowca] Jestem gotowy. Czekam na instrukcje...");

    // Sprzątanie przy wyjściu
    shmdt(bus);
    return 0;
}
