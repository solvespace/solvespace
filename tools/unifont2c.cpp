#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <zlib.h>
#include <png.h>
#include <map>

#define die(msg) do { fprintf(stderr, "%s\n", msg); abort(); } while(0)

unsigned short* read_png(const char *filename) {
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
    if(width != 16 || height != 16)
        die("not a 16x16 png");

    png_read_update_info(png, png_info);

    if (setjmp(png_jmpbuf(png)))
        die("png_read_image failed");

    png_bytepp image = (png_bytepp) malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++)
        image[y] = (png_bytep) malloc(png_get_rowbytes(png, png_info));

    png_read_image(png, (png_bytepp) image);

    unsigned short *glyph = (unsigned short *) calloc(16, 2);

    for(int y = 0; y < height; y++) {
        for(int x = 0; x < (int)png_get_rowbytes(png, png_info); x += 3) {
            unsigned char r = image[y][x + 0],
                          g = image[y][x + 1],
                          b = image[y][x + 2];

            if(r + g + b >= 11) {
                int pos = y * width + (width - x / 3);
                glyph[pos / 16] |= 1 << (pos % 16);
            }
       }
    }

    for (int y = 0; y < height; y++)
        free(image[y]);
    free(image);

    fclose(fp);

    return glyph;
}

const static unsigned short replacement[16] = {
    0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA,
    0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA,
    0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA,
    0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA,
};

int main(int argc, char** argv) {
    if(argc < 3) {
        fprintf(stderr, "Usage: %s <header/source out> <unifont.hex> <png glyph>...\n"
                        "  where <png glyph>s are mapped into private use area\n"
                        "  starting at U+0080.\n",
                        argv[0]);
        return 1;
    }

    unsigned short *font[256] = {};

    const int private_start = 0x80, private_count = argc - 3;
    for(int i = 3; i < argc; i++) {
        font[private_start + i - 3] = read_png(argv[i]);
    }

    gzFile unifont = gzopen(argv[2], "rb");
    if(!unifont)
        die("unifont fopen failed");

    while(1) {
        unsigned short codepoint;
        unsigned short *glyph = (unsigned short *) calloc(32, 1);

        char buf[100];
        if(!gzgets(unifont, buf, sizeof(buf))){
            if(gzeof(unifont)) {
                break;
            } else {
                die("unifont gzgets failed");
            }
        }

        if(       sscanf(buf, "%4hx:%4hx%4hx%4hx%4hx%4hx%4hx%4hx%4hx"
                                   "%4hx%4hx%4hx%4hx%4hx%4hx%4hx%4hx\n",
                                &codepoint,
                                &glyph[0],  &glyph[1],  &glyph[2],  &glyph[3],
                                &glyph[4],  &glyph[5],  &glyph[6],  &glyph[7],
                                &glyph[8],  &glyph[9],  &glyph[10], &glyph[11],
                                &glyph[12], &glyph[13], &glyph[14], &glyph[15]) == 17) {
            /* read 16x16 character */
        } else if(sscanf(buf, "%4hx:%2hx%2hx%2hx%2hx%2hx%2hx%2hx%2hx"
                                   "%2hx%2hx%2hx%2hx%2hx%2hx%2hx%2hx\n",
                                &codepoint,
                                &glyph[0],  &glyph[1],  &glyph[2],  &glyph[3],
                                &glyph[4],  &glyph[5],  &glyph[6],  &glyph[7],
                                &glyph[8],  &glyph[9],  &glyph[10], &glyph[11],
                                &glyph[12], &glyph[13], &glyph[14], &glyph[15]) == 17) {
            /* read 8x16 character */
            for(int i = 0; i < 16; i++)
                glyph[i] <<= 8;
        } else {
            die("parse unifont character");
        }

        if(codepoint >= 0x00 && codepoint < 0x80) {
            font[codepoint] = glyph;
        } else {
            free(glyph);
        }
    }

    gzclose(unifont);

    FILE *source = fopen(argv[1], "wt");
    if(!source)
        die("source fopen failed");

    fprintf(source, "/**** This is a generated file - do not edit ****/\n\n");
    fprintf(source, "static const unsigned char FontTexture[256 * 16 * 16] = {\n");

    for(int codepoint = 0; codepoint < 0x100; codepoint++) {
        const unsigned short *glyph = font[codepoint] != NULL ? font[codepoint] : replacement;
        for(int x = 15; x >= 0; x--) {
                for(int y = 0; y < 16; y++) {
                int pos = y * 16 + x;
                if(glyph[pos / 16] & (1 << (pos % 16))) {
                    fprintf(source, "255, ");
                } else {
                    fprintf(source, "  0, ");
                }
            }
            fprintf(source, "\n");
        }
        fprintf(source, "\n");
    }

    fprintf(source, "};\n");

    fclose(source);
}
