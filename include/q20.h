#ifndef Q20_H
#define Q20_H
#include <stddef.h>
#include <stdint.h>
typedef struct Q20Model Q20Model;
typedef struct Q20Game Q20Game;
typedef uint32_t (*Q20Random)(void *context); /* Supplies original-helper outputs, 0..INT32_MAX. */
typedef struct {
    uint32_t id;
    int32_t score;
    uint32_t expected_answer, symbol;
} Q20Candidate;
typedef struct {
    uint32_t kind, id, number;
    int32_t outcome;
    uint32_t candidates;
    uint64_t random_calls;
    const char *strategy;
} Q20Status;
/* kinds: 0 unselected, 1 category, 2 question, 3 guess, 99 finished. */
Q20Model *q20_load(const char *path, char *error, size_t error_size);
void q20_unload(Q20Model *model);
uint32_t q20_target_count(const Q20Model *model);
uint32_t q20_question_count(const Q20Model *model);
/* Physical slots include the inaccessible final slot. Reader IDs are slot+1. */
const char *q20_text(const Q20Model *model, int question, uint32_t slot);
const char *q20_raw_text(const Q20Model *model, int question, uint32_t slot);
uint32_t q20_read(const Q20Model *model, int32_t question_id, int32_t target_id, int format);
/* Supported numeric formats: 4 (authoritative code), 2 (inference). Invalid -> UINT32_MAX. */
const char *q20_answer_label(const Q20Model *model, uint32_t kind, uint32_t answer_id);
Q20Game *q20_create(const Q20Model *model, uint32_t seed);
void q20_destroy(Q20Game *game);
void q20_reset(Q20Game *game, uint32_t seed);
void q20_set_random(Q20Game *game, Q20Random random, void *context);
int q20_advance(
    Q20Game *game); /* 0 presented, 1 ended, -1 cannot select, -2 original random endpoint fault */
int q20_submit(Q20Game *game, int32_t answer); /* Original numeric ID or ASCII symbol. */
Q20Status q20_status(const Q20Game *game);
int q20_candidate(const Q20Game *game, uint32_t index, Q20Candidate *candidate);
#endif
