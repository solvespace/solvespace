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

    std::string val;
    if(r->type == Request::Type::GEAR_RACK) {
        switch(link) {
            case 'z': val = ssprintf("%d", r->gear.teeth); break;
            case 'l': val = SS.MmToString(PI * r->gear.module * r->gear.teeth, true); break;
            case 'b': val = SS.MmToString(r->gear.rackBase, true); break;
            default: return;
        }
    } else {
        val = GearParamString(r->gear, link, /*editable=*/true);
    }
    SS.TW.ShowEditControl(20, val);
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

// Number of teeth of a rack from an expression, given as a number of teeth
// or, for link 'l', as a length that is rounded to whole teeth.
static bool RackTeethFrom(Expr *e, int link, double module, int *teeth) {
    int n;
    if(link == 'l') {
        double len = SS.ExprToMm(e);
        if(!(len > 0)) {
            Error(_("The length must be greater than zero."));
            return false;
        }
        n = std::max(1, (int)floor(len / (PI * module) + 0.5));
    } else {
        n = (int)floor(e->Eval() + 0.5);
    }
    if(n < GEAR_RACK_MIN_TEETH || n > GEAR_RACK_MAX_TEETH) {
        Error(_("The number of teeth must be between %d and %d."),
              GEAR_RACK_MIN_TEETH, GEAR_RACK_MAX_TEETH);
        return false;
    }
    *teeth = n;
    return true;
}

bool TextWindow::EditControlDoneForGear(const std::string &s) {
    Request *r = SK.request.FindByIdNoOops(edit.request);
    if(r == NULL) return false;
    bool rack = (r->type == Request::Type::GEAR_RACK);
    if(r->type != Request::Type::GEAR && !rack) return false;

    Expr *e = Expr::From(s, /*popUpError=*/true);
    if(e == NULL) return false;

    if(edit.meaning == Edit::GEAR_RACK_SOLID_PARAM) {
        GearRackSolidParams &sp = gearRackSolid;
        if(edit.i == 'n') {
            int n = (int)floor(e->Eval() + 0.5);
            if(n < 1) {
                Error(_("There must be at least one hole."));
                return false;
            }
            sp.holes = n;
        } else {
            double val = SS.ExprToMm(e);
            if(!(val > 0)) {
                Error(_("The value must be greater than zero."));
                return false;
            }
            switch(edit.i) {
                case 'w': sp.width = val;        break;
                case 'b': sp.base = val;         break;
                case 'd': sp.holeDiameter = val; break;
                case 'h': sp.holeDepth = val;    break;
            }
        }
        SS.ScheduleShowTW();
        return true;
    }

    if(edit.meaning == Edit::GEAR_RACK_PARAM) {
        if(edit.i == 'b') {
            double val = SS.ExprToMm(e);
            if(!(val > 0)) {
                Error(_("The value must be greater than zero."));
                return false;
            }
            gearRackBase = val;
        } else if(!RackTeethFrom(e, edit.i, r->gear.module, &gearRackTeeth)) {
            return false;
        }
        SS.ScheduleShowTW();
        return true;
    }

    if(rack) {
        GearParams gp = r->gear;
        if(edit.i == 'b') {
            double val = SS.ExprToMm(e);
            if(!(val > 0)) {
                Error(_("The value must be greater than zero."));
                return false;
            }
            gp.rackBase = val;
        } else if(edit.i == 'z' || edit.i == 'l') {
            if(!RackTeethFrom(e, edit.i, gp.module, &gp.teeth)) return false;
        } else {
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
    for(Request &o : SK.request) {
        if(o.type != Request::Type::GEAR_RACK || o.gearDriver != r->h) continue;
        Printf(firstMate, "  meshes with %Fi%s%E: %Fi%d%E teeth, pitch line at %Fi%s%E",
               o.DescriptionString().c_str(), o.gear.teeth,
               SS.MmToString(GearRackDistance(r->gear)).c_str());
        firstMate = false;
    }

    Printf(true, "  %Fl%Lt%f%D[add mating gear by number of teeth]%E",
           &ScreenAddGearMate, hv);
    Printf(false, "  %Fl%Lr%f%D[add mating gear by ratio]%E",
           &ScreenAddGearMate, hv);
    Printf(false, "  %Fl%Lk%f%D[add rack]%E",
           &ScreenGearRack, hv);
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
    std::vector<std::string> warnings;
    if(!hp.Warning().empty()) warnings.push_back(hp.Warning());
    // The hub must stay clear of the teeth of the gears that mesh with this
    // one, in case they are as wide as the hub is high.
    for(Request &o : SK.request) {
        if(o.type != Request::Type::GEAR || o.h == r->h) continue;
        if(o.gearDriver != r->h && r->gearDriver != o.h) continue;
        double room = GearCenterDistance(r->gear, o.gear) - GearComputeInfo(o.gear).tipRadius;
        if(hp.hubDiameter / 2 > room) {
            warnings.push_back(ssprintf(_("The hub reaches into the teeth of %s."),
                                        o.DescriptionString().c_str()));
        }
    }
    if(!problem.empty()) {
        Printf(true, "  %Fx%s%E", problem.c_str());
    } else {
        bool firstWarning = true;
        for(const std::string &w : warnings) {
            Printf(firstWarning, "  %Fx%s%E", w.c_str());
            firstWarning = false;
        }
        Printf(true, "  %Fl%f%Ll[create]%E", &ScreenCreateGearHub);
    }
    Printf(true, "(or %Fl%Ll%fback to home screen%E)", &ScreenHome);
}



//-----------------------------------------------------------------------------
// A selected rack.
//-----------------------------------------------------------------------------
void TextWindow::DescribeGearRack(Entity *e) {
    const GearParams &gp = e->gear;
    bool editable = e->h.isFromRequest();
    uint32_t hv = editable ? e->h.request().v : 0;
    GearRackInfo info = GearRackComputeInfo(gp);

    Printf(false, "%FtGEAR RACK%E");
    struct Row { const char *label; char link; std::string value; };
    const Row rows[] = {
        { "teeth          ", 'z', ssprintf("%d", gp.teeth) },
        { "length         ", 'l', SS.MmToString(PI * gp.module * gp.teeth) },
        { "back thickness ", 'b', SS.MmToString(gp.rackBase) },
    };
    bool first = true;
    for(const Row &row : rows) {
        if(editable) {
            std::string fmt = std::string("  %s = %Fi%s%E %Fl%L") + row.link + "%f%D[change]%E";
            Printf(first, fmt.c_str(), row.label, row.value.c_str(), &ScreenEditGear, hv);
        } else {
            Printf(first, "  %s = %Fi%s%E", row.label, row.value.c_str());
        }
        first = false;
    }
    Printf(false, "  %FdA length is rounded to whole teeth.%E");

    Printf(true,  "  module m       = %Fi%s%E", SS.MmToString(gp.module).c_str());
    Printf(false, "  pressure angle = %Fi%.2f°%E", gp.pressureAngle);
    Printf(false, "  clearance c*   = %Fi%.3f%E", gp.clearance);
    Printf(false, "  backlash       = %Fi%s%E", SS.MmToString(gp.backlash).c_str());

    if(info.valid) {
        Printf(true,  "  pitch          = %Fi%s%E", SS.MmToString(info.pitch).c_str());
        Printf(false, "  tooth height   = %Fi%s%E",
               SS.MmToString(info.addendum + info.dedendum).c_str());
        Printf(false, "  total height   = %Fi%s%E", SS.MmToString(info.height).c_str());
        Printf(false, "  tip width      = %Fi%s%E", SS.MmToString(info.tipWidth).c_str());
    } else {
        Printf(true, "  %Fx%s%E", _("no tooth shape possible for these values"));
    }

    if(!editable) return;
    Request *r = SK.request.FindByIdNoOops(e->h.request());
    if(r == NULL) return;
    if(Request *d = SK.request.FindByIdNoOops(r->gearDriver)) {
        if(d->type == Request::Type::GEAR) {
            Printf(true, "  meshes with %Fi%s%E, which sets module, pressure angle,",
                   d->DescriptionString().c_str());
            Printf(false, "  clearance and backlash");
        }
    }
    Printf(true, "  %Fl%Ls%f%D[make solid with mounting holes]%E",
           &ScreenGearRackSolid, hv);
}

//-----------------------------------------------------------------------------
// The screen that sets up a rack for a gear.
//-----------------------------------------------------------------------------
void TextWindow::ScreenGearRack(int link, uint32_t v) {
    hRequest hr = { v };
    Request *r = SK.request.FindByIdNoOops(hr);
    if(r == NULL) return;

    SS.TW.shown.gear = hr;
    SS.TW.gearRackTeeth = 10;
    SS.TW.gearRackBase = 2 * r->gear.module;
    SS.GW.ClearSelection();
    SS.TW.GoToScreen(Screen::GEAR_RACK);
    SS.ScheduleShowTW();
}

void TextWindow::ScreenChangeGearRack(int link, uint32_t v) {
    Request *r = SK.request.FindByIdNoOops(SS.TW.shown.gear);
    if(r == NULL) return;
    std::string val;
    switch(link) {
        case 'z': val = ssprintf("%d", SS.TW.gearRackTeeth); break;
        case 'l': val = SS.MmToString(PI * r->gear.module * SS.TW.gearRackTeeth, true); break;
        case 'b': val = SS.MmToString(SS.TW.gearRackBase, true); break;
        default: return;
    }
    SS.TW.ShowEditControl(20, val);
    SS.TW.edit.meaning = Edit::GEAR_RACK_PARAM;
    SS.TW.edit.request = SS.TW.shown.gear;
    SS.TW.edit.i = link;
}

void TextWindow::ScreenCreateGearRack(int link, uint32_t v) {
    SS.UndoRemember();
    hRequest hr = GearAddRack(SS.TW.shown.gear, SS.TW.gearRackTeeth, SS.TW.gearRackBase);
    if(hr.v == 0) return;
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    SS.ScheduleShowTW();
    SS.GW.Invalidate();
}

void TextWindow::ShowGearRack() {
    Request *r = SK.request.FindByIdNoOops(shown.gear);
    if(r == NULL || r->type != Request::Type::GEAR) {
        GoToScreen(Screen::LIST_OF_GROUPS);
        Show();
        return;
    }
    GearParams gp = r->gear;
    gp.teeth = gearRackTeeth;
    gp.profileShift = 0;
    gp.rackBase = gearRackBase;
    GearRackInfo info = GearRackComputeInfo(gp);

    Printf(true, "%FtRACK%E");
    Printf(false, "  for gear %Fi%s%E", r->DescriptionString().c_str());

    struct Row { const char *label; char link; std::string value; };
    const Row rows[] = {
        { "teeth          ", 'z', ssprintf("%d", gearRackTeeth) },
        { "length         ", 'l', SS.MmToString(PI * gp.module * gearRackTeeth) },
        { "back thickness ", 'b', SS.MmToString(gearRackBase) },
    };
    bool first = true;
    for(const Row &row : rows) {
        std::string fmt = std::string("  %s = %Fi%s%E %Fl%L") + row.link + "%f[change]%E";
        Printf(first, fmt.c_str(), row.label, row.value.c_str(), &ScreenChangeGearRack);
        first = false;
    }
    Printf(false, "  %FdA length is rounded to whole teeth.%E");
    if(info.valid) {
        Printf(true, "  total height   = %Fi%s%E", SS.MmToString(info.height).c_str());
    }

    Printf(true, "  %FdModule, pressure angle, clearance and backlash%E");
    Printf(false, "  %Fdfollow the gear. The rack goes below the gear in%E");
    Printf(false, "  %Fda sketch of its own, and can be moved along and%E");
    Printf(false, "  %Fdaround the gear.%E");

    if(!info.valid) {
        Printf(true, "  %Fx%s%E", _("no tooth shape possible for these values"));
    } else {
        Printf(true, "  %Fl%f%Ll[create]%E", &ScreenCreateGearRack);
    }
    Printf(true, "(or %Fl%Ll%fback to home screen%E)", &ScreenHome);
}

//-----------------------------------------------------------------------------
// The screen that makes a rack a solid with mounting holes.
//-----------------------------------------------------------------------------
void TextWindow::ScreenGearRackSolid(int link, uint32_t v) {
    hRequest hr = { v };
    Request *r = SK.request.FindByIdNoOops(hr);
    if(r == NULL) return;

    SS.TW.shown.gear = hr;
    SS.TW.gearRackSolid = GearRackSolidParams::Default(r->gear);
    SS.GW.ClearSelection();
    SS.TW.GoToScreen(Screen::GEAR_RACK_SOLID);
    SS.ScheduleShowTW();
}

void TextWindow::ScreenChangeGearRackSolid(int link, uint32_t v) {
    GearRackSolidParams &sp = SS.TW.gearRackSolid;
    using Mount = GearRackSolidParams::Mount;
    std::string val;
    switch(link) {
        case 'w': val = SS.MmToString(sp.width, true);        break;
        case 'b': val = SS.MmToString(sp.base, true);         break;
        case 'd': val = SS.MmToString(sp.holeDiameter, true); break;
        case 'h': val = SS.MmToString(sp.holeDepth, true);    break;
        case 'n': val = ssprintf("%d", sp.holes);             break;

        case 'I':
        case 'C':
        case 'N':
        {
            Request *r = SK.request.FindByIdNoOops(SS.TW.shown.gear);
            if(r == NULL) return;
            int holes = sp.holes;
            sp = GearRackSolidParams::Default(r->gear,
                (link == 'I') ? Mount::INSERT : (link == 'C') ? Mount::CROSS : Mount::NONE);
            sp.holes = holes;
            SS.ScheduleShowTW();
            return;
        }

        default:
            return;
    }
    SS.TW.ShowEditControl(20, val);
    SS.TW.edit.meaning = Edit::GEAR_RACK_SOLID_PARAM;
    SS.TW.edit.request = SS.TW.shown.gear;
    SS.TW.edit.i = link;
}

void TextWindow::ScreenCreateGearRackSolid(int link, uint32_t v) {
    SS.UndoRemember();
    std::string error;
    if(!GearAddRackSolid(SS.TW.shown.gear, SS.TW.gearRackSolid, &error)) {
        Error("%s", error.c_str());
        return;
    }
    SS.GW.ClearSelection();
    SS.TW.GoToScreen(Screen::LIST_OF_GROUPS);
    SS.ScheduleShowTW();
    SS.GW.Invalidate();
}

void TextWindow::ShowGearRackSolid() {
    Request *r = SK.request.FindByIdNoOops(shown.gear);
    if(r == NULL || r->type != Request::Type::GEAR_RACK) {
        GoToScreen(Screen::LIST_OF_GROUPS);
        Show();
        return;
    }
    const GearRackSolidParams &sp = gearRackSolid;
    using Mount = GearRackSolidParams::Mount;

    Printf(true, "%FtRACK AS A SOLID%E");
    Printf(false, "  for rack %Fi%s%E", r->DescriptionString().c_str());

    struct Row { const char *label; char link; std::string value; bool show; };
    const Row rows[] = {
        { "width          ", 'w', SS.MmToString(sp.width),        true },
        { "back thickness ", 'b', SS.MmToString(sp.base),         true },
        { "holes          ", 'n', ssprintf("%d", sp.holes),       sp.mount != Mount::NONE },
        { "hole diameter  ", 'd', SS.MmToString(sp.holeDiameter), sp.mount != Mount::NONE },
        { "hole depth     ", 'h', SS.MmToString(sp.holeDepth),    sp.mount == Mount::INSERT },
    };
    bool first = true;
    for(const Row &row : rows) {
        if(!row.show) continue;
        std::string fmt = std::string("  %s = %Fi%s%E %Fl%L") + row.link + "%f[change]%E";
        Printf(first, fmt.c_str(), row.label, row.value.c_str(), &ScreenChangeGearRackSolid);
        first = false;
    }

    Printf(true, "  %Ftmounting%E");
    std::string insert = ssprintf("M3 heat-set inserts from the back, %.1f mm long",
                                  GEAR_INSERT_LENGTH);
    Printf(false, "  %f%LI%Fd%s  %s%E",
           &ScreenChangeGearRackSolid, sp.mount == Mount::INSERT ? RADIO_TRUE : RADIO_FALSE,
           insert.c_str());
    Printf(false, "  %f%LC%Fd%s  M3 through holes across the back%E",
           &ScreenChangeGearRackSolid, sp.mount == Mount::CROSS ? RADIO_TRUE : RADIO_FALSE);
    Printf(false, "  %f%LN%Fd%s  none%E",
           &ScreenChangeGearRackSolid, sp.mount == Mount::NONE ? RADIO_TRUE : RADIO_FALSE);

    Printf(true, "  %FdA new mounting suggests width, back and holes%E");
    Printf(false, "  %Fdagain. The holes are spaced evenly along the rack.%E");

    std::string problem = sp.Problem(r->gear);
    if(!problem.empty()) {
        Printf(true, "  %Fx%s%E", problem.c_str());
    } else {
        if(!sp.Warning().empty()) Printf(true, "  %Fx%s%E", sp.Warning().c_str());
        Printf(true, "  %Fl%f%Ll[create]%E", &ScreenCreateGearRackSolid);
    }
    Printf(true, "(or %Fl%Ll%fback to home screen%E)", &ScreenHome);
}

}
