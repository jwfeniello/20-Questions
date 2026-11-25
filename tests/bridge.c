/* Testing only; no emulator layouts in the public gameplay API. */
#include "internal.h"
#include <string.h>
void test_import(Q20Game *g, const uint32_t *words) {
    memcpy(g->w, words, sizeof(g->w));
}
void test_export(Q20Game *g, uint32_t *words) {
    memcpy(words, g->w, sizeof(g->w));
}
uint64_t test_calls(Q20Game *g) {
    return g->calls;
}
void test_rng(Q20Game *g, uint32_t seed) {
    g->rng = seed;
    g->calls = 0;
}
uint32_t test_target_score(Q20Game *g, uint32_t slot) {
    return slot < g->model->nt ? g->target_scores[slot] : 0;
}
void test_read_all(const Q20Model *m, int format, unsigned char *out) {
    for (uint32_t t = 1; t <= m->nt; t++)
        for (uint32_t q = 1; q <= m->nq; q++)
            *out++ = (unsigned char)q20_read(m, (int32_t)q, (int32_t)t, format);
}
