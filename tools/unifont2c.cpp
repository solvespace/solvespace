#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <zlib.h>
#include <png.h>

#define die(msg) do { fprintf(stderr, "%s\n", msg); abort(); } while(0)

#ifdef NDEBUG
#define COMPRESSION_LEVEL 9
#else
#define COMPRESSION_LEVEL 5
#endif

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
                glyph[y] |= 1 << (width - x / 3);
            }
       }
    }

    for (int y = 0; y < height; y++)
        free(image[y]);
    free(image);

    fclose(fp);

    png_destroy_read_struct(&png, &png_info, NULL);

    return glyph;
}

const static unsigned short replacement[16] = {
    0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA,
    0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA,
    0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA,
    0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA,
};

struct CodepointProperties {
    bool exists:1;
    bool isWide:1;
};

int main(int argc, char** argv) {
    if(argc < 3) {
        fprintf(stderr, "Usage: %s <header/source out> <unifont.hex> <png glyph>...\n"
                        "  where <png glyph>s are mapped into private use area\n"
                        "  starting at U+E000.\n",
                        argv[0]);
        return 1;
    }

    const int codepoint_count = 0x10000;
    unsigned short **font =
        (unsigned short **)calloc(sizeof(unsigned short*), codepoint_count);
    CodepointProperties *properties =
        (CodepointProperties *)calloc(sizeof(CodepointProperties), codepoint_count);

    const int private_start = 0xE000;
    for(int i = 3; i < argc; i++) {
        int codepoint = private_start + i - 3;
        font[codepoint] = read_png(argv[i]);
        properties[codepoint].exists = true;
    }

    gzFile unifont = gzopen(argv[2], "rb");
    if(!unifont)
        die("unifont fopen failed");

    while(1) {
        char buf[100];
        if(!gzgets(unifont, buf, sizeof(buf))){
            if(gzeof(unifont)) {
                break;
            } else {
                die("unifont gzgets failed");
            }
        }

        unsigned short codepoint;
        unsigned short *glyph = (unsigned short *) calloc(32, 1);
        bool isWide;
        if(       sscanf(buf, "%4hx:%4hx%4hx%4hx%4hx%4hx%4hx%4hx%4hx"
                                   "%4hx%4hx%4hx%4hx%4hx%4hx%4hx%4hx\n",
                                &codepoint,
                                &glyph[0],  &glyph[1],  &glyph[2],  &glyph[3],
                                &glyph[4],  &glyph[5],  &glyph[6],  &glyph[7],
                                &glyph[8],  &glyph[9],  &glyph[10], &glyph[11],
                                &glyph[12], &glyph[13], &glyph[14], &glyph[15]) == 17) {
            /* read 16x16 character */
            isWide = true;
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
            isWide = false;
        } else {
            die("parse unifont character");
        }

        font[codepoint] = glyph;
        properties[codepoint].exists = true;
        properties[codepoint].isWide = isWide;
    }

    gzclose(unifont);

    FILE *source = fopen(argv[1], "wt");
    if(!source)
        die("source fopen failed");

    const int chunk_size      = 64 * 64,
              chunks          = codepoint_count / chunk_size;

    const int chunk_input_size = chunk_size * 16 * 16;
    unsigned int chunk_output_size[chunks] = {};

    unsigned char *chunk_data = (unsigned char *)calloc(1, chunk_input_size);
    unsigned int chunk_data_index;

    fprintf(source, "/**** This is a generated file - do not edit ****/\n\n");

    for(int chunk_index = 0; chunk_index < chunks; chunk_index++) {
        chunk_data_index = 0;

        const int chunk_start = chunk_index * chunk_size;
        for(int codepoint = chunk_start; codepoint < chunk_start + chunk_size; codepoint++) {
            const unsigned short *glyph = font[codepoint] != NULL ? font[codepoint] : replacement;
            for(int x = 15; x >= 0; x--) {
                for(int y = 0; y < 16; y++) {
                    chunk_data[chunk_data_index++] = (glyph[y] & (1 << x)) ? 0xff : 0;
                }
            }

            if(font[codepoint] != NULL)
                free(font[codepoint]);
        }

        fprintf(source, "static const uint8_t CompressedFontTextureChunk%d[] = {\n",
                chunk_start / chunk_size);

        z_stream stream;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        if(deflateInit(&stream, COMPRESSION_LEVEL) != Z_OK)
            die("deflateInit failed");

        stream.next_in = chunk_data;
        stream.avail_in = chunk_input_size;

        do {
            unsigned char compressed_chunk_data[16384] = {};
            stream.next_out = compressed_chunk_data;
            stream.avail_out = sizeof(compressed_chunk_data);
            deflate(&stream, Z_FINISH);

            chunk_output_size[chunk_index] += sizeof(compressed_chunk_data) - stream.avail_out;
            for(size_t i = 0; i < sizeof(compressed_chunk_data) - stream.avail_out; i += 16) {
                unsigned char *d = &compressed_chunk_data[i];
                fprintf(source, "    %3d, %3d, %3d, %3d, %3d, %3d, %3d, %3d, "
                                    "%3d, %3d, %3d, %3d, %3d, %3d, %3d, %3d,\n",
                                d[ 0], d[ 1], d[ 2], d[ 3], d[ 4], d[ 5], d[ 6], d[ 7],
                                d[ 8], d[ 9], d[10], d[11], d[12], d[13], d[14], d[15]);
            }
        } while(stream.avail_out == 0);

        deflateEnd(&stream);

        fprintf(source, "};\n\n");
    }

    free(chunk_data);
    free(font);

    fprintf(source, "static const struct {\n"
                    "    size_t length;"
                    "    const uint8_t *data;"
                    "} CompressedFontTexture[%d] = {\n", chunks);
    for(int i = 0; i < chunks; i++) {
        fprintf(source, "    { %d, CompressedFontTextureChunk%d },\n",
                        chunk_output_size[i], i);
    }
    fprintf(source, "};\n\n");

    fprintf(source, "struct GlyphProperties {\n"
                    "    bool exists:1;\n"
                    "    bool isWide:1;\n"
                    "} CodepointProperties[%d] = {\n", codepoint_count);
    for(int i = 0; i < codepoint_count; i++) {
        fprintf(source, "    { %s, %s },\n",
                        properties[i].exists ? "true" : "false",
                        properties[i].isWide ? "true" : "false");
    }
    fprintf(source, "};\n");

    free(properties);

    fclose(source);
}
