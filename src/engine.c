#include "internal.h"
#include <stdlib.h>
#include <string.h>
uint32_t q20_rand(Q20Game *g) {
    g->calls++;
    if (g->random)
        return g->random(g->random_context);
    g->rng = g->rng * UINT32_C(1103515245) + 12345u;
    return g->rng & 0x7fffffffu;
}
Q20Game *q20_create(const Q20Model *m, uint32_t seed) {
    Q20Game *g = calloc(1, sizeof(*g));
    if (!g)
        return NULL;
    g->target_scores = calloc(m->nt, sizeof(uint32_t));
    if (!g->target_scores) {
        free(g);
        return NULL;
    }
    g->model = m;
    W(g, 0x679c) = 7;
    W(g, 0x67a0) = 7;
    W(g, 0x67a4) = 27;
    q20_reset(g, seed);
    return g;
}
void q20_destroy(Q20Game *g) {
    if (g) {
        free(g->target_scores);
        free(g);
    }
}
void q20_reset(Q20Game *g, uint32_t seed) {
    for (unsigned o = 8; o <= 0x24; o += 4)
        W(g, o) = 0;
    W(g, 0x10) = 1;
    W(g, 0x67b8) = W(g, 0x67b4) = 0;
    if (!W(g, 0x6798))
        W(g, 0x6798) = 30;
    memset(g->w + 0x28 / 4, 0, 0x280);
    memset(g->w + 0x2a8 / 4, 0, 0x2490);
    g->rng = seed;
    g->calls = 0;
    g->strategy = "category";
}
void q20_set_random(Q20Game *g, Q20Random fn, void *ctx) {
    g->random = fn;
    g->random_context = ctx;
}
Q20Status q20_status(const Q20Game *g) {
    Q20Status s = {W(g, 8), W(g, 12), W(g, 16), si(W(g, 24)), W(g, 32), g->calls, g->strategy};
    return s;
}
int q20_candidate(const Q20Game *g, uint32_t i, Q20Candidate *c) {
    if (i >= W(g, 32) || i >= 20)
        return -1;
    unsigned o = 0x28 + i * 16;
    c->id = W(g, o);
    c->score = si(W(g, o + 4));
    c->expected_answer = W(g, o + 8);
    c->symbol = W(g, o + 12);
    return 0;
}
static void add_answer(const Q20Model *m, uint32_t answer, uint32_t code, uint32_t *reward,
                       uint32_t *penalty) {
    const unsigned char *r = m->fixed + 28 * answer;
    uint32_t scale = (code & 2) ? 2 : 1;
    if (((le32(r + 8) ^ code) & 1) == 0)
        *reward += le32(r + 12) * scale;
    else
        *penalty += le32(r + 16) * scale;
}
/* PSP 0xEB94: stable top-20 insertion, raw per-target scores, then shortlist cutoff. */
uint32_t q20_score(Q20Game *g, uint32_t flags) {
    const Q20Model *m = g->model;
    uint32_t old = W(g, 0x1c), count = 0, best = 0;
    memcpy(g->w + 0x168 / 4, g->w + 0x28 / 4, 0x140);
    W(g, 0x24) = W(g, 0x20);
    W(g, 0x20) = W(g, 0x1c) = W(g, 0x67b8) = W(g, 0x67b4) = W(g, 0x67a8) = 0;
    for (uint32_t t = 1; t < m->nt; t++) {
        g->target_scores[t - 1] = 0;
        const char *text = q20_text(m, 0, t - 1);
        if (!text || *text == '-')
            continue;
        uint32_t reward = t == old ? (si(W(g, 0x10)) > 10 ? 10 : W(g, 0x10)) : 0, penalty = 0;
        int rejected = 0;
        for (uint32_t h = 0; h < W(g, 0x14); h++) {
            uint32_t o = 0x2a8 + 36 * h, type = W(g, o + 8), answer = W(g, o + 12);
            if (type == 3) {
                if (W(g, o + 4) == t && (!(flags & 2) || answer == 2)) {
                    rejected = 1;
                    break;
                }
            } else if (answer) {
                if (type == 1) {
                    for (uint32_t q = 1; q <= 4; q++)
                        add_answer(m, q == answer ? 4 : 3, q20_read(m, (int32_t)q, (int32_t)t, 2),
                                   &reward, &penalty);
                } else if (answer <= 11)
                    add_answer(m, answer, q20_read(m, si(W(g, o + 4)), (int32_t)t, 2), &reward,
                               &penalty);
            }
        }
        uint32_t score = reward - penalty + (q20_rand(g) & W(g, 0x679c));
        if (t == W(g, 0x67b0))
            W(g, 0x67b8) = score;
        if (rejected || si(penalty) > si(reward))
            continue;
        if (si(W(g, 0x10) << 2) <= si(score))
            W(g, 0x67a8)++;
        g->target_scores[t - 1] = score;
        uint32_t pos = 0;
        while (pos < count && pos < 20 && si(W(g, 0x2c + 16 * pos)) >= si(score))
            pos++;
        if (pos < 20) {
            if (count < 20)
                count++;
            if (pos + 1 < count)
                memmove(g->w + (0x28 + 16 * (pos + 1)) / 4, g->w + (0x28 + 16 * pos) / 4,
                        (count - pos - 1) * 16);
            W(g, 0x28 + 16 * pos) = t;
            W(g, 0x2c + 16 * pos) = score;
            W(g, 0x34 + 16 * pos) = 0x2d;
            if (si(score) >= 0 && si(best) < si(score)) {
                best = score;
                W(g, 0x1c) = t;
            }
        }
    }
    W(g, 0x67ac) = best;
    W(g, 0x20) = count;
    if (!(flags & 4)) {
        uint32_t third = (uint32_t)(si(best) / 3),
                 cut = best - third - (W(g, 0x10) == 19 ? 48u : 32u);
        if (si(cut) < 0)
            cut = 0;
        uint32_t kept = 0;
        for (uint32_t i = 0; i < count; i++) {
            W(g, 0x2c + 16 * i) -= third;
            if (si(cut) < si(W(g, 0x2c + 16 * i)))
                kept++;
        }
        W(g, 0x67b8) -= third;
        if (si(W(g, 0x67b8)) < 0)
            W(g, 0x67b8) = 0;
        count = kept;
        W(g, 0x20) = count;
        for (uint32_t i = count; i < 20; i++)
            W(g, 0x28 + 16 * i) = W(g, 0x2c + 16 * i) = 0;
    }
    return count;
}
static int asked(const Q20Game *g, uint32_t q) {
    for (uint32_t i = 0; i < W(g, 0x14); i++) {
        uint32_t o = 0x2a8 + i * 36;
        if (W(g, o + 8) == 2 && W(g, o + 4) == q)
            return 1;
    }
    return 0;
}
/* PSP 0xE474. Strength multiplies candidate scores by THREE here. */
int32_t q20_divide(Q20Game *g) {
    if (si(W(g, 0x20)) < 1)
        return 0;
    int32_t best = -1000, result = -1;
    for (uint32_t q = 5; q < g->model->nq; q++) {
        if (asked(g, q))
            continue;
        uint32_t positive = 0, negative = 0;
        for (uint32_t i = 0; i < W(g, 0x20); i++) {
            uint32_t c = q20_read(g->model, (int32_t)q, si(W(g, 0x28 + i * 16)), 2),
                     v = W(g, 0x2c + i * 16);
            if (c & 2)
                v *= 3;
            if (c & 1)
                positive += v;
            else
                negative += v;
        }
        uint32_t score =
            si(negative) < si(positive) ? negative * 2 - positive : positive * 2 - negative;
        score += q20_rand(g) & W(g, 0x67a0);
        if (best < si(score)) {
            best = si(score);
            result = (int32_t)q;
        }
    }
    return result;
}
/* PSP 0xE6D0. Rivals contribute 1/2; a strong leader doubles the margin. */
int32_t q20_isolate(Q20Game *g) {
    if (si(W(g, 0x20)) < 1)
        return 0;
    int32_t best = -1000, result = -1;
    for (uint32_t q = 5; q < g->model->nq; q++) {
        if (asked(g, q))
            continue;
        uint32_t positive = 0, negative = 0;
        for (uint32_t i = W(g, 0x20) - 1; i > 0; i--) {
            uint32_t c = q20_read(g->model, (int32_t)q, si(W(g, 0x28 + 16 * i)), 2),
                     v = (c & 2) ? 2u : 1u;
            if (c & 1)
                positive += v;
            else
                negative += v;
        }
        uint32_t leader = q20_read(g->model, (int32_t)q, si(W(g, 0x28)), 2),
                 score = (leader & 1) ? negative - positive : positive - negative;
        if (leader & 2)
            score <<= 1;
        score += q20_rand(g) & W(g, 0x679c);
        if (best < si(score)) {
            best = si(score);
            result = (int32_t)q;
        }
    }
    return best < 0 ? -1 : result;
}
int32_t q20_random_question(Q20Game *g) {
    for (unsigned tries = 0; tries < 20; tries++) {
        uint32_t q = (uint32_t)(si(q20_rand(g)) % (int32_t)(g->model->nq - 4)) + 5;
        /* Original accessor rejects endpoint, then caller dereferences NULL.
           Standalone preserves failure rather than dereferencing host NULL. */
        if (q >= g->model->nq) {
            g->strategy = "random-endpoint-fault";
            return -2;
        }
        if (!asked(g, q))
            return (int32_t)q;
    }
    return 0;
}
static uint32_t predicted(Q20Game *g, uint32_t q, uint32_t target, uint32_t *symbol) {
    static const unsigned char ids[4] = {3, 4, 1, 2}, symbols[4] = {'D', 'P', 'N', 'Y'},
                               cats[5] = {'U', 'A', 'V', 'M', 'O'};
    if (q) {
        uint32_t c = q20_read(g->model, si(q), si(target), 2) & 3;
        *symbol = symbols[c];
        return ids[c];
    }
    uint32_t best = 0, result = 0;
    *symbol = 'U';
    for (uint32_t c = 1; c <= 4; c++) {
        uint32_t v = q20_read(g->model, (int32_t)c, si(target), 2);
        if ((v & 1) && si(best) < si(v)) {
            best = v;
            result = c;
            *symbol = cats[c];
        }
    }
    return result;
}
void q20_feedback(Q20Game *g) {
    for (uint32_t i = 0; i < 8; i++)
        W(g, 0x681c + 4 * i) = W(g, 0x67fc + 4 * i) = W(g, 0x67dc + 4 * i) = 0;
    if (!W(g, 0x20))
        return;
    uint32_t q = 0;
    if (W(g, 8) == 2)
        q = W(g, 12);
    else if (W(g, 8) != 1)
        return;
    for (uint32_t i = 0; i < W(g, 0x20); i++) {
        uint32_t sym, id = predicted(g, q, W(g, 0x28 + 16 * i), &sym);
        W(g, 0x30 + 16 * i) = id;
        W(g, 0x34 + 16 * i) = sym;
    }
    if (W(g, 0x67b0)) {
        uint32_t sym;
        predicted(g, q, W(g, 0x67b0), &sym);
        W(g, 0x67b4) = sym;
    }
    int32_t top = si(W(g, 0x2c));
    if (top < 1)
        top = 10000;
    for (uint32_t i = 0; i < W(g, 0x20) && i < 8; i++) {
        W(g, 0x681c + 4 * i) = (uint32_t)(si(W(g, 0x2c + 16 * i) * 100u) / top);
        for (uint32_t j = 0; j < W(g, 0x24); j++)
            if (W(g, 0x28 + 16 * i) == W(g, 0x168 + 16 * j)) {
                W(g, 0x67dc + 4 * i) = j + 1;
                break;
            }
    }
    for (uint32_t i = 0; i < W(g, 0x24) && i < 8; i++)
        for (uint32_t j = 0; j < W(g, 0x20); j++)
            if (W(g, 0x168 + 16 * i) == W(g, 0x28 + 16 * j)) {
                W(g, 0x67fc + 4 * i) = j + 1;
                break;
            }
}
void q20_history_feedback(Q20Game *g) {
    static const uint32_t answer_ids[4] = {3, 4, 1, 2};
    for (unsigned i = 0; i < 6; i++)
        W(g, 0x67c4 + 4 * i) = 0;
    if (!W(g, 0x20))
        return;
    for (uint32_t h = 0; h < W(g, 0x14); h++) {
        uint32_t o = 0x2a8 + 36 * h;
        if (W(g, o + 8) == 2) {
            uint32_t positive = 0, negative = 0;
            for (uint32_t i = 0; i < W(g, 0x20); i++) {
                uint32_t c = q20_read(g->model, si(W(g, o + 4)), si(W(g, 0x28 + 16 * i)), 2);
                if (!i)
                    W(g, o + 28) = answer_ids[c & 3];
                uint32_t strength = (c & 2) ? 2 : 1;
                if (c & 1)
                    positive += strength;
                else
                    negative += strength;
            }
            int32_t agreement = si(positive - negative) / si(W(g, 0x20));
            uint32_t n;
            const unsigned char *table = q20_table(g->model, 2, &n);
            for (uint32_t j = 0; j < n - 1; j++)
                if (le32(table + j * 28) == W(g, o + 16) ||
                    le32(table + j * 28 + 4) == W(g, o + 16)) {
                    if (!le32(table + j * 28 + 8))
                        agreement = si(0u - (uint32_t)agreement);
                    break;
                }
            W(g, o + 24) = (uint32_t)agreement;
            if (agreement > -3 && agreement < 3)
                W(g, 0x67c4 + 4 * (2 - agreement))++;
            else
                W(g, 0x67d8)++;
        } else if (W(g, o + 8) == 1) {
            uint32_t totals[4] = {0};
            for (uint32_t i = 0; i < W(g, 0x20); i++) {
                uint32_t best = 0, cat = 0;
                for (uint32_t c = 1; c <= 4; c++) {
                    uint32_t v = q20_read(g->model, (int32_t)c, si(W(g, 0x28 + 16 * i)), 2),
                             s = (v & 2) ? 2 : 1;
                    totals[c - 1] += (v & 1) ? s : 0u - s;
                    if ((v & 1) && si(best) < si(v)) {
                        best = v;
                        cat = c;
                    }
                }
                if (!i)
                    W(g, o + 28) = cat;
            }
            uint32_t best = 0, cat = 0;
            for (uint32_t c = 1; c <= 4; c++)
                if (si(best) <= si(totals[c - 1])) {
                    cat = c;
                    best = totals[c - 1];
                }
            W(g, o + 24) = cat;
        }
    }
}
/* PSP 0xF7EC. Random-question return is deliberately discarded. */
int q20_advance(Q20Game *g) {
    if (W(g, 8))
        return 0;
    uint32_t number = W(g, 0x10);
    if (si(W(g, 0x6798)) < si(number) && !W(g, 0x18))
        W(g, 0x18) = UINT32_MAX;
    if (W(g, 0x18)) {
        W(g, 8) = 99;
        W(g, 12) = number;
        W(g, 0x6790)++;
        if (si(W(g, 0x18)) > 0 && si(number) < 22)
            W(g, 0x6794)++;
        g->strategy = "finished";
        return 1;
    }
    int has_category = 0;
    for (uint32_t h = 0; h < W(g, 0x14); h++)
        if (W(g, 0x2a8 + 36 * h + 8) == 1) {
            has_category = 1;
            break;
        }
    if (!has_category) {
        W(g, 8) = 1;
        W(g, 12) = 0;
        g->strategy = "category";
        q20_feedback(g);
        return 0;
    }
    uint32_t count = q20_score(g, 0);
    q20_history_feedback(g);
    int lead = si(number) > 6 && count > 1 && si(W(g, 0x3c) + W(g, 0x67a4)) <= si(W(g, 0x2c));
    if (lead || count == 1 || number == 20 || si(number) >= 28) {
        W(g, 8) = 3;
        W(g, 12) = W(g, 0x1c);
        g->strategy = "guess";
    }
    if (!W(g, 8)) {
        int32_t q = 0;
        if (W(g, 0x28) == W(g, 0x168)) {
            q = q20_isolate(g);
            g->strategy = "isolate";
        }
        if (q < 1) {
            q = q20_divide(g);
            g->strategy = "divide";
            if (q < 1) {
                int32_t ignored = q20_random_question(g);
                if (ignored == -2)
                    return -2;
                g->strategy = "random-return-discarded";
            }
        }
        if (q > 0) {
            W(g, 8) = 2;
            W(g, 12) = (uint32_t)q;
            q20_feedback(g);
        }
    }
    if (!W(g, 8)) {
        if (count) {
            W(g, 8) = 3;
            W(g, 12) = W(g, 0x1c);
            g->strategy = "fallback-guess";
        } else
            return -1;
    }
    return 0;
}
/* PSP 0xFA80 (and 0xEAA8 for undo). Unknown does not advance the counter. */
int q20_submit(Q20Game *g, int32_t answer) {
    uint32_t nh = W(g, 0x14);
    if (answer < 0 || answer == 'X') {
        int32_t index = answer < 0 ? si(nh + (uint32_t)answer) : answer;
        if (index < 0 || (uint32_t)index >= nh)
            return -1;
        uint32_t o = 0x2a8 + 36 * (uint32_t)index;
        if (W(g, o + 12))
            W(g, 0x10)--;
        W(g, 8) = W(g, o + 8);
        W(g, 12) = W(g, o + 4);
        memmove(g->w + o / 4, g->w + (o + 36) / 4, (259u - (uint32_t)index) * 36);
        W(g, 0x14)--;
        return 0;
    }
    uint32_t kind = W(g, 8);
    if (kind < 1 || kind > 3 || (kind != 1 && si(W(g, 12)) < 1))
        return -1;
    unsigned unknown = 0;
    for (uint32_t i = 0; i < 260; i++) {
        uint32_t o = 0x2a8 + 36 * i;
        if ((W(g, o + 8) == 1 || W(g, o + 8) == 2) && !W(g, o + 12))
            unknown++;
    }
    if (answer == 0 || answer == 'U')
        unknown++;
    if (nh >= 260 || unknown > 16) {
        for (uint32_t i = 0; i < 260; i++) {
            uint32_t o = 0x2a8 + 36 * i;
            if ((W(g, o + 8) == 1 || W(g, o + 8) == 2) && !W(g, o + 12)) {
                memmove(g->w + o / 4, g->w + (o + 36) / 4, (259 - i) * 36);
                W(g, 0x14)--;
                nh--;
                break;
            }
        }
    }
    if (nh >= 260)
        return -1;
    uint32_t n;
    const unsigned char *table = q20_table(g->model, kind, &n), *row = NULL;
    for (uint32_t i = 0; i < n; i++) {
        const unsigned char *r = table + 28 * i;
        uint32_t ch = le32(r);
        if (!ch)
            break;
        int32_t symbol = ch == 255 ? -1 : (int32_t)ch;
        if (symbol == answer || le32(r + 4) == (uint32_t)answer) {
            row = r;
            break;
        }
    }
    if (!row)
        return -1;
    uint32_t o = 0x2a8 + 36 * nh, id = le32(row + 4),
             symbol = le32(row) == 255 ? UINT32_MAX : le32(row);
    W(g, o + 8) = kind;
    W(g, o + 16) = symbol;
    W(g, o + 4) = W(g, 12);
    W(g, o + 12) = id;
    W(g, o + 20) = W(g, 0x1c);
    if (id) {
        W(g, o) = W(g, 0x10);
        W(g, 0x10)++;
    }
    W(g, 8) = 0;
    W(g, 0x14)++;
    W(g, 12) = 0;
    if (kind == 3 && id == 2)
        W(g, 0x18) = W(g, o + 4);
    return 0;
}
