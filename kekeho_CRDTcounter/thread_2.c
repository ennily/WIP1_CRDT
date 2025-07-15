/*共有メモリにアクセス*/

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>   // sleep()

#define MAX_WRITES 2  // writer updates x twice (10, 20)

static int x    = 0;  // shared variable
static int done = 0;  // signals writer completion
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

void *writer(void *arg) {
    for (int i = 1; i <= MAX_WRITES; ++i) {
        sleep(2);                 // emulate work (2‑second interval)
        pthread_mutex_lock(&mtx);
        x = i * 10;               // 10, 20, ...
        printf("#Writer: Set x = %d\n", x);
        pthread_mutex_unlock(&mtx);
    }
    pthread_mutex_lock(&mtx);
    done = 1;                     // let reader know we're finished
    pthread_mutex_unlock(&mtx);
    return NULL;
}

void *reader(void *arg) {
    int last = -1;                // track last printed value
    while (1) {
        pthread_mutex_lock(&mtx);
        int val      = x;
        int finished = done;
        pthread_mutex_unlock(&mtx);

        if (val != last) {        // value changed → print
            printf("#Reader: Read x = %d\n", val);
            last = val;
        }
        if (finished && last == val) break; // exit after final value
    }
    return NULL;
}

int main(void) {
    pthread_t tid_reader, tid_writer;

    // spawn threads
    if (pthread_create(&tid_writer, NULL, writer, NULL) != 0) {
        return 1;
    }
    if (pthread_create(&tid_reader, NULL, reader, NULL) != 0) {
        return 1;
    }

    // wait for both to finish
    pthread_join(tid_writer, NULL);
    pthread_join(tid_reader, NULL);
    return 0;
}
