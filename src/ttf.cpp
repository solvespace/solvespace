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

#undef HAVE_STDINT_H /* no thanks, we have our own config.h */

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

    for(const std::string &font : GetFontFiles()) {
        TtfFont tf = {};
        tf.fontFile = font;
        if(tf.LoadFromFile(fontLibrary))
            l.Add(&tf);
    }

    // Sort fonts according to their actual name, not filename.
    std::sort(&l.elem[0], &l.elem[l.n],
        [](const TtfFont &a, const TtfFont &b) { return a.name < b.name; });

    // Filter out fonts with the same family and style name. This is not
    // strictly necessarily the exact same font, but it will almost always be.
    TtfFont *it = std::unique(&l.elem[0], &l.elem[l.n],
        [](const TtfFont &a, const TtfFont &b) { return a.name == b.name; });
    l.RemoveLast(&l.elem[l.n] - it);

    // TODO: identify fonts by their name and not filename, which may change
    // between OSes.

    loaded = true;
}

void TtfFontList::PlotString(const std::string &font, const std::string &str,
                             SBezierList *sbl, Vector origin, Vector u, Vector v)
{
    LoadAll();

    TtfFont *tf = std::find_if(&l.elem[0], &l.elem[l.n],
        [&](const TtfFont &tf) { return tf.FontFileBaseName() == font; });

    if(!str.empty() && tf != &l.elem[l.n]) {
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

//-----------------------------------------------------------------------------
// Return the basename of our font filename; that's how the requests and
// entities that reference us will store it.
//-----------------------------------------------------------------------------
std::string TtfFont::FontFileBaseName() const {
    std::string baseName = fontFile;
    size_t pos = baseName.rfind(PATH_SEP);
    if(pos != std::string::npos)
        return baseName.erase(0, pos + 1);
    return "";
}

//-----------------------------------------------------------------------------
// Load a TrueType font into memory. We care about the curves that define
// the letter shapes, and about the mappings that determine which glyph goes
// with which character.
//-----------------------------------------------------------------------------
bool TtfFont::LoadFromFile(FT_Library fontLibrary) {
    FT_Open_Args args = {};
    args.flags    = FT_OPEN_PATHNAME;
    args.pathname = &fontFile[0]; // FT_String is char* for historical reasons

    // We don't use ssfopen() here to let freetype do its own memory management.
    // This is OK because on Linux/OS X we just delegate to fopen and on Windows
    // we only look into C:\Windows\Fonts, which has a known short path.
    if(int fterr = FT_Open_Face(fontLibrary, &args, 0, &fontFace)) {
        dbp("freetype: loading font from file '%s' failed: %s",
            fontFile.c_str(), ft_error_string(fterr));
        return false;
    }

    if(int fterr = FT_Select_Charmap(fontFace, FT_ENCODING_UNICODE)) {
        dbp("freetype: loading unicode CMap for file '%s' failed: %s",
            fontFile.c_str(), ft_error_string(fterr));
        FT_Done_Face(fontFace);
        return false;
    }

    name = std::string(fontFace->family_name) +
           " (" + std::string(fontFace->style_name) + ")";
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

static const FT_Outline_Funcs outline_funcs = {
    MoveTo, LineTo, ConicTo, CubicTo, 0, 0
};

void TtfFont::PlotString(const std::string &str,
                         SBezierList *sbl, Vector origin, Vector u, Vector v)
{
    FT_Pos dx = 0;
    for(char32_t chr : ReadUTF8(str)) {
        uint32_t gid = FT_Get_Char_Index(fontFace, chr);
        if (gid == 0) {
            dbp("freetype: CID-to-GID mapping for CID 0x%04x failed: %s; using CID as GID",
                chr, ft_error_string(gid));
        }

        FT_F26Dot6 scale = fontFace->units_per_EM;
        if(int fterr = FT_Set_Char_Size(fontFace, scale, scale, 72, 72)) {
            dbp("freetype: cannot set character size: %s",
                ft_error_string(fterr));
            return;
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
            dbp("freetype: cannot load glyph (gid %d): %s",
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
        data.factor  = 1.0f/(float)scale;
        data.bx      = bx;
        if(int fterr = FT_Outline_Decompose(&fontFace->glyph->outline, &outline_funcs, &data)) {
            dbp("freetype: bezier decomposition failed (gid %d): %s",
                gid, ft_error_string(fterr));
        }

        // And we're done, so advance our position by the requested advance
        // width, plus the user-requested extra advance.
        dx += fontFace->glyph->advance.x;
    }
}
