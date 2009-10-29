#include "solvespace.h"

#define clamp01(x) (max(0, min(1, (x))))

const Style::Default Style::Defaults[] = {
    { ACTIVE_GRP,   "ActiveGrp",    RGBf(1.0, 1.0, 1.0), 1.5, },
    { CONSTRUCTION, "Construction", RGBf(0.1, 0.7, 0.1), 1.5, },
    { INACTIVE_GRP, "InactiveGrp",  RGBf(0.5, 0.3, 0.0), 1.5, },
    { DATUM,        "Datum",        RGBf(0.0, 0.8, 0.0), 1.5, },
    { SOLID_EDGE,   "SolidEdge",    RGBf(0.8, 0.8, 0.8), 1.0, },
    { CONSTRAINT,   "Constraint",   RGBf(1.0, 0.1, 1.0), 1.0, },
    { SELECTED,     "Selected",     RGBf(1.0, 0.0, 0.0), 1.5, },
    { HOVERED,      "Hovered",      RGBf(1.0, 1.0, 0.0), 1.5, },
    { CONTOUR_FILL, "ContourFill",  RGBf(0.0, 0.1, 0.1), 1.0, },
    { NORMALS,      "Normals",      RGBf(0.0, 0.4, 0.4), 1.0, },
    { ANALYZE,      "Analyze",      RGBf(0.0, 1.0, 1.0), 1.0, },
    { DRAW_ERROR,   "DrawError",    RGBf(1.0, 0.0, 0.0), 8.0, },
    { DIM_SOLID,    "DimSolid",     RGBf(0.1, 0.1, 0.1), 1.0, },
    { 0,            NULL,           0,                   0.0, },
};

char *Style::CnfColor(char *prefix) {
    static char name[100];
    sprintf(name, "Style_%s_Color", prefix);
    return name;
}
char *Style::CnfWidth(char *prefix) {
    static char name[100];
    sprintf(name, "Style_%s_Width", prefix);
    return name;
}

char *Style::CnfPrefixToName(char *prefix) {
    static char name[100];
    int i = 0, j;
    strcpy(name, "#def-");
    j = 5;
    while(prefix[i] && j < 90) {
        if(isupper(prefix[i]) && i != 0) {
            name[j++] = '-';
        }
        name[j++] = tolower(prefix[i]);
        i++;
    }
    name[j++] = '\0';
    return name;
}

void Style::CreateAllDefaultStyles(void) {
    const Default *d;
    for(d = &(Defaults[0]); d->h.v; d++) {
        (void)Get(d->h);
    }
}

void Style::CreateDefaultStyle(hStyle h) {
    bool isDefaultStyle = true;
    const Default *d;
    for(d = &(Defaults[0]); d->h.v; d++) {
        if(d->h.v == h.v) break;
    }
    if(!d->h.v) {
        // Not a default style; so just create it the same as our default
        // active group entity style.
        d = &(Defaults[0]);
        isDefaultStyle = false;
    }

    Style ns;
    ZERO(&ns);
    ns.color        = CnfThawDWORD(d->color, CnfColor(d->cnfPrefix));
    ns.width        = CnfThawFloat((float)(d->width), CnfWidth(d->cnfPrefix));
    ns.widthAs      = UNITS_AS_PIXELS;
    ns.textHeight   = DEFAULT_TEXT_HEIGHT;
    ns.textHeightAs = UNITS_AS_PIXELS;
    ns.textOrigin   = 0;
    ns.textAngle    = 0;
    ns.visible      = true;
    ns.exportable   = true;
    ns.filled       = false;
    ns.fillColor    = RGBf(0.3, 0.3, 0.3);
    ns.h            = h;
    if(isDefaultStyle) {
        ns.name.strcpy(CnfPrefixToName(d->cnfPrefix));
    } else {
        ns.name.strcpy("new-custom-style");
    }

    SK.style.Add(&ns);
}

void Style::LoadFactoryDefaults(void) {
    const Default *d;
    for(d = &(Defaults[0]); d->h.v; d++) {
        Style *s = Get(d->h);

        s->color        = d->color;
        s->width        = d->width;
        s->widthAs      = UNITS_AS_PIXELS;
        s->textHeight   = DEFAULT_TEXT_HEIGHT;
        s->textHeightAs = UNITS_AS_PIXELS;
        s->textOrigin   = 0;
        s->textAngle    = 0;
        s->visible      = true;
        s->exportable   = true;
        s->filled       = false;
        s->fillColor    = RGBf(0.3, 0.3, 0.3);
        s->name.strcpy(CnfPrefixToName(d->cnfPrefix));
    }
    SS.backgroundColor = RGB(0, 0, 0);
}

void Style::FreezeDefaultStyles(void) {
    const Default *d;
    for(d = &(Defaults[0]); d->h.v; d++) {
        CnfFreezeDWORD(Color(d->h), CnfColor(d->cnfPrefix));
        CnfFreezeFloat((float)Width(d->h), CnfWidth(d->cnfPrefix));
    }
}

DWORD Style::CreateCustomStyle(void) {
    SS.UndoRemember();
    DWORD vs = max(Style::FIRST_CUSTOM, SK.style.MaximumId() + 1);
    hStyle hs = { vs };
    (void)Style::Get(hs);
    return hs.v;
}

void Style::AssignSelectionToStyle(DWORD v) {
    bool showError = false;
    SS.GW.GroupSelection();

    SS.UndoRemember();
    int i;
    for(i = 0; i < SS.GW.gs.entities; i++) {
        hEntity he = SS.GW.gs.entity[i];
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
        if(c->type != Constraint::COMMENT) continue;

        c->disp.style.v = v;
    }

    if(showError) {
        Error("Can't assign style to an entity that's derived from another "
              "entity; try assigning a style to this entity's parent.");
    }

    SS.GW.ClearSelection();
    InvalidateGraphics();
    SS.later.generateAll = true;

    // And show that style's info screen in the text window.
    SS.TW.GoToScreen(TextWindow::SCREEN_STYLE_INFO);
    SS.TW.shown.style.v = v;
    SS.later.showTW = true;
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
DWORD Style::Color(int s, bool forExport) {
    hStyle hs = { s };
    return Color(hs, forExport);
}
float Style::Width(int s) {
    hStyle hs = { s };
    return Width(hs);
}

//-----------------------------------------------------------------------------
// Return the color associated with our style as 8-bit RGB.
//-----------------------------------------------------------------------------
DWORD Style::Color(hStyle h, bool forExport) {
    Style *s = Get(h);
    if(forExport) {
        Vector rgb = Vector::From(REDf(s->color),
                                  GREENf(s->color),
                                  BLUEf(s->color));
        rgb = rgb.Minus(Vector::From(1, 1, 1));
        if(rgb.Magnitude() < 0.4 && SS.fixExportColors) {
            // This is an almost-white color in a default style, which is
            // good for the default on-screen view (black bg) but probably
            // not desired in the exported files, which typically are shown
            // against white backgrounds.
            return RGB(0, 0, 0);
        }
    }
    return s->color;
}

//-----------------------------------------------------------------------------
// Return the width associated with our style in pixels..
//-----------------------------------------------------------------------------
float Style::Width(hStyle h) {
    double r = 1.0;
    Style *s = Get(h);
    if(s->widthAs == UNITS_AS_MM) {
        r = s->width * SS.GW.scale;
    } else if(s->widthAs == UNITS_AS_PIXELS) {
        r = s->width;
    }
    // This returns a float because glLineWidth expects a float, avoid casts.
    return (float)r;
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
double Style::TextHeight(hStyle hs) {
    Style *s = Get(hs);
    if(s->textHeightAs == UNITS_AS_MM) {
        return s->textHeight * SS.GW.scale;
    } else if(s->textHeightAs == UNITS_AS_PIXELS) {
        return s->textHeight;
    } else {
        return DEFAULT_TEXT_HEIGHT;
    }
}

//-----------------------------------------------------------------------------
// Should lines and curves from this style appear in the output file? Only
// if it's both shown and exportable.
//-----------------------------------------------------------------------------
bool Style::Exportable(int si) {
    hStyle hs = { si };
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
    if(e->group.v != SS.GW.activeGroup.v) {
        hs.v = INACTIVE_GRP;
    } else if(e->construction) {
        hs.v = CONSTRUCTION;
    } else {
        hs.v = ACTIVE_GRP;
    }
    return hs;
}

char *Style::DescriptionString(void) {
    static char ret[100];
    if(name.str[0]) {
        sprintf(ret, "s%03x-%s", h.v, name.str);
    } else {
        sprintf(ret, "s%03x-(unnamed)", h.v);
    }
    return ret;
}


void TextWindow::ScreenShowListOfStyles(int link, DWORD v) {
    SS.TW.GoToScreen(SCREEN_LIST_OF_STYLES);
}
void TextWindow::ScreenShowStyleInfo(int link, DWORD v) {
    SS.TW.GoToScreen(SCREEN_STYLE_INFO);
    SS.TW.shown.style.v = v;
}

void TextWindow::ScreenLoadFactoryDefaultStyles(int link, DWORD v) {
    Style::LoadFactoryDefaults();
    SS.TW.GoToScreen(SCREEN_LIST_OF_STYLES);
}

void TextWindow::ScreenCreateCustomStyle(int link, DWORD v) {
    Style::CreateCustomStyle();
}

void TextWindow::ScreenChangeBackgroundColor(int link, DWORD v) {
    DWORD rgb = SS.backgroundColor;
    char str[300];
    sprintf(str, "%.2f, %.2f, %.2f", REDf(rgb), GREENf(rgb), BLUEf(rgb));
    ShowTextEditControl(v, 3, str);
    SS.TW.edit.meaning = EDIT_BACKGROUND_COLOR;
}

void TextWindow::ShowListOfStyles(void) {
    Printf(true, "%Ft color  style-name");

    bool darkbg = false;
    Style *s;
    for(s = SK.style.First(); s; s = SK.style.NextAfter(s)) {
        Printf(false, "%Bp  %Bp   %Bp   %Fl%Ll%f%D%s%E",
            darkbg ? 'd' : 'a',
            0x80000000 | s->color,
            darkbg ? 'd' : 'a',
            ScreenShowStyleInfo, s->h.v,
            s->DescriptionString());

        darkbg = !darkbg;
    }

    Printf(true, "  %Fl%Ll%fcreate a new custom style%E", 
        &ScreenCreateCustomStyle);

    Printf(false, "");

    DWORD rgb = SS.backgroundColor;
    Printf(false, "%Ft background color (r, g, b)%E");
    Printf(false, "%Ba   %@, %@, %@ %Fl%D%f%Ll[change]%E",
        REDf(rgb), GREENf(rgb), BLUEf(rgb),
        top[rows-1] + 2, &ScreenChangeBackgroundColor);

    Printf(false, "");
    Printf(false, "  %Fl%Ll%fload factory defaults%E",
        &ScreenLoadFactoryDefaultStyles);
}


void TextWindow::ScreenChangeStyleName(int link, DWORD v) {
    hStyle hs = { v };
    Style *s = Style::Get(hs);
    ShowTextEditControl(10, 13, s->name.str);
    SS.TW.edit.style = hs;
    SS.TW.edit.meaning = EDIT_STYLE_NAME;   
}

void TextWindow::ScreenDeleteStyle(int link, DWORD v) {
    SS.UndoRemember();
    hStyle hs = { v };
    Style *s = SK.style.FindByIdNoOops(hs);
    if(s) {
        SK.style.RemoveById(hs);
        // And it will get recreated automatically if something is still using
        // the style, so no need to do anything else.
    }
    SS.TW.GoToScreen(SCREEN_LIST_OF_STYLES);
    InvalidateGraphics();
}

void TextWindow::ScreenChangeStyleWidthOrTextHeight(int link, DWORD v) {
    hStyle hs = { v };
    Style *s = Style::Get(hs);
    double val   = (link == 't') ? s->textHeight   : s->width;
    int    units = (link == 't') ? s->textHeightAs : s->widthAs;

    char str[300];
    if(units == Style::UNITS_AS_PIXELS) {
        sprintf(str, "%.2f", val);
    } else {
        strcpy(str, SS.MmToString(val));
    }
    int row = 0;
    if(link == 'w') {
        row = 16;               // width for a default style
    } else if(link == 'W') {
        row = 16;               // width for a custom style
    } else if(link == 't') {
        row = 27;               // text height (for custom styles only)
    }
    ShowTextEditControl(row, 13, str);
    SS.TW.edit.style = hs;
    SS.TW.edit.meaning = (link == 't') ? EDIT_STYLE_TEXT_HEIGHT :
                                         EDIT_STYLE_WIDTH;
}

void TextWindow::ScreenChangeStyleTextAngle(int link, DWORD v) {
    hStyle hs = { v };
    Style *s = Style::Get(hs);
    char str[300];
    sprintf(str, "%.2f", s->textAngle);
    ShowTextEditControl(32, 13, str);
    SS.TW.edit.style = hs;
    SS.TW.edit.meaning = EDIT_STYLE_TEXT_ANGLE;
}

void TextWindow::ScreenChangeStyleColor(int link, DWORD v) {
    hStyle hs = { v };
    Style *s = Style::Get(hs);
    // Same function used for stroke and fill colors
    int row, col, em;
    DWORD rgb;
    if(link == 's') {
        row = 13; col = 17;
        em = EDIT_STYLE_COLOR;
        rgb = s->color;
    } else if(link == 'f') {
        row = 21; col = 17;
        em = EDIT_STYLE_FILL_COLOR;
        rgb = s->fillColor;
    } else {
        oops();
    }
    char str[300];
    sprintf(str, "%.2f, %.2f, %.2f", REDf(rgb), GREENf(rgb), BLUEf(rgb));
    ShowTextEditControl(row, col, str);
    SS.TW.edit.style = hs;
    SS.TW.edit.meaning = em;
}

void TextWindow::ScreenChangeStyleYesNo(int link, DWORD v) {
    SS.UndoRemember();
    hStyle hs = { v };
    Style *s = Style::Get(hs);
    switch(link) {
        case 'w':
            // Units for the width
            if(s->widthAs == Style::UNITS_AS_PIXELS) {
                s->widthAs = Style::UNITS_AS_MM;
                s->width /= SS.GW.scale;
            } else {
                s->widthAs = Style::UNITS_AS_PIXELS;
                s->width *= SS.GW.scale;
            }
            break;

        case 'h':
            // Units for the height
            if(s->textHeightAs == Style::UNITS_AS_PIXELS) {
                s->textHeightAs = Style::UNITS_AS_MM;
                s->textHeight /= SS.GW.scale;
            } else {
                s->textHeightAs = Style::UNITS_AS_PIXELS;
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
            s->textOrigin |=  Style::ORIGIN_LEFT;
            s->textOrigin &= ~Style::ORIGIN_RIGHT;
            break;
        case 'H':
            s->textOrigin &= ~Style::ORIGIN_LEFT;
            s->textOrigin &= ~Style::ORIGIN_RIGHT;
            break;
        case 'R':
            s->textOrigin &= ~Style::ORIGIN_LEFT;
            s->textOrigin |=  Style::ORIGIN_RIGHT;
            break;

        // Vertical text alignment
        case 'B':
            s->textOrigin |=  Style::ORIGIN_BOT;
            s->textOrigin &= ~Style::ORIGIN_TOP;
            break;
        case 'V':
            s->textOrigin &= ~Style::ORIGIN_BOT;
            s->textOrigin &= ~Style::ORIGIN_TOP;
            break;
        case 'T':
            s->textOrigin &= ~Style::ORIGIN_BOT;
            s->textOrigin |=  Style::ORIGIN_TOP;
            break;
    }
    InvalidateGraphics();
}

bool TextWindow::EditControlDoneForStyles(char *str) {
    Style *s;
    switch(edit.meaning) {
        case EDIT_STYLE_TEXT_HEIGHT:
        case EDIT_STYLE_WIDTH: {
            SS.UndoRemember();
            s = Style::Get(edit.style);

            double v;
            int units = (edit.meaning == EDIT_STYLE_TEXT_HEIGHT) ?
                            s->textHeightAs : s->widthAs;
            if(units == Style::UNITS_AS_MM) {
                v = SS.StringToMm(str);
            } else {
                v = atof(str);
            }
            v = max(0, v);
            if(edit.meaning == EDIT_STYLE_TEXT_HEIGHT) {
                s->textHeight = v;
            } else {
                s->width = v;
            }
            break;
        }
        case EDIT_STYLE_TEXT_ANGLE:
            SS.UndoRemember();
            s = Style::Get(edit.style);
            s->textAngle = WRAP_SYMMETRIC(atof(str), 360);
            break;

        case EDIT_BACKGROUND_COLOR:
        case EDIT_STYLE_FILL_COLOR:
        case EDIT_STYLE_COLOR: {
            double r, g, b;
            if(sscanf(str, "%lf, %lf, %lf", &r, &g, &b)==3) {
                r = clamp01(r);
                g = clamp01(g);
                b = clamp01(b);
                if(edit.meaning == EDIT_STYLE_COLOR) {
                    SS.UndoRemember();
                    s = Style::Get(edit.style);
                    s->color = RGBf(r, g, b);
                } else if(edit.meaning == EDIT_STYLE_FILL_COLOR) {
                    SS.UndoRemember();
                    s = Style::Get(edit.style);
                    s->fillColor = RGBf(r, g, b);
                } else {
                    SS.backgroundColor = RGBf(r, g, b);
                }
            } else {
                Error("Bad format: specify color as r, g, b");
            }
            break;
        }
        case EDIT_STYLE_NAME:
            if(!StringAllPrintable(str) || !*str) {
                Error("Invalid characters. Allowed are: A-Z a-z 0-9 _ -");
            } else {
                SS.UndoRemember();
                s = Style::Get(edit.style);
                s->name.strcpy(str);
            }
            break;

        default: return false;
    }
    return true;
}

void TextWindow::ShowStyleInfo(void) {
    Printf(true, "%Fl%f%Ll(back to list of styles)%E", &ScreenShowListOfStyles);

    Style *s = Style::Get(shown.style);

    if(s->h.v < Style::FIRST_CUSTOM) {
        Printf(true, "%FtSTYLE   %E%s ", s->DescriptionString());
    } else {
        Printf(true, "%FtSTYLE   %E%s "
                     "[%Fl%Ll%D%frename%E/%Fl%Ll%D%fdel%E]",
            s->DescriptionString(),
            s->h.v, &ScreenChangeStyleName,
            s->h.v, &ScreenDeleteStyle);
    }

    Printf(true, "%FtLINE COLOR   %E%Bp  %Bd (%@, %@, %@) %D%f%Ls%Fl[chng]%E",
        0x80000000 | s->color,
        REDf(s->color), GREENf(s->color), BLUEf(s->color),
        s->h.v, ScreenChangeStyleColor);

    char *unit = (SS.viewUnits == SolveSpace::UNIT_INCHES) ? "inches" : "mm";

    // The line width, and its units
    if(s->widthAs == Style::UNITS_AS_PIXELS) {
        Printf(true, "%FtLINE WIDTH   %E%@ %D%f%Lp%Fl[change]%E",
            s->width,
            s->h.v, &ScreenChangeStyleWidthOrTextHeight,
            (s->h.v < Style::FIRST_CUSTOM) ? 'w' : 'W');
    } else {
        Printf(true, "%FtLINE WIDTH   %E%s %D%f%Lp%Fl[change]%E",
            SS.MmToString(s->width),
            s->h.v, &ScreenChangeStyleWidthOrTextHeight,
            (s->h.v < Style::FIRST_CUSTOM) ? 'w' : 'W');
    }

    bool widthpx = (s->widthAs == Style::UNITS_AS_PIXELS);
    if(s->h.v < Style::FIRST_CUSTOM) {
        Printf(false,"%FtIN UNITS OF  %Fspixels%E");
    } else {
        Printf(false,"%FtIN UNITS OF  "
                            "%Fh%D%f%Lw%s%E%Fs%s%E / %Fh%D%f%Lw%s%E%Fs%s%E",
            s->h.v, &ScreenChangeStyleYesNo,
            ( widthpx ? "" : "pixels"),
            ( widthpx ? "pixels" : ""),
            s->h.v, &ScreenChangeStyleYesNo,
            (!widthpx ? "" : unit),
            (!widthpx ? unit : ""));
    }

    if(s->h.v >= Style::FIRST_CUSTOM) {
        // The fill color, and whether contours are filled
        Printf(true,
            "%FtFILL COLOR   %E%Bp  %Bd (%@, %@, %@) %D%f%Lf%Fl[chng]%E",
                0x80000000 | s->fillColor,
                REDf(s->fillColor), GREENf(s->fillColor), BLUEf(s->fillColor),
                s->h.v, ScreenChangeStyleColor);
        Printf(false, "%FtCONTOURS ARE %E"
                            "%Fh%D%f%Lf%s%E%Fs%s%E / %Fh%D%f%Lf%s%E%Fs%s%E",
            s->h.v, &ScreenChangeStyleYesNo,
            ( s->filled ? "" : "filled"),
            ( s->filled ? "filled" : ""),
            s->h.v, &ScreenChangeStyleYesNo,
            (!s->filled ? "" : "not filled"),
            (!s->filled ? "not filled" : ""));
    }

    // The text height, and its units
    Printf(false, "");
    char *chng = (s->h.v < Style::FIRST_CUSTOM) ? "" : "[change]";
    if(s->textHeightAs == Style::UNITS_AS_PIXELS) {
        Printf(false, "%FtTEXT HEIGHT  %E%@ %D%f%Lt%Fl%s%E",
            s->textHeight,
            s->h.v, &ScreenChangeStyleWidthOrTextHeight,
            chng);
    } else {
        Printf(false, "%FtTEXT HEIGHT  %E%s %D%f%Lt%Fl%s%E",
            SS.MmToString(s->textHeight),
            s->h.v, &ScreenChangeStyleWidthOrTextHeight,
            chng);
    }

    bool textHeightpx = (s->textHeightAs == Style::UNITS_AS_PIXELS);
    if(s->h.v < Style::FIRST_CUSTOM) {
        Printf(false,"%FtIN UNITS OF  %Fspixels%E");
    } else {
        Printf(false,"%FtIN UNITS OF  "
                            "%Fh%D%f%Lh%s%E%Fs%s%E / %Fh%D%f%Lh%s%E%Fs%s%E",
            s->h.v, &ScreenChangeStyleYesNo,
            ( textHeightpx ? "" : "pixels"),
            ( textHeightpx ? "pixels" : ""),
            s->h.v, &ScreenChangeStyleYesNo,
            (!textHeightpx ? "" : unit),
            (!textHeightpx ? unit : ""));
    }

    if(s->h.v >= Style::FIRST_CUSTOM) {
        Printf(true, "%FtTEXT ANGLE   %E%@ %D%f%Ll%Fl[change]%E",
            s->textAngle,
            s->h.v, &ScreenChangeStyleTextAngle);

        bool neither;
        neither = !(s->textOrigin & (Style::ORIGIN_LEFT | Style::ORIGIN_RIGHT));
        Printf(true, "%FtALIGN TEXT   "
                            "%Fh%D%f%LL%s%E%Fs%s%E   / "
                            "%Fh%D%f%LH%s%E%Fs%s%E / "
                            "%Fh%D%f%LR%s%E%Fs%s%E",
            s->h.v, &ScreenChangeStyleYesNo,
            ((s->textOrigin & Style::ORIGIN_LEFT) ? "" : "left"),
            ((s->textOrigin & Style::ORIGIN_LEFT) ? "left" : ""),
            s->h.v, &ScreenChangeStyleYesNo,
            (neither ? "" : "center"),
            (neither ? "center" : ""),
            s->h.v, &ScreenChangeStyleYesNo,
            ((s->textOrigin & Style::ORIGIN_RIGHT) ? "" : "right"),
            ((s->textOrigin & Style::ORIGIN_RIGHT) ? "right" : ""));

        neither = !(s->textOrigin & (Style::ORIGIN_BOT | Style::ORIGIN_TOP));
        Printf(false, "%Ft             "
                            "%Fh%D%f%LB%s%E%Fs%s%E / "
                            "%Fh%D%f%LV%s%E%Fs%s%E / "
                            "%Fh%D%f%LT%s%E%Fs%s%E",
            s->h.v, &ScreenChangeStyleYesNo,
            ((s->textOrigin & Style::ORIGIN_BOT) ? "" : "bottom"),
            ((s->textOrigin & Style::ORIGIN_BOT) ? "bottom" : ""),
            s->h.v, &ScreenChangeStyleYesNo,
            (neither ? "" : "center"),
            (neither ? "center" : ""),
            s->h.v, &ScreenChangeStyleYesNo,
            ((s->textOrigin & Style::ORIGIN_TOP) ? "" : "top"),
            ((s->textOrigin & Style::ORIGIN_TOP) ? "top" : ""));
    }

    if(s->h.v >= Style::FIRST_CUSTOM) {
        Printf(false, "");
        Printf(false,
            "%FtOBJECTS ARE  %Fh%D%f%Lv%s%E%Fs%s%E / %Fh%D%f%Lv%s%E%Fs%s%E",
                s->h.v, &ScreenChangeStyleYesNo,
                ( s->visible ? "" : "shown"),
                ( s->visible ? "shown" : ""),
                s->h.v, &ScreenChangeStyleYesNo,
                (!s->visible ? "" : "hidden"),
                (!s->visible ? "hidden" : ""));
        Printf(false,
            "%Ft             %Fh%D%f%Le%s%E%Fs%s%E / %Fh%D%f%Le%s%E%Fs%s%E",
                s->h.v, &ScreenChangeStyleYesNo,
                ( s->exportable ? "" : "exported"),
                ( s->exportable ? "exported" : ""),
                s->h.v, &ScreenChangeStyleYesNo,
                (!s->exportable ? "" : "not exported"),
                (!s->exportable ? "not exported" : ""));

        Printf(false, "");
        Printf(false, "To assign lines or curves to this style,");
        Printf(false, "select them on the drawing. Then commit");
        Printf(false, "by clicking the link at the bottom of");
        Printf(false, "this window.");
    }
}

void TextWindow::ScreenAssignSelectionToStyle(int link, DWORD v) {
    Style::AssignSelectionToStyle(v);
}

