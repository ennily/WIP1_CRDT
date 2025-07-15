/*
 * Interactive state‑based G‑Counter (CvRDT) — multi‑process demo only
 *
 * Build:
 *   gcc -std=c11 -Wall -Wextra -pthread gcounter.c -o demo
 * Run:
 *   ./demo
 *   → すべてのレプリカについてインクリメント回数を入力すると集計結果が表示される。
 */

#define _POSIX_C_SOURCE 200112L
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#ifndef N_REPLICAS
#define N_REPLICAS 3   /* フォークするレプリカ数（コンパイル時に -DN_REPLICAS=5 などで変更可） */
#endif

/* ------------------------------------------------------------------
 * データ構造（固定長ベクタ）
 * ------------------------------------------------------------------*/

typedef struct {
    int id;                                         /* このレプリカの ID */
    atomic_uint_least64_t state[N_REPLICAS];        /* 各レプリカの累積値 */
} GCounter;

/* ---------------- CRDT 基本操作 ---------------- */
static void gcounter_inc(GCounter *g) {
    atomic_fetch_add_explicit(&g->state[g->id], 1, memory_order_relaxed);
}

static void gcounter_snapshot(const GCounter *g, uint64_t out[N_REPLICAS]) {
    for (size_t i = 0; i < N_REPLICAS; ++i)
        out[i] = atomic_load_explicit(&g->state[i], memory_order_relaxed);
}

static void gcounter_merge_raw(GCounter *g, const uint64_t snap[N_REPLICAS]) {
    for (size_t i = 0; i < N_REPLICAS; ++i) {
        uint64_t local = atomic_load_explicit(&g->state[i], memory_order_relaxed);
        if (snap[i] > local)
            atomic_store_explicit(&g->state[i], snap[i], memory_order_relaxed);
    }
}

static uint64_t gcounter_value(const GCounter *g) {
    uint64_t sum = 0;
    for (size_t i = 0; i < N_REPLICAS; ++i)
        sum += atomic_load_explicit(&g->state[i], memory_order_relaxed);
    return sum;
}

/* ------------------------------------------------------------------
 * マルチプロセスデモ
 * 1. 親プロセスが各レプリカのインクリメント回数を標準入力から取得。
 * 2. fork() で N_REPLICAS 個の子プロセスを生成。
 * 3. 各子が自分の回数だけ gcounter_inc し、スナップショットをパイプ送信。
 * 4. 親がすべてのスナップショットをマージし、合計値を表示。
 * ------------------------------------------------------------------*/
int main(void) {
    int incr[N_REPLICAS] = {0};
    printf("=== G‑Counter interactive demo (%d replicas) ===\n", N_REPLICAS);
    for (int i = 0; i < N_REPLICAS; ++i) {
        printf("Replica %d — increments? ", i);
        fflush(stdout);
        if (scanf("%d", &incr[i]) != 1 || incr[i] < 0) {
            fprintf(stderr, "Invalid input.\n");
            return 1;
        }
    }

    int pipes[N_REPLICAS][2];
    pid_t pids[N_REPLICAS] = {0};

    for (int i = 0; i < N_REPLICAS; ++i)
        if (pipe(pipes[i])) { perror("pipe"); return 1; }

    for (int i = 0; i < N_REPLICAS; ++i) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid == 0) {            /* ---- child ---- */
            close(pipes[i][0]);    /* 読み口閉じる */

            GCounter g = {.id = i};
            for (int k = 0; k < incr[i]; ++k)
                gcounter_inc(&g);

            uint64_t snap[N_REPLICAS];
            gcounter_snapshot(&g, snap);
            write(pipes[i][1], snap, sizeof snap);
            close(pipes[i][1]);
            printf("[child %d] local total = %llu\n", i, (unsigned long long)gcounter_value(&g));
            _Exit(0);
        }
        /* ---- parent ---- */
        pids[i] = pid;
        close(pipes[i][1]);        /* 書き口閉じる */
    }

    GCounter agg = {.id = 0};
    for (int i = 0; i < N_REPLICAS; ++i) {
        uint64_t snap[N_REPLICAS];
        read(pipes[i][0], snap, sizeof snap);
        close(pipes[i][0]);
        gcounter_merge_raw(&agg, snap);
    }

    for (int i = 0; i < N_REPLICAS; ++i) waitpid(pids[i], NULL, 0);

    uint64_t total = gcounter_value(&agg);
    printf("[parent] aggregated total = %llu\n", (unsigned long long)total);
    return 0;
}
