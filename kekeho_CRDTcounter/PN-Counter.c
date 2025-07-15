/*
 * PN‑Counter (Positive‑Negative Counter) CRDT implementation in C
 *
 * – Concurrent‑safe across up to MAX_REPLICAS replicas
 * – Associative, commutative, idempotent merge (eventual consistency)
 *
 * Build demo (default, includes main):
 *     gcc -std=c11 -Wall -o pn_counter crdt_pn_counter.c
 *
 * Build as library (exclude main):
 *     gcc -std=c11 -Wall -DPN_COUNTER_LIB -c crdt_pn_counter.c
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h> /* PRId64 / PRIu64 */
#include <string.h>
#include <stdlib.h>

#ifndef MAX_REPLICAS
#define MAX_REPLICAS 8
#endif

/*                    inc «plus»          dec «minus» */
typedef struct {
    uint64_t inc[MAX_REPLICAS];
    uint64_t dec[MAX_REPLICAS];
} pn_counter;

/* Initialise all components to zero */
static inline void pn_init(pn_counter *c) {
    memset(c, 0, sizeof *c);
}

/* Local replica r increments by delta (≥ 0) */
static inline void pn_increment(pn_counter *c, uint32_t r, uint64_t delta) {
    if (r < MAX_REPLICAS) c->inc[r] += delta;
}

/* Local replica r decrements by delta (≥ 0) */
static inline void pn_decrement(pn_counter *c, uint32_t r, uint64_t delta) {
    if (r < MAX_REPLICAS) c->dec[r] += delta;
}

/* Join‑merge: A := A ⊔ B */
static inline void pn_merge(pn_counter *a, const pn_counter *b) {
    for (uint32_t i = 0; i < MAX_REPLICAS; ++i) {
        if (b->inc[i] > a->inc[i]) a->inc[i] = b->inc[i];
        if (b->dec[i] > a->dec[i]) a->dec[i] = b->dec[i];
    }
}

/* Current counter value */
static inline int64_t pn_value(const pn_counter *c) {
    uint64_t sum_inc = 0, sum_dec = 0;
    for (uint32_t i = 0; i < MAX_REPLICAS; ++i) {
        sum_inc += c->inc[i];
        sum_dec += c->dec[i];
    }
    return (int64_t)(sum_inc - sum_dec);
}

#ifndef PN_COUNTER_LIB  /* demo harness — compiled unless PN_COUNTER_LIB is defined */
static void dump(const char *label, const pn_counter *c) {
    printf("%s value=%" PRId64 "\n", label, pn_value(c));
    printf("  inc: ");
    for (uint32_t i = 0; i < MAX_REPLICAS; ++i) printf("%" PRIu64 " ", c->inc[i]);
    printf("\n  dec: ");
    for (uint32_t i = 0; i < MAX_REPLICAS; ++i) printf("%" PRIu64 " ", c->dec[i]);
    printf("\n\n");
}

int main(void) {
    pn_counter a, b;
    pn_init(&a);
    pn_init(&b);

    /* Replica 0:+5  Replica 1:‑2 */
    pn_increment(&a, 0, 5);
    pn_decrement(&b, 1, 2);

    dump("A", &a);
    dump("B", &b);

    /* two‑way merge (order doesn’t matter) */
    pn_merge(&a, &b);
    pn_merge(&b, &a);

    dump("A after merge", &a);
    dump("B after merge", &b);

    return 0;
}
#endif /* PN_COUNTER_LIB */
