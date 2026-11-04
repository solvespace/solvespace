//-----------------------------------------------------------------------------
// The property screen of a selected gear: its parameters, the dimensions
// that follow from them, warnings, and the gears it meshes with.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

// Link characters for the parameters of a gear.
static const char GEAR_LINK_MODULE    = 'm';
static const char GEAR_LINK_TEETH     = 'z';
static const char GEAR_LINK_ANGLE     = 'a';
static const char GEAR_LINK_SHIFT     = 'x';
static const char GEAR_LINK_CLEARANCE = 'c';
static const char GEAR_LINK_BACKLASH  = 'j';
static const char GEAR_LINK_BORE      = 'b';

static std::string GearParamString(const GearParams &gp, int link, bool editable) {
    switch(link) {
        case GEAR_LINK_MODULE:    return SS.MmToString(gp.module, editable);
        case GEAR_LINK_TEETH:     return ssprintf("%d", gp.teeth);
        case GEAR_LINK_ANGLE:     return ssprintf("%.2f", gp.pressureAngle);
        case GEAR_LINK_SHIFT:     return ssprintf("%.3f", gp.profileShift);
        case GEAR_LINK_CLEARANCE: return ssprintf("%.3f", gp.clearance);
        case GEAR_LINK_BACKLASH:  return SS.MmToString(gp.backlash, editable);
        case GEAR_LINK_BORE:      return SS.MmToString(gp.bore, editable);
    }
    return "";
}

void TextWindow::ScreenEditGear(int link, uint32_t v) {
    hRequest hr = { v };
    Request *r = SK.request.FindByIdNoOops(hr);
    if(r == NULL) return;

    SS.TW.ShowEditControl(20, GearParamString(r->gear, link, /*editable=*/true));
    SS.TW.edit.meaning = Edit::GEAR_PARAM;
    SS.TW.edit.request = hr;
    SS.TW.edit.i = link;
}

void TextWindow::ScreenAddGearMate(int link, uint32_t v) {
    hRequest hr = { v };
    Request *r = SK.request.FindByIdNoOops(hr);
    if(r == NULL) return;

    if(link == 'r') {
        SS.TW.ShowEditControl(4, "2");
        SS.TW.edit.meaning = Edit::GEAR_MATE_RATIO;
    } else {
        SS.TW.ShowEditControl(4, ssprintf("%d", r->gear.teeth * 2));
        SS.TW.edit.meaning = Edit::GEAR_MATE_TEETH;
    }
    SS.TW.edit.request = hr;
}

bool TextWindow::EditControlDoneForGear(const std::string &s) {
    Request *r = SK.request.FindByIdNoOops(edit.request);
    if(r == NULL || r->type != Request::Type::GEAR) return false;

    Expr *e = Expr::From(s, /*popUpError=*/true);
    if(e == NULL) return false;

    if(edit.meaning == Edit::GEAR_HUB_PARAM) {
        double val = SS.ExprToMm(e);
        if(!(val > 0)) {
            Error(_("The value must be greater than zero."));
            return false;
        }
        switch(edit.i) {
            case 'w': gearHub.width = val; break;
            case 's':
                gearHub.shaft = val;
                gearHub.SuggestHub();
                break;
            case 'd': gearHub.hubDiameter = val; break;
            case 'l': gearHub.hubLength = val; break;
            case 'h': gearHub.clampHole = val; break;
        }
        SS.ScheduleShowTW();
        return true;
    }

    if(edit.meaning == Edit::GEAR_MATE_TEETH || edit.meaning == Edit::GEAR_MATE_RATIO) {
        int teeth;
        if(edit.meaning == Edit::GEAR_MATE_RATIO) {
            double ratio = e->Eval();
            if(!(ratio > 0)) {
                Error(_("The ratio must be greater than zero."));
                return false;
            }
            teeth = (int)floor(r->gear.teeth * ratio + 0.5);
        } else {
            teeth = (int)floor(e->Eval() + 0.5);
        }
        SS.UndoRemember();
        hRequest hm = GearAddMate(r->h, teeth);
        if(hm.v == 0) return false;
        SS.GW.ClearSelection();
        SS.GW.MakeSelected(hm.entity(0));
        SS.ScheduleShowTW();
        return true;
    }

    GearParams gp = r->gear;
    switch(edit.i) {
        case GEAR_LINK_MODULE: {
            double m = SS.ExprToMm(e);
            if(!(m >= 0.05 && m <= 100)) {
                Error(_("The module must be between 0.05 mm and 100 mm."));
                return false;
            }
            gp.module = m;
            break;
        }
        case GEAR_LINK_TEETH: {
            int z = (int)floor(e->Eval() + 0.5);
            if(z < GEAR_MIN_TEETH || z > GEAR_MAX_TEETH) {
                Error(_("The number of teeth must be between %d and %d."),
                      GEAR_MIN_TEETH, GEAR_MAX_TEETH);
                return false;
            }
            gp.teeth = z;
            break;
        }
        case GEAR_LINK_ANGLE: {
            double a = e->Eval();
            if(!(a >= 10 && a <= 35)) {
                Error(_("The pressure angle must be between 10 and 35 degrees."));
                return false;
            }
            gp.pressureAngle = a;
            break;
        }
        case GEAR_LINK_SHIFT: {
            double x = e->Eval();
            if(!(x >= -1 && x <= 2)) {
                Error(_("The profile shift factor must be between -1 and 2."));
                return false;
            }
            gp.profileShift = x;
            break;
        }
        case GEAR_LINK_CLEARANCE: {
            double c = e->Eval();
            if(!(c >= 0 && c <= 0.5)) {
                Error(_("The clearance factor must be between 0 and 0.5."));
                return false;
            }
            gp.clearance = c;
            break;
        }
        case GEAR_LINK_BACKLASH: {
            double j = SS.ExprToMm(e);
            if(!(j >= 0 && j <= gp.module)) {
                Error(_("The backlash must be between zero and the module."));
                return false;
            }
            gp.backlash = j;
            break;
        }
        case GEAR_LINK_BORE: {
            double d = SS.ExprToMm(e);
            if(!(d >= 0)) {
                Error(_("The bore diameter must not be negative."));
                return false;
            }
            gp.bore = d;
            break;
        }
        default:
            return false;
    }
    if(gp.Equals(r->gear)) return true;

    SS.UndoRemember();
    r = SK.GetRequest(edit.request);
    r->gear = gp;
    GearSyncLinked(r->h);
    SS.MarkGroupDirty(r->group);
    SS.ScheduleShowTW();
    return true;
}

void TextWindow::DescribeGear(Entity *e) {
    const GearParams &gp = e->gear;
    bool editable = e->h.isFromRequest();
    uint32_t hv = editable ? e->h.request().v : 0;

    Printf(false, "%FtINVOLUTE GEAR%E");
    struct Row { const char *label; char link; const char *unit; };
    const Row rows[] = {
        { "module m       ", GEAR_LINK_MODULE,    ""   },
        { "teeth z        ", GEAR_LINK_TEETH,     ""   },
        { "pressure angle ", GEAR_LINK_ANGLE,     "°" },
        { "profile shift x", GEAR_LINK_SHIFT,     ""   },
        { "clearance c*   ", GEAR_LINK_CLEARANCE, ""   },
        { "backlash       ", GEAR_LINK_BACKLASH,  ""   },
        { "bore           ", GEAR_LINK_BORE,      ""   },
    };
    bool first = true;
    for(const Row &row : rows) {
        std::string value = GearParamString(gp, row.link, /*editable=*/false);
        if(row.link == GEAR_LINK_BORE && gp.bore <= 0) value = _("none");
        value += row.unit;
        if(editable) {
            std::string fmt = std::string("  %s = %Fi%s%E %Fl%L") + row.link + "%f%D[change]%E";
            Printf(first, fmt.c_str(), row.label, value.c_str(), &ScreenEditGear, hv);
        } else {
            Printf(first, "  %s = %Fi%s%E", row.label, value.c_str());
        }
        first = false;
    }

    GearInfo info = GearComputeInfo(gp);
    if(info.valid) {
        Printf(true,  "  pitch diameter = %Fi%s%E", SS.MmToString(2 * info.pitchRadius).c_str());
        Printf(false, "  tip diameter   = %Fi%s%E", SS.MmToString(2 * info.tipRadius).c_str());
        Printf(false, "  root diameter  = %Fi%s%E", SS.MmToString(2 * info.rootRadius).c_str());
        Printf(false, "  base diameter  = %Fi%s%E", SS.MmToString(2 * info.baseRadius).c_str());
    }

    // Problems, mostly in view of a printed gear.
    bool firstWarning = true;
    auto warn = [&](const std::string &s) {
        Printf(firstWarning, "  %Fx%s%E", s.c_str());
        firstWarning = false;
    };
    if(!info.valid) {
        warn(_("no tooth shape possible for these values"));
    } else {
        if(info.undercut) {
            double sa = sin(gp.pressureAngle * PI / 180);
            double xmin = 1 - gp.teeth * sa * sa / 2;
            warn(ssprintf(_("teeth are undercut; profile shift x >= %.2f avoids it"), xmin));
        }
        if(info.pointed) {
            warn(_("teeth are pointed; reduce the profile shift"));
        } else if(info.tipThickness < 0.25 * gp.module) {
            warn(_("tooth tips are very thin; reduce the profile shift"));
        }
        if(gp.bore > 0 && gp.bore / 2 >= info.rootRadius - LENGTH_EPS) {
            warn(_("bore is larger than the root circle and is not drawn"));
        }
    }
    if(gp.module < GEAR_MIN_PRINTABLE_MODULE) {
        warn(ssprintf(_("a module below %.1f mm is hard to print with a 0.4 mm nozzle"),
                      GEAR_MIN_PRINTABLE_MODULE));
    }

    if(!editable) return;

    // Gears that mesh with this one.
    Request *r = SK.request.FindByIdNoOops(e->h.request());
    if(r == NULL) return;
    bool firstMate = true;
    auto showMate = [&](Request *o) {
        double ratio = (double)o->gear.teeth / r->gear.teeth;
        std::string dist = SS.MmToString(GearCenterDistance(r->gear, o->gear));
        Printf(firstMate, "  meshes with %Fi%s%E: ratio %Fi1 : %#%E, center distance %Fi%s%E",
               o->DescriptionString().c_str(), ratio, dist.c_str());
        firstMate = false;
    };
    if(Request *d = SK.request.FindByIdNoOops(r->gearDriver)) {
        if(d->type == Request::Type::GEAR) showMate(d);
    }
    for(Request &o : SK.request) {
        if(o.type == Request::Type::GEAR && o.gearDriver == r->h) showMate(&o);
    }

    Printf(true, "  %Fl%Lt%f%D[add mating gear by number of teeth]%E",
           &ScreenAddGearMate, hv);
    Printf(false, "  %Fl%Lr%f%D[add mating gear by ratio]%E",
           &ScreenAddGearMate, hv);
    Printf(false, "  %Fl%Lh%f%D[make solid with hub and clamp screw]%E",
           &ScreenGearHub, hv);
}

//-----------------------------------------------------------------------------
// The screen that sets up a hub with clamp screw for a gear.
//-----------------------------------------------------------------------------
void TextWindow::ScreenGearHub(int link, uint32_t v) {
    hRequest hr = { v };
    Request *r = SK.request.FindByIdNoOops(hr);
    if(r == NULL) return;

    SS.TW.shown.gear = hr;
    SS.TW.gearHub = GearHubParams::Default(r->gear);
    SS.GW.ClearSelection();
    SS.TW.GoToScreen(Screen::GEAR_HUB);
    SS.ScheduleShowTW();
}

void TextWindow::ScreenChangeGearHub(int link, uint32_t v) {
    GearHubParams &hp = SS.TW.gearHub;
    double val = 0;
    switch(link) {
        case 'w': val = hp.width;       break;
        case 's': val = hp.shaft;       break;
        case 'd': val = hp.hubDiameter; break;
        case 'l': val = hp.hubLength;   break;
        case 'h': val = hp.clampHole;   break;

        case 'n':
        case 'i':
        case 't':
            hp.clamp = (link == 'i') ? GearHubParams::Clamp::INSERT :
                       (link == 't') ? GearHubParams::Clamp::TAPPED :
                                       GearHubParams::Clamp::NONE;
            hp.SuggestHub();
            SS.ScheduleShowTW();
            return;

        default:
            return;
    }
    SS.TW.ShowEditControl(20, SS.MmToString(val, true));
    SS.TW.edit.meaning = Edit::GEAR_HUB_PARAM;
    SS.TW.edit.request = SS.TW.shown.gear;
    SS.TW.edit.i = link;
}

void TextWindow::ScreenCreateGearHub(int link, uint32_t v) {
    SS.UndoRemember();
    std::string error;
    if(!GearAddHub(SS.TW.shown.gear, SS.TW.gearHub, &error)) {
        Error("%s", error.c_str());
        return;
    }
    SS.GW.ClearSelection();
    SS.TW.GoToScreen(Screen::LIST_OF_GROUPS);
    SS.ScheduleShowTW();
    SS.GW.Invalidate();
}

void TextWindow::ShowGearHub() {
    Request *r = SK.request.FindByIdNoOops(shown.gear);
    if(r == NULL || r->type != Request::Type::GEAR) {
        GoToScreen(Screen::LIST_OF_GROUPS);
        Show();
        return;
    }
    const GearHubParams &hp = gearHub;
    using Clamp = GearHubParams::Clamp;

    Printf(true, "%FtGEAR WITH HUB AND CLAMP SCREW%E");
    Printf(false, "  for gear %Fi%s%E", r->DescriptionString().c_str());

    struct Row { const char *label; char link; double value; bool show; };
    const Row rows[] = {
        { "gear width     ", 'w', hp.width,       true },
        { "shaft diameter ", 's', hp.shaft,       true },
        { "hub diameter   ", 'd', hp.hubDiameter, true },
        { "hub length     ", 'l', hp.hubLength,   true },
        { "clamp hole     ", 'h', hp.clampHole,   hp.clamp != Clamp::NONE },
    };
    bool first = true;
    for(const Row &row : rows) {
        if(!row.show) continue;
        std::string fmt = std::string("  %s = %Fi%s%E %Fl%L") + row.link + "%f[change]%E";
        Printf(first, fmt.c_str(), row.label, SS.MmToString(row.value).c_str(),
               &ScreenChangeGearHub);
        first = false;
    }

    Printf(true, "  %Ftclamp screw%E");
    std::string insert = ssprintf("M3 heat-set insert, %.1f mm long", GEAR_INSERT_LENGTH);
    Printf(false, "  %f%Li%Fd%s  %s%E",
           &ScreenChangeGearHub, hp.clamp == Clamp::INSERT ? RADIO_TRUE : RADIO_FALSE,
           insert.c_str());
    Printf(false, "  %f%Lt%Fd%s  M3 screw tapping into the hub%E",
           &ScreenChangeGearHub, hp.clamp == Clamp::TAPPED ? RADIO_TRUE : RADIO_FALSE);
    Printf(false, "  %f%Ln%Fd%s  none%E",
           &ScreenChangeGearHub, hp.clamp == Clamp::NONE ? RADIO_TRUE : RADIO_FALSE);

    Printf(true, "  %FdA new shaft diameter or clamp type%E");
    Printf(false, "  %Fdsuggests the hub and hole again.%E");

    std::string problem = hp.Problem(r->gear);
    std::string warning = hp.Warning();
    if(!problem.empty()) {
        Printf(true, "  %Fx%s%E", problem.c_str());
    } else {
        if(!warning.empty()) Printf(true, "  %Fx%s%E", warning.c_str());
        Printf(true, "  %Fl%f%Ll[create]%E", &ScreenCreateGearHub);
    }
    Printf(true, "(or %Fl%Ll%fback to home screen%E)", &ScreenHome);
}

}
