//-----------------------------------------------------------------------------
// Routines to read a TrueType font as vector outlines, and generate them
// as entities, since they're always representable as either lines or
// quadratic Bezier curves.
//
// Copyright 2016 whitequark, Peter Barfuss.
//-----------------------------------------------------------------------------

#ifndef __TTF_H
#define __TTF_H

class TtfFont {
public:
    std::string     fontFile;
    std::string     name;
    FT_FaceRec_    *fontFace;
    double          capHeight;

    std::string FontFileBaseName() const;
    bool LoadFromFile(FT_LibraryRec_ *fontLibrary, bool nameOnly = true);

    void PlotString(const std::string &str,
                    SBezierList *sbl, Vector origin, Vector u, Vector v);
};

class TtfFontList {
public:
    FT_LibraryRec_ *fontLibrary;
    bool            loaded;
    List<TtfFont>   l;

    TtfFontList();
    ~TtfFontList();

    void LoadAll();

    void PlotString(const std::string &font, const std::string &str,
                    SBezierList *sbl, Vector origin, Vector u, Vector v);
};

#endif
