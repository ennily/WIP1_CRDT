/*スレッドの作成*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>


void *hello (void *arg){
    printf ("Hello, World! from thread #%d\n", *(int *)arg); /*(int *)は、ここにはintがあるという方情報をつける*/
    return 0;
}


int main(void) {
    int i;
    pthread_t tid[2];
    int ids[2] = {1,2};

    /*スレッドの作成*/
    for (i=0; i<2; i++) {
        pthread_create(&tid[i], NULL, hello, &ids[i]);
    }

    /*スレッドの終了待機*/
    for (i=0; i<2; i++) {
        pthread_join(tid[i], NULL);
    }    
    return 0;
}