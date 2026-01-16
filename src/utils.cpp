#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

void check_error(int result, const char* msg) {
    if (result == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}
