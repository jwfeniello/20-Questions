#ifndef Q20_INTERNAL_H
#define Q20_INTERNAL_H
#include "q20.h"
#define Q20_WORDS (0x6844u / 4u)
struct Q20Model {
    uint32_t nt, nq, packed_n, pool_n, text_n;
    unsigned char *file, *packed, *pool, *records, *fixed, *labels, *text;
};
struct Q20Game {
    const Q20Model *model;
    uint32_t w[Q20_WORDS];
    uint32_t rng;
    uint64_t calls;
    Q20Random random;
    void *random_context;
    const char *strategy;
    uint32_t *target_scores;
};
#define W(g, o) ((g)->w[(o) / 4u])
static inline int32_t si(uint32_t v) {
    return v <= INT32_MAX ? (int32_t)v : -1 - (int32_t)(UINT32_MAX - v);
}
static inline uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
uint32_t q20_rand(Q20Game *g);
uint32_t q20_score(Q20Game *g, uint32_t flags);
int32_t q20_divide(Q20Game *g);
int32_t q20_isolate(Q20Game *g);
int32_t q20_random_question(Q20Game *g);
void q20_feedback(Q20Game *g);
void q20_history_feedback(Q20Game *g);
const unsigned char *q20_table(const Q20Model *m, uint32_t kind, uint32_t *count);
#endif
