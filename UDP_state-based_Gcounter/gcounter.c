#include <stdio.h>

#define MAX_NODES 10  // ノード数の上限

// G-Counterの構造体
struct GCounter{
    int node_id;                // 自分のノード番号
    int num_nodes;              // 全体のノード数
    int state[MAX_NODES];       // 状態ベクトル
} g;

// G-Counterの初期化
void gcounter_init(GCounter *gc, int node_id, int num_nodes) {
    gc->node_id = node_id;
    gc->num_nodes = num_nodes;
    for(int i = 0; i < num_nodes; i++) {
        gc->state[i] = 0;
    }
}
// インクリメント
void gcounter_increment(GCounter *gc) {
    gc->state[gc->node_id]++;
}

// 受信した状態とマージ
void gcounter_merge(GCounter *gc, int *received_state) {
    for(int i = 0; i < gc->num_nodes; i++) {
        if(received_state[i] > gc->state[i])
            gc->state[i] = received_state[i];
    }
}

// カウンタの合計値取得
int gcounter_value(GCounter *gc) {
    int sum = 0;
    for(int i = 0; i < gc->num_nodes; i++)
        sum += gc->state[i];
    return sum;
}

// 状態を表示（デバッグ用）
void gcounter_print_state(GCounter *gc) {
    printf("State [ ");
    for(int i = 0; i < gc->num_nodes; i++) {
        printf("%d ", gc->state[i]);
    }
    printf("]  Total: %d\n", gcounter_value(gc));
}


// ---- メイン関数 ----

int main() {
    GCounter gc;
    gcounter_init(&gc, 0, 3);  // 自ノードID:0、全3ノード

    // インクリメントして状態表示
    gcounter_increment(&gc);
    printf("After increment:\n");
    gcounter_print_state(&gc);

    // 他ノードから受信した状態（仮）をマージ
    int other_state[3] = {1, 2, 1};
    gcounter_merge(&gc, other_state);
    printf("After merge:\n");
    gcounter_print_state(&gc);

    return 0;
}
