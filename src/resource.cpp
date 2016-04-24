//-----------------------------------------------------------------------------
// Discovery and loading of our resources (icons, fonts, templates, etc).
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include <zlib.h>
#include <png.h>
#include <regex>
#include "solvespace.h"

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

    bool AtEnd() const {
        return pos == end;
    }

    size_t CountUntilEOL() const {
        return std::find(pos, end, '\n') - pos;
    }

    void SkipUntilEOL() {
        pos = std::find(pos, end, '\n');
    }

    char ReadChar() {
        if(AtEnd()) oops();
        return *pos++;
    }

    bool TryChar(char c) {
        if(AtEnd()) oops();
        if(*pos == c) {
            pos++;
            return true;
        } else {
            return false;
        }
    }

    void ExpectChar(char c) {
        if(ReadChar() != c) oops();
    }

    uint8_t Read4HexBits() {
        char c = ReadChar();
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

    double ReadDoubleString() {
        char *endptr;
        double d = strtod(&*pos, &endptr);
        if(&*pos == endptr) oops();
        pos += endptr - &*pos;
        return d;
    }

    bool ReadRegex(const std::regex &re, std::smatch *m) {
        if(std::regex_search(pos, end, *m, re, std::regex_constants::match_continuous)) {
            pos = (*m)[0].second;
            return true;
        } else {
            return false;
        }
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
        reader.ExpectChar(':');

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
        int glyphLength = reader.CountUntilEOL();
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

//-----------------------------------------------------------------------------
// Vector font manipulation
//-----------------------------------------------------------------------------

const static int ARC_POINTS = 8;
static void MakePwlArc(VectorFont::Contour *contour, bool isReversed,
                       const Point2d &cp, double radius, double a1, double a2) {
    if (radius < LENGTH_EPS) return;

    double aSign = 1.0;
    if(isReversed) {
        if(a1 <= a2 + LENGTH_EPS) a1 += 2.0 * M_PI;
        aSign = -1.0;
    } else {
        if(a2 <= a1 + LENGTH_EPS) a2 += 2.0 * M_PI;
    }

    double aStep = aSign * fabs(a2 - a1) / (double)ARC_POINTS;
    for(int i = 0; i <= ARC_POINTS; i++) {
        contour->points.emplace_back(cp.Plus(Point2d::FromPolar(radius, a1 + aStep * i)));
    }
}

static void MakePwlBulge(VectorFont::Contour *contour, const Point2d &v, double bulge) {
    bool reversed = bulge < 0.0;
    double alpha = atan(bulge) * 4.0;
    const Point2d &point = contour->points.back();

    Point2d middle = point.Plus(v).ScaledBy(0.5);
    double dist = point.DistanceTo(v) / 2.0;
    double angle = point.AngleTo(v);

    // alpha can't be 0.0 at this point
    double radius = fabs(dist / sin(alpha / 2.0));
    double wu = fabs(radius*radius - dist*dist);
    double h = sqrt(wu);

    if(bulge > 0.0) {
        angle += M_PI_2;
    } else {
        angle -= M_PI_2;
    }

    if (fabs(alpha) > M_PI) {
        h = -h;
    }

    Point2d center = Point2d::FromPolar(h, angle).Plus(middle);
    double a1 = center.AngleTo(point);
    double a2 = center.AngleTo(v);
    MakePwlArc(contour, reversed, center, radius, a1, a2);
}

static void GetGlyphBBox(const VectorFont::Glyph &glyph,
                         double *rminx, double *rmaxx, double *rminy, double *rmaxy) {
    double minx = 0.0, maxx = 0.0, miny = 0.0, maxy = 0.0;
    if(!glyph.contours.empty()) {
        const Point2d &start = glyph.contours[0].points[0];
        minx = maxx = start.x;
        miny = maxy = start.y;
        for(const VectorFont::Contour &c : glyph.contours) {
            for(const Point2d &p : c.points) {
                maxx = std::max(maxx, p.x);
                minx = std::min(minx, p.x);
                maxy = std::max(maxy, p.y);
                miny = std::min(miny, p.y);
            }
        }
    }

    if(rminx) *rminx = minx;
    if(rmaxx) *rmaxx = maxx;
    if(rminy) *rminy = miny;
    if(rmaxy) *rmaxy = maxy;
}

VectorFont VectorFont::From(std::string &&lffData) {
    VectorFont font = {};
    font.lffData = std::move(lffData);

    ASCIIReader reader = ASCIIReader::From(font.lffData);
    std::smatch m;
    while(reader.ReadRegex(std::regex("#\\s*(\\w+)\\s*:\\s*(.+?)\n"), &m)) {
        std::string name  = m.str(1),
                    value = m.str(2);
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name == "letterspacing") {
            font.rightSideBearing = std::stod(value);
        } else if (name == "wordspacing") {
            Glyph space = {};
            space.advanceWidth = std::stod(value);
            font.glyphs.emplace(' ', std::move(space));
        }
    }

    GetGlyphBBox(font.GetGlyph('A'), nullptr, nullptr, nullptr, &font.capHeight);
    GetGlyphBBox(font.GetGlyph('h'), nullptr, nullptr, nullptr, &font.ascender);
    GetGlyphBBox(font.GetGlyph('p'), nullptr, nullptr, &font.descender, nullptr);

    return font;
}

const VectorFont::Glyph &VectorFont::GetGlyph(char32_t codepoint) {
    auto it = glyphs.find(codepoint);
    if(it != glyphs.end()) {
        return (*it).second;
    }

    auto firstGlyph = std::find(lffData.cbegin(), lffData.cend(), '[');
    if(firstGlyph == lffData.cend()) oops();

    // Find the serialized representation in the (sorted) lff file.
    auto first = firstGlyph,
         last  = lffData.cend();
    while(first <= last) {
        auto mid = first + (last - first) / 2;
        while(mid > first) {
            if(*mid == '[' && *(mid - 1) == '\n') break;
            mid--;
        }

        // Read the codepoint.
        ASCIIReader reader = { mid, lffData.cend() };
        reader.ExpectChar('[');
        char32_t foundCodepoint = reader.Read16HexBits();
        reader.ExpectChar(']');
        reader.SkipUntilEOL();

        if(foundCodepoint > codepoint) {
            last = mid - 1;
            continue; // and first stays the same
        }
        if(foundCodepoint < codepoint) {
            first = mid + 1;
            while(first != lffData.cend()) {
                if(*first == '[' && *(first - 1) == '\n') break;
                first++;
            }
            continue; // and last stays the same
        }

        // Found the codepoint.
        VectorFont::Glyph glyph = {};

        // Read glyph contours.
        while(!reader.AtEnd()) {
            if(reader.TryChar('\n')) {
                // Skip.
            } else if(reader.TryChar('[')) {
                // End of glyph.
                if(glyph.contours.back().points.empty()) {
                    // Remove an useless empty contour, if any.
                    glyph.contours.pop_back();
                }
                break;
            } else if(reader.TryChar('C')) {
                // Another character is referenced in this one.
                char32_t baseCodepoint = reader.Read16HexBits();
                const VectorFont::Glyph &baseGlyph = GetGlyph(baseCodepoint);
                std::copy(baseGlyph.contours.begin(), baseGlyph.contours.end(),
                          std::back_inserter(glyph.contours));
            } else {
                Contour contour;
                do {
                    Point2d p;
                    p.x = reader.ReadDoubleString();
                    reader.ExpectChar(',');
                    p.y = reader.ReadDoubleString();

                    if(reader.TryChar(',')) {
                        // Point with a bulge.
                        reader.ExpectChar('A');
                        double bulge = reader.ReadDoubleString();
                        MakePwlBulge(&contour, p, bulge);
                    } else {
                        // Just a point.
                        contour.points.emplace_back(std::move(p));
                    }
                } while(reader.TryChar(';'));
                reader.ExpectChar('\n');
                glyph.contours.emplace_back(std::move(contour));
            }
        }

        // Calculate metrics.
        GetGlyphBBox(glyph, &glyph.leftSideBearing, &glyph.boundingWidth, nullptr, nullptr);
        glyph.advanceWidth = glyph.leftSideBearing + glyph.boundingWidth + rightSideBearing;

        it = glyphs.emplace(codepoint, std::move(glyph)).first;
        return (*it).second;
    }

    // Glyph doesn't exist; return replacement glyph instead.
    if(codepoint == 0xfffd) oops();
    return GetGlyph(0xfffd);
}

}
