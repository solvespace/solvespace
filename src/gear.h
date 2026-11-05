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
    // For a rack: the thickness of its back below the root line.
    double rackBase;

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

// Dimensions of a rack that meshes with gears of the same module, pressure
// angle, clearance and backlash. Its teeth have the shape of the basic rack,
// thinner by half the backlash, with a sharp tip and a rounded root.
class GearRackInfo {
public:
    double pitch;
    double length;        // teeth times pitch
    double addendum;      // tip above the pitch line
    double dedendum;      // root below the pitch line
    double height;        // tip to back
    double filletRadius;
    double tipWidth;      // flat of the tooth tip
    bool   valid;
};

static const int    GEAR_RACK_MIN_TEETH = 1;
static const int    GEAR_RACK_MAX_TEETH = 1000;

GearRackInfo GearRackComputeInfo(const GearParams &gp);

// Appends the closed outline of the rack with gp.teeth teeth. Its pitch line
// starts at p0 and runs along the unit vector xd for the length of the rack,
// the teeth point toward the unit vector yd, and the back lies gp.rackBase
// below the root line. Both ends lie in the middle of a tooth space. All
// lengths are multiplied by scale.
void GearMakeRackOutline(const GearParams &gp, Vector p0, Vector xd, Vector yd,
                         double scale, SBezierList *sbl);

// Distance from the center of a gear to the pitch line of a rack that meshes
// with it without backlash: the pitch radius plus the profile shift.
double GearRackDistance(const GearParams &gear);

// Position along the pitch line of a rack, measured from the start of the
// rack, of the foot of the perpendicular from the gear center, for which the
// rack meshes with the gear. footPos is the current position, toothAngle the
// angle of a tooth middle of the gear measured counterclockwise from the
// direction toward the rack, with the rack running counterclockwise around
// the gear. The result is the position nearest to footPos.
double GearRackFootPosition(const GearParams &gear, double footPos, double toothAngle);

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

// A rack made a solid, with holes to fasten it.
class GearRackSolidParams {
public:
    enum class Mount : uint32_t {
        NONE   = 0,
        // Blind holes from the back for M3 heat-set inserts.
        INSERT = 1,
        // Through holes for M3 screws across the back, along the width.
        CROSS  = 2
    };

    double width;         // face width, the depth of the extrusion
    double base;          // back thickness, set on the rack
    Mount  mount;
    int    holes;         // number of holes, evenly spaced along the rack
    double holeDiameter;
    double holeDepth;     // for inserts

    // Suggested values for a rack and mount: a width of four modules, but
    // not under 5 mm, the back of the rack, as many holes as suit the
    // length, and a hole size, depth, back and width that suit the mount.
    static GearRackSolidParams Default(const GearParams &rack, Mount mount = Mount::INSERT);

    // An error that keeps the solid from being made, or an empty string.
    std::string Problem(const GearParams &rack) const;
    // A hint about a solid that can be made but may not work well.
    std::string Warning() const;
};

// Where GearAddHub puts its groups: true after the active group, false
// right after the sketch of the gear.
static const bool GEAR_HUB_AFTER_ACTIVE_GROUP = true;
// Where GearAddMate puts the sketch of the mating gear: true after the
// active group, false after all other groups.
static const bool GEAR_MATE_AFTER_ACTIVE_GROUP = true;
// Where GearAddRack puts the sketch of the rack, in the same way.
static const bool GEAR_RACK_AFTER_ACTIVE_GROUP = true;

class hRequest;
class hConstraint;

// Gears and racks made to mesh with each other share module, pressure
// angle, clearance and backlash. After a change to the gear or rack hr,
// copies these to all gears and racks linked to it, sets the distance
// constraints between them, turns each mating gear and moves each rack so
// that the teeth mesh again.
void GearSyncLinked(hRequest hr);

// Adds a gear with the given number of teeth that meshes with the gear
// driver, to the right of it, in a new sketch after the active group (see
// GEAR_MATE_AFTER_ACTIVE_GROUP), and a distance constraint between the
// centers. Returns the new request, or a null handle with the reason in
// error.
hRequest GearAddMate(hRequest driver, int teeth, std::string *error);

// The distance constraint between the centers of two gears, if any.
hConstraint GearFindCenterConstraint(hRequest a, hRequest b);

// Adds a rack with the given number of teeth and back thickness that
// meshes with the gear driver, below it, in a new sketch after the active
// group (see GEAR_RACK_AFTER_ACTIVE_GROUP). The pitch line of the rack is
// also drawn as a construction line, which has a distance constraint to the
// center of the gear. Returns the new request, or a null handle with the
// reason in error.
hRequest GearAddRack(hRequest driver, int teeth, double base, std::string *error);

// The construction line on the pitch line of a rack, and the distance
// constraint between it and the center of the gear, if any.
hRequest GearFindRackLine(hRequest rack);
hConstraint GearFindRackConstraint(hRequest gear, hRequest rack);

// Makes the gear a solid with a hub. Gives the gear a bore for the shaft,
// and adds after the active group (see GEAR_HUB_AFTER_ACTIVE_GROUP), which
// must not come before the sketch of the gear: an extrusion of that sketch, a
// sketch of the hub ring on the gear face and its extrusion, and for a clamp
// a sketch of the clamp hole across the hub and its extrusion as a
// difference. These are ordinary groups that can be edited later. Returns
// false with an error message if that is not possible.
bool GearAddHub(hRequest gear, const GearHubParams &hp, std::string *error);

// Makes the rack a solid. Sets its back thickness, and adds after the active
// group (as GEAR_HUB_AFTER_ACTIVE_GROUP says for hubs), which must not come
// before the sketch of the rack: an extrusion of that sketch, and for a
// mount a sketch of the holes and its extrusion as a difference. Holes for
// inserts are drawn on the back, which the sketch of the rack marks with a
// construction line from the start of the pitch line. Returns false with an
// error message if that is not possible.
bool GearAddRackSolid(hRequest rack, const GearRackSolidParams &sp, std::string *error);

// The construction line from the start of the pitch line of a rack to its
// back, if any.
hRequest GearFindRackBackLine(hRequest rack);

}

#endif
