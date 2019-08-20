//-----------------------------------------------------------------------------
// Routines to read a TrueType font as vector outlines, and generate them
// as entities, since they're always representable as either lines or
// quadratic Bezier curves.
//
// Copyright 2016 whitequark, Peter Barfuss.
//-----------------------------------------------------------------------------
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_ADVANCES_H

/* Yecch. Irritatingly, you need to do this nonsense to get the error string table,
   since nobody thought to put this exact function into FreeType itsself. */
#undef __FTERRORS_H__
#define FT_ERRORDEF(e, v, s) { (e), (s) },
#define FT_ERROR_START_LIST
#define FT_ERROR_END_LIST { 0, NULL }

struct ft_error {
    int err;
    const char *str;
};

static const struct ft_error ft_errors[] = {
#include FT_ERRORS_H
};

extern "C" const char *ft_error_string(int err) {
    const struct ft_error *e;
    for(e = ft_errors; e->str; e++)
        if(e->err == err)
            return e->str;
    return "Unknown error";
}

/* Okay, we're done with that. */
#undef FT_ERRORDEF
#undef FT_ERROR_START_LIST
#undef FT_ERROR_END_LIST

#include "solvespace.h"

//-----------------------------------------------------------------------------
// Get the list of available font filenames, and load the name for each of
// them. Only that, though, not the glyphs too.
//-----------------------------------------------------------------------------
TtfFontList::TtfFontList() {
    FT_Init_FreeType(&fontLibrary);
}

TtfFontList::~TtfFontList() {
    FT_Done_FreeType(fontLibrary);
}

void TtfFontList::LoadAll() {
    if(loaded) return;

    for(const Platform::Path &font : Platform::GetFontFiles()) {
        TtfFont tf = {};
        tf.fontFile = font;
        if(tf.LoadFromFile(fontLibrary))
            l.Add(&tf);
    }

    // Add builtin font to end of font list so it is displayed first in the UI
    {
        TtfFont tf = {};
        tf.SetResourceID("fonts/BitstreamVeraSans-Roman-builtin.ttf");
        if(tf.LoadFromResource(fontLibrary))
            l.Add(&tf);
    }

    // Sort fonts according to their actual name, not filename.
    std::sort(l.begin(), l.end(),
        [](const TtfFont &a, const TtfFont &b) { return a.name < b.name; });

    // Filter out fonts with the same family and style name. This is not
    // strictly necessarily the exact same font, but it will almost always be.
    TtfFont *it = std::unique(l.begin(), l.end(),
                              [](const TtfFont &a, const TtfFont &b) { return a.name == b.name; });
    l.RemoveLast(&l[l.n] - it);

    //! @todo identify fonts by their name and not filename, which may change
    //! between OSes.

    loaded = true;
}

TtfFont *TtfFontList::LoadFont(const std::string &font)
{
    LoadAll();

    TtfFont *tf = std::find_if(l.begin(), l.end(),
        [&font](const TtfFont &tf) { return tf.FontFileBaseName() == font; });

    if(tf != l.end()) {
        if(tf->fontFace == NULL) {
            if(tf->IsResource())
                tf->LoadFromResource(fontLibrary, /*nameOnly=*/false);
            else
                tf->LoadFromFile(fontLibrary, /*nameOnly=*/false);
        }
        return tf;
    } else {
        return NULL;
    }
}

void TtfFontList::PlotString(const std::string &font, const std::string &str,
                             SBezierList *sbl, Vector origin, Vector u, Vector v)
{
    TtfFont *tf = LoadFont(font);
    if(!str.empty() && tf != NULL) {
        tf->PlotString(str, sbl, origin, u, v);
    } else {
        // No text or no font; so draw a big X for an error marker.
        SBezier sb;
        sb = SBezier::From(origin, origin.Plus(u).Plus(v));
        sbl->l.Add(&sb);
        sb = SBezier::From(origin.Plus(v), origin.Plus(u));
        sbl->l.Add(&sb);
    }
}

double TtfFontList::AspectRatio(const std::string &font, const std::string &str)
{
    TtfFont *tf = LoadFont(font);
    if(tf != NULL) {
        return tf->AspectRatio(str);
    }

    return 0.0;
}

//-----------------------------------------------------------------------------
// Return the basename of our font filename; that's how the requests and
// entities that reference us will store it.
//-----------------------------------------------------------------------------
std::string TtfFont::FontFileBaseName() const {
    return fontFile.FileName();
}

//-----------------------------------------------------------------------------
// Convenience method to set fontFile for resource-loaded fonts as res://<path>
//-----------------------------------------------------------------------------
void TtfFont::SetResourceID(const std::string &resource) {
    fontFile = { "res://" + resource };
}

bool TtfFont::IsResource() const {
    return fontFile.raw.compare(0, 6, "res://") == 0;
}

//-----------------------------------------------------------------------------
// Load a TrueType font into memory.
//-----------------------------------------------------------------------------
bool TtfFont::LoadFromFile(FT_Library fontLibrary, bool nameOnly) {
    ssassert(!IsResource(), "Cannot load a font provided by a resource as a file.");

    FT_Open_Args args = {};
    args.flags    = FT_OPEN_PATHNAME;
    args.pathname = &fontFile.raw[0]; // FT_String is char* for historical reasons

    // We don't use OpenFile() here to let freetype do its own memory management.
    // This is OK because on Linux/OS X we just delegate to fopen and on Windows
    // we only look into C:\Windows\Fonts, which has a known short path.
    if(int fterr = FT_Open_Face(fontLibrary, &args, 0, &fontFace)) {
        dbp("freetype: loading font from file '%s' failed: %s",
            fontFile.raw.c_str(), ft_error_string(fterr));
        return false;
    }

    return ExtractTTFData(nameOnly);
}

//-----------------------------------------------------------------------------
// Load a TrueType from resource in memory. Implemented to load bundled fonts
// through theresource system.
//-----------------------------------------------------------------------------
bool TtfFont::LoadFromResource(FT_Library fontLibrary, bool nameOnly) {
    ssassert(IsResource(), "Font to be loaded as resource is not provided by a resource "
             "or does not have the 'res://' prefix.");

    size_t _size;
    // substr to cut off 'res://' (length: 6)
    const void *_buffer = Platform::LoadResource(fontFile.raw.substr(6, fontFile.raw.size()),
                                                 &_size);

    FT_Long size = static_cast<FT_Long>(_size);
    const FT_Byte *buffer = reinterpret_cast<const FT_Byte*>(_buffer);

    if(int fterr = FT_New_Memory_Face(fontLibrary, buffer, size, 0, &fontFace)) {
            dbp("freetype: loading font '%s' from memory failed: %s",
                fontFile.raw.c_str(), ft_error_string(fterr));
            return false;
    }

    return ExtractTTFData(nameOnly);
}

//-----------------------------------------------------------------------------
// Extract font information. We care about the font name and unit size.
//-----------------------------------------------------------------------------
bool TtfFont::ExtractTTFData(bool nameOnly) {
    if(int fterr = FT_Select_Charmap(fontFace, FT_ENCODING_UNICODE)) {
        dbp("freetype: loading unicode CMap for file '%s' failed: %s",
            fontFile.raw.c_str(), ft_error_string(fterr));
        FT_Done_Face(fontFace);
        fontFace = NULL;
        return false;
    }

    name = std::string(fontFace->family_name) +
           " (" + std::string(fontFace->style_name) + ")";

    if(nameOnly) {
        FT_Done_Face(fontFace);
        fontFace = NULL;
        return true;
    }

    // We always ask Freetype to give us a unit size character.
    // It uses fixed point; put the unit size somewhere in the middle of the dynamic
    // range of its 26.6 fixed point type, and adjust the factors so that the unit
    // matches cap height.
    FT_Size_RequestRec sizeRequest;
    sizeRequest.type           = FT_SIZE_REQUEST_TYPE_REAL_DIM;
    sizeRequest.width          = 1 << 16;
    sizeRequest.height         = 1 << 16;
    sizeRequest.horiResolution = 128;
    sizeRequest.vertResolution = 128;
    if(int fterr = FT_Request_Size(fontFace, &sizeRequest)) {
        dbp("freetype: cannot set character size: %s",
            ft_error_string(fterr));
        FT_Done_Face(fontFace);
        fontFace = NULL;
        return false;
    }

    char chr = 'A';
    uint32_t gid = FT_Get_Char_Index(fontFace, 'A');
    if (gid == 0) {
        dbp("freetype: CID-to-GID mapping for CID 0x%04x failed: %s; using CID as GID",
            chr, ft_error_string(gid));
        dbp("Assuming cap height is the same as requested height (this is likely wrong).");
        capHeight = (double)sizeRequest.height;
    }

    if(gid) {
        if(int fterr = FT_Load_Glyph(fontFace, gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING)) {
            dbp("freetype: cannot load glyph for GID 0x%04x: %s",
                gid, ft_error_string(fterr));
            FT_Done_Face(fontFace);
            fontFace = NULL;
            return false;
        }

        FT_BBox bbox;
        FT_Outline_Get_CBox(&fontFace->glyph->outline, &bbox);
        capHeight = (double)bbox.yMax;
    }

    return true;
}

typedef struct OutlineData {
    Vector       origin, u, v; // input parameters
    SBezierList *beziers;      // output bezier list
    float        factor;       // ratio between freetype and solvespace coordinates
    FT_Pos       bx;           // x offset of the current glyph
    FT_Pos       px, py;       // current point
} OutlineData;

static Vector Transform(OutlineData *data, FT_Pos x, FT_Pos y) {
    Vector r = data->origin;
    r = r.Plus(data->u.ScaledBy((float)(data->bx + x) * data->factor));
    r = r.Plus(data->v.ScaledBy((float)y * data->factor));
    return r;
}

static int MoveTo(const FT_Vector *p, void *cc)
{
    OutlineData *data = (OutlineData *) cc;
    data->px = p->x;
    data->py = p->y;
    return 0;
}

static int LineTo(const FT_Vector *p, void *cc)
{
    OutlineData *data = (OutlineData *) cc;
    SBezier sb = SBezier::From(
        Transform(data, data->px, data->py),
        Transform(data, p->x,     p->y));
    data->beziers->l.Add(&sb);
    data->px = p->x;
    data->py = p->y;
    return 0;
}

static int ConicTo(const FT_Vector *c, const FT_Vector *p, void *cc)
{
    OutlineData *data = (OutlineData *) cc;
    SBezier sb = SBezier::From(
        Transform(data, data->px, data->py),
        Transform(data, c->x,     c->y),
        Transform(data, p->x,     p->y));
    data->beziers->l.Add(&sb);
    data->px = p->x;
    data->py = p->y;
    return 0;
}

static int CubicTo(const FT_Vector *c1, const FT_Vector *c2, const FT_Vector *p, void *cc)
{
    OutlineData *data = (OutlineData *) cc;
    SBezier sb = SBezier::From(
        Transform(data, data->px, data->py),
        Transform(data, c1->x,    c1->y),
        Transform(data, c2->x,    c2->y),
        Transform(data, p->x,     p->y));
    data->beziers->l.Add(&sb);
    data->px = p->x;
    data->py = p->y;
    return 0;
}

void TtfFont::PlotString(const std::string &str,
                         SBezierList *sbl, Vector origin, Vector u, Vector v)
{
    ssassert(fontFace != NULL, "Expected font face to be loaded");

    FT_Outline_Funcs outlineFuncs;
    outlineFuncs.move_to  = MoveTo;
    outlineFuncs.line_to  = LineTo;
    outlineFuncs.conic_to = ConicTo;
    outlineFuncs.cubic_to = CubicTo;
    outlineFuncs.shift    = 0;
    outlineFuncs.delta    = 0;

    FT_Pos dx = 0;
    for(char32_t cid : ReadUTF8(str)) {
        uint32_t gid = FT_Get_Char_Index(fontFace, cid);
        if (gid == 0) {
            dbp("freetype: CID-to-GID mapping for CID 0x%04x failed: %s; using CID as GID",
                cid, ft_error_string(gid));
            gid = cid;
        }

        /*
         * Stupid hacks:
         *  - if we want fake-bold, use FT_Outline_Embolden(). This actually looks
         *    quite good.
         *  - if we want fake-italic, apply a shear transform [1 s s 1 0 0] here using
         *    FT_Set_Transform. This looks decent at small font sizes and bad at larger
         *    ones, antialiasing mitigates this considerably though.
         */
        if(int fterr = FT_Load_Glyph(fontFace, gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING)) {
            dbp("freetype: cannot load glyph for GID 0x%04x: %s",
                gid, ft_error_string(fterr));
            return;
        }

        /* A point that has x = xMin should be plotted at (dx0 + lsb); fix up
         * our x-position so that the curve-generating code will put stuff
         * at the right place.
         *
         * There's no point in getting the glyph BBox here - not only can it be
         * needlessly slow sometimes, but because we're about to render a single glyph,
         * what we want actually *is* the CBox.
         *
         * This is notwithstanding that this makes extremely little sense, this
         * looks like a workaround for either mishandling the start glyph on a line,
         * or as a really hacky pseudo-track-kerning (in which case it works better than
         * one would expect! especially since most fonts don't set track kerning).
         */
        FT_BBox cbox;
        FT_Outline_Get_CBox(&fontFace->glyph->outline, &cbox);
        FT_Pos bx = dx - cbox.xMin;
        // Yes, this is what FreeType calls left-side bearing.
        // Then interchangeably uses that with "left-side bearing". Sigh.
        bx += fontFace->glyph->metrics.horiBearingX;

        OutlineData data = {};
        data.origin  = origin;
        data.u       = u;
        data.v       = v;
        data.beziers = sbl;
        data.factor  = (float)(1.0 / capHeight);
        data.bx      = bx;
        if(int fterr = FT_Outline_Decompose(&fontFace->glyph->outline, &outlineFuncs, &data)) {
            dbp("freetype: bezier decomposition failed (gid %d): %s",
                gid, ft_error_string(fterr));
        }

        // And we're done, so advance our position by the requested advance
        // width, plus the user-requested extra advance.
        dx += fontFace->glyph->advance.x;
    }
}

double TtfFont::AspectRatio(const std::string &str) {
    ssassert(fontFace != NULL, "Expected font face to be loaded");

    // We always request a unit size character, so the aspect ratio is the same as advance length.
    double dx = 0;
    for(char32_t chr : ReadUTF8(str)) {
        uint32_t gid = FT_Get_Char_Index(fontFace, chr);
        if (gid == 0) {
            dbp("freetype: CID-to-GID mapping for CID 0x%04x failed: %s; using CID as GID",
                chr, ft_error_string(gid));
        }

        if(int fterr = FT_Load_Glyph(fontFace, gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING)) {
            dbp("freetype: cannot load glyph (GID 0x%04x): %s",
                gid, ft_error_string(fterr));
            break;
        }

        dx += (double)fontFace->glyph->advance.x / capHeight;
    }

    return dx;
}
