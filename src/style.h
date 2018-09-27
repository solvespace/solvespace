//
// Created by benjamin on 9/26/18.
//

#ifndef SOLVESPACE_STYLE_H
#define SOLVESPACE_STYLE_H

#include "handle.h"
#include "render/render.h"


namespace SolveSpace {

class Style {
public:
    int         tag;
    hStyle      h;

    enum {
        // If an entity has no style, then it will be colored according to
        // whether the group that it's in is active or not, whether it's
        // construction or not, and so on.
                NO_STYLE       = 0,

        ACTIVE_GRP     = 1,
        CONSTRUCTION   = 2,
        INACTIVE_GRP   = 3,
        DATUM          = 4,
        SOLID_EDGE     = 5,
        CONSTRAINT     = 6,
        SELECTED       = 7,
        HOVERED        = 8,
        CONTOUR_FILL   = 9,
        NORMALS        = 10,
        ANALYZE        = 11,
        DRAW_ERROR     = 12,
        DIM_SOLID      = 13,
        HIDDEN_EDGE    = 14,
        OUTLINE        = 15,

        FIRST_CUSTOM   = 0x100
    };

    std::string name;

    enum class UnitsAs : uint32_t {
        PIXELS   = 0,
        MM       = 1
    };
    double      width;
    UnitsAs     widthAs;
    double      textHeight;
    UnitsAs     textHeightAs;
    enum class TextOrigin : uint32_t {
        NONE    = 0x00,
        LEFT    = 0x01,
        RIGHT   = 0x02,
        BOT     = 0x04,
        TOP     = 0x08
    };
    TextOrigin  textOrigin;
    double      textAngle;
    RgbaColor   color;
    bool        filled;
    RgbaColor   fillColor;
    bool        visible;
    bool        exportable;
    StipplePattern stippleType;
    double      stippleScale;
    int         zIndex;

    // The default styles, for entities that don't have a style assigned yet,
    // and for datums and such.
    typedef struct {
        hStyle      h;
        const char *cnfPrefix;
        RgbaColor   color;
        double      width;
        int         zIndex;
    } Default;
    static const Default Defaults[];

    static std::string CnfColor(const std::string &prefix);
    static std::string CnfWidth(const std::string &prefix);
    static std::string CnfTextHeight(const std::string &prefix);
    static std::string CnfPrefixToName(const std::string &prefix);

    static void CreateAllDefaultStyles();
    static void CreateDefaultStyle(hStyle h);
    static void FillDefaultStyle(Style *s, const Default *d = NULL, bool factory = false);
    static void FreezeDefaultStyles(Platform::SettingsRef settings);
    static void LoadFactoryDefaults();

    static void AssignSelectionToStyle(uint32_t v);
    static uint32_t CreateCustomStyle(bool rememberForUndo = true);

    static RgbaColor RewriteColor(RgbaColor rgb);

    static Style *Get(hStyle hs);
    static RgbaColor Color(hStyle hs, bool forExport=false);
    static RgbaColor Color(int hs, bool forExport=false);
    static RgbaColor FillColor(hStyle hs, bool forExport=false);
    static double Width(hStyle hs);
    static double Width(int hs);
    static double WidthMm(int hs);
    static double TextHeight(hStyle hs);
    static double DefaultTextHeight();
    static Canvas::Stroke Stroke(hStyle hs);
    static Canvas::Stroke Stroke(int hs);
    static bool Exportable(int hs);
    static hStyle ForEntity(hEntity he);
    static StipplePattern PatternType(hStyle hs);
    static double StippleScaleMm(hStyle hs);

    std::string DescriptionString() const;

    void Clear() {}
};

}

#endif //SOLVESPACE_STYLE_H
