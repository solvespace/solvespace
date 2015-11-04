#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>

#define die(msg) do { fprintf(stderr, "%s\n", msg); abort(); } while(0)

void write_png(FILE *out, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp)
        die("png fopen failed");

    png_byte header[8] = {};
    if(fread(header, 1, 8, fp) != 8)
        die("png fread failed");

    if(png_sig_cmp(header, 0, 8))
        die("png_sig_cmp failed");

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png)
        die("png_create_read_struct failed");

    png_set_expand(png);
    png_set_strip_alpha(png);

    png_infop png_info = png_create_info_struct(png);
    if (!png_info)
        die("png_create_info_struct failed");

    if (setjmp(png_jmpbuf(png)))
        die("png_init_io failed");

    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);

    png_read_info(png, png_info);

    int width = png_get_image_width(png, png_info);
    int height = png_get_image_height(png, png_info);
    if(width != 24 || height != 24)
        die("not a 24x24 png");

    png_read_update_info(png, png_info);

    if (setjmp(png_jmpbuf(png)))
        die("png_read_image failed");

    png_bytepp image = (png_bytepp) malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++)
        image[y] = (png_bytep) malloc(png_get_rowbytes(png, png_info));

    png_read_image(png, (png_bytepp) image);

    for(int y = height - 1; y >= 0; y--) {
        for(int x = 0; x < (int)png_get_rowbytes(png, png_info); x += 3) {
            unsigned char r = image[y][x + 0],
                          g = image[y][x + 1],
                          b = image[y][x + 2];

            if(r + g + b < 11)
                r = g = b = 30;
            fprintf(out, "    0x%02x, 0x%02x, 0x%02x,\n", r, g, b);
        }
    }

    for (int y = 0; y < height; y++)
        free(image[y]);
    free(image);

    fclose(fp);

    png_destroy_read_struct(&png, &png_info, NULL);
}

int main(int argc, char** argv) {
    if(argc < 3) {
        fprintf(stderr, "Usage: %s <source out> <header out> <png in>...\n",
                        argv[0]);
        return 1;
    }

    FILE *source = fopen(argv[1], "wt");
    if(!source)
        die("source fopen failed");

    FILE *header = fopen(argv[2], "wt");
    if(!header)
        die("header fopen failed");

    fprintf(source, "/**** This is a generated file - do not edit ****/\n\n");
    fprintf(header, "/**** This is a generated file - do not edit ****/\n\n");

    for(int i = 3; i < argc; i++) {
        const char *filename = argv[i];
        const char *basename = strrchr(filename, '/'); /* cmake uses / even on Windows */
        if(basename == NULL) {
            basename = filename;
        } else {
            basename++; // skip separator
        }

        char *stemname = (char*) calloc(strlen(basename), 1);
        strncpy(stemname, basename, strchr(basename, '.') - basename);
        for(size_t j = 0; j < strlen(stemname); j++) {
            if(!isalnum(stemname[j]))
                stemname[j] = '_';
        }

        fprintf(header, "extern unsigned char Icon_%s[24*24*3];\n", stemname);

        fprintf(source, "unsigned char Icon_%s[24*24*3] = {\n", stemname);
        write_png(source, filename);
        fprintf(source, "};\n\n");

        free(stemname);
    }

    fclose(source);
    fclose(header);
}
