#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ----------------------- PNG CONSTANTS -----------------------
#define PNG_SIG_SIZE 8
static const uint8_t PNG_SIG[8] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
};

#define MAX_CHUNKS 1024

// ----------------------- CRC32 IMPLEMENTATION -----------------------
static uint32_t crc_table[256];
static int crc_table_computed = 0;

static void make_crc_table(void) {
    uint32_t c;
    for (int n = 0; n < 256; n++) {
        c = n;
        for (int k = 0; k < 8; k++) {
            if (c & 1) c = 0xedb88320L ^ (c >> 1);
            else       c >>= 1;
        }
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}

static uint32_t update_crc(uint32_t crc, const uint8_t *buf, int len) {
    if (!crc_table_computed) make_crc_table();
    uint32_t c = crc;
    for (int n = 0; n < len; n++) {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

static uint32_t calculate_crc(const uint8_t *buf, int len) {
    return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

// ----------------------- MUTATOR STATE -----------------------
typedef struct {
    uint8_t *out_buf;
    size_t out_buf_size;
} mutator_state_t;

void *afl_custom_init(void *afl, unsigned int seed) {
    mutator_state_t *s = calloc(1, sizeof(mutator_state_t));
    srand(seed);
    return s;
}

void afl_custom_deinit(void *data) {
    mutator_state_t *s = data;
    free(s->out_buf);
    free(s);
}

// ----------------------- UTILITIES -----------------------
static void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8)  & 0xFF;
    p[3] = (v)       & 0xFF;
}

static void mutate_havoc(uint8_t *buf, size_t size) {
    if (size == 0) return;
    size_t pos = rand() % size;
    int type = rand() % 3;

    if (type == 0) buf[pos] ^= 1 << (rand() % 8);
    else if (type == 1) buf[pos] = rand() & 0xFF;
    else buf[pos] = buf[pos] + ((rand() % 11) - 5);
}

// ----------------------- PNG MUTATION -----------------------
size_t afl_custom_fuzz(
    void *data,
    uint8_t *buf, size_t buf_size,
    uint8_t **out_buf,
    uint8_t *add_buf, size_t add_buf_size,
    size_t max_size
) {
    mutator_state_t *s = data;

    // Ensure output buffer is large enough
    if (s->out_buf_size < buf_size + 1024) {
        s->out_buf = realloc(s->out_buf, buf_size + 1024);
        s->out_buf_size = buf_size + 1024;
    }

    memcpy(s->out_buf, buf, buf_size);
    *out_buf = s->out_buf;

    // FAST EXIT: Not PNG → just havoc
    if (buf_size < PNG_SIG_SIZE ||
        memcmp(buf, PNG_SIG, PNG_SIG_SIZE) != 0 ||
        (rand() % 100 < 10)) {

        mutate_havoc(s->out_buf, buf_size);
        return buf_size;
    }

    // ----------------------- PARSE PNG CHUNKS -----------------------
    size_t chunk_offsets[MAX_CHUNKS];
    int chunk_count = 0;

    size_t off = PNG_SIG_SIZE;
    while (off + 12 <= buf_size && chunk_count < MAX_CHUNKS) {

        // Read length safely
        uint32_t len =
            (s->out_buf[off+0] << 24) |
            (s->out_buf[off+1] << 16) |
            (s->out_buf[off+2] << 8)  |
            (s->out_buf[off+3]);

        size_t total_chunk = 12 + (size_t)len;

        // Validate chunk boundaries
        if (len > 0x7FFFFFFF) break;                     // insane length
        if (off + total_chunk > buf_size) break;         // truncated
        if (off + total_chunk < off) break;              // overflow wrap

        chunk_offsets[chunk_count++] = off;
        off += total_chunk;

        if (memcmp(s->out_buf + off - 12 + 4, "IEND", 4) == 0)
            break;
    }

    // If no chunks found → havoc
    if (chunk_count == 0) {
        mutate_havoc(s->out_buf, buf_size);
        return buf_size;
    }

    // ----------------------- SELECT CHUNK -----------------------
    size_t chunk_off = chunk_offsets[rand() % chunk_count];

    uint32_t len =
        (s->out_buf[chunk_off+0] << 24) |
        (s->out_buf[chunk_off+1] << 16) |
        (s->out_buf[chunk_off+2] << 8)  |
        (s->out_buf[chunk_off+3]);

    uint8_t *type_ptr = s->out_buf + chunk_off + 4;
    uint8_t *data_ptr = s->out_buf + chunk_off + 8;
    uint8_t *crc_ptr  = data_ptr + len;

    // Safety: ensure crc_ptr is in bounds
    if (data_ptr + len + 4 > s->out_buf + buf_size) {
        mutate_havoc(s->out_buf, buf_size);
        return buf_size;
    }

    // ----------------------- MUTATE -----------------------
    int strategy = rand() % 4;

    if (strategy == 0 && len > 0) {
        // DATA MUTATION
        int n = 1 + (rand() % 5);
        for (int i = 0; i < n; i++) {
            size_t pos = rand() % len;
            data_ptr[pos] ^= (rand() & 0xFF);
        }

    } else if (strategy == 1) {
        // TYPE MUTATION (flip ancillary/critical)
        type_ptr[rand() % 4] ^= 0x20;

    } else if (strategy == 2) {
        // LENGTH CORRUPTION → no CRC fix
        s->out_buf[chunk_off + (rand() % 4)] ^= (rand() & 0xFF);
        return buf_size;

    } else {
        // IHDR mutation
        if (memcmp(type_ptr, "IHDR", 4) == 0 && len == 13) {
            int f = rand() % 5;

            if (f < 2) { // width/height
                mutate_havoc(data_ptr + (f * 4), 4);
            } else {
                data_ptr[8 + (f - 2)] ^= (rand() & 0xFF);
            }
        } else if (len > 0) {
            data_ptr[rand() % len] ^= 0xFF;
        }
    }

    // ----------------------- FIX CRC -----------------------
    uint32_t crc = calculate_crc(type_ptr, 4 + len);
    write_be32(crc_ptr, crc);

    return buf_size;
}
