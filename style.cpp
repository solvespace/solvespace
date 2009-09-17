#include "solvespace.h"

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
    int i = 0, j = 0;
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

void Style::CreateDefaultStyle(hStyle h) {
    const Default *d;
    for(d = &(Defaults[0]); d->h.v; d++) {
        if(d->h.v == h.v) break;
    }
    if(!d->h.v) {
        // Not a default style; so just create it the same as our default
        // active group entity style.
        d = &(Defaults[0]);
    }

    Style ns;
    ZERO(&ns);
    ns.color      = CnfThawDWORD(d->color, CnfColor(d->cnfPrefix));
    ns.width      = CnfThawFloat((float)(d->width), CnfWidth(d->cnfPrefix));
    ns.widthHow   = WIDTH_PIXELS;
    ns.visible    = true;
    ns.exportable = true;
    ns.name.strcpy(CnfPrefixToName(d->cnfPrefix));
    ns.h          = h;

    SK.style.Add(&ns);
}

void Style::LoadFactoryDefaults(void) {
    const Default *d;
    for(d = &(Defaults[0]); d->h.v; d++) {
        Style *s = Get(d->h);

        s->color      = d->color;
        s->width      = d->width;
        s->widthHow   = WIDTH_PIXELS;
        s->visible    = true;
        s->exportable = true;
        s->name.strcpy(CnfPrefixToName(d->cnfPrefix));
    }
}

void Style::FreezeDefaultStyles(void) {
    const Default *d;
    for(d = &(Defaults[0]); d->h.v; d++) {
        CnfFreezeDWORD(Color(d->h), CnfColor(d->cnfPrefix));
        CnfFreezeFloat((float)Width(d->h), CnfWidth(d->cnfPrefix));
    }
}

//-----------------------------------------------------------------------------
// Look up a style by its handle. If that style does not exist, then create
// the style, according to our table of default styles.
//-----------------------------------------------------------------------------
Style *Style::Get(hStyle h) {
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
    return s->color;
}

//-----------------------------------------------------------------------------
// Return the width associated with our style in pixels..
//-----------------------------------------------------------------------------
float Style::Width(hStyle h) {
    double r = 1.0;
    Style *s = Get(h);
    if(s->widthHow == WIDTH_MM) {
        r = s->width * SS.GW.scale;
    } else if(s->widthHow == WIDTH_PIXELS) {
        r = s->width;
    }
    // This returns a float because glLineWidth expects a float, avoid casts.
    return (float)r;
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

