#include <png.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 3) return 0;

    FILE *fp = fopen(argv[1], "rb");
    char *outfile = argv[2];
    if (!fp) return 0;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return 0;
    }

    /* Provide I/O */
    png_init_io(png, fp);

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return 0;
    }

    if (setjmp(png_jmpbuf(png))) {
        /* Swallow all libpng longjmp errors. */
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    /// Insert APIs to test
    /// Some interesting APIs to test that modify the PNG attributes:
    /// png_set_expand, png_set_gray_to_rgb, png_set_palette_to_rgb, png_set_filler, png_set_scale_16, png_set_packing
    /// Some interesting APIs to test that fetch the PNG attributes:
    /// png_get_channels, png_get_color_type, png_get_rowbytes, png_get_image_width, png_get_image_height,

    // Read header/info
    png_read_info(png, info);

    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
    int compression_type, filter_method;

    png_get_IHDR(png, info,
                 &width, &height,
                 &bit_depth, &color_type,
                 &interlace_type,
                 &compression_type,
                 &filter_method);

    // Modify attributes (transformations)
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if (bit_depth == 16)
        png_set_strip_16(png);  // could also use png_set_scale_16 in other contexts

    // Ensure we end up with RGBA pixels (adds filler if needed)
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    png_set_gray_to_rgb(png);  // if grayscale, convert it

    // Apply transforms
    png_read_update_info(png, info);

    // Fetch attributes
    png_size_t rowbytes = png_get_rowbytes(png, info);
    int channels = png_get_channels(png, info);
    int final_color_type = png_get_color_type(png, info);
    (void)channels;
    (void)final_color_type;

    // Simple safety cap to avoid absurd allocations when fuzzing
    if (height == 0 || rowbytes == 0 ||
        height > 100000 ||
        (size_t)height * rowbytes > 100000000) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    // Allocate rows and read the image
    png_bytep *rows = (png_bytep *)malloc(sizeof(png_bytep) * height);
    if (!rows) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    for (png_uint_32 y = 0; y < height; y++) {
        rows[y] = (png_bytep)malloc(rowbytes);
        if (!rows[y]) {
            for (png_uint_32 i = 0; i < y; i++) free(rows[i]);
            free(rows);
            png_destroy_read_struct(&png, &info, NULL);
            fclose(fp);
            return 0;
        }
    }

    png_read_image(png, rows);
    png_read_end(png, info);

    /// Optional write API
    FILE *out = fopen(outfile, "wb");
    if (!out) {
        for (png_uint_32 y = 0; y < height; y++) free(rows[y]);
        free(rows);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    png_structp wpng = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop   winfo = png_create_info_struct(wpng);
    if (!wpng || !winfo) {
        if (wpng || winfo) png_destroy_write_struct(&wpng, &winfo);
        fclose(out);
        for (png_uint_32 y = 0; y < height; y++) free(rows[y]);
        free(rows);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    if (setjmp(png_jmpbuf(wpng))) {
        fclose(out);
        png_destroy_write_struct(&wpng, &winfo);
        for (png_uint_32 y = 0; y < height; y++) free(rows[y]);
        free(rows);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 1;
    }

    png_init_io(wpng, out);

    png_set_IHDR(wpng, winfo,
                 width, height, 8,
                 PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);

    png_write_info(wpng, winfo);
    png_write_image(wpng, rows);
    png_write_end(wpng, winfo);

    // Cleanup
    fclose(out);
    png_destroy_write_struct(&wpng, &winfo);

    for (png_uint_32 y = 0; y < height; y++) free(rows[y]);
    free(rows);

    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    return 0;
}
