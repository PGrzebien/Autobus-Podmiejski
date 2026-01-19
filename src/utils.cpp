#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/sem.h>

void check_error(int result, const char* msg) {
    if (result == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}
// Funkcja P  - opuszczenie semafora (zajęcie zasobu)
void semaphore_p(int semid, int sem_num) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = -1;      // Zmniejsz wartość o 1 (czekaj jeśli 0)
    sb.sem_flg = SEM_UNDO; // Bezpieczeństwo: zwolnij przy crashu
    check_error(semop(semid, &sb, 1), "semaphore_p");
}

// Funkcja V - podniesienie semafora (zwolnienie zasobu)
void semaphore_v(int semid, int sem_num) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = 1;       // Zwiększ wartość o 1
    sb.sem_flg = SEM_UNDO;
    check_error(semop(semid, &sb, 1), "semaphore_v");
}
