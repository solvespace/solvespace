//-----------------------------------------------------------------------------
// Discovery and loading of our resources (icons, fonts, templates, etc).
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include <zlib.h>
#include <png.h>

namespace SolveSpace {

//-----------------------------------------------------------------------------
// Resource loading functions
//-----------------------------------------------------------------------------

std::string LoadString(const std::string &name) {
    size_t size;
    const void *data = LoadResource(name, &size);
    if(data == NULL) oops();

    return std::string(static_cast<const char *>(data), size);
}

std::string LoadStringFromGzip(const std::string &name) {
    size_t size;
    const void *data = LoadResource(name, &size);
    if(data == NULL) oops();

    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    if(inflateInit2(&stream, /*decode gzip header*/16) != Z_OK)
        oops();

    // Extract length mod 2**32 from the gzip trailer.
    std::string result;
    if(size < 4) oops();
    result.resize(*(uint32_t *)((uintptr_t)data + size - 4));

    stream.next_in = (Bytef *)data;
    stream.avail_in = size;
    stream.next_out = (Bytef *)&result[0];
    stream.avail_out = result.length();
    if(inflate(&stream, Z_NO_FLUSH) != Z_STREAM_END)
        oops();
    if(stream.avail_out != 0)
        oops();

    inflateEnd(&stream);

    return result;
}

Pixmap LoadPNG(const std::string &name) {
    size_t size;
    const void *data = LoadResource(name, &size);
    if(data == NULL) oops();

    Pixmap pixmap = Pixmap::FromPNG(static_cast<const uint8_t *>(data), size);
    if(pixmap.IsEmpty()) oops();

    return pixmap;
}

//-----------------------------------------------------------------------------
// Pixmap manipulation
//-----------------------------------------------------------------------------

void Pixmap::Clear() {
    *this = {};
}

RgbaColor Pixmap::GetPixel(size_t x, size_t y) const {
    uint8_t *pixel = &data[y * stride + x * GetBytesPerPixel()];

    if(hasAlpha) {
        return RgbaColor::From(pixel[0], pixel[1], pixel[2], pixel[3]);
    } else {
        return RgbaColor::From(pixel[0], pixel[1], pixel[2]);
    }
}

static Pixmap ReadPNGIntoPixmap(png_struct *png_ptr, png_info *info_ptr) {
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_GRAY_TO_RGB, NULL);

    Pixmap pixmap = {};
    pixmap.width    = png_get_image_width(png_ptr, info_ptr);
    pixmap.height   = png_get_image_height(png_ptr, info_ptr);
    pixmap.hasAlpha = png_get_color_type(png_ptr, info_ptr) & PNG_COLOR_MASK_ALPHA;

    size_t stride = pixmap.width * pixmap.GetBytesPerPixel();
    if(stride % 4 != 0) stride += 4 - stride % 4;
    pixmap.stride = stride;

    pixmap.data = std::unique_ptr<uint8_t[]>(new uint8_t[pixmap.stride * pixmap.height]);
    uint8_t **rows = png_get_rows(png_ptr, info_ptr);
    for(size_t y = 0; y < pixmap.height; y++) {
        memcpy(&pixmap.data[pixmap.stride * y], rows[y],
               pixmap.width * pixmap.GetBytesPerPixel());
    }

    return pixmap;
}

Pixmap Pixmap::FromPNG(const uint8_t *data, size_t size) {
    Pixmap pixmap = {};
    struct Slice { const uint8_t *data; size_t size; };
    Slice dataSlice = { data, size };
    png_struct *png_ptr = NULL;
    png_info *info_ptr = NULL;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png_ptr) goto exit;
    info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr) goto exit;

    if(setjmp(png_jmpbuf(png_ptr))) goto exit;

    png_set_read_fn(png_ptr, &dataSlice,
        [](png_struct *png_ptr, uint8_t *data, size_t size) {
            Slice *dataSlice = (Slice *)png_get_io_ptr(png_ptr);
            if(size <= dataSlice->size) {
                memcpy(data, dataSlice->data, size);
                dataSlice->data += size;
                dataSlice->size -= size;
            } else {
                png_error(png_ptr, "EOF");
            }
        });

    pixmap = ReadPNGIntoPixmap(png_ptr, info_ptr);

exit:
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return pixmap;
}

Pixmap Pixmap::FromPNG(FILE *f) {
    Pixmap pixmap = {};

    png_struct *png_ptr = NULL;
    png_info *info_ptr = NULL;

    uint8_t header[8];
    if(fread(header, 1, sizeof(header), f) != sizeof(header)) goto exit;
    if(png_sig_cmp(header, 0, sizeof(header))) goto exit;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png_ptr) goto exit;
    info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr) goto exit;

    if(setjmp(png_jmpbuf(png_ptr))) goto exit;

    png_init_io(png_ptr, f);
    png_set_sig_bytes(png_ptr, sizeof(header));

    pixmap = ReadPNGIntoPixmap(png_ptr, info_ptr);

exit:
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return pixmap;
}

//-----------------------------------------------------------------------------
// ASCII sequence parsing
//-----------------------------------------------------------------------------

class ASCIIReader {
public:
    std::string::const_iterator pos, end;

    static ASCIIReader From(const std::string &str) {
        return ASCIIReader({ str.cbegin(), str.cend() });
    }

    size_t LengthToEOL() {
        return std::find(pos, end, '\n') - pos;
    }

    void ReadChar(char c) {
        if(pos == end) oops();
        if(*pos++ != c) oops();
    }

    uint8_t Read4HexBits() {
        if(pos == end) oops();
        char c = *pos++;
        if(c >= '0' && c <= '9') {
            return c - '0';
        } else if(c >= 'a' && c <= 'f') {
            return 10 + (c - 'a');
        } else if(c >= 'A' && c <= 'F') {
            return 10 + (c - 'A');
        } else oops();
    }

    uint8_t Read8HexBits() {
        uint8_t h = Read4HexBits(),
                l = Read4HexBits();
        return (h << 4) + l;
    }

    uint16_t Read16HexBits() {
        uint16_t h = Read8HexBits(),
                 l = Read8HexBits();
        return (h << 8) + l;
    }
};

//-----------------------------------------------------------------------------
// Bitmap font manipulation
//-----------------------------------------------------------------------------

static const size_t CHARS_PER_ROW = BitmapFont::TEXTURE_DIM / 16;

static uint8_t *BitmapFontTextureRow(uint8_t *texture, uint16_t position, size_t y) {
    // position = 0;
    size_t col = position % CHARS_PER_ROW,
           row = position / CHARS_PER_ROW;
    return &texture[BitmapFont::TEXTURE_DIM * (16 * row + y) + 16 * col];
}

BitmapFont BitmapFont::From(std::string &&unifontData) {
    BitmapFont font  = {};
    font.unifontData = std::move(unifontData);
    font.texture     = std::unique_ptr<uint8_t[]>(new uint8_t[TEXTURE_DIM * TEXTURE_DIM]);
    return font;
}

void BitmapFont::AddGlyph(char32_t codepoint, const Pixmap &pixmap) {
    if(pixmap.width != 8 && pixmap.width != 16 && pixmap.height != 16) oops();
    if(glyphs.find(codepoint) != glyphs.end()) oops();
    if(nextPosition == 0xffff) oops();

    BitmapFont::Glyph glyph = {};
    glyph.advanceCells = pixmap.width / 8;
    glyph.position     = nextPosition++;
    glyphs.emplace(codepoint, std::move(glyph));

    for(size_t y = 0; y < pixmap.height; y++) {
        uint8_t *row = BitmapFontTextureRow(texture.get(), glyph.position, y);
        for(size_t x = 0; x < pixmap.width; x++) {
            if(pixmap.GetPixel(x, y).ToPackedInt() != 0) {
                row[x] = 255;
            }
        }
    }
}

const BitmapFont::Glyph &BitmapFont::GetGlyph(char32_t codepoint) {
    auto it = glyphs.find(codepoint);
    if(it != glyphs.end()) {
        return (*it).second;
    }

    if(nextPosition == 0xffff) oops();

    // Find the hex representation in the (sorted) Unifont file.
    auto first = unifontData.cbegin(),
         last  = unifontData.cend();
    while(first <= last) {
        auto mid = first + (last - first) / 2;
        while(mid != unifontData.cbegin()) {
            if(*mid == '\n') {
                mid++;
                break;
            }
            mid--;
        }

        // Read the codepoint.
        ASCIIReader reader = { mid, unifontData.cend() };
        char32_t foundCodepoint = reader.Read16HexBits();
        reader.ReadChar(':');

        if(foundCodepoint > codepoint) {
            last = mid - 1;
            continue; // and first stays the same
        }
        if(foundCodepoint < codepoint) {
            first = mid + 1;
            while(first != unifontData.cend()) {
                if(*first == '\n') break;
                first++;
            }
            continue; // and last stays the same
        }

        // Found the codepoint.
        Glyph glyph = {};
        glyph.position = nextPosition++;

        // Read glyph bits.
        unsigned short glyphBits[16];
        int glyphLength = reader.LengthToEOL();
        if(glyphLength == 4 * 16) {
            glyph.advanceCells = 2;
            for(size_t i = 0; i < 16; i++) {
                glyphBits[i] = reader.Read16HexBits();
            }
        } else if(glyphLength == 2 * 16) {
            glyph.advanceCells = 1;
            for(size_t i = 0; i < 16; i++) {
                glyphBits[i] = (uint16_t)reader.Read8HexBits() << 8;
            }
        } else oops();

        // Fill in the texture (one texture byte per glyph bit).
        for(size_t y = 0; y < 16; y++) {
            uint8_t *row = BitmapFontTextureRow(texture.get(), glyph.position, y);
            for(size_t x = 0; x < 16; x++) {
                if(glyphBits[y] & (1 << (15 - x))) {
                    row[x] = 255;
                }
            }
        }

        it = glyphs.emplace(codepoint, std::move(glyph)).first;
        return (*it).second;
    }

    // Glyph doesn't exist; return replacement glyph instead.
    if(codepoint == 0xfffd) oops();
    return GetGlyph(0xfffd);
}

bool BitmapFont::LocateGlyph(char32_t codepoint,
                             double *s0, double *t0, double *s1, double *t1,
                             size_t *w, size_t *h) {
    bool textureUpdated = (glyphs.find(codepoint) == glyphs.end());
    const Glyph &glyph = GetGlyph(codepoint);
    *w  = glyph.advanceCells * 8;
    *h  = 16;
    *s0 = (16.0 * (glyph.position % CHARS_PER_ROW)) / TEXTURE_DIM;
    *s1 = *s0 + (double)(*w) / TEXTURE_DIM;
    *t0 = (16.0 * (glyph.position / CHARS_PER_ROW)) / TEXTURE_DIM;
    *t1 = *t0 + (double)(*h) / TEXTURE_DIM;
    return textureUpdated;
}

}
