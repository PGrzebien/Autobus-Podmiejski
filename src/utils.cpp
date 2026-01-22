#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "constants.h"

void check_error(int result, const char* msg) {
    if (result == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}
// Funkcja P  - opuszczenie semafora
void semaphore_p(int semid, int sem_num) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = -1;      // Zmniejsz wartość o 1 (czekaj jeśli 0)
    sb.sem_flg = SEM_UNDO; // Bezpieczeństwo: zwolnij przy crashu
    check_error(semop(semid, &sb, 1), "semaphore_p");
}

// Funkcja V - podniesienie semafora
void semaphore_v(int semid, int sem_num) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = 1;       // Zwiększ wartość o 1
    sb.sem_flg = SEM_UNDO;
    check_error(semop(semid, &sb, 1), "semaphore_v");
}
// Funkcja logująca - pisze jednocześnie na ekran i do pliku.
void log_action(const char* format, ...) {
    va_list args;
    char buffer[256];
    
    // Pobranie czasu
    time_t now = time(NULL);
    char* time_str = ctime(&now);
    time_str[strlen(time_str)-1] = '\0'; // Usunięcie entera z końca daty

    // Formatowanie wiadomości
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // 1. Wypisanie na ekran
    printf("[%s] %s\n", time_str, buffer);

    // 2. Zapis do pliku
    FILE* f = fopen(LOG_FILE, "a");
    if (f) {
        fprintf(f, "[%s] %s\n", time_str, buffer);
        fclose(f);
    }
}
