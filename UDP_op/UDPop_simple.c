/* ./UDPop_simple <replica_id> <listen_port> <peer_host:port> */

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
    unsigned long value; // 自レプリカのカウンタ値
    pthread_mutex_t lock;           // 共有データ保護
    unsigned long msg;
} GCounter;


/*gc_increment関数*/
void gc_increment(GCounter *gc, unsigned long delta) {
    pthread_mutex_lock(&gc->lock);
    gc->value += delta;
    pthread_mutex_unlock(&gc->lock);
}


/*merge処理*/
void gc_merge_str(GCounter *gc, unsigned long *incoming) {
    pthread_mutex_lock(&gc->lock);
    gc->value += *incoming;
    pthread_mutex_unlock(&gc->lock);
}



//受信スレッド起動時に起動する関数

typedef struct {
    int sockfd;
    GCounter *gc;
} ReceiverArgs;

void *receiver_thread(void *arg) {
    ReceiverArgs *args = (ReceiverArgs *)arg;
    char buf[BUF_SIZE] = {0};
    unsigned long ulbuf; /*受け取ったmsgをbufに突っ込む*/

    while (1) {
        ssize_t len = recvfrom(args->sockfd, buf, sizeof(buf), 0, NULL, NULL);
        if (len <= 0) continue; 
        // TODO: bufを数字に変換
        ulbuf = atoi(buf);
        gc_merge_str(args->gc, &ulbuf); /*gc_merge_str関数でマージ処理を行う*/
        printf("[Recv] %lu\n", ulbuf);
        printf("  → total=%lu\n", args->gc->value);
    }

    return NULL;
}






int main(int argc, char *argv[]) { /*arg[]はプログラム実行時に入力する文字列の配列*/

    int replica_id = atoi(argv[1]);
    int listen_port = atoi(argv[2]);

    /*入力が不十分だとエラー表示*/
    if (argc < 4) {
        fprintf(stderr, "正しく入力してください。Usage: %s <replica_id> <listen_port> <peer_host:port> [...]\n", argv[0]);
        return 1;
    } /*peer_host2個で、レプリカIDなしとかだとエラー表示できなくない？*/

    /*レプリカIDの値が不適切だとエラー表示*/
    if (replica_id < 0 || replica_id >= MAX_REPLICAS) {
        fprintf(stderr, "replica_id must be between 0 and %d\n", MAX_REPLICAS - 1);
        return 1;
    }


    /*ソケット作成＆バインド*/

    /*ソケットを生成*/
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0); /*変数sockfdを定義*/
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    /*ソケットの設定を作成*/
    struct sockaddr_in addr = {0}; /*全ての変数を0で初期化*/ /*struct sockaddrどこやねん*/
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; /*自分のIPアドレスを決める*/
    addr.sin_port = htons((unsigned short)listen_port); /*自分のポート番号を決める*/

    /*ソケットに設定を適用(bind)*/
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }


    // --- G‑Counter 初期化 ---
    GCounter gc = {0}; /*構造体の全部の変数を０で初期化*/
    gc.replica_id = replica_id;
    pthread_mutex_init(&gc.lock, NULL);


    // --- Peer アドレス一覧を保存 ---
    int peer_count = argc - 3; /*peerの個数がargc-3個*/
    struct sockaddr_in *peers = calloc(peer_count, sizeof(struct sockaddr_in)); /*peer_count個ぶんのstruct sockaddr_in構造体をゼロ初期化して確保し、その先頭アドレスをpeersに格納する*/
    if (!peers) { /*callocが失敗したら*/
        perror("calloc");
        close(sockfd);
        return 1;
    }
    for (int i = 0; i < peer_count; ++i) {
        char *hostport = argv[i + 3]; /* 全ての<peer_host:port>を*hostportに格納 */
        char *colon = strchr(hostport, ':'); /*hostport内の:のアドレスをcolonに格納*/
        if (!colon) { /*colonにNULL値が入ってきたら（エラー）*/
            fprintf(stderr, "Invalid peer format: %s\n", hostport);
            return 1;
        }
        *colon = '\0'; /*colonに格納されてる:のアドレスを終端文字に置き換える*/
        const char *host = hostport; /*hostportの先頭アドレスをhostに格納*/ /*hostport[8000,127.0.0.1,8001,...]ってなってるなら配列だから最初の8のアドレスだけ格納されるはずでは？*/
        int port = atoi(colon + 1); /*<peer_host:port>のportの先頭アドレスをportに格納*/

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
    pthread_create(&recv_tid, NULL, receiver_thread, &rargs);/*スレッド生成してreceiver_thread(rargs)起動、スレッドIDはrecv_tidに保存*/



    /*メインスレッド*/
    char line[128];
    time_t last_broadcast = 0;

    while (1) { /*無限ループ*/
        // 標準入力があれば自プロセス内で処理
        if (fgets(line, sizeof(line), stdin)) { /*標準入力で得た値をlineに格納*/
            unsigned long delta = strtoul(line, NULL, 10); /*文字列→符号なし長整数でdeltaに格納。*/
            if (delta > 0) {
                gc_increment(&gc, delta); /*gc_increment関数呼び出して自分のvalueにdelta付け足す*/
                printf("[Local] +%lu (total=%lu)\n", delta, gc.value);
            }
            gc.msg += delta; /*msgにdeltaを追加*/
        }

        // 一定間隔で状態をブロードキャスト
        time_t now = time(NULL); /*現時間をnowに格納*/
        if (now - last_broadcast >= BROADCAST_INTERVAL_SEC) { /*最後にブロードキャストしてからINTERVAL_SECがすぎたら、*/
            char msg[BUF_SIZE];
            for (int i = 0; i < peer_count; ++i) {
                snprintf(msg, BUF_SIZE, "%lu", gc.msg);
                sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&peers[i], sizeof(peers[i])); /*sockfd：自分のソケット。msgを送信*/
            }
            gc.msg = 0; /*msgを初期化*/
            last_broadcast = now;
        }

    }
    // never reached
    free(peers);
    close(sockfd);
    return 0;
}