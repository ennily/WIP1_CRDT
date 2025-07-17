// -*- coding: utf-8 -*-
// ------------------------------------------------------------
// UDP で通信する state‑based CRDT "G‑Counter" の最小実装
// ------------------------------------------------------------
// 使い方:
//   $ gcc -pthread -o gcounter_udp g_counter_udp.c
//   $ ./gcounter_udp <replica_id> <listen_port> <peer_host:port> [...]
//
//   例) 端末 A: ./gcounter_udp 0 9000 127.0.0.1:9001
//       端末 B: ./gcounter_udp 1 9001 127.0.0.1:9000
//
//   実行中に数値を入力するとその分インクリメントし、
//   内部状態(各レプリカのカウンタ)を UDP ブロードキャストします。
//   受信側は JSON 風 "id=value,id=value" 形式の文字列を解析しマージします。
// ------------------------------------------------------------
// ⚠️ 本実装は学習用サンプルです。エラー処理・入力検証は簡略化しています。
// ------------------------------------------------------------

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_REPLICAS 256
#define BUF_SIZE 4096
#define BROADCAST_INTERVAL_SEC 5

typedef struct {
    int replica_id;                 // 自分の ID
    unsigned long values[MAX_REPLICAS]; // 各レプリカのカウンタ値
    pthread_mutex_t lock;           // 共有データ保護
} GCounter;

// -------------------- ユーティリティ関数 --------------------
unsigned long gc_total(GCounter *gc) {
    unsigned long sum = 0;
    for (int i = 0; i < MAX_REPLICAS; ++i) {
        sum += gc->values[i];
    }
    return sum;
}

void gc_increment(GCounter *gc, unsigned long delta) {
    pthread_mutex_lock(&gc->lock);
    gc->values[gc->replica_id] += delta;
    pthread_mutex_unlock(&gc->lock);
}

// incoming="id1=value1,id2=value2,..."
void gc_merge_str(GCounter *gc, const char *incoming) {
    pthread_mutex_lock(&gc->lock);
    char tmp[BUF_SIZE];
    strncpy(tmp, incoming, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *token = strtok(tmp, ",");
    while (token) {
        int id;
        unsigned long val;
        if (sscanf(token, "%d=%lu", &id, &val) == 2 && id >= 0 && id < MAX_REPLICAS) {
            if (val > gc->values[id]) {
                gc->values[id] = val; // 要素毎の max を取る
            }
        }
        token = strtok(NULL, ",");
    }
    pthread_mutex_unlock(&gc->lock);
}

// 自身の状態を文字列化 → "id=val,id=val,..."
size_t gc_serialize(GCounter *gc, char *out, size_t out_size) {
    size_t used = 0;
    pthread_mutex_lock(&gc->lock);
    for (int i = 0; i < MAX_REPLICAS; ++i) {
        if (gc->values[i] == 0) continue; // 0 のエントリは送らない
        int n = snprintf(out + used, out_size - used, "%d=%lu,", i, gc->values[i]);
        if (n < 0 || used + (size_t)n >= out_size) {
            break; // 余裕なし
        }
        used += (size_t)n;
    }
    pthread_mutex_unlock(&gc->lock);

    if (used > 0 && out[used - 1] == ',') {
        out[used - 1] = '\0'; // 末尾のカンマを削除
        --used;
    }
    return used;
}

// -------------------- 通信スレッド --------------------

typedef struct {
    int sockfd;
    GCounter *gc;
} ReceiverArgs;

void *receiver_thread(void *arg) {
    ReceiverArgs *args = (ReceiverArgs *)arg;
    char buf[BUF_SIZE];
    struct sockaddr_in src;
    socklen_t srclen = sizeof(src);

    while (1) {
        ssize_t len = recvfrom(args->sockfd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&src, &srclen);
        if (len <= 0) continue;
        buf[len] = '\0';
        gc_merge_str(args->gc, buf);
        printf("[Recv] %s\n", buf);
        printf("  → total=%lu\n", gc_total(args->gc));
    }
    return NULL;
}

// -------------------- メイン --------------------

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <replica_id> <listen_port> <peer_host:port> [...]\n", argv[0]);
        return 1;
    }

    int replica_id = atoi(argv[1]);
    if (replica_id < 0 || replica_id >= MAX_REPLICAS) {
        fprintf(stderr, "replica_id must be between 0 and %d\n", MAX_REPLICAS - 1);
        return 1;
    }

    int listen_port = atoi(argv[2]);

    // --- ソケット作成 & バインド (UDP) ---
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)listen_port);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    // --- G‑Counter 初期化 ---
    GCounter gc = {0};
    gc.replica_id = replica_id;
    memset(gc.values, 0, sizeof(gc.values));
    pthread_mutex_init(&gc.lock, NULL);

    // --- Peer アドレス一覧を保存 ---
    int peer_count = argc - 3;
    struct sockaddr_in *peers = calloc(peer_count, sizeof(struct sockaddr_in));
    if (!peers) {
        perror("calloc");
        close(sockfd);
        return 1;
    }

    for (int i = 0; i < peer_count; ++i) {
        char *hostport = argv[i + 3];
        char *colon = strchr(hostport, ':');
        if (!colon) {
            fprintf(stderr, "Invalid peer format: %s\n", hostport);
            return 1;
        }
        *colon = '\0';
        const char *host = hostport;
        int port = atoi(colon + 1);

        peers[i].sin_family = AF_INET;
        peers[i].sin_port = htons((unsigned short)port);
        if (inet_pton(AF_INET, host, &peers[i].sin_addr) <= 0) {
            fprintf(stderr, "Invalid IP: %s\n", host);
            return 1;
        }
    }

    // --- 受信スレッド起動 ---
    pthread_t recv_tid;
    ReceiverArgs rargs = {sockfd, &gc};
    pthread_create(&recv_tid, NULL, receiver_thread, &rargs);

    // --- メインループ: 入力受付 & 定期ブロードキャスト ---
    char line[128];
    time_t last_broadcast = 0;

    while (1) {
        // 標準入力があれば処理
        if (fgets(line, sizeof(line), stdin)) {
            unsigned long delta = strtoul(line, NULL, 10);
            if (delta > 0) {
                gc_increment(&gc, delta);
                printf("[Local] +%lu (total=%lu)\n", delta, gc_total(&gc));
            }
        }

        // 一定間隔で状態をブロードキャスト
        time_t now = time(NULL);
        if (now - last_broadcast >= BROADCAST_INTERVAL_SEC) {
            char msg[BUF_SIZE];
            gc_serialize(&gc, msg, sizeof(msg));
            for (int i = 0; i < peer_count; ++i) {
                sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&peers[i], sizeof(peers[i]));
            }
            last_broadcast = now;
        }

        usleep(100000); // 0.1 秒スリープ
    }

    // never reached
    free(peers);
    close(sockfd);
    return 0;
}
