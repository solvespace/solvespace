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
    return std::string(static_cast<const char *>(data), size);
}

std::string LoadStringFromGzip(const std::string &name) {
    size_t size;
    const void *data = LoadResource(name, &size);

    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    ssassert(inflateInit2(&stream, /*decode gzip header*/16) == Z_OK,
             "Cannot start inflation");

    // Extract length mod 2**32 from the gzip trailer.
    std::string result;
    ssassert(size >= 4, "Resource too small to have gzip trailer");
    result.resize(*(uint32_t *)((uintptr_t)data + size - 4));

    stream.next_in = (Bytef *)data;
    stream.avail_in = size;
    stream.next_out = (Bytef *)&result[0];
    stream.avail_out = result.length();
    ssassert(inflate(&stream, Z_NO_FLUSH) == Z_STREAM_END, "Cannot inflate resource");
    ssassert(stream.avail_out == 0, "Inflated resource larger than what trailer indicates");

    inflateEnd(&stream);

    return result;
}

std::shared_ptr<Pixmap> LoadPng(const std::string &name) {
    size_t size;
    const void *data = LoadResource(name, &size);

    std::shared_ptr<Pixmap> pixmap = Pixmap::FromPng(static_cast<const uint8_t *>(data), size);
    ssassert(pixmap != nullptr, "Cannot load pixmap");

    return pixmap;
}

//-----------------------------------------------------------------------------
// Pixmap manipulation
//-----------------------------------------------------------------------------

size_t Pixmap::GetBytesPerPixel() const {
    switch(format) {
        case Format::RGBA: return 4;
        case Format::RGB:  return 3;
        case Format::A:    return 1;
    }
    ssassert(false, "Unexpected pixmap format");
}

RgbaColor Pixmap::GetPixel(size_t x, size_t y) const {
    const uint8_t *pixel = &data[y * stride + x * GetBytesPerPixel()];

    switch(format) {
        case Format::RGBA:
            return RgbaColor::From(pixel[0], pixel[1], pixel[2], pixel[3]);

        case Format::RGB:
            return RgbaColor::From(pixel[0], pixel[1], pixel[2],      255);

        case Format::A:
            return RgbaColor::From(     255,      255,      255, pixel[0]);
    }
    ssassert(false, "Unexpected resource format");
}

static std::shared_ptr<Pixmap> ReadPngIntoPixmap(png_struct *png_ptr, png_info *info_ptr) {
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_GRAY_TO_RGB, NULL);

    std::shared_ptr<Pixmap> pixmap = std::make_shared<Pixmap>();
    pixmap->width    = png_get_image_width(png_ptr, info_ptr);
    pixmap->height   = png_get_image_height(png_ptr, info_ptr);
    if((png_get_color_type(png_ptr, info_ptr) & PNG_COLOR_MASK_ALPHA) != 0) {
        pixmap->format = Pixmap::Format::RGBA;
    } else {
        pixmap->format = Pixmap::Format::RGB;
    }

    size_t stride = pixmap->width * pixmap->GetBytesPerPixel();
    if(stride % 4 != 0) stride += 4 - stride % 4;
    pixmap->stride = stride;

    pixmap->data = std::vector<uint8_t>(pixmap->stride * pixmap->height);
    uint8_t **rows = png_get_rows(png_ptr, info_ptr);
    for(size_t y = 0; y < pixmap->height; y++) {
        memcpy(&pixmap->data[pixmap->stride * y], rows[y],
               pixmap->width * pixmap->GetBytesPerPixel());
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return pixmap;
}

std::shared_ptr<Pixmap> Pixmap::FromPng(const uint8_t *data, size_t size) {
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

    return ReadPngIntoPixmap(png_ptr, info_ptr);

exit:
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return nullptr;
}

std::shared_ptr<Pixmap> Pixmap::ReadPng(FILE *f) {
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

    return ReadPngIntoPixmap(png_ptr, info_ptr);

exit:
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return nullptr;
}

bool Pixmap::WritePng(FILE *f, bool flip) {
    int colorType;
    switch(format) {
        case Format::RGBA: colorType = PNG_COLOR_TYPE_RGBA; break;
        case Format::RGB:  colorType = PNG_COLOR_TYPE_RGB;  break;
        case Format::A:    ssassert(false, "Unexpected pixmap format");
    }

    std::vector<uint8_t *> rows;
    for(size_t y = 0; y < height; y++) {
        if(flip) {
            rows.push_back(&data[stride * y]);
        } else {
            rows.push_back(&data[stride * (height - y)]);
        }
    }

    png_struct *png_ptr = NULL;
    png_info *info_ptr = NULL;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png_ptr) goto exit;
    info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr) goto exit;

    if(setjmp(png_jmpbuf(png_ptr))) goto exit;

    png_init_io(png_ptr, f);
    png_set_IHDR(png_ptr, info_ptr, width, height, 8,
                 colorType, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, &rows[0]);
    png_write_end(png_ptr, info_ptr);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    return true;

exit:
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return false;
}

std::shared_ptr<Pixmap> Pixmap::Create(Format format, size_t width, size_t height) {
    std::shared_ptr<Pixmap> pixmap = std::make_shared<Pixmap>();
    pixmap->format = format;
    pixmap->width  = width;
    pixmap->height = height;
    // Align to fulfill OpenGL texture requirements.
    size_t stride = pixmap->width * pixmap->GetBytesPerPixel();
    if(stride % 4 != 0) stride += 4 - stride % 4;
    pixmap->stride = stride;
    pixmap->data   = std::vector<uint8_t>(pixmap->stride * pixmap->height);
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
        ssassert(!AtEnd(), "Unexpected EOF");
        return *pos++;
    }

    bool TryChar(char c) {
        ssassert(!AtEnd(), "Unexpected EOF");
        if(*pos == c) {
            pos++;
            return true;
        } else {
            return false;
        }
    }

    void ExpectChar(char c) {
        ssassert(ReadChar() == c, "Unexpected character");
    }

    uint8_t Read4HexBits() {
        char c = ReadChar();
        if(c >= '0' && c <= '9') {
            return c - '0';
        } else if(c >= 'a' && c <= 'f') {
            return 10 + (c - 'a');
        } else if(c >= 'A' && c <= 'F') {
            return 10 + (c - 'A');
        } else ssassert(false, "Unexpected hex digit");
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
        ssassert(&*pos != endptr, "Cannot read a double-precision number");
        pos += endptr - &*pos;
        return d;
    }

    bool TryRegex(const std::regex &re, std::smatch *m) {
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

static uint8_t *BitmapFontTextureRow(std::shared_ptr<Pixmap> texture,
                                     uint16_t position, size_t y) {
    // position = 0;
    size_t col = position % (texture->width / 16),
           row = position / (texture->width / 16);
    return &texture->data[texture->stride * (16 * row + y) + 16 * col];
}

BitmapFont BitmapFont::From(std::string &&unifontData) {
    BitmapFont font  = {};
    font.unifontData = std::move(unifontData);
    font.texture     = Pixmap::Create(Pixmap::Format::A, 1024, 1024);

    return font;
}

void BitmapFont::AddGlyph(char32_t codepoint, std::shared_ptr<const Pixmap> pixmap) {
    ssassert((pixmap->width == 8 || pixmap->width == 16) && pixmap->height == 16,
             "Unexpected pixmap dimensions");
    ssassert(pixmap->format == Pixmap::Format::RGB,
             "Unexpected pixmap format");
    ssassert(glyphs.find(codepoint) == glyphs.end(),
             "Glyph with this codepoint already exists");
    ssassert(nextPosition != 0xffff,
             "Too many glyphs for current texture size");

    BitmapFont::Glyph glyph = {};
    glyph.advanceCells = pixmap->width / 8;
    glyph.position     = nextPosition++;
    glyphs.emplace(codepoint, std::move(glyph));

    for(size_t y = 0; y < pixmap->height; y++) {
        uint8_t *row = BitmapFontTextureRow(texture, glyph.position, y);
        for(size_t x = 0; x < pixmap->width; x++) {
            if((pixmap->GetPixel(x, y).ToPackedInt() & 0xffffff) != 0) {
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

    ssassert(nextPosition != 0xffff,
             "Too many glyphs for current texture size");

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
        } else ssassert(false, "Unexpected glyph bitmap length");

        // Fill in the texture (one texture byte per glyph bit).
        for(size_t y = 0; y < 16; y++) {
            uint8_t *row = BitmapFontTextureRow(texture, glyph.position, y);
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
    ssassert(codepoint != 0xfffd, "Cannot parse replacement glyph");
    return GetGlyph(0xfffd);
}

bool BitmapFont::LocateGlyph(char32_t codepoint,
                             double *s0, double *t0, double *s1, double *t1,
                             size_t *w, size_t *h) {
    bool textureUpdated = (glyphs.find(codepoint) == glyphs.end());
    const Glyph &glyph = GetGlyph(codepoint);
    *w  = glyph.advanceCells * 8;
    *h  = 16;
    *s0 = (16.0 * (glyph.position % (texture->width / 16))) / texture->width;
    *s1 = *s0 + (double)(*w) / texture->width;
    *t0 = (16.0 * (glyph.position / (texture->width / 16))) / texture->height;
    *t1 = *t0 + (double)(*h) / texture->height;
    return textureUpdated;
}

size_t BitmapFont::GetWidth(char32_t codepoint) {
    if(codepoint >= 0xe000 && codepoint <= 0xefff) {
        // These are special-cased because checkboxes predate support for 2 cell wide
        // characters; and so all Printf() calls pad them with spaces.
        return 1;
    }

    return GetGlyph(codepoint).advanceCells;
}

size_t BitmapFont::GetWidth(const std::string &str) {
    size_t width = 0;
    for(char32_t codepoint : ReadUTF8(str)) {
        width += GetWidth(codepoint);
    }
    return width;
}

BitmapFont *BitmapFont::Builtin() {
    static BitmapFont Font;
    if(Font.IsEmpty()) {
        Font = BitmapFont::From(LoadStringFromGzip("fonts/unifont.hex.gz"));
        // Unifont doesn't have a glyph for U+0020.
        Font.AddGlyph(0x0020, Pixmap::Create(Pixmap::Format::RGB, 8, 16));
        Font.AddGlyph(0xE000, LoadPng("fonts/private/0-check-false.png"));
        Font.AddGlyph(0xE001, LoadPng("fonts/private/1-check-true.png"));
        Font.AddGlyph(0xE002, LoadPng("fonts/private/2-radio-false.png"));
        Font.AddGlyph(0xE003, LoadPng("fonts/private/3-radio-true.png"));
        Font.AddGlyph(0xE004, LoadPng("fonts/private/4-stipple-dot.png"));
        Font.AddGlyph(0xE005, LoadPng("fonts/private/5-stipple-dash-long.png"));
        Font.AddGlyph(0xE006, LoadPng("fonts/private/6-stipple-dash.png"));
        Font.AddGlyph(0xE007, LoadPng("fonts/private/7-stipple-zigzag.png"));
    }
    return &Font;
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
    while(reader.TryRegex(std::regex("#\\s*(\\w+)\\s*:\\s*(.+?)\n"), &m)) {
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

    ssassert(!font.IsEmpty(), "Expected to load a font");
    return font;
}

const VectorFont::Glyph &VectorFont::GetGlyph(char32_t codepoint) {
    auto it = glyphs.find(codepoint);
    if(it != glyphs.end()) {
        return (*it).second;
    }

    auto firstGlyph = std::find(lffData.cbegin(), lffData.cend(), '[');
    ssassert(firstGlyph != lffData.cend(), "Vector font contains no glyphs");

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
    ssassert(codepoint != 0xfffd, "Cannot parse replacement glyph");
    return GetGlyph(0xfffd);
}

VectorFont *VectorFont::Builtin() {
    static VectorFont Font;
    if(Font.IsEmpty()) {
        Font = VectorFont::From(LoadStringFromGzip("fonts/unicode.lff.gz"));
    }
    return &Font;
}

double VectorFont::GetCapHeight(double forCapHeight) const {
    ssassert(!IsEmpty(), "Expected a loaded font");

    return forCapHeight;
}

double VectorFont::GetHeight(double forCapHeight) const {
    ssassert(!IsEmpty(), "Expected a loaded font");

    return (ascender - descender) * (forCapHeight / capHeight);
}

double VectorFont::GetWidth(double forCapHeight, const std::string &str) {
    ssassert(!IsEmpty(), "Expected a loaded font");

    double width = 0;
    for(char32_t codepoint : ReadUTF8(str)) {
        width += GetGlyph(codepoint).advanceWidth;
    }
    width -= rightSideBearing;
    return width * (forCapHeight / capHeight);
}

Vector VectorFont::GetExtents(double forCapHeight, const std::string &str) {
    Vector ex = {};
    ex.x = GetWidth(forCapHeight, str);
    ex.y = GetHeight(forCapHeight);
    return ex;
}

void VectorFont::Trace(double forCapHeight, Vector o, Vector u, Vector v, const std::string &str,
                       std::function<void(Vector, Vector)> traceEdge) {
    ssassert(!IsEmpty(), "Expected a loaded font");

    double scale = (forCapHeight / capHeight);
    u = u.ScaledBy(scale);
    v = v.ScaledBy(scale);

    for(char32_t codepoint : ReadUTF8(str)) {
        const Glyph &glyph = GetGlyph(codepoint);

        for(const VectorFont::Contour &contour : glyph.contours) {
            Vector prevp;
            bool penUp = true;
            for(const Point2d &pt : contour.points) {
                Vector p = o.Plus(u.ScaledBy(pt.x))
                            .Plus(v.ScaledBy(pt.y));
                if(!penUp) traceEdge(prevp, p);
                prevp = p;
                penUp = false;
            }
        }

        o = o.Plus(u.ScaledBy(glyph.advanceWidth));
    }
}

void VectorFont::Trace(double forCapHeight, Vector o, Vector u, Vector v, const std::string &str,
                       std::function<void(Vector, Vector)> traceEdge, const Camera &camera) {
    ssassert(!IsEmpty(), "Expected a loaded font");

    // Perform grid-fitting only when the text is parallel to the view plane.
    if(camera.hasPixels && !(u.WithMagnitude(1).Equals(camera.projRight) &&
                             v.WithMagnitude(1).Equals(camera.projUp))) {
        return Trace(forCapHeight, o, u, v, str, traceEdge);
    }

    double scale = forCapHeight / capHeight;
    u = u.ScaledBy(scale);
    v = v.ScaledBy(scale);

    for(char32_t codepoint : ReadUTF8(str)) {
        const Glyph &glyph = GetGlyph(codepoint);
        double actualWidth = std::max(1.0, glyph.boundingWidth);

        // Align (o+lsb), (o+lsb+u) and (o+lsb+v) to pixel grid.
        Vector ao =  o.Plus(u.ScaledBy(glyph.leftSideBearing));
        Vector au = ao.Plus(u.ScaledBy(actualWidth));
        Vector av = ao.Plus(v.ScaledBy(capHeight));

        ao = camera.AlignToPixelGrid(ao);
        au = camera.AlignToPixelGrid(au);
        av = camera.AlignToPixelGrid(av);

        au = au.Minus(ao).ScaledBy(1.0 / actualWidth);
        av = av.Minus(ao).ScaledBy(1.0 / capHeight);

        for(const VectorFont::Contour &contour : glyph.contours) {
            Vector prevp;
            bool penUp = true;
            for(const Point2d &pt : contour.points) {
                Vector p = ao.Plus(au.ScaledBy(pt.x - glyph.leftSideBearing))
                             .Plus(av.ScaledBy(pt.y));
                if(!penUp) traceEdge(prevp, p);
                prevp = p;
                penUp = false;
            }
        }

        o = o.Plus(u.ScaledBy(glyph.advanceWidth));
    }
}

}
