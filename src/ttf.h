//-----------------------------------------------------------------------------
// Routines to read a TrueType font as vector outlines, and generate them
// as entities, since they're always representable as either lines or
// quadratic Bezier curves.
//
// Copyright 2016 whitequark, Peter Barfuss.
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_TTF_H
#define SOLVESPACE_TTF_H

class TtfFont {
public:
    Platform::Path  fontFile; // or resource path/name as res://<path>
    std::string     name;
    FT_FaceRec_    *fontFace;
    double          capHeight;

    void SetResourceID(const std::string &resource);
    bool IsResource() const;

    std::string FontFileBaseName() const;
    bool LoadFromFile(FT_LibraryRec_ *fontLibrary, bool nameOnly = true);
    bool LoadFromResource(FT_LibraryRec_ *fontLibrary, bool nameOnly = true);

    void PlotString(const std::string &str,
                    SBezierList *sbl, Vector origin, Vector u, Vector v);
    double AspectRatio(const std::string &str);

    bool ExtractTTFData(bool nameOnly);
};

class TtfFontList {
public:
    FT_LibraryRec_ *fontLibrary;
    bool            loaded;
    List<TtfFont>   l;

    TtfFontList();
    ~TtfFontList();

    void LoadAll();
    TtfFont *LoadFont(const std::string &font);

    void PlotString(const std::string &font, const std::string &str,
                    SBezierList *sbl, Vector origin, Vector u, Vector v);
    double AspectRatio(const std::string &font, const std::string &str);
};

#endif
