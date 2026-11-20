// Basic STEP surface appearance. No STEPcode or generated schema types.
#include "solvespace.h"
#include "stepcolor.h"
#include "stepgeometry.h"
#include "stepreader.h"

#include <map>

namespace SolveSpace {
namespace Step {
namespace {

using Kind = Value::Kind;

const size_t MAX_STYLE_WORK = 4096;
const unsigned MAX_STYLE_DEPTH = 32;

struct Appearance {
    bool invalid = false;
    bool hasColor = false, hasOpacity = false;
    Vector color = {};
    double opacity = 1;

    static Appearance Invalid() {
        Appearance result;
        result.invalid = true;
        return result;
    }

    // Sets have no ordering/last-wins semantics. Duplicate, agreeing styles are
    // harmless; conflicting values at the same specificity must not be guessed.
    void Merge(const Appearance &other) {
        invalid |= other.invalid;
        if(other.hasColor) {
            if(hasColor && (!EXACT(color.x == other.color.x) ||
                            !EXACT(color.y == other.color.y) ||
                            !EXACT(color.z == other.color.z))) invalid = true;
            color = other.color;
            hasColor = true;
        }
        if(other.hasOpacity) {
            if(hasOpacity && !EXACT(opacity == other.opacity)) invalid = true;
            opacity = other.opacity;
            hasOpacity = true;
        }
    }

    RgbaColor Apply(RgbaColor inherited) const {
        if(invalid) return RGBi(150, 150, 150);
        if(hasColor) {
            auto rgb = RgbaColor::FromFloat((float)color.x, (float)color.y, (float)color.z);
            inherited = rgb.WithAlpha(inherited.alpha);
        }
        if(hasOpacity) {
            inherited.alpha = RgbaColor::FromFloat(0, 0, 0, (float)opacity).alpha;
        }
        return inherited;
    }
};

class SurfaceColors {
public:
    explicit SurfaceColors(const Document &document) : document(document) {
        // Index once for all solids/faces. Do not scan the presentation graph
        // for every generated Bezier patch, or interpret boundary/curve colours
        // as surface colours.
        for(int styled : document.EntitiesOfType("STYLED_ITEM")) {
            Value item;
            if(!document.Attribute(styled, "item", &item) || item.kind != Kind::Reference ||
               (!document.IsA(item.reference, "ADVANCED_FACE") &&
                !document.IsA(item.reference, "CLOSED_SHELL") &&
                !document.IsA(item.reference, "SOLID_MODEL"))) continue;
            size_t work = 0;
            items[item.reference].Merge(Read(styled, 0, &work));
        }
    }

    RgbaColor ForFace(int solid, int shell, int orientedShell, int face) const {
        RgbaColor color = RGBi(150, 150, 150);
        // A directly styled face overrides shell/solid defaults. RGB and opacity
        // inherit independently when a more specific style does not supply both.
        for(int item : {solid, shell, orientedShell, face}) {
            auto found = items.find(item);
            if(found != items.end()) color = found->second.Apply(color);
        }
        return color;
    }

private:
    const Document &document;
    std::map<int, Appearance> items, cache;

    bool UnitNumber(int id, const char *name, double *number) const {
        Value value;
        if(!document.Attribute(id, name, &value) ||
           (value.kind != Kind::Real && value.kind != Kind::Integer)) return false;
        *number = value.kind == Kind::Real ? value.real : (double)value.integer;
        return std::isfinite(*number) && *number >= 0 && *number <= 1;
    }

    Appearance Reference(int id, const char *name, unsigned depth, size_t *work) {
        Value value;
        if(!document.Attribute(id, name, &value) || value.kind != Kind::Reference) {
            return Appearance::Invalid();
        }
        return Read(value.reference, depth + 1, work);
    }

    Appearance Aggregate(int id, const char *name, unsigned depth, size_t *work,
                         bool surfaceSide = false) {
        Value value;
        if(!document.Attribute(id, name, &value) || value.kind != Kind::Aggregate ||
           value.elements.size() > MAX_STYLE_WORK) return Appearance::Invalid();
        Appearance result, rendering;
        for(const auto &entry : value.elements) {
            // NULL_STYLE is a select alternative, not a presentation entity.
            if(entry.kind == Kind::Enumeration && entry.selectedType == "NULL_STYLE") continue;
            if(entry.kind != Kind::Reference) return Appearance::Invalid();
            auto style = Read(entry.reference, depth + 1, work);
            if(surfaceSide && document.IsA(entry.reference, "SURFACE_STYLE_RENDERING")) {
                rendering.Merge(style);
            } else {
                result.Merge(style);
            }
        }
        // Shaded surfaces use the rendering colour when one is provided; fill
        // colour is the fallback for the simpler, commonly exported style chain.
        return surfaceSide && (rendering.hasColor || rendering.invalid) ? rendering : result;
    }

    Appearance Read(int id, unsigned depth, size_t *work) {
        // Appearance cannot make a valid solid fail conversion. Bound both
        // recursive depth and graph traversal, and fall back instead of throwing.
        if(depth > MAX_STYLE_DEPTH || ++(*work) > MAX_STYLE_WORK) return Appearance::Invalid();
        auto found = cache.find(id);
        if(found != cache.end()) return found->second;
        Appearance result;
        if(document.IsA(id, "OVER_RIDING_STYLED_ITEM") ||
           document.IsA(id, "PRESENTATION_STYLE_BY_CONTEXT")) {
            // No occurrence/context selection or override-chain guessing.
            result = Appearance::Invalid();
        } else if(document.IsA(id, "STYLED_ITEM") ||
                  document.IsA(id, "PRESENTATION_STYLE_ASSIGNMENT")) {
            result = Aggregate(id, "styles", depth, work);
        } else if(document.IsA(id, "SURFACE_STYLE_USAGE")) {
            Value side;
            if(!document.Attribute(id, "side", &side) || side.kind != Kind::Enumeration ||
               side.text != "BOTH") {
                // Native surfaces have one RGBA colour, not separate front/back
                // materials. Do not apply a one-sided style to both sides.
                result = Appearance::Invalid();
            } else {
                result = Reference(id, "style", depth, work);
            }
        } else if(document.IsA(id, "SURFACE_SIDE_STYLE")) {
            result = Aggregate(id, "styles", depth, work, true);
        } else if(document.IsA(id, "SURFACE_STYLE_FILL_AREA")) {
            result = Reference(id, "fill_area", depth, work);
        } else if(document.IsA(id, "FILL_AREA_STYLE")) {
            result = Aggregate(id, "fill_styles", depth, work);
        } else if(document.IsA(id, "FILL_AREA_STYLE_COLOUR")) {
            result = Reference(id, "fill_colour", depth, work);
        } else if(document.IsA(id, "SURFACE_STYLE_RENDERING")) {
            result = Reference(id, "surface_colour", depth, work);
            if(document.IsA(id, "SURFACE_STYLE_RENDERING_WITH_PROPERTIES")) {
                result.Merge(Aggregate(id, "properties", depth, work));
            }
        } else if(document.IsA(id, "SURFACE_STYLE_TRANSPARENT")) {
            double transparency;
            if(!UnitNumber(id, "transparency", &transparency)) {
                result = Appearance::Invalid();
            } else {
                result.hasOpacity = true;
                result.opacity = 1 - transparency;
            }
        } else if(document.IsA(id, "COLOUR_RGB")) {
            result.hasColor = UnitNumber(id, "red", &result.color.x) &&
                              UnitNumber(id, "green", &result.color.y) &&
                              UnitNumber(id, "blue", &result.color.z);
            result.invalid = !result.hasColor;
        } else if(document.IsA(id, "DRAUGHTING_PRE_DEFINED_COLOUR")) {
            // These eight standard names are ASCII; no STEP string unescaping
            // or locale-dependent colour-name interpretation is necessary.
            static const std::map<std::string, Vector> colors = {
                {"'black'", {0, 0, 0}}, {"'white'", {1, 1, 1}},
                {"'red'", {1, 0, 0}}, {"'green'", {0, 1, 0}},
                {"'blue'", {0, 0, 1}}, {"'yellow'", {1, 1, 0}},
                {"'magenta'", {1, 0, 1}}, {"'cyan'", {0, 1, 1}}
            };
            Value name;
            if(document.Attribute(id, "name", &name) && name.kind == Kind::String) {
                auto color = colors.find(name.text);
                if(color != colors.end()) {
                    result.color = color->second;
                    result.hasColor = true;
                }
            }
            result.invalid = !result.hasColor;
        } else if(document.IsA(id, "CURVE_STYLE") || document.IsA(id, "POINT_STYLE") ||
                  document.IsA(id, "SYMBOL_STYLE") || document.IsA(id, "TEXT_STYLE") ||
                  document.IsA(id, "SURFACE_STYLE_BOUNDARY") ||
                  document.IsA(id, "SURFACE_STYLE_CONTROL_GRID") ||
                  document.IsA(id, "SURFACE_STYLE_PARAMETER_LINE") ||
                  document.IsA(id, "SURFACE_STYLE_SEGMENTATION_CURVE") ||
                  document.IsA(id, "SURFACE_STYLE_SILHOUETTE") ||
                  document.IsA(id, "SURFACE_STYLE_REFLECTANCE_AMBIENT")) {
            // Curve/text styles and lighting/reflection properties are not surface
            // RGB/opacity. In particular, never follow their colour references.
        } else {
            // Unsupported colours, patterns, textures and external materials must
            // not silently inherit a potentially misleading solid/shell colour.
            result = Appearance::Invalid();
        }
        cache.emplace(id, result);
        return result;
    }
};

} // namespace

void ApplySurfaceColors(const Document &document, GeometryResult *geometry) {
    if(geometry->solids.empty()) return;
    SurfaceColors colors(document);
    for(auto &solid : geometry->solids) for(const auto &face : solid->faces) {
        RgbaColor color = colors.ForFace(solid->solid, face.shell, face.orientedShell, face.face);
        for(hSSurface surface : face.surfaces) {
            solid->shell.surface.FindById(surface)->color = color;
        }
    }
}

} // namespace Step
} // namespace SolveSpace
