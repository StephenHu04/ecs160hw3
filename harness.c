#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Shared cleanup for read-side resources.
 */
static void cleanup_and_exit(FILE *fp,
                             png_structp png,
                             png_infop info,
                             png_bytep *row_pointers,
                             png_uint_32 height) {
    // 1. Free pixel row data
    if (row_pointers) {
        for (png_uint_32 y = 0; y < height; y++) {
            if (row_pointers[y]) {
                free(row_pointers[y]);
            }
        }
        free(row_pointers);
    }

    // 2. Destroy libpng structures
    if (png || info) {
        png_destroy_read_struct(png ? &png : NULL,
                                info ? &info : NULL,
                                NULL);
    }

    // 3. Close the file
    if (fp) fclose(fp);
}

int main(int argc, char **argv) {
    if (argc < 3) return 0;

    FILE *fp      = fopen(argv[1], "rb");
    char *outfile = argv[2];
    if (!fp) return 0;

    // --- PNG signature check (from old code) ---
    unsigned char header[8];
    if (fread(header, 1, 8, fp) != 8) {
        fclose(fp);
        return 0;
    }
    if (png_sig_cmp(header, 0, 8)) {
        // Not a PNG file, exit quietly.
        fclose(fp);
        return 0;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return 0;
    }

    /* Provide I/O */
    // We already opened fp above and read 8 signature bytes.
    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);  // tell libpng we've already read 8 bytes

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return 0;
    }

    // Data pointers and dimensions (from old code)
    png_bytep  *row_pointers = NULL;
    png_uint_32 width = 0, height = 0;

    if (setjmp(png_jmpbuf(png))) {
        /* Swallow all libpng longjmp errors. */
        cleanup_and_exit(fp, png, info, row_pointers, height);
        return 0;
    }

    /// Insert APIs to test
    /// Some interesting APIs to test that modify the PNG attributes:
    /// png_set_expand, png_set_gray_to_rgb, png_set_palette_to_rgb, png_set_filler, png_set_scale_16, png_set_packing
    /// Some interesting APIs to test that fetch the PNG attributes:
    /// png_get_channels, png_get_color_type, png_get_rowbytes, png_get_image_width, png_get_image_height,

    // --- Read PNG header & apply your transform pipeline ---

    // Read PNG header and info chunks (up to the pixel data)
    png_read_info(png, info);

    int bit_depth, color_type, interlace_type;
    int compression_type, filter_method;

    // Get IHDR details (from old code)
    png_get_IHDR(png, info,
                 &width, &height,
                 &bit_depth, &color_type,
                 &interlace_type,
                 &compression_type,
                 &filter_method);

    // Convert palette to RGB
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    // Expand grayscale to 8-bit
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    // Add alpha channel if tRNS (transparency) chunk exists
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    // Strip 16-bit data down to 8-bit
    if (bit_depth == 16)
        png_set_strip_16(png);

    // (Optional) ensure RGBA via filler + gray_to_rgb, if you like:
    // png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    // png_set_gray_to_rgb(png);

    // Update info after transformations
    png_read_update_info(png, info);

    // Get final row byte count AFTER transformations
    png_size_t rowbytes = png_get_rowbytes(png, info);

    // Fuzzing safety check: Avoid huge allocations that cause DoS (from old code)
    if (height == 0 || rowbytes == 0 ||
        height > 100000 ||
        (size_t)height * rowbytes > 100000000) {
        cleanup_and_exit(fp, png, info, NULL, 0);
        return 0;
    }

    // Allocate array of row pointers
    row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
    if (!row_pointers) {
        cleanup_and_exit(fp, png, info, NULL, 0);
        return 0;
    }

    // Allocate memory for each row's pixel data
    for (png_uint_32 y = 0; y < height; y++) {
        row_pointers[y] = (png_bytep)malloc(rowbytes);
        if (!row_pointers[y]) {
            // y = number of rows successfully allocated so far
            cleanup_and_exit(fp, png, info, row_pointers, y);
            return 0;
        }
    }

    // Read the entire image data (this exercises decompression and filtering)
    png_read_image(png, row_pointers);

    // Read end info (IEND + ancillary data)
    png_read_end(png, info);

    // Check and read gamma for additional coverage
    double gamma;
    if (png_get_gAMA(png, info, &gamma)) {
        (void)gamma;
    }

    // Example of get APIs (already executed, but illustrative)
    png_get_image_width(png, info);
    png_get_image_height(png, info);
    png_get_channels(png, info);
    png_get_color_type(png, info);
    png_get_rowbytes(png, info);

    /// Optional write API
    FILE *out = fopen(outfile, "wb");
    if (!out) {
        perror("open output");
        cleanup_and_exit(fp, png, info, row_pointers, height);
        return 1;
    }

    png_structp wpng = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop   winfo = png_create_info_struct(wpng);
    if (!wpng || !winfo) {
        fclose(out);
        cleanup_and_exit(fp, png, info, row_pointers, height);
        if (wpng || winfo) png_destroy_write_struct(&wpng, &winfo);
        return 1;
    }

    if (setjmp(png_jmpbuf(wpng))) {
        fclose(out);
        png_destroy_write_struct(&wpng, &winfo);
        cleanup_and_exit(fp, png, info, row_pointers, height);
        return 1;
    }

    png_init_io(wpng, out);

    // Write as 8-bit RGBA (matches transforms if you added filler/gray_to_rgb)
    png_set_IHDR(wpng, winfo,
                 width, height, 8,
                 PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);

    png_write_info(wpng, winfo);
    png_write_image(wpng, row_pointers);
    png_write_end(wpng, winfo);

    fclose(out);
    png_destroy_write_struct(&wpng, &winfo);

    // Final cleanup for read-side resources
    cleanup_and_exit(fp, png, info, row_pointers, height);

    return 0;
}
