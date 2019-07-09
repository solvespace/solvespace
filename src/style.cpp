//-----------------------------------------------------------------------------
// Implementation of a cosmetic line style, which determines the color and
// other appearance of a line or curve on-screen and in exported files. Some
// styles are predefined, and others can be created by the user.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

const Style::Default Style::Defaults[] = {
    { { ACTIVE_GRP },   "ActiveGrp",    RGBf(1.0, 1.0, 1.0), 1.5, 4 },
    { { CONSTRUCTION }, "Construction", RGBf(0.1, 0.7, 0.1), 1.5, 0 },
    { { INACTIVE_GRP }, "InactiveGrp",  RGBf(0.5, 0.3, 0.0), 1.5, 3 },
    { { DATUM },        "Datum",        RGBf(0.0, 0.8, 0.0), 1.5, 0 },
    { { SOLID_EDGE },   "SolidEdge",    RGBf(0.8, 0.8, 0.8), 1.0, 2 },
    { { CONSTRAINT },   "Constraint",   RGBf(1.0, 0.1, 1.0), 1.0, 0 },
    { { SELECTED },     "Selected",     RGBf(1.0, 0.0, 0.0), 1.5, 0 },
    { { HOVERED },      "Hovered",      RGBf(1.0, 1.0, 0.0), 1.5, 0 },
    { { CONTOUR_FILL }, "ContourFill",  RGBf(0.0, 0.1, 0.1), 1.0, 0 },
    { { NORMALS },      "Normals",      RGBf(0.0, 0.4, 0.4), 1.0, 0 },
    { { ANALYZE },      "Analyze",      RGBf(0.0, 1.0, 1.0), 3.0, 0 },
    { { DRAW_ERROR },   "DrawError",    RGBf(1.0, 0.0, 0.0), 8.0, 0 },
    { { DIM_SOLID },    "DimSolid",     RGBf(0.1, 0.1, 0.1), 1.0, 0 },
    { { HIDDEN_EDGE },  "HiddenEdge",   RGBf(0.8, 0.8, 0.8), 1.0, 1 },
    { { OUTLINE },      "Outline",      RGBf(0.8, 0.8, 0.8), 3.0, 5 },
    { { 0 },            NULL,           RGBf(0.0, 0.0, 0.0), 0.0, 0 }
};

std::string Style::CnfColor(const std::string &prefix) {
    return "Style_" + prefix + "_Color";
}
std::string Style::CnfWidth(const std::string &prefix) {
    return "Style_" + prefix + "_Width";
}
std::string Style::CnfTextHeight(const std::string &prefix) {
    return "Style_" + prefix + "_TextHeight";
}

std::string Style::CnfPrefixToName(const std::string &prefix) {
    std::string name = "#def-";

    for(size_t i = 0; i < prefix.length(); i++) {
        if(isupper(prefix[i]) && i != 0)
            name += '-';
        name += tolower(prefix[i]);
    }

    return name;
}

void Style::CreateAllDefaultStyles() {
    const Default *d;
    for(d = &(Defaults[0]); d->h.v; d++) {
        (void)Get(d->h);
    }
}

void Style::CreateDefaultStyle(hStyle h) {
    bool isDefaultStyle = true;
    const Default *d;
    for(d = &(Defaults[0]); d->h.v; d++) {
        if(d->h == h) break;
    }
    if(!d->h.v) {
        // Not a default style; so just create it the same as our default
        // active group entity style.
        d = &(Defaults[0]);
        isDefaultStyle = false;
    }

    Style ns = {};
    FillDefaultStyle(&ns, d);
    ns.h = h;
    if(isDefaultStyle) {
        ns.name = CnfPrefixToName(d->cnfPrefix);
    } else {
        ns.name = "new-custom-style";
    }

    SK.style.Add(&ns);
}

void Style::FillDefaultStyle(Style *s, const Default *d, bool factory) {
    Platform::SettingsRef settings = Platform::GetSettings();

    if(d == NULL) d = &Defaults[0];
    s->color         = (factory)
                        ? d->color
                        : settings->ThawColor(CnfColor(d->cnfPrefix), d->color);
    s->width         = (factory)
                        ? d->width
                        : settings->ThawFloat(CnfWidth(d->cnfPrefix), (float)(d->width));
    s->widthAs       = UnitsAs::PIXELS;
    s->textHeight    = (factory) ? 11.5
                                 : settings->ThawFloat(CnfTextHeight(d->cnfPrefix), 11.5);
    s->textHeightAs  = UnitsAs::PIXELS;
    s->textOrigin    = TextOrigin::NONE;
    s->textAngle     = 0;
    s->visible       = true;
    s->exportable    = true;
    s->filled        = false;
    s->fillColor     = RGBf(0.3, 0.3, 0.3);
    s->stippleType   = (d->h.v == Style::HIDDEN_EDGE) ? StipplePattern::DASH
                                                      : StipplePattern::CONTINUOUS;
    s->stippleScale  = 15.0;
    s->zIndex        = d->zIndex;
}

void Style::LoadFactoryDefaults() {
    const Default *d;
    for(d = &(Defaults[0]); d->h.v; d++) {
        Style *s = Get(d->h);
        FillDefaultStyle(s, d, /*factory=*/true);
    }
    SS.backgroundColor = RGBi(0, 0, 0);
}

void Style::FreezeDefaultStyles(Platform::SettingsRef settings) {
    const Default *d;
    for(d = &(Defaults[0]); d->h.v; d++) {
        settings->FreezeColor(CnfColor(d->cnfPrefix), Color(d->h));
        settings->FreezeFloat(CnfWidth(d->cnfPrefix), (float)Width(d->h));
        settings->FreezeFloat(CnfTextHeight(d->cnfPrefix), (float)TextHeight(d->h));
    }
}

uint32_t Style::CreateCustomStyle(bool rememberForUndo) {
    if(rememberForUndo) SS.UndoRemember();
    uint32_t vs = max((uint32_t)Style::FIRST_CUSTOM, SK.style.MaximumId() + 1);
    hStyle hs = { vs };
    (void)Style::Get(hs);
    return hs.v;
}

void Style::AssignSelectionToStyle(uint32_t v) {
    bool showError = false;
    SS.GW.GroupSelection();

    SS.UndoRemember();
    int i;
    for(i = 0; i < SS.GW.gs.entities; i++) {
        hEntity he = SS.GW.gs.entity[i];
        Entity *e = SK.GetEntity(he);
        if(!e->IsStylable()) continue;

        if(!he.isFromRequest()) {
            showError = true;
            continue;
        }

        hRequest hr = he.request();
        Request *r = SK.GetRequest(hr);
        r->style.v = v;
        SS.MarkGroupDirty(r->group);
    }
    for(i = 0; i < SS.GW.gs.constraints; i++) {
        hConstraint hc = SS.GW.gs.constraint[i];
        Constraint *c = SK.GetConstraint(hc);
        if(!c->IsStylable()) continue;

        c->disp.style.v = v;
        SS.MarkGroupDirty(c->group);
    }

    if(showError) {
        Error(_("Can't assign style to an entity that's derived from another "
                "entity; try assigning a style to this entity's parent."));
    }

    SS.GW.ClearSelection();
    SS.GW.Invalidate();

    // And show that style's info screen in the text window.
    SS.TW.GoToScreen(TextWindow::Screen::STYLE_INFO);
    SS.TW.shown.style.v = v;
    SS.ScheduleShowTW();
}

//-----------------------------------------------------------------------------
// Look up a style by its handle. If that style does not exist, then create
// the style, according to our table of default styles.
//-----------------------------------------------------------------------------
Style *Style::Get(hStyle h) {
    if(h.v == 0) h.v = ACTIVE_GRP;

    Style *s = SK.style.FindByIdNoOops(h);
    if(s) {
        // It exists, good.
        return s;
    } else {
        // It doesn't exist; so we should create it and then return that.
        CreateDefaultStyle(h);
        return SK.style.FindById(h);
    }
}

//-----------------------------------------------------------------------------
// A couple of wrappers, so that I can call these functions with either an
// hStyle or with the integer corresponding to that hStyle.v.
//-----------------------------------------------------------------------------
RgbaColor Style::Color(int s, bool forExport) {
    hStyle hs = { (uint32_t)s };
    return Color(hs, forExport);
}
double Style::Width(int s) {
    hStyle hs = { (uint32_t)s };
    return Width(hs);
}

//-----------------------------------------------------------------------------
// If a color is almost white, then we can rewrite it to black, just so that
// it won't disappear on file formats with a light background.
//-----------------------------------------------------------------------------
RgbaColor Style::RewriteColor(RgbaColor rgbin) {
    Vector rgb = Vector::From(rgbin.redF(), rgbin.greenF(), rgbin.blueF());
    rgb = rgb.Minus(Vector::From(1, 1, 1));
    if(rgb.Magnitude() < 0.4 && SS.fixExportColors) {
        // This is an almost-white color in a default style, which is
        // good for the default on-screen view (black bg) but probably
        // not desired in the exported files, which typically are shown
        // against white backgrounds.
        return RGBi(0, 0, 0);
    } else {
        return rgbin;
    }
}

//-----------------------------------------------------------------------------
// Return the stroke color associated with our style as 8-bit RGB.
//-----------------------------------------------------------------------------
RgbaColor Style::Color(hStyle h, bool forExport) {
    Style *s = Get(h);
    if(forExport) {
        return RewriteColor(s->color);
    } else {
        return s->color;
    }
}

//-----------------------------------------------------------------------------
// Return the fill color associated with our style as 8-bit RGB.
//-----------------------------------------------------------------------------
RgbaColor Style::FillColor(hStyle h, bool forExport) {
    Style *s = Get(h);
    if(forExport) {
        return RewriteColor(s->fillColor);
    } else {
        return s->fillColor;
    }
}

//-----------------------------------------------------------------------------
// Return the width associated with our style in pixels..
//-----------------------------------------------------------------------------
double Style::Width(hStyle h) {
    Style *s = Get(h);
    switch(s->widthAs) {
        case UnitsAs::MM:     return s->width * SS.GW.scale;
        case UnitsAs::PIXELS: return s->width;
    }
    ssassert(false, "Unexpected units");
}

//-----------------------------------------------------------------------------
// Return the width associated with our style in millimeters..
//-----------------------------------------------------------------------------
double Style::WidthMm(int hs) {
    double widthpx = Width(hs);
    return widthpx / SS.GW.scale;
}

//-----------------------------------------------------------------------------
// Return the associated text height, in pixels.
//-----------------------------------------------------------------------------
double Style::TextHeight(hStyle h) {
    Style *s = Get(h);
    switch(s->textHeightAs) {
        case UnitsAs::MM:     return s->textHeight * SS.GW.scale;
        case UnitsAs::PIXELS: return s->textHeight;
    }
    ssassert(false, "Unexpected units");
}

double Style::DefaultTextHeight() {
    hStyle hs { Style::CONSTRAINT };
    return TextHeight(hs);
}

//-----------------------------------------------------------------------------
// Return the parameters of this style, as a canvas stroke.
//-----------------------------------------------------------------------------
Canvas::Stroke Style::Stroke(hStyle hs) {
    Canvas::Stroke stroke = {};
    Style *style = Style::Get(hs);
    stroke.color = style->color;
    stroke.stipplePattern = style->stippleType;
    stroke.stippleScale = style->stippleScale;
    stroke.width = style->width;
    switch(style->widthAs) {
        case Style::UnitsAs::PIXELS:
            stroke.unit = Canvas::Unit::PX;
            break;
        case Style::UnitsAs::MM:
            stroke.unit = Canvas::Unit::MM;
            break;
    }
    return stroke;
}

Canvas::Stroke Style::Stroke(int hsv) {
    hStyle hs = { (uint32_t) hsv };
    return Style::Stroke(hs);
}

//-----------------------------------------------------------------------------
// Should lines and curves from this style appear in the output file? Only
// if it's both shown and exportable.
//-----------------------------------------------------------------------------
bool Style::Exportable(int si) {
    hStyle hs = { (uint32_t)si };
    Style *s = Get(hs);
    return (s->exportable) && (s->visible);
}

//-----------------------------------------------------------------------------
// Return the appropriate style for our entity. If the entity has a style
// explicitly assigned, then it's that style. Otherwise it's the appropriate
// default style.
//-----------------------------------------------------------------------------
hStyle Style::ForEntity(hEntity he) {
    Entity *e = SK.GetEntity(he);
    // If the entity has a special style, use that. If that style doesn't
    // exist yet, then it will get created automatically later.
    if(e->style.v != 0) {
        return e->style;
    }

    // Otherwise, we use the default rules.
    hStyle hs;
    if(e->group != SS.GW.activeGroup) {
        hs.v = INACTIVE_GRP;
    } else if(e->construction) {
        hs.v = CONSTRUCTION;
    } else {
        hs.v = ACTIVE_GRP;
    }
    return hs;
}

StipplePattern Style::PatternType(hStyle hs) {
    Style *s = Get(hs);
    return s->stippleType;
}

double Style::StippleScaleMm(hStyle hs) {
    Style *s = Get(hs);
    if(s->widthAs == UnitsAs::MM) {
        return s->stippleScale;
    } else if(s->widthAs == UnitsAs::PIXELS) {
        return s->stippleScale / SS.GW.scale;
    }
    return 1.0;
}

std::string Style::DescriptionString() const {
    if(name.empty()) {
        return ssprintf("s%03x-(unnamed)", h.v);
    } else {
        return ssprintf("s%03x-%s", h.v, name.c_str());
    }
}


void TextWindow::ScreenShowListOfStyles(int link, uint32_t v) {
    SS.TW.GoToScreen(Screen::LIST_OF_STYLES);
}
void TextWindow::ScreenShowStyleInfo(int link, uint32_t v) {
    SS.TW.GoToScreen(Screen::STYLE_INFO);
    SS.TW.shown.style.v = v;
}

void TextWindow::ScreenLoadFactoryDefaultStyles(int link, uint32_t v) {
    Style::LoadFactoryDefaults();
    SS.TW.GoToScreen(Screen::LIST_OF_STYLES);
}

void TextWindow::ScreenCreateCustomStyle(int link, uint32_t v) {
    Style::CreateCustomStyle();
}

void TextWindow::ScreenChangeBackgroundColor(int link, uint32_t v) {
    RgbaColor rgb = SS.backgroundColor;
    SS.TW.ShowEditControlWithColorPicker(3, rgb);
    SS.TW.edit.meaning = Edit::BACKGROUND_COLOR;
}

void TextWindow::ShowListOfStyles() {
    Printf(true, "%Ft color  style-name");

    bool darkbg = false;
    Style *s;
    for(s = SK.style.First(); s; s = SK.style.NextAfter(s)) {
        Printf(false, "%Bp  %Bz   %Bp   %Fl%Ll%f%D%s%E",
            darkbg ? 'd' : 'a',
            &s->color,
            darkbg ? 'd' : 'a',
            ScreenShowStyleInfo, s->h.v,
            s->DescriptionString().c_str());

        darkbg = !darkbg;
    }

    Printf(true, "  %Fl%Ll%fcreate a new custom style%E",
        &ScreenCreateCustomStyle);

    Printf(false, "");

    RgbaColor rgb = SS.backgroundColor;
    Printf(false, "%Ft background color (r, g, b)%E");
    Printf(false, "%Ba   %@, %@, %@ %Fl%D%f%Ll[change]%E",
        rgb.redF(), rgb.greenF(), rgb.blueF(),
        top[rows-1] + 2, &ScreenChangeBackgroundColor);

    Printf(false, "");
    Printf(false, "  %Fl%Ll%fload factory defaults%E",
        &ScreenLoadFactoryDefaultStyles);
}


void TextWindow::ScreenChangeStyleName(int link, uint32_t v) {
    hStyle hs = { v };
    Style *s = Style::Get(hs);
    SS.TW.ShowEditControl(12, s->name);
    SS.TW.edit.style = hs;
    SS.TW.edit.meaning = Edit::STYLE_NAME;
}

void TextWindow::ScreenDeleteStyle(int link, uint32_t v) {
    SS.UndoRemember();
    hStyle hs = { v };
    Style *s = SK.style.FindByIdNoOops(hs);
    if(s) {
        SK.style.RemoveById(hs);
        // And it will get recreated automatically if something is still using
        // the style, so no need to do anything else.
    }
    SS.TW.GoToScreen(Screen::LIST_OF_STYLES);
    SS.GW.Invalidate();
}

void TextWindow::ScreenChangeStylePatternType(int link, uint32_t v) {
    hStyle hs = { v };
    Style *s = Style::Get(hs);
    s->stippleType = (StipplePattern)(link - 1);
    SS.GW.persistentDirty = true;
}

void TextWindow::ScreenChangeStyleMetric(int link, uint32_t v) {
    hStyle hs = { v };
    Style *s = Style::Get(hs);
    double val;
    Style::UnitsAs units;
    Edit meaning;
    int col;
    switch(link) {
        case 't':
            val = s->textHeight;
            units = s->textHeightAs;
            col = 10;
            meaning = Edit::STYLE_TEXT_HEIGHT;
            break;

        case 's':
            val = s->stippleScale;
            units = s->widthAs;
            col = 17;
            meaning = Edit::STYLE_STIPPLE_PERIOD;
            break;

        case 'w':
        case 'W':
            val = s->width;
            units = s->widthAs;
            col = 9;
            meaning = Edit::STYLE_WIDTH;
            break;

        default: ssassert(false, "Unexpected link");
    }

    std::string edit_value;
    if(units == Style::UnitsAs::PIXELS) {
        edit_value = ssprintf("%.2f", val);
    } else {
        edit_value = SS.MmToString(val);
    }
    SS.TW.ShowEditControl(col, edit_value);
    SS.TW.edit.style = hs;
    SS.TW.edit.meaning = meaning;
}

void TextWindow::ScreenChangeStyleTextAngle(int link, uint32_t v) {
    hStyle hs = { v };
    Style *s = Style::Get(hs);
    SS.TW.ShowEditControl(9, ssprintf("%.2f", s->textAngle));
    SS.TW.edit.style = hs;
    SS.TW.edit.meaning = Edit::STYLE_TEXT_ANGLE;
}

void TextWindow::ScreenChangeStyleColor(int link, uint32_t v) {
    hStyle hs = { v };
    Style *s = Style::Get(hs);
    // Same function used for stroke and fill colors
    Edit em;
    RgbaColor rgb;
    if(link == 's') {
        em = Edit::STYLE_COLOR;
        rgb = s->color;
    } else if(link == 'f') {
        em = Edit::STYLE_FILL_COLOR;
        rgb = s->fillColor;
    } else ssassert(false, "Unexpected link");
    SS.TW.ShowEditControlWithColorPicker(13, rgb);
    SS.TW.edit.style = hs;
    SS.TW.edit.meaning = em;
}

void TextWindow::ScreenChangeStyleYesNo(int link, uint32_t v) {
    SS.UndoRemember();
    hStyle hs = { v };
    Style *s = Style::Get(hs);
    switch(link) {
        // Units for the width
        case 'w':
            if(s->widthAs != Style::UnitsAs::MM) {
                s->widthAs = Style::UnitsAs::MM;
                s->width /= SS.GW.scale;
                s->stippleScale /= SS.GW.scale;
            }
            break;
        case 'W':
            if(s->widthAs != Style::UnitsAs::PIXELS) {
                s->widthAs = Style::UnitsAs::PIXELS;
                s->width *= SS.GW.scale;
                s->stippleScale *= SS.GW.scale;
            }
            break;

        // Units for the height
        case 'g':
            if(s->textHeightAs != Style::UnitsAs::MM) {
                s->textHeightAs = Style::UnitsAs::MM;
                s->textHeight /= SS.GW.scale;
            }
            break;

        case 'G':
            if(s->textHeightAs != Style::UnitsAs::PIXELS) {
                s->textHeightAs = Style::UnitsAs::PIXELS;
                s->textHeight *= SS.GW.scale;
            }
            break;

        case 'e':
            s->exportable = !(s->exportable);
            break;

        case 'v':
            s->visible = !(s->visible);
            break;

        case 'f':
            s->filled = !(s->filled);
            break;

        // Horizontal text alignment
        case 'L':
            s->textOrigin = (Style::TextOrigin)((uint32_t)s->textOrigin |  (uint32_t)Style::TextOrigin::LEFT);
            s->textOrigin = (Style::TextOrigin)((uint32_t)s->textOrigin & ~(uint32_t)Style::TextOrigin::RIGHT);
            break;
        case 'H':
            s->textOrigin = (Style::TextOrigin)((uint32_t)s->textOrigin & ~(uint32_t)Style::TextOrigin::LEFT);
            s->textOrigin = (Style::TextOrigin)((uint32_t)s->textOrigin & ~(uint32_t)Style::TextOrigin::RIGHT);
            break;
        case 'R':
            s->textOrigin = (Style::TextOrigin)((uint32_t)s->textOrigin & ~(uint32_t)Style::TextOrigin::LEFT);
            s->textOrigin = (Style::TextOrigin)((uint32_t)s->textOrigin |  (uint32_t)Style::TextOrigin::RIGHT);
            break;

        // Vertical text alignment
        case 'B':
            s->textOrigin = (Style::TextOrigin)((uint32_t)s->textOrigin |  (uint32_t)Style::TextOrigin::BOT);
            s->textOrigin = (Style::TextOrigin)((uint32_t)s->textOrigin & ~(uint32_t)Style::TextOrigin::TOP);
            break;
        case 'V':
            s->textOrigin = (Style::TextOrigin)((uint32_t)s->textOrigin & ~(uint32_t)Style::TextOrigin::BOT);
            s->textOrigin = (Style::TextOrigin)((uint32_t)s->textOrigin & ~(uint32_t)Style::TextOrigin::TOP);
            break;
        case 'T':
            s->textOrigin = (Style::TextOrigin)((uint32_t)s->textOrigin & ~(uint32_t)Style::TextOrigin::BOT);
            s->textOrigin = (Style::TextOrigin)((uint32_t)s->textOrigin |  (uint32_t)Style::TextOrigin::TOP);
            break;
    }
    SS.GW.Invalidate(/*clearPersistent=*/true);
}

bool TextWindow::EditControlDoneForStyles(const std::string &str) {
    Style *s;
    switch(edit.meaning) {
        case Edit::STYLE_STIPPLE_PERIOD:
        case Edit::STYLE_TEXT_HEIGHT:
        case Edit::STYLE_WIDTH: {
            SS.UndoRemember();
            s = Style::Get(edit.style);

            double v;
            Style::UnitsAs units = (edit.meaning == Edit::STYLE_TEXT_HEIGHT) ?
                            s->textHeightAs : s->widthAs;
            if(units == Style::UnitsAs::MM) {
                v = SS.StringToMm(str);
            } else {
                v = atof(str.c_str());
            }
            v = max(0.0, v);
            if(edit.meaning == Edit::STYLE_TEXT_HEIGHT) {
                s->textHeight = v;
            } else if(edit.meaning == Edit::STYLE_STIPPLE_PERIOD) {
                s->stippleScale = v;
            } else {
                s->width = v;
            }
            break;
        }
        case Edit::STYLE_TEXT_ANGLE:
            SS.UndoRemember();
            s = Style::Get(edit.style);
            s->textAngle = WRAP_SYMMETRIC(atof(str.c_str()), 360);
            break;

        case Edit::BACKGROUND_COLOR:
        case Edit::STYLE_FILL_COLOR:
        case Edit::STYLE_COLOR: {
            Vector rgb;
            if(sscanf(str.c_str(), "%lf, %lf, %lf", &rgb.x, &rgb.y, &rgb.z)==3) {
                rgb = rgb.ClampWithin(0, 1);
                if(edit.meaning == Edit::STYLE_COLOR) {
                    SS.UndoRemember();
                    s = Style::Get(edit.style);
                    s->color = RGBf(rgb.x, rgb.y, rgb.z);
                } else if(edit.meaning == Edit::STYLE_FILL_COLOR) {
                    SS.UndoRemember();
                    s = Style::Get(edit.style);
                    s->fillColor = RGBf(rgb.x, rgb.y, rgb.z);
                } else {
                    SS.backgroundColor = RGBf(rgb.x, rgb.y, rgb.z);
                }
            } else {
                Error(_("Bad format: specify color as r, g, b"));
            }
            break;
        }
        case Edit::STYLE_NAME:
            if(str.empty()) {
                Error(_("Style name cannot be empty"));
            } else {
                SS.UndoRemember();
                s = Style::Get(edit.style);
                s->name = str;
            }
            break;

        default: return false;
    }
    SS.GW.persistentDirty = true;
    return true;
}

void TextWindow::ShowStyleInfo() {
    Printf(true, "%Fl%f%Ll(back to list of styles)%E", &ScreenShowListOfStyles);

    Style *s = Style::Get(shown.style);

    if(s->h.v < Style::FIRST_CUSTOM) {
        Printf(true, "%FtSTYLE  %E%s ", s->DescriptionString().c_str());
    } else {
        Printf(true, "%FtSTYLE  %E%s "
                     "[%Fl%Ll%D%frename%E/%Fl%Ll%D%fdel%E]",
            s->DescriptionString().c_str(),
            s->h.v, &ScreenChangeStyleName,
            s->h.v, &ScreenDeleteStyle);
    }
    Printf(true, "%Ft line stroke style%E");
    Printf(false, "%Ba   %Ftcolor %E%Bz  %Ba (%@, %@, %@) %D%f%Ls%Fl[change]%E",
        &s->color,
        s->color.redF(), s->color.greenF(), s->color.blueF(),
        s->h.v, ScreenChangeStyleColor);

    // The line width, and its units
    if(s->widthAs == Style::UnitsAs::PIXELS) {
        Printf(false, "   %Ftwidth%E %@ %D%f%Lp%Fl[change]%E",
            s->width,
            s->h.v, &ScreenChangeStyleMetric,
            (s->h.v < Style::FIRST_CUSTOM) ? 'w' : 'W');
    } else {
        Printf(false, "   %Ftwidth%E %s %D%f%Lp%Fl[change]%E",
            SS.MmToString(s->width).c_str(),
            s->h.v, &ScreenChangeStyleMetric,
            (s->h.v < Style::FIRST_CUSTOM) ? 'w' : 'W');
    }

    if(s->widthAs == Style::UnitsAs::PIXELS) {
        Printf(false, "%Ba   %Ftstipple width%E %@ %D%f%Lp%Fl[change]%E",
            s->stippleScale,
            s->h.v, &ScreenChangeStyleMetric, 's');
    } else {
        Printf(false, "%Ba   %Ftstipple width%E %s %D%f%Lp%Fl[change]%E",
            SS.MmToString(s->stippleScale).c_str(),
            s->h.v, &ScreenChangeStyleMetric, 's');
    }

    bool widthpx = (s->widthAs == Style::UnitsAs::PIXELS);
    if(s->h.v < Style::FIRST_CUSTOM) {
        Printf(false,"   %Ftin units of %Fdpixels%E");
    } else {
        Printf(false,"%Ba   %Ftin units of  %Fd"
                            "%D%f%LW%s pixels%E  "
                            "%D%f%Lw%s %s",
            s->h.v, &ScreenChangeStyleYesNo,
            widthpx ? RADIO_TRUE : RADIO_FALSE,
            s->h.v, &ScreenChangeStyleYesNo,
            !widthpx ? RADIO_TRUE : RADIO_FALSE,
            SS.UnitName());
    }

    Printf(false,"%Ba   %Ftstipple type:%E");

    const size_t patternCount = (size_t)StipplePattern::LAST + 1;
    const char *patternsSource[patternCount] = {
        "___________",
        "-  -  -  - ",
        "- - - - - -",
        "__ __ __ __",
        "-.-.-.-.-.-",
        "..-..-..-..",
        "...........",
        "~~~~~~~~~~~",
        "__~__~__~__"
    };
    std::string patterns[patternCount];

    for(uint32_t i = 0; i <= (uint32_t)StipplePattern::LAST; i++) {
        const char *str = patternsSource[i];
        do {
            switch(*str) {
                case ' ': patterns[i] += " "; break;
                case '.': patterns[i] += "\xEE\x80\x84"; break;
                case '_': patterns[i] += "\xEE\x80\x85"; break;
                case '-': patterns[i] += "\xEE\x80\x86"; break;
                case '~': patterns[i] += "\xEE\x80\x87"; break;
                default: ssassert(false, "Unexpected stipple pattern element");
            }
        } while(*(++str));
    }

    for(uint32_t i = 0; i <= (uint32_t)StipplePattern::LAST; i++) {
        const char *radio = s->stippleType == (StipplePattern)i ? RADIO_TRUE : RADIO_FALSE;
        Printf(false, "%Bp     %D%f%Lp%s %s%E",
            (i % 2 == 0) ? 'd' : 'a',
            s->h.v, &ScreenChangeStylePatternType,
            i + 1, radio, patterns[i].c_str());
    }

    if(s->h.v >= Style::FIRST_CUSTOM) {
        // The fill color, and whether contours are filled

        Printf(false, "");
        Printf(false, "%Ft contour fill style%E");
        Printf(false,
            "%Ba   %Ftcolor %E%Bz  %Ba (%@, %@, %@) %D%f%Lf%Fl[change]%E",
            &s->fillColor,
            s->fillColor.redF(), s->fillColor.greenF(), s->fillColor.blueF(),
            s->h.v, ScreenChangeStyleColor);

        Printf(false, "%Bd   %D%f%Lf%s  contours are filled%E",
            s->h.v, &ScreenChangeStyleYesNo,
            s->filled ? CHECK_TRUE : CHECK_FALSE);
    }

    // The text height, and its units
    Printf(false, "");
    Printf(false, "%Ft text style%E");

    if(s->textHeightAs == Style::UnitsAs::PIXELS) {
        Printf(false, "%Ba   %Ftheight %E%@ %D%f%Lt%Fl%s%E",
            s->textHeight,
            s->h.v, &ScreenChangeStyleMetric,
            "[change]");
    } else {
        Printf(false, "%Ba   %Ftheight %E%s %D%f%Lt%Fl%s%E",
            SS.MmToString(s->textHeight).c_str(),
            s->h.v, &ScreenChangeStyleMetric,
            "[change]");
    }

    bool textHeightpx = (s->textHeightAs == Style::UnitsAs::PIXELS);
    if(s->h.v < Style::FIRST_CUSTOM) {
        Printf(false,"%Bd   %Ftin units of %Fdpixels");
    } else {
        Printf(false,"%Bd   %Ftin units of  %Fd"
                            "%D%f%LG%s pixels%E  "
                            "%D%f%Lg%s %s",
            s->h.v, &ScreenChangeStyleYesNo,
            textHeightpx ? RADIO_TRUE : RADIO_FALSE,
            s->h.v, &ScreenChangeStyleYesNo,
            !textHeightpx ? RADIO_TRUE : RADIO_FALSE,
            SS.UnitName());
    }

    if(s->h.v >= Style::FIRST_CUSTOM) {
        Printf(false, "%Ba   %Ftangle %E%@ %D%f%Ll%Fl[change]%E",
            s->textAngle,
            s->h.v, &ScreenChangeStyleTextAngle);

        Printf(false, "");
        Printf(false, "%Ft text comment alignment%E");
        bool neither;
        neither = !((uint32_t)s->textOrigin & ((uint32_t)Style::TextOrigin::LEFT | (uint32_t)Style::TextOrigin::RIGHT));
        Printf(false, "%Ba   "
                      "%D%f%LL%s left%E    "
                      "%D%f%LH%s center%E  "
                      "%D%f%LR%s right%E  ",
            s->h.v, &ScreenChangeStyleYesNo,
            ((uint32_t)s->textOrigin & (uint32_t)Style::TextOrigin::LEFT) ? RADIO_TRUE : RADIO_FALSE,
            s->h.v, &ScreenChangeStyleYesNo,
            neither ? RADIO_TRUE : RADIO_FALSE,
            s->h.v, &ScreenChangeStyleYesNo,
            ((uint32_t)s->textOrigin & (uint32_t)Style::TextOrigin::RIGHT) ? RADIO_TRUE : RADIO_FALSE);

        neither = !((uint32_t)s->textOrigin & ((uint32_t)Style::TextOrigin::BOT | (uint32_t)Style::TextOrigin::TOP));
        Printf(false, "%Bd   "
                      "%D%f%LB%s bottom%E  "
                      "%D%f%LV%s center%E  "
                      "%D%f%LT%s top%E  ",
            s->h.v, &ScreenChangeStyleYesNo,
            ((uint32_t)s->textOrigin & (uint32_t)Style::TextOrigin::BOT) ? RADIO_TRUE : RADIO_FALSE,
            s->h.v, &ScreenChangeStyleYesNo,
            neither ? RADIO_TRUE : RADIO_FALSE,
            s->h.v, &ScreenChangeStyleYesNo,
            ((uint32_t)s->textOrigin & (uint32_t)Style::TextOrigin::TOP) ? RADIO_TRUE : RADIO_FALSE);
    }

    if(s->h.v >= Style::FIRST_CUSTOM) {
        Printf(false, "");

        Printf(false, "  %Fd%D%f%Lv%s  show these objects on screen%E",
                s->h.v, &ScreenChangeStyleYesNo,
                s->visible ? CHECK_TRUE : CHECK_FALSE);

        Printf(false, "  %Fd%D%f%Le%s  export these objects%E",
                s->h.v, &ScreenChangeStyleYesNo,
                s->exportable ? CHECK_TRUE : CHECK_FALSE);

        Printf(false, "");
        Printf(false, "To assign lines or curves to this style,");
        Printf(false, "right-click them on the drawing.");
    }
}

void TextWindow::ScreenAssignSelectionToStyle(int link, uint32_t v) {
    Style::AssignSelectionToStyle(v);
}
