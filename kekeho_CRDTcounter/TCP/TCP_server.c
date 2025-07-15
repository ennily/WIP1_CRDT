#include <stdio.h>
#include <netinet/in.h>
/*
#include <netinet/in.h>の中で以下のように定義されている。
struct sockaddr_in {
    sa_family_t    sin_family;
    in_port_t      sin_port;
    struct in_addr sin_addr;
    char           sin_zero[8];
};
*/



int main (void) {
    int sock0;
    struct sockaddr_in addr; /*strunct IPv4インターネット用ソケットアドレス構造体 変数名*/
    struct sockaddr_in client;

    /*ソケットの作成 int socket(プロトコルファミリー、ソケットのタイプ、使用するプロトコル)*/
    sock0 = socket(AF_INET, SOCK_STREAM, 0);

    /*ソケットの設定*/
    addr.sin_family = AF_INET; /*インターネットプロトコルを指定*/
    addr.sin_port = htons(12345); /*ポートを指定*/
    addr.sin_addr.s_addr = INADDR_ANY; /* */
    addr.sin_len = sizeof(addr); /*addrの使うバイト数*/

    /* TCPクライアントからの接続要求を待てる状態にする */
    listen(sock0, 5); /*最大5件までの接続要求をキューに溜めておけるようにする。*/

    /* TCPクライアントからの接続要求を受け付ける */
    len = sizeof(client);
    sock = accept(sock0, (struct sockaddr *)&client, &len); /*接続要求を受け取り、新しいソケットをsockに返す。*/

    return 0;

}