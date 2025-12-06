// png_mutator.c
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------
// Helper macros
// ---------------------------------------
#define IHDR "IHDR"
#define IDAT "IDAT"

// Valid IHDR bit depth values
int valid_bit_depths[] = {1, 2, 4, 8, 16};

// Valid IHDR color types
int valid_color_types[] = {0, 2, 3, 4, 6};


// ---------------------------------------
// Required AFL++ API function
// ---------------------------------------
size_t afl_custom_mutator(uint8_t *buf, 
         size_t buf_size, 
         uint8_t *out, 
         size_t max_out_size, 
         unsigned int seed) {
   memcpy(out, buf, buf_size);
   srand(seed);

   if (buf_size < 33) return buf_size; // too small to be valid PNG

   // Skip PNG header (8 bytes)
   size_t offset = 8;

   while (offset + 12 < buf_size) {
   // Chunk format:
   // 4 bytes length
   // 4 bytes type
   // N bytes data
   // 4 bytes CRC

      uint32_t length =
         (out[offset] << 24) |
         (out[offset+1] << 16) |
         (out[offset+2] << 8) |
         (out[offset+3]);

      if (offset + 12 + length > buf_size) break;

      uint8_t *chunk_type = &out[offset+4];
      uint8_t *chunk_data = &out[offset+8];

      // ---------------------------------------
      // Mutate IHDR (only bit depth and color type)
      // ---------------------------------------
      if (!memcmp(chunk_type, IHDR, 4)) {

            // IHDR layout:
            // width 4
            // height 4
            // bit depth 1   (offset + 8 + 8)
            // color type 1  (offset + 8 + 9)
            // compression 1
            // filter 1
            // interlace 1

         int bit_depth_offset = offset + 8 + 8;
         int color_type_offset = offset + 8 + 9;

         // Mutate bit depth randomly (valid only)
         if (rand() % 3 == 0) {
            int idx = rand() % (sizeof(valid_bit_depths)/sizeof(int));
            out[bit_depth_offset] = valid_bit_depths[idx];
         }

         // Mutate color type randomly (valid only)
         if (rand() % 3 == 0) {
            int idx = rand() % (sizeof(valid_color_types)/sizeof(int));
            out[color_type_offset] = valid_color_types[idx];
         }
      }

      // ---------------------------------------
      // Mutate IDAT chunk data (pixel data)
      // ---------------------------------------
      else if (!memcmp(chunk_type, IDAT, 4)) {
         for (uint32_t i = 0; i < length; i++) {
               if (rand() % 30 == 0) { // ~3% of bytes
                  chunk_data[i] ^= (rand() & 0xFF);
               }
         }
      }

      // ---------------------------------------
      // Mutate ancillary chunks (tEXt, zTXt, iTXt, gAMA, pHYs, etc.)
      // ---------------------------------------
      else if (chunk_type[0] & 0x20) { 
         // Ancillary chunks always have bit 5 of first letter set
         for (uint32_t i = 0; i < length; i++) {
               if (rand() % 20 == 0) { // ~5% mutation
                  chunk_data[i] ^= (rand() & 0xFF);
               }
         }
      }

      // Move to next chunk
      offset += 12 + length;
   }

   return buf_size;
}

// Required for AFL++ but unused
uint8_t *afl_custom_fuzz(
   uint8_t *buf, size_t buf_size,
   uint8_t **out_buf, size_t *out_size,
   unsigned int seed
){
   *out_buf = NULL;
   *out_size = 0;
   return NULL;
}

size_t afl_custom_init(void *data, unsigned int seed){ return 0; }
void afl_custom_deinit(void *data) {}

