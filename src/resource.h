//-----------------------------------------------------------------------------
// Discovery and loading of our resources (icons, fonts, templates, etc).
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------

#ifndef __RESOURCE_H
#define __RESOURCE_H

class Point2d;
class Pixmap;

// Only the following function is platform-specific.
// It returns a pointer to resource contents that is aligned to at least
// sizeof(void*) and has a global lifetime, or NULL if a resource with
// the specified name does not exist.
const void *LoadResource(const std::string &name, size_t *size);

std::string LoadString(const std::string &name);
std::string LoadStringFromGzip(const std::string &name);
Pixmap LoadPNG(const std::string &name);

class Pixmap {
public:
    size_t                     width;
    size_t                     height;
    size_t                     stride;
    bool                       hasAlpha;
    std::vector<uint8_t>       data;

    static Pixmap FromPNG(const uint8_t *data, size_t size);
    static Pixmap FromPNG(FILE *f);

    bool IsEmpty() const { return width == 0 && height == 0; }
    size_t GetBytesPerPixel() const { return hasAlpha ? 4 : 3; }
    RgbaColor GetPixel(size_t x, size_t y) const;

    void Clear();
};

class BitmapFont {
public:
    struct Glyph {
        uint8_t  advanceCells;
        uint16_t position;
    };

    static const size_t TEXTURE_DIM = 1024;

    std::string                unifontData;
    std::map<char32_t, Glyph>  glyphs;
    std::vector<uint8_t>       texture;
    uint16_t                   nextPosition;

    static BitmapFont From(std::string &&unifontData);

    bool IsEmpty() const { return unifontData.empty(); }
    const Glyph &GetGlyph(char32_t codepoint);
    bool LocateGlyph(char32_t codepoint, double *s0, double *t0, double *s1, double *t1,
                     size_t *advanceWidth, size_t *boundingHeight);

    void AddGlyph(char32_t codepoint, const Pixmap &pixmap);
};

class VectorFont {
public:
    struct Contour {
        std::vector<Point2d>   points;
    };

    struct Glyph {
        std::vector<Contour>   contours;
        double                 leftSideBearing;
        double                 boundingWidth;
        double                 advanceWidth;
    };

    std::string                lffData;
    std::map<char32_t, Glyph>  glyphs;
    double                     rightSideBearing;
    double                     capHeight;
    double                     ascender;
    double                     descender;

    static VectorFont From(std::string &&lffData);

    bool IsEmpty() const { return lffData.empty(); }

    const Glyph &GetGlyph(char32_t codepoint);
};

#endif
