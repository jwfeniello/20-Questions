#include "q20.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
static void usage(void) {
    puts("20Q offline\nUsage: twentyq [--bank en|de|es|fr|it|jp] [--data DIR] [--model FILE]\n"
         "               [--seed UINT32] [--trace]\n"
         "Commands during play: restart, quit, candidates, undo.\n"
         "Answer with a displayed number, label, or original letter (Y, N, U, etc.).");
}
static int equal(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++))
            return 0;
    }
    return *a == *b;
}
static void candidates(Q20Game *g, Q20Model *m) {
    Q20Status s = q20_status(g);
    printf("[strategy=%s kind=%" PRIu32 " id=%" PRIu32 " counter=%" PRIu32 " candidates=%" PRIu32
           " random_calls=%" PRIu64 "]\n",
           s.strategy, s.kind, s.id, s.number, s.candidates, s.random_calls);
    for (uint32_t i = 0; i < s.candidates; i++) {
        Q20Candidate c;
        if (q20_candidate(g, i, &c))
            break;
        printf("  %2" PRIu32 ". target=%" PRIu32 " score=%" PRId32 " %s\n", i + 1, c.id, c.score,
               q20_text(m, 0, c.id - 1));
    }
}
int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    const char *bank = "en", *dir = "data", *model_path = NULL;
    uint32_t seed = 1;
    int trace = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) {
            usage();
            return 0;
        }
        if (!strcmp(argv[i], "--trace")) {
            trace = 1;
            continue;
        }
        if (i + 1 >= argc) {
            usage();
            return 2;
        }
        const char *option = argv[i], *value = argv[++i];
        if (!strcmp(option, "--bank"))
            bank = value;
        else if (!strcmp(option, "--data"))
            dir = value;
        else if (!strcmp(option, "--model"))
            model_path = value;
        else if (!strcmp(option, "--seed")) {
            char *end;
            errno = 0;
            unsigned long long v = strtoull(value, &end, 10);
            if (errno || *end || !*value || *value == '-' || v > UINT32_MAX) {
                fputs("Invalid seed.\n", stderr);
                return 2;
            }
            seed = (uint32_t)v;
        } else {
            usage();
            return 2;
        }
    }
    if (!strcmp(bank, "el")) {
        fputs("Greek is unsupported: the supplied build contains an incomplete zero stub.\n",
              stderr);
        return 2;
    }
    if (strcmp(bank, "en") && strcmp(bank, "de") && strcmp(bank, "es") && strcmp(bank, "fr") &&
        strcmp(bank, "it") && strcmp(bank, "jp")) {
        fputs("Unsupported bank.\n", stderr);
        return 2;
    }
    char path[4096], error[512];
    if (!model_path) {
        int n = snprintf(path, sizeof(path), "%s/%s.q20", dir, bank);
        if (n < 0 || (size_t)n >= sizeof(path))
            return 2;
        model_path = path;
    }
    Q20Model *m = q20_load(model_path, error, sizeof(error));
    if (!m) {
        fprintf(stderr, "%s\n", error);
        return 2;
    }
    Q20Game *g = q20_create(m, seed);
    if (!g) {
        q20_unload(m);
        return 2;
    }
    printf("20Q offline | bank %s | seed %" PRIu32
           "\nThink of an object. Type restart, quit, candidates, or undo at any prompt.\n",
           bank, seed);
    char input[256];
    int result = 0;
    for (;;) {
        int rc = q20_advance(g);
        Q20Status s = q20_status(g);
        if (trace)
            candidates(g, m);
        if (rc < 0 || (s.kind != 1 && s.kind != 99 && s.id == 0)) {
            printf("The original selection path cannot continue (status %d, strategy %s).\nType "
                   "restart or quit.\n",
                   rc, s.strategy);
        } else if (s.kind == 99) {
            if (s.outcome > 0)
                printf("I guessed it: %s!\n", q20_text(m, 0, (uint32_t)s.outcome - 1));
            else
                puts("I could not guess it within the game's question limit.");
            puts("Type restart to play again, or quit.");
        } else {
            if (s.kind == 1)
                puts("Is it classified as Animal, Vegetable, Mineral, or Other?");
            else if (s.kind == 2)
                printf("%" PRIu32 ". %s?\n", s.number, q20_text(m, 1, s.id - 1));
            else
                printf("%" PRIu32 ". I am guessing that it is %s. Am I right?\n", s.number,
                       q20_text(m, 0, s.id - 1));
            uint32_t limit = s.kind == 1 ? 5 : s.kind == 2 ? 12 : 4;
            for (uint32_t i = 0; i < limit; i++) {
                const char *label = q20_answer_label(m, s.kind, i);
                if (label)
                    printf("%" PRIu32 "=%s%s", i, label, i + 1 < limit ? " | " : "\n");
            }
        }
        fputs("> ", stdout);
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin))
            break;
        size_t len = strcspn(input, "\r\n");
        input[len] = 0;
        while (len && isspace((unsigned char)input[len - 1]))
            input[--len] = 0;
        char *value = input;
        while (isspace((unsigned char)*value))
            value++;
        if (equal(value, "quit") || equal(value, "q"))
            break;
        if (equal(value, "restart")) {
            q20_reset(g, seed);
            puts("New game, same seed. Think of an object.");
            continue;
        }
        if (equal(value, "candidates")) {
            candidates(g, m);
            continue;
        }
        if (equal(value, "undo")) {
            if (q20_submit(g, -1))
                puts("There is no previous answer to undo.");
            continue;
        }
        if (s.kind == 99 || rc < 0) {
            puts("Please type restart or quit.");
            continue;
        }
        int32_t answer = INT32_MIN;
        char *end;
        errno = 0;
        long parsed = strtol(value, &end, 10);
        if (*value && !*end && !errno && parsed >= 0 && parsed <= 11)
            answer = (int32_t)parsed;
        else if (strlen(value) == 1)
            answer = toupper((unsigned char)*value);
        else
            for (uint32_t i = 0; i < 12; i++) {
                const char *label = q20_answer_label(m, s.kind, i);
                if (label && equal(value, label)) {
                    answer = (int32_t)i;
                    break;
                }
            }
        if (answer == INT32_MIN || q20_submit(g, answer))
            puts("That answer is not supported for this prompt.");
    }
    q20_destroy(g);
    q20_unload(m);
    return result;
}
