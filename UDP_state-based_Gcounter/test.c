#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("argc: %d\n", argc);
    for (size_t i = 0; i < argc; i++) {
        printf("argv[%zu]: %s\n", i, argv[i]);
    }
}