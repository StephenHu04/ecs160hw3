// harness.c
// Simple libpng reader harness for AFL++
// Compile with AFL's afl-cc and link against libpng + zlib.

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void cleanup_and_exit(FILE *fp,
                             png_structp png,
                             png_infop info,
                             png_bytep *row_pointers,
                             png_uint_32 height) {
    if (row_pointers) {
        for (png_uint_32 y = 0; y < height; y++) {
            free(row_pointers[y]);
        }
        free(row_pointers);
    }
    if (png || info) {
        png_destroy_read_struct(png ? &png : NULL,
                                info ? &info : NULL,
                                NULL);
    }
    if (fp) fclose(fp);
}

// Very small wrapper around libpng reading logic.
int main(int argc, char **argv) {
    if (argc < 2) {
        return 0;
    }

    const char *filename = argv[1];

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return 0;
    }

    // Read and check PNG signature (optional but useful)
    unsigned char header[8];
    if (fread(header, 1, 8, fp) != 8) {
        fclose(fp);
        return 0;
    }
    if (png_sig_cmp(header, 0, 8)) {
        // Not a PNG file, just exit quietly.
        fclose(fp);
        return 0;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                             NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return 0;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return 0;
    }

    // libpng error handling with setjmp/longjmp
    if (setjmp(png_jmpbuf(png))) {
        // Any libpng error will end up here
        cleanup_and_exit(fp, png, info, NULL, 0);
        return 0;
    }

    png_init_io(png, fp);
    png_set_sig_bytes(png, 8); // we already read 8 bytes

    // Read PNG header and info
    png_read_info(png, info);

    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
    int compression_type, filter_method;

    // IHDR
    png_get_IHDR(png, info,
                 &width, &height,
                 &bit_depth, &color_type,
                 &interlace_type,
                 &compression_type,
                 &filter_method);

    // Exercise some common transformations:
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if (bit_depth == 16)
        png_set_strip_16(png);

    // Update info after transforms
    png_read_update_info(png, info);

    // Get row bytes AFTER update
    png_size_t rowbytes = png_get_rowbytes(png, info);

    if (height == 0 || rowbytes == 0 || height > 100000) {
        // extremely suspicious values; avoid huge allocations
        cleanup_and_exit(fp, png, info, NULL, 0);
        return 0;
    }

    png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
    if (!row_pointers) {
        cleanup_and_exit(fp, png, info, NULL, 0);
        return 0;
    }

    for (png_uint_32 y = 0; y < height; y++) {
        row_pointers[y] = (png_bytep)malloc(rowbytes);
        if (!row_pointers[y]) {
            cleanup_and_exit(fp, png, info, row_pointers, height);
            return 0;
        }
    }

    // Read the whole image
    png_read_image(png, row_pointers);

    // Read end info (exercise more code)
    png_read_end(png, info);

    // Optionally touch some ancillary info for coverage
    double gamma;
    if (png_get_gAMA(png, info, &gamma)) {
        // no-op, just reading is enough
        (void)gamma;
    }

    // Clean up
    cleanup_and_exit(fp, png, info, row_pointers, height);

    return 0;
}
