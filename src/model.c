#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t crc32(const unsigned char *p, size_t n) {
    uint32_t c = ~0u;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int j = 0; j < 8; j++)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
    }
    return ~c;
}
Q20Model *q20_load(const char *path, char *error, size_t es) {
    FILE *f = fopen(path, "rb");
    Q20Model *m = NULL;
    unsigned char *b = NULL;
    long size = 0;
    const char *why = "Cannot open model";
    if (!f)
        goto fail;
    why = "Cannot read model";
    if (fseek(f, 0, SEEK_END) || ((size = ftell(f)) < 64) || size > 16000000 ||
        fseek(f, 0, SEEK_SET))
        goto fail;
    b = malloc((size_t)size);
    if (!b || fread(b, 1, (size_t)size, f) != (size_t)size)
        goto fail;
    fclose(f);
    f = NULL;
    why = "Unsupported or corrupt Q20 container";
    if (memcmp(b, "Q20PSP1\0", 8) || le32(b + 8) != 1 || le32(b + 36) != (uint32_t)size - 64 ||
        le32(b + 44) != 1228 || crc32(b + 64, (size_t)size - 64) != le32(b + 32))
        goto fail;
    const char *langs[] = {"en", "de", "es", "fr", "it", "jp"};
    int known = 0;
    for (int i = 0; i < 6; i++)
        if (!memcmp(b + 40, langs[i], 2))
            known = 1;
    if (!known)
        goto fail;
    for (size_t i = 48; i < 64; i++)
        if (b[i])
            goto fail;
    m = calloc(1, sizeof(*m));
    if (!m)
        goto fail;
    m->nt = le32(b + 12);
    m->nq = le32(b + 16);
    m->packed_n = le32(b + 20);
    m->pool_n = le32(b + 24);
    m->text_n = le32(b + 28);
    if (m->nt < 2 || m->nt > 10000 || m->nq < 5 || m->nq > 1000 ||
        m->packed_n != ((uint64_t)m->nt * m->nq + 1) / 2)
        goto fail;
    uint64_t expected =
        64ull + m->packed_n + m->pool_n + 16ull * (m->nt + m->nq) + 1228 + 96 + m->text_n;
    if (expected != (uint64_t)size)
        goto fail;
    m->file = b;
    m->packed = b + 64;
    m->pool = m->packed + m->packed_n;
    m->records = m->pool + m->pool_n;
    m->fixed = m->records + 16u * (m->nt + m->nq);
    m->labels = m->fixed + 1228;
    m->text = m->labels + 96;
    for (uint32_t i = 0; i < m->nt + m->nq; i++) {
        const unsigned char *r = m->records + 16 * i;
        uint32_t d = le32(r + 8), raw = le32(r + 12);
        if (le32(r) != (i < m->nt ? i : i - m->nt) || d >= m->text_n || raw >= m->pool_n ||
            !memchr(m->text + d, 0, m->text_n - d) || !memchr(m->pool + raw, 0, m->pool_n - raw))
            goto fail;
    }
    for (uint32_t i = 0; i < 24; i++) {
        uint32_t d = le32(m->labels + 4 * i);
        if (d >= m->text_n || !memchr(m->text + d, 0, m->text_n - d))
            goto fail;
    }
    return m;
fail:
    if (f)
        fclose(f);
    free(b);
    free(m);
    if (error && es)
        snprintf(error, es, "%s: %s", why, path);
    return NULL;
}
void q20_unload(Q20Model *m) {
    if (m) {
        free(m->file);
        free(m);
    }
}
uint32_t q20_target_count(const Q20Model *m) {
    return m->nt;
}
uint32_t q20_question_count(const Q20Model *m) {
    return m->nq;
}
static const unsigned char *record(const Q20Model *m, int q, uint32_t slot) {
    if (slot >= (q ? m->nq : m->nt))
        return NULL;
    return m->records + 16 * (slot + (q ? m->nt : 0));
}
const char *q20_text(const Q20Model *m, int q, uint32_t slot) {
    const unsigned char *r = record(m, q, slot);
    return r ? (const char *)m->text + le32(r + 8) : NULL;
}
const char *q20_raw_text(const Q20Model *m, int q, uint32_t slot) {
    const unsigned char *r = record(m, q, slot);
    return r ? (const char *)m->pool + le32(r + 12) : NULL;
}
uint32_t q20_read(const Q20Model *m, int32_t q, int32_t t, int format) {
    if (q < 1 || t < 1 || (uint32_t)q >= m->nq || (uint32_t)t >= m->nt)
        return UINT32_MAX;
    uint32_t k = ((uint32_t)t - 1) * m->nq + (uint32_t)q - 1,
             c = (m->packed[k / 2] >> ((k & 1) * 4)) & 15;
    if (format == 4)
        return c;
    if (format == 2)
        return (uint32_t)!!(c & 8) | ((c & 7) > 2 ? 2u : 0u);
    return UINT32_MAX;
}
const unsigned char *q20_table(const Q20Model *m, uint32_t kind, uint32_t *count) {
    if (kind == 1) {
        *count = 6;
        return m->fixed + 0x170;
    }
    if (kind == 2) {
        *count = 13;
        return m->fixed;
    }
    if (kind == 3) {
        *count = 5;
        return m->fixed + 0x218;
    }
    *count = 0;
    return NULL;
}
const char *q20_answer_label(const Q20Model *m, uint32_t kind, uint32_t id) {
    uint32_t n;
    const unsigned char *p = q20_table(m, kind, &n);
    uint32_t base = kind == 1 ? 13 : kind == 3 ? 19 : 0;
    for (uint32_t i = 0; i < n; i++)
        if (le32(p + 28 * i) && le32(p + 28 * i + 4) == id)
            return (const char *)m->text + le32(m->labels + 4 * (base + i));
    return NULL;
}
