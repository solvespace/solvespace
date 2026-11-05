//-----------------------------------------------------------------------------
// Involute spur gear outlines, generated as the envelope of a standard basic
// rack with rounded tip, so that undercut and profile shift come out right.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_GEAR_H
#define SOLVESPACE_GEAR_H

namespace SolveSpace {

// The parameters of one gear. Lengths are in mm, angles in degrees, the
// remaining values are factors of the module.
class GearParams {
public:
    double module;
    int    teeth;
    double pressureAngle;
    double profileShift;
    double clearance;
    // Circumferential play at the pitch circle for a pair of gears with the
    // same value. Each tooth is made thinner by half of it.
    double backlash;
    // Diameter of a center hole, or zero for none.
    double bore;

    static GearParams Default();
    bool Equals(const GearParams &o) const;
};

// Dimensions and problems of a gear, for display and checks.
class GearInfo {
public:
    double pitchRadius;
    double baseRadius;
    double tipRadius;
    double rootRadius;
    double filletRadius;
    // Radius where the involute begins; above the base circle if the tooth
    // is undercut.
    double involuteStartRadius;
    // Tooth thickness on the tip circle, as an arc length; zero if pointed.
    double tipThickness;
    bool   undercut;
    bool   pointed;
    bool   valid;
};

// Smallest module that still prints reasonably with a 0.4 mm nozzle.
static const double GEAR_MIN_PRINTABLE_MODULE = 0.8;
static const int    GEAR_MIN_TEETH = 6;
static const int    GEAR_MAX_TEETH = 400;

GearInfo GearComputeInfo(const GearParams &gp);

// Appends the closed outline of the gear, and the bore if any, to sbl. The
// gear lies in the plane spanned by the unit vectors xd and yd, with its
// center at c and the middle of one tooth in direction xd. All lengths are
// multiplied by scale. The bore is split into quarter arcs at the
// directions of boreU, if given, as circles in a workplane with that u axis
// are, so that faces of solids made from both share their edges exactly.
void GearMakeOutline(const GearParams &gp, Vector c, Vector xd, Vector yd,
                     double scale, SBezierList *sbl, Vector boreU = Vector::From(0, 0, 0));

// Center distance of two gears in mesh without backlash, taking the profile
// shifts into account. Both gears must have the same module and pressure
// angle.
double GearCenterDistance(const GearParams &a, const GearParams &b);

// Angle of a tooth middle of a mating gear, measured from the line toward
// the driving gear, given the angle of a tooth middle of the driving gear,
// measured from the line toward the mating gear. Both angles counterclockwise
// in the same plane.
double GearMatePhase(int driverTeeth, int mateTeeth, double driverPhase);

// A hub on one face of a gear, with a radial hole for a clamp screw.
class GearHubParams {
public:
    enum class Clamp : uint32_t {
        NONE   = 0,
        // A hole for an M3 heat-set insert, 5.7 mm long.
        INSERT = 1,
        // A hole that an M3 screw cuts its own thread into.
        TAPPED = 2
    };

    double width;         // gear face width
    double shaft;         // diameter of the shaft hole
    double hubDiameter;
    double hubLength;     // beyond the gear face
    Clamp  clamp;
    double clampHole;     // diameter of the clamp screw hole

    // Suggested values for a gear: the shaft from its bore, or 5 mm, and a
    // hub that holds the clamp screw.
    static GearHubParams Default(const GearParams &gp);
    // Hub diameter, length and clamp hole that suit the shaft and clamp.
    void SuggestHub();

    // An error that keeps the hub from being made, or an empty string.
    std::string Problem(const GearParams &gp) const;
    // A hint about a hub that can be made but may not work well.
    std::string Warning() const;
};

static const double GEAR_INSERT_LENGTH = 5.7;

// Where GearAddHub puts its groups: true after the active group, false
// right after the sketch of the gear.
static const bool GEAR_HUB_AFTER_ACTIVE_GROUP = true;

class hRequest;
class hConstraint;

// Gears made to mesh with each other share module, pressure angle,
// clearance and backlash. After a change to the gear hr, copies these to
// all gears linked to it, sets the distance constraints between linked
// centers, and turns each mating gear so that its teeth mesh again.
void GearSyncLinked(hRequest hr);

// Adds a gear with the given number of teeth that meshes with the gear
// driver, to the right of it, and a distance constraint between the
// centers. Returns the new request, or a null handle with an error shown.
hRequest GearAddMate(hRequest driver, int teeth);

// The distance constraint between the centers of two gears, if any.
hConstraint GearFindCenterConstraint(hRequest a, hRequest b);

// Makes the gear a solid with a hub. Gives the gear a bore for the shaft,
// and adds after the active group (see GEAR_HUB_AFTER_ACTIVE_GROUP), which
// must not come before the sketch of the gear: an extrusion of that sketch, a
// sketch of the hub ring on the gear face and its extrusion, and for a clamp
// a sketch of the clamp hole across the hub and its extrusion as a
// difference. These are ordinary groups that can be edited later. Returns
// false with an error message if that is not possible.
bool GearAddHub(hRequest gear, const GearHubParams &hp, std::string *error);

}

#endif
