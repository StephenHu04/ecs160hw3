#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define PNG_SIG_SIZE 8
static const uint8_t PNG_SIG[PNG_SIG_SIZE] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
};

#define MAX_CHUNKS 2048
#define IHDR "IHDR"
#define IDAT "IDAT"

// Valid IHDR and IDAT chunks of PNG file
int valid_bit_depths[] = {1, 2, 4, 8, 16};
int valid_color_types[] = {0, 2, 3, 4, 6};

static uint32_t crc_table[256];
static int crc_table_computed = 0;

typedef struct {
    uint8_t *mutated_buf;
    size_t buf_size;
} my_mutator_t;

// CRC table checksum to detect accidental changes
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

// Helper function to update CRC over buffer of byts
static uint32_t update_crc(uint32_t crc, const uint8_t *buf, int len) {
    if (!crc_table_computed) make_crc_table();
    uint32_t c = crc;
    for (int n = 0; n < len; n++) {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

// Helper function to calculate CRC of a PNG
static uint32_t calculate_crc(const uint8_t *buf, int len) {
    return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

static void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8)  & 0xFF;
    p[3] = (v)       & 0xFF;
}

// Allocate an initial buffer 
void *afl_custom_init(void *afl, unsigned int seed) {
    my_mutator_t *data = calloc(1, sizeof(my_mutator_t));
    if (!data) {
        perror("afl_custom_init alloc");
        return NULL;
    }

    data->mutated_buf = malloc(MAX_CHUNKS * 1024); 
    data->buf_size = MAX_CHUNKS * 1024;
    srand(seed); 
    return data;
}

// Helper function to free memory
void afl_custom_deinit(void *data) {
    my_mutator_t *mutator = (my_mutator_t *)data;
    if (mutator) {
        free(mutator->mutated_buf);
        free(mutator);
    }
}

// AFL++ signature
size_t afl_custom_fuzz(void *data, 
                       uint8_t *buf, size_t buf_size, 
                       uint8_t **out_buf, 
                       uint8_t *add_buf, size_t add_buf_size, 
                       size_t max_size) {
    
    my_mutator_t *mutator = (my_mutator_t *)data;

    // Size checking buffer
    if (buf_size > mutator->buf_size) {
        mutator->mutated_buf = realloc(mutator->mutated_buf, buf_size);
        mutator->buf_size = buf_size;
    }

    // Copy original input to our scratch buffer
    memcpy(mutator->mutated_buf, buf, buf_size);
    
    // Pointer for AFL to read from
    *out_buf = mutator->mutated_buf;

    // Validate PNG signature
    if (buf_size < 33 || memcmp(mutator->mutated_buf, PNG_SIG, PNG_SIG_SIZE) != 0) {
        return buf_size;
    } 

    uint8_t *out = mutator->mutated_buf;
    
    size_t chunk_offsets[MAX_CHUNKS];
    int chunk_count = 0;
    // Skip first 8 bytes of PNG signature
    size_t offset = PNG_SIG_SIZE;

    // Parse chunks
    while (offset + 12 < buf_size && chunk_count < MAX_CHUNKS) {
        uint32_t len = (out[offset] << 24) | (out[offset+1] << 16) |
                       (out[offset+2] << 8)  | (out[offset+3]);
        
        // Overflow checks
        if (len > 0x7FFFFFFF) break;
        size_t total_chunk = 12 + (size_t)len;
        if (offset + total_chunk > buf_size) break;
        if (offset + total_chunk < offset) break; 

        chunk_offsets[chunk_count++] = offset;

        offset += total_chunk;
        // Basic IEND check to stop parsing
        if (offset >= 12 && memcmp(out + offset - 12 + 4, "IEND", 4) == 0) break;
    }

    if (chunk_count == 0) return buf_size;

    // Pick a random chunk to mutate
    size_t c_off = chunk_offsets[rand() % chunk_count];
    uint32_t len = (out[c_off] << 24) | (out[c_off+1] << 16) |
                   (out[c_off+2] << 8)  | (out[c_off+3]);

    uint8_t *type = out + c_off + 4;
    uint8_t *chunk_data = out + c_off + 8;
    uint8_t *crc_ptr = chunk_data + len;

    // Mutate IDAT chunk data
    int is_idat = (memcmp(type, IDAT, 4) == 0);

    // Mutate IHDR bit and color within valid ranges
    if (!memcmp(type, IHDR, 4) && len >= 13) {
        if (rand() % 2 == 0) {
            // Flip bit depth
            out[c_off + 8 + 8] = valid_bit_depths[rand() % 5];
        } else {
            // Flip color type
            out[c_off + 8 + 9] = valid_color_types[rand() % 5];
        }
    }
    
    // Mutate IDAT chunk data
    else if (is_idat) {
        if (len > 0) {
            size_t pos = rand() % len;
            chunk_data[pos] ^= (rand() & 0xFF);  // mutate the IDAT chunk data
        }
    }

    // Mutate ancillary chunk data
    else if (len > 0) {
        size_t pos = rand() % len;
        chunk_data[pos] ^= (rand() & 0xFF);
    }

    // Fix CRC
    uint32_t crc = calculate_crc(type, 4 + len);
    write_be32(crc_ptr, crc);

    return buf_size;
}