#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief
 */

static void cleanup_and_exit(FILE *fp,
                             png_structp png,
                             png_infop info,
                             png_bytep *row_pointers,
                             png_uint_32 height) {
    // 1. Free pixel row data
    if (row_pointers) {
        for (png_uint_32 y = 0; y < height; y++) {
            // Only free rows that were successfully allocated
            if (row_pointers[y]) {
                free(row_pointers[y]);
            }
        }
        // Free the array of pointers
        free(row_pointers);
    }
    
    // 2. Destroy libpng structures
    if (png || info) {
        // Pass addresses of structures, NULL is fine if one is already NULL
        png_destroy_read_struct(png ? &png : NULL,
                                info ? &info : NULL,
                                NULL);
    }
    
    // 3. Close the file
    if (fp) fclose(fp);
}

// Very small wrapper around libpng reading logic.
int main(int argc, char **argv) {
    // Check for input file (needs 2 arguments: program name + file path)
    if (argc < 2) {
        return 0;
    }

    const char *filename = argv[1];


    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return 0;
    }

    // Read and check PNG signature
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

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return 0;
    }

    // Define pointers for data and dimensions
    png_bytep *row_pointers = NULL;
    png_uint_32 width = 0, height = 0; // Initialize to 0 for cleanup safety

    if (setjmp(png_jmpbuf(png))) {
        // Any libpng error will longjmp here. Perform full cleanup.
        cleanup_and_exit(fp, png, info, row_pointers, height);
        return 0;
    }
    /// Insert APIs to test
    /// Some interesting APIs to test that modify the PNG attributes:
    /// png_set_expand, png_set_gray_to_rgb, png_set_palette_to_rgb, png_set_filler, png_set_scale_16, png_set_packing
    /// Some interesting APIs to test that fetch the PNG attributes:
    /// png_get_channels, png_get_color_type, png_get_rowbytes, png_get_image_width, png_get_image_height, 
    png_init_io(png, fp);
    png_set_sig_bytes(png, 8); // We already read the first 8 bytes

    // Read PNG header and info chunks (up to the pixel data)
    png_read_info(png, info);

    int bit_depth, color_type, interlace_type;
    int compression_type, filter_method;

    // Get IHDR details
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

    // Update info after transformations
    png_read_update_info(png, info);

    // Get final row byte count AFTER transformations
    png_size_t rowbytes = png_get_rowbytes(png, info);

    // Fuzzing safety check: Avoid huge allocations that cause DoS
    if (height == 0 || rowbytes == 0 || height > 100000 || (size_t)height * rowbytes > 100000000) {
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
        // If allocation fails, we must jump to the cleanup handler
        if (!row_pointers[y]) {
            // Need to set height to 'y' here for partial cleanup
            cleanup_and_exit(fp, png, info, row_pointers, y);
            return 0;
        }
    }

    // Read the entire image data (this exercises the decompression and filtering)
    png_read_image(png, row_pointers);

    // Read end info (read remaining chunks like IEND and process ancillary data)
    png_read_end(png, info);
    
    // Check and read gamma for additional coverage
    double gamma;
    if (png_get_gAMA(png, info, &gamma)) {
        // Simply reading the value is enough to test the API
        (void)gamma;
    }
    
    // Example of using other 'get' APIs (already executed, but illustrative)
    png_get_image_width(png, info);
    png_get_image_height(png, info);

    cleanup_and_exit(fp, png, info, row_pointers, height);

    return 0;
}
