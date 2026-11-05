#include "solvespace.h"

#include "harness.h"

static GearParams Params(double m, int z, double x = 0, double backlash = 0) {
    GearParams gp = GearParams::Default();
    gp.module = m;
    gp.teeth = z;
    gp.profileShift = x;
    gp.backlash = backlash;
    return gp;
}

static void Outline(const GearParams &gp, SBezierList *sbl) {
    GearMakeOutline(gp, Vector::From(0, 0, 0), Vector::From(1, 0, 0),
                    Vector::From(0, 1, 0), 1.0, sbl);
}

// Each curve starts where the one before it ends, around the whole gear.
static bool IsClosedChain(const SBezierList &sbl, size_t from, size_t to) {
    for(size_t i = from; i < to; i++) {
        size_t next = (i + 1 < to) ? i + 1 : from;
        if(!sbl.l[i].Finish().Equals(sbl.l[next].Start(), 1e-9)) return false;
    }
    return true;
}

// Sampled points of the outline, all within the root and tip circles.
static void RadiusRange(const SBezierList &sbl, double *rmin, double *rmax) {
    *rmin = VERY_POSITIVE;
    *rmax = 0;
    for(const SBezier &sb : sbl.l) {
        for(int i = 0; i <= 20; i++) {
            double R = sb.PointAt(i / 20.0).Magnitude();
            *rmin = std::min(*rmin, R);
            *rmax = std::max(*rmax, R);
        }
    }
}

// Half the angle of the tooth along +x where it crosses the circle of
// radius R, found on the first flank of the outline.
static double HalfToothAngle(const SBezierList &sbl, double R) {
    for(const SBezier &sb : sbl.l) {
        double r0 = sb.Start().Magnitude(), r1 = sb.Finish().Magnitude();
        if(sb.deg != 3 || (r0 - R) * (r1 - R) > 0) continue;
        double lo = 0, hi = 1;
        for(int i = 0; i < 60; i++) {
            double mid = (lo + hi) / 2;
            if((sb.PointAt(mid).Magnitude() - R) * (r0 - R) > 0) lo = mid; else hi = mid;
        }
        Vector p = sb.PointAt(lo);
        return fabs(atan2(p.y, p.x));
    }
    return -1;
}

TEST_CASE(standard_dimensions) {
    GearParams gp = Params(1.5, 20);
    GearInfo info = GearComputeInfo(gp);
    CHECK_TRUE(info.valid);
    CHECK_FALSE(info.undercut);
    CHECK_FALSE(info.pointed);
    CHECK_EQ_EPS(info.pitchRadius, 15.0);
    CHECK_EQ_EPS(info.tipRadius, 16.5);
    CHECK_EQ_EPS(info.rootRadius, 13.125);
    CHECK_EQ_EPS(info.baseRadius, 15.0 * cos(20 * PI / 180));

    SBezierList sbl = {};
    Outline(gp, &sbl);
    CHECK_TRUE(sbl.l.n > 20 * 6);
    CHECK_TRUE(IsClosedChain(sbl, 0, sbl.l.n));
    double rmin, rmax;
    RadiusRange(sbl, &rmin, &rmax);
    CHECK_EQ_EPS(rmin, 13.125);
    CHECK_EQ_EPS(rmax, 16.5);

    // Tooth thickness at the pitch circle is half the pitch.
    double s = 2 * 15.0 * HalfToothAngle(sbl, 15.0);
    CHECK_TRUE(fabs(s - PI * 1.5 / 2) < 2e-3);
    sbl.Clear();
}

TEST_CASE(involute_flank) {
    GearParams gp = Params(2, 25);
    GearInfo info = GearComputeInfo(gp);
    SBezierList sbl = {};
    Outline(gp, &sbl);
    double alpha = 20 * PI / 180, r = 25.0, rb = r * cos(alpha);
    double psi = (PI * 2 / 4) / r;
    auto inv = [](double a) { return tan(a) - a; };
    double worst = 0;
    for(int i = 0; i <= 50; i++) {
        double R = info.involuteStartRadius + 1e-6 +
                   (info.tipRadius - info.involuteStartRadius - 2e-6) * i / 50;
        double exact = psi + inv(alpha) - inv(acos(rb / R));
        worst = std::max(worst, fabs(HalfToothAngle(sbl, R) - exact) * R);
    }
    CHECK_TRUE(worst < 2e-3);
    sbl.Clear();
}

TEST_CASE(backlash_thins_teeth) {
    GearParams gp = Params(1.5, 20, 0, 0.2);
    SBezierList sbl = {};
    Outline(gp, &sbl);
    double s = 2 * 15.0 * HalfToothAngle(sbl, 15.0);
    CHECK_TRUE(fabs(s - (PI * 1.5 / 2 - 0.1)) < 2e-3);
    sbl.Clear();
}

TEST_CASE(undercut) {
    CHECK_TRUE(GearComputeInfo(Params(1, 8)).undercut);
    CHECK_TRUE(GearComputeInfo(Params(1, 12)).undercut);
    CHECK_FALSE(GearComputeInfo(Params(1, 18)).undercut);
    // A positive profile shift of (17 - z) / 17 avoids it.
    CHECK_FALSE(GearComputeInfo(Params(1, 8, 0.6)).undercut);

    GearParams gp = Params(1, 8);
    GearInfo info = GearComputeInfo(gp);
    CHECK_TRUE(info.valid);
    CHECK_TRUE(info.involuteStartRadius > info.baseRadius + 0.02);
    SBezierList sbl = {};
    Outline(gp, &sbl);
    CHECK_TRUE(IsClosedChain(sbl, 0, sbl.l.n));
    double rmin, rmax;
    RadiusRange(sbl, &rmin, &rmax);
    CHECK_EQ_EPS(rmin, 2.75);
    CHECK_EQ_EPS(rmax, 5.0);
    sbl.Clear();
}

TEST_CASE(pointed_teeth) {
    // A large positive shift on few teeth makes the tips pointed.
    GearInfo info = GearComputeInfo(Params(1, 8, 1.2));
    CHECK_TRUE(info.valid);
    CHECK_TRUE(info.pointed);
    CHECK_FALSE(GearComputeInfo(Params(1, 20)).pointed);
    CHECK_TRUE(GearComputeInfo(Params(1, 20)).tipThickness > 0.6);
}

TEST_CASE(bore) {
    GearParams gp = Params(1.5, 20);
    gp.bore = 5;
    SBezierList sbl = {};
    Outline(gp, &sbl);
    // Four quarter arcs at the end, closed among themselves.
    int n = sbl.l.n;
    CHECK_TRUE(IsClosedChain(sbl, 0, n - 4));
    CHECK_TRUE(IsClosedChain(sbl, n - 4, n));
    CHECK_EQ_EPS(sbl.l[n - 1].PointAt(0.5).Magnitude(), 2.5);
    sbl.Clear();
}

TEST_CASE(invalid_parameters) {
    CHECK_FALSE(GearComputeInfo(Params(1, 3)).valid);
    CHECK_FALSE(GearComputeInfo(Params(0, 20)).valid);
    SBezierList sbl = {};
    Outline(Params(1, 3), &sbl);
    CHECK_TRUE(sbl.l.IsEmpty());
}

TEST_CASE(center_distance) {
    GearParams a = Params(1.5, 20), b = Params(1.5, 40);
    CHECK_EQ_EPS(GearCenterDistance(a, b), 45.0);

    // With shifted profiles the working pressure angle aw satisfies
    // inv(aw) = inv(a) + 2 tan(a) (x1 + x2) / (z1 + z2).
    a.profileShift = 0.3;
    b.profileShift = 0.2;
    double d = GearCenterDistance(a, b);
    double alpha = 20 * PI / 180;
    double aw = acos(45.0 * cos(alpha) / d);
    auto inv = [](double t) { return tan(t) - t; };
    CHECK_EQ_EPS(inv(aw), inv(alpha) + 2 * tan(alpha) * 0.5 / 60);
    CHECK_TRUE(d > 45.0);
}

TEST_CASE(mate_phase) {
    // A tooth of the driver on the line of centers faces a tooth space of
    // the mate.
    CHECK_EQ_EPS(GearMatePhase(20, 40, 0), PI / 40);
    // Turning the driver by one tooth turns the mate by one tooth back.
    CHECK_EQ_EPS(GearMatePhase(20, 40, 2 * PI / 20), PI / 40 - 2 * PI / 40);
}
// The sketch in normal.slvs: a gear with default parameters and a 5 mm
// bore, centered on the origin of the XY plane, with its tooth marker
// constrained horizontal, so that it has no degrees of freedom left.
static const hRequest GEAR_REQUEST = { 4 };

static Entity *GearEntity() {
    return SK.GetEntity(GEAR_REQUEST.entity(0));
}

static void AddExtrusion(double depth) {
    Group g = {};
    g.visible = true;
    g.color = RGBi(100, 100, 100);
    g.scale = 1;
    g.type = Group::Type::EXTRUDE;
    g.opA = SS.GW.activeGroup;
    g.predef.entityB = SS.GW.ActiveWorkplane();
    g.subtype = Group::Subtype::ONE_SIDED;
    g.name = "extrude";
    g.order = SK.GetGroup(SS.GW.activeGroup)->order + 1;
    SK.group.AddAndAssignId(&g);
    SS.GW.activeGroup = g.h;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    // A one-sided extrusion goes twice as far as its vector.
    SK.GetGroup(g.h)->ExtrusionForceVectorTo(Vector::From(0, 0, depth / 2));
    SS.MarkGroupDirty(g.h);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
}

static void Polyline(Entity *e, std::vector<Vector> *pts) {
    SBezierList sbl = {};
    e->GenerateBezierCurves(&sbl);
    for(const SBezier &sb : sbl.l) {
        List<Vector> lv = {};
        sb.MakePwlInto(&lv, 0.0005);
        for(int i = 0; i + 1 < lv.n; i++) pts->push_back(lv[i]);
        lv.Clear();
    }
    sbl.Clear();
}

// Even-odd test in the XY plane against a closed polyline.
static bool Inside(const std::vector<Vector> &poly, Vector p) {
    bool in = false;
    for(size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const Vector &a = poly[i], &b = poly[j];
        if((a.y > p.y) != (b.y > p.y) &&
           p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) {
            in = !in;
        }
    }
    return in;
}

// Two gears mesh if neither cuts into the other, and the flanks come close
// to each other, within the backlash.
static bool Mesh(Entity *a, Entity *b, double backlash) {
    std::vector<Vector> pa, pb;
    Polyline(a, &pa);
    Polyline(b, &pb);
    for(const Vector &p : pb) if(Inside(pa, p)) return false;
    for(const Vector &p : pa) if(Inside(pb, p)) return false;
    double closest = VERY_POSITIVE;
    for(const Vector &p : pa) {
        for(const Vector &q : pb) closest = std::min(closest, p.Minus(q).Magnitude());
    }
    return closest < backlash;
}

TEST_CASE(sketch_roundtrip) {
    CHECK_LOAD("normal.slvs");
    Entity *e = GearEntity();
    CHECK_TRUE(e->type == Entity::Type::GEAR);
    CHECK_EQ_EPS(e->gear.module, 1.5);
    CHECK_TRUE(e->gear.teeth == 20);
    CHECK_EQ_EPS(e->gear.bore, 5.0);
    CHECK_TRUE(SK.GetGroup(SS.GW.activeGroup)->solved.dof == 0);
    CHECK_SAVE("normal.slvs");
}

// The tooth marker stays on the pitch circle when the number of teeth
// changes, and only turns the gear.
TEST_CASE(marker_on_pitch_circle) {
    CHECK_LOAD("normal.slvs");
    SK.GetRequest(GEAR_REQUEST)->gear.teeth = 30;
    SS.MarkGroupDirty(SS.GW.activeGroup);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Vector p = SK.GetEntity(GEAR_REQUEST.entity(2))->PointGetNum();
    CHECK_EQ_EPS(p.x, 22.5);
    CHECK_EQ_EPS(p.y, 0.0);
}

// The extruded gear is a closed solid; its volume is the area of the
// outline, less the bore, times the depth.
TEST_CASE(extrude) {
    CHECK_LOAD("normal.slvs");
    std::vector<Vector> outline;
    {
        SBezierList sbl = {};
        GearEntity()->GenerateBezierCurves(&sbl);
        int n = sbl.l.n - 4;
        for(int i = 0; i < n; i++) {
            List<Vector> lv = {};
            sbl.l[i].MakePwlInto(&lv, 0.0005);
            for(int k = 0; k + 1 < lv.n; k++) outline.push_back(lv[k]);
            lv.Clear();
        }
        sbl.Clear();
    }
    double area = 0;
    for(size_t i = 0, j = outline.size() - 1; i < outline.size(); j = i++) {
        area += outline[j].x * outline[i].y - outline[i].x * outline[j].y;
    }
    area = fabs(area) / 2 - PI * 2.5 * 2.5;

    AddExtrusion(8);
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    SEdgeList el = {};
    bool inters, leaks;
    SKdNode::From(m)->MakeCertainEdgesInto(&el, EdgeKind::NAKED_OR_SELF_INTER,
                                           /*coplanarIsInter=*/true, &inters, &leaks);
    CHECK_TRUE(el.l.IsEmpty());
    CHECK_FALSE(inters);
    CHECK_FALSE(leaks);
    el.Clear();
    CHECK_TRUE(fabs(m->CalculateVolume() / (area * 8) - 1) < 2e-3);

    // The copy of the gear on the far end of the extrusion has the same
    // outline, moved by the depth.
    int copies = 0;
    for(Entity &e : SK.entity) {
        if(e.group != g->h || e.type != Entity::Type::GEAR) continue;
        SBezierList a = {}, b = {};
        e.GenerateBezierCurves(&a);
        GearEntity()->GenerateBezierCurves(&b);
        CHECK_TRUE(a.l.n == b.l.n);
        Vector d = a.l[3].ctrl[2].Minus(b.l[3].ctrl[2]);
        CHECK_TRUE(d.Equals(Vector::From(0, 0, 8)) || d.Equals(Vector::From(0, 0, 0)));
        a.Clear();
        b.Clear();
        copies++;
    }
    CHECK_TRUE(copies == 2);
}

TEST_CASE(mating_gear) {
    CHECK_LOAD("normal.slvs");
    hRequest hm = GearAddMate(GEAR_REQUEST, 40);
    CHECK_TRUE(hm.v != 0);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Request *mate = SK.GetRequest(hm);
    CHECK_TRUE(mate->gearDriver == GEAR_REQUEST);
    CHECK_TRUE(mate->gear.teeth == 40);
    CHECK_EQ_EPS(mate->gear.module, 1.5);

    hConstraint hc = GearFindCenterConstraint(GEAR_REQUEST, hm);
    CHECK_TRUE(hc != Constraint::NO_CONSTRAINT);
    CHECK_EQ_EPS(SK.GetConstraint(hc)->valA, 45.0);
    Vector c2 = SK.GetEntity(hm.entity(1))->PointGetNum();
    CHECK_EQ_EPS(c2.Magnitude(), 45.0);
    CHECK_TRUE(Mesh(GearEntity(), SK.GetEntity(hm.entity(0)), 0.15));

    // A change of module on the mate carries over to the driver, the
    // center distance follows, and the gears still mesh.
    mate->gear.module = 2;
    mate->gear.profileShift = 0.4;
    GearSyncLinked(hm);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(SK.GetRequest(GEAR_REQUEST)->gear.module, 2.0);
    double a = GearCenterDistance(SK.GetRequest(GEAR_REQUEST)->gear, SK.GetRequest(hm)->gear);
    CHECK_TRUE(a > 60.0);
    CHECK_EQ_EPS(SK.GetConstraint(hc)->valA, a);
    CHECK_EQ_EPS(SK.GetEntity(hm.entity(1))->PointGetNum().Magnitude(), a);
    CHECK_TRUE(Mesh(GearEntity(), SK.GetEntity(hm.entity(0)), 0.15));
}

// A gear turned by an odd angle still gets a meshing mate.
TEST_CASE(mating_gear_turned) {
    CHECK_LOAD("normal.slvs");
    // Drop the horizontal constraint and turn the gear.
    for(Constraint &c : SK.constraint) {
        if(c.type == Constraint::Type::HORIZONTAL) c.tag = 1;
    }
    SK.constraint.RemoveTagged();
    SK.GetEntity(GEAR_REQUEST.entity(2))->PointForceTo(
        Vector::From(15 * cos(0.37), 15 * sin(0.37), 0));
    SS.MarkGroupDirty(SS.GW.activeGroup);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    for(int z : { 9, 13, 31 }) {
        hRequest hm = GearAddMate(GEAR_REQUEST, z);
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        CHECK_TRUE(Mesh(GearEntity(), SK.GetEntity(hm.entity(0)), 0.15));
        SK.request.FindById(hm)->tag = 1;
        SK.request.RemoveTagged();
        for(Constraint &c : SK.constraint) {
            c.tag = (c.ptB == hm.entity(1)) ? 1 : 0;
        }
        SK.constraint.RemoveTagged();
        SS.MarkGroupDirty(SS.GW.activeGroup);
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    }
}

static bool IsWatertight(SMesh *m) {
    SEdgeList el = {};
    bool inters, leaks;
    SKdNode::From(m)->MakeCertainEdgesInto(&el, EdgeKind::NAKED_OR_SELF_INTER,
                                           /*coplanarIsInter=*/true, &inters, &leaks);
    bool ok = el.l.IsEmpty() && !inters && !leaks;
    el.Clear();
    return ok;
}

static Group *GroupNamed(const char *name) {
    for(Group &g : SK.group) if(g.name == name) return &g;
    return NULL;
}

// Area of the gear outline without bore, from its first loop.
static double GearArea() {
    SBezierList sbl = {};
    GearEntity()->GenerateBezierCurves(&sbl);
    std::vector<Vector> pts;
    for(int i = 0; i < sbl.l.n - 4; i++) {
        List<Vector> lv = {};
        sbl.l[i].MakePwlInto(&lv, 0.0005);
        for(int k = 0; k + 1 < lv.n; k++) pts.push_back(lv[k]);
        lv.Clear();
    }
    sbl.Clear();
    double area = 0;
    for(size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++) {
        area += pts[j].x * pts[i].y - pts[i].x * pts[j].y;
    }
    return fabs(area) / 2;
}

// The default hub for the gear of normal.slvs: 6 mm face width, 5 mm shaft,
// a hub of 18.5 mm diameter and 8 mm length, with a 4 mm hole for a heat-set
// insert.
TEST_CASE(hub_defaults) {
    GearParams gp = GearParams::Default();
    gp.bore = 5;
    GearHubParams hp = GearHubParams::Default(gp);
    CHECK_EQ_EPS(hp.width, 6.0);
    CHECK_EQ_EPS(hp.shaft, 5.0);
    CHECK_EQ_EPS(hp.hubDiameter, 18.5);
    CHECK_EQ_EPS(hp.hubLength, 8.0);
    CHECK_EQ_EPS(hp.clampHole, 4.0);
    CHECK_TRUE(hp.Problem(gp).empty());
    CHECK_TRUE(hp.Warning().empty());

    hp.shaft = 25;
    CHECK_FALSE(hp.Problem(gp).empty());
    hp.shaft = 5;
    hp.hubDiameter = 12;
    CHECK_FALSE(hp.Warning().empty());
}

TEST_CASE(hub_with_insert) {
    CHECK_LOAD("normal.slvs");
    double gearArea = GearArea() - PI * 2.5 * 2.5;
    GearHubParams hp = GearHubParams::Default(GearEntity()->gear);
    std::string error;
    CHECK_TRUE(GearAddHub(GEAR_REQUEST, hp, &error));
    CHECK_TRUE(error.empty());

    for(const char *name : { "gear", "hub-sketch", "hub", "clamp-sketch", "clamp-hole" }) {
        Group *g = GroupNamed(name);
        CHECK_TRUE(g != NULL);
        CHECK_TRUE(g->solved.how == SolveResult::OKAY);
        CHECK_TRUE(g->solved.dof == 0);
    }
    Group *last = GroupNamed("clamp-hole");
    CHECK_TRUE(SS.GW.activeGroup == last->h);
    last->GenerateDisplayItems();
    SMesh *m = &last->displayMesh;
    CHECK_TRUE(IsWatertight(m));
    double ring = PI / 4 * (18.5 * 18.5 - 5 * 5) * 8;
    double hole = PI * 2 * 2 * (18.5 - 5) / 2;
    CHECK_TRUE(fabs(m->CalculateVolume() / (gearArea * 6 + ring - hole) - 1) < 0.01);

    // The clamp hole follows a longer hub.
    Group *hub = GroupNamed("hub");
    for(Constraint &c : SK.constraint) {
        if(c.group == hub->h && c.type == Constraint::Type::PT_PT_DISTANCE) c.valA = 10;
    }
    SS.MarkGroupDirty(hub->h);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *clamp = GroupNamed("clamp-sketch");
    for(Request &r : SK.request) {
        if(r.group != clamp->h || r.type != Request::Type::CIRCLE) continue;
        CHECK_EQ_EPS(SK.GetEntity(r.h.entity(1))->PointGetNum().z, 6 + 5);
    }
    CHECK_TRUE(GroupNamed("clamp-sketch")->solved.dof == 0);
}

TEST_CASE(hub_without_clamp) {
    CHECK_LOAD("normal.slvs");
    GearHubParams hp = GearHubParams::Default(GearEntity()->gear);
    hp.clamp = GearHubParams::Clamp::NONE;
    hp.SuggestHub();
    std::string error;
    CHECK_TRUE(GearAddHub(GEAR_REQUEST, hp, &error));
    CHECK_TRUE(GroupNamed("clamp-sketch") == NULL);
    Group *hub = GroupNamed("hub");
    hub->GenerateDisplayItems();
    CHECK_TRUE(IsWatertight(&hub->displayMesh));

    // A second hub on the same gear is refused.
    CHECK_FALSE(GearAddHub(GEAR_REQUEST, hp, &error));
    CHECK_FALSE(error.empty());
}

// A mating gear gets a sketch of its own, so that each gear can be made a
// solid with its own hub, before or after the other one.
static void CheckHubsOnBoth(Test::Helper *helper, bool mateFirst) {
    GearHubParams hp = GearHubParams::Default(GearEntity()->gear);
    std::string error;
    hRequest hm;
    if(mateFirst) {
        hm = GearAddMate(GEAR_REQUEST, 30);
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        CHECK_TRUE(GearAddHub(GEAR_REQUEST, hp, &error));
    } else {
        CHECK_TRUE(GearAddHub(GEAR_REQUEST, hp, &error));
        hm = GearAddMate(GEAR_REQUEST, 30);
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    }
    CHECK_TRUE(hm.v != 0);
    Request *mate = SK.GetRequest(hm);
    CHECK_TRUE(mate->group != SK.GetRequest(GEAR_REQUEST)->group);
    CHECK_TRUE(SK.GetGroup(mate->group)->solved.how == SolveResult::OKAY);
    CHECK_TRUE(Mesh(GearEntity(), SK.GetEntity(hm.entity(0)), 0.15));

    GearHubParams mp = GearHubParams::Default(mate->gear);
    mp.shaft = 8;
    mp.clamp = GearHubParams::Clamp::TAPPED;
    mp.SuggestHub();
    CHECK_TRUE(GearAddHub(hm, mp, &error));
    CHECK_TRUE(error.empty());

    int sketches = 0;
    for(Group &g : SK.group) {
        if(g.type == Group::Type::DRAWING_WORKPLANE) sketches++;
        CHECK_TRUE(g.solved.how == SolveResult::OKAY);
    }
    // The gear, its hub and clamp, and the same for the mate.
    CHECK_TRUE(sketches == 6);

    Group *last = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(last->name == "clamp-hole");
    last->GenerateDisplayItems();
    CHECK_TRUE(IsWatertight(&last->displayMesh));
}

TEST_CASE(hub_on_both_mate_first) {
    CHECK_LOAD("normal.slvs");
    CheckHubsOnBoth(helper, /*mateFirst=*/true);
}

TEST_CASE(hub_on_both_mate_after) {
    CHECK_LOAD("normal.slvs");
    CheckHubsOnBoth(helper, /*mateFirst=*/false);
}

// A turned gear gets a closed solid, too. Its bore is split where the
// circles of the hub sketch are, whatever way its teeth point.
TEST_CASE(hub_on_turned_gear) {
    for(double angle : { 0.37 }) {
        CHECK_LOAD("normal.slvs");
        for(Constraint &c : SK.constraint) {
            c.tag = (c.type == Constraint::Type::HORIZONTAL) ? 1 : 0;
        }
        SK.constraint.RemoveTagged();
        SK.GetEntity(GEAR_REQUEST.entity(2))->PointForceTo(
            Vector::From(15 * cos(angle), 15 * sin(angle), 0));
        SS.MarkGroupDirty(SS.GW.activeGroup);
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        std::string error;
        CHECK_TRUE(GearAddHub(GEAR_REQUEST, GearHubParams::Default(GearEntity()->gear), &error));
        Group *g = GroupNamed("clamp-hole");
        g->GenerateDisplayItems();
        CHECK_TRUE(IsWatertight(&g->displayMesh));
    }
}

// The groups of a hub follow the active group. With a mating gear active,
// the hub of the first gear comes after it, so that both gears stay in
// view. A group before the sketch of the gear cannot take a hub.
TEST_CASE(hub_after_active_group) {
    if(!GEAR_HUB_AFTER_ACTIVE_GROUP) return;
    CHECK_LOAD("normal.slvs");
    std::string error;
    hGroup sketch = SK.GetRequest(GEAR_REQUEST)->group;
    SS.GW.activeGroup = Group::HGROUP_REFERENCES;
    CHECK_FALSE(GearAddHub(GEAR_REQUEST, GearHubParams::Default(GearEntity()->gear), &error));
    CHECK_FALSE(error.empty());
    CHECK_TRUE(SK.GetRequest(GEAR_REQUEST)->gear.bore == 5);

    SS.GW.activeGroup = sketch;
    hRequest hm = GearAddMate(GEAR_REQUEST, 30);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    hGroup mateSketch = SK.GetRequest(hm)->group;
    CHECK_TRUE(SS.GW.activeGroup == mateSketch);

    error.clear();
    CHECK_TRUE(GearAddHub(GEAR_REQUEST, GearHubParams::Default(GearEntity()->gear), &error));
    Group *last = SK.GetGroup(SK.groupOrder[SK.groupOrder.n - 1]);
    CHECK_TRUE(last->h == SS.GW.activeGroup);
    CHECK_TRUE(last->name == "clamp-hole");
    CHECK_TRUE(SK.GetGroup(mateSketch)->IsVisible());
    CHECK_TRUE(SK.GetGroup(mateSketch)->order < GroupNamed("gear")->order);
}

// The sketch of a mating gear follows the active group, also when that is
// not the last one. A group before the sketch of the gear cannot take one.
TEST_CASE(mate_after_active_group) {
    if(!GEAR_MATE_AFTER_ACTIVE_GROUP) return;
    CHECK_LOAD("normal.slvs");
    hGroup sketch = SK.GetRequest(GEAR_REQUEST)->group;
    SS.GW.activeGroup = Group::HGROUP_REFERENCES;
    CHECK_TRUE(GearAddMate(GEAR_REQUEST, 30).v == 0);

    std::string error;
    SS.GW.activeGroup = sketch;
    CHECK_TRUE(GearAddHub(GEAR_REQUEST, GearHubParams::Default(GearEntity()->gear), &error));
    // Back to the gear extrusion, and a mate after it, before the hub.
    SS.GW.activeGroup = GroupNamed("gear")->h;
    hRequest hm = GearAddMate(GEAR_REQUEST, 30);
    CHECK_TRUE(hm.v != 0);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *mate = SK.GetGroup(SK.GetRequest(hm)->group);
    CHECK_TRUE(mate->order == GroupNamed("gear")->order + 1);
    CHECK_TRUE(mate->order < GroupNamed("hub-sketch")->order);
    CHECK_TRUE(SS.GW.activeGroup == mate->h);
    CHECK_TRUE(mate->solved.how == SolveResult::OKAY);
    CHECK_TRUE(Mesh(GearEntity(), SK.GetEntity(hm.entity(0)), 0.15));
}

//-----------------------------------------------------------------------------
// Racks.
//-----------------------------------------------------------------------------

static GearParams RackParams(double m, int n, double base, double backlash = 0) {
    GearParams gp = Params(m, n, 0, backlash);
    gp.rackBase = base;
    return gp;
}

// Positions along the pitch line where the outline crosses it, in order.
static std::vector<double> PitchLineCrossings(const SBezierList &sbl) {
    std::vector<double> xs;
    for(const SBezier &sb : sbl.l) {
        double y0 = sb.Start().y, y1 = sb.Finish().y;
        if(y0 * y1 > 0 || y0 == y1) continue;
        double lo = 0, hi = 1;
        for(int i = 0; i < 60; i++) {
            double mid = (lo + hi) / 2;
            if(sb.PointAt(mid).y * y0 > 0) lo = mid; else hi = mid;
        }
        xs.push_back(sb.PointAt(lo).x);
    }
    std::sort(xs.begin(), xs.end());
    return xs;
}

TEST_CASE(rack_dimensions) {
    GearParams gp = RackParams(1.5, 5, 3, 0.2);
    GearRackInfo info = GearRackComputeInfo(gp);
    CHECK_TRUE(info.valid);
    double p = PI * 1.5;
    CHECK_EQ_EPS(info.pitch, p);
    CHECK_EQ_EPS(info.length, 5 * p);
    CHECK_EQ_EPS(info.addendum, 1.5);
    CHECK_EQ_EPS(info.dedendum, 1.875);
    CHECK_EQ_EPS(info.height, 1.5 + 1.875 + 3);
    CHECK_EQ_EPS(info.filletRadius, 0.25 * 1.5 / (1 - sin(20 * PI / 180)));

    SBezierList sbl = {};
    GearMakeRackOutline(gp, Vector::From(0, 0, 0), Vector::From(1, 0, 0),
                        Vector::From(0, 1, 0), 1.0, &sbl);
    CHECK_TRUE(IsClosedChain(sbl, 0, sbl.l.n));
    double xmin = VERY_POSITIVE, xmax = VERY_NEGATIVE, ymin = VERY_POSITIVE, ymax = VERY_NEGATIVE;
    for(const SBezier &sb : sbl.l) {
        for(int i = 0; i <= 20; i++) {
            Vector q = sb.PointAt(i / 20.0);
            xmin = std::min(xmin, q.x); xmax = std::max(xmax, q.x);
            ymin = std::min(ymin, q.y); ymax = std::max(ymax, q.y);
        }
    }
    CHECK_EQ_EPS(xmin, 0.0);
    CHECK_EQ_EPS(xmax, 5 * p);
    CHECK_EQ_EPS(ymin, -1.875 - 3);
    CHECK_EQ_EPS(ymax, 1.5);

    // Two flanks per tooth cross the pitch line, the teeth are half the
    // pitch thick less half the backlash, and centered between the ends of
    // the tooth spaces.
    std::vector<double> xs = PitchLineCrossings(sbl);
    CHECK_TRUE(xs.size() == 10);
    for(int k = 0; k < 5; k++) {
        CHECK_EQ_EPS(xs[2*k + 1] - xs[2*k], p / 2 - 0.1);
        CHECK_EQ_EPS((xs[2*k + 1] + xs[2*k]) / 2, (k + 0.5) * p);
    }
    sbl.Clear();

    // Without clearance there is no fillet, and the outline is still closed.
    gp.clearance = 0;
    CHECK_TRUE(GearRackComputeInfo(gp).valid);
    GearMakeRackOutline(gp, Vector::From(0, 0, 0), Vector::From(1, 0, 0),
                        Vector::From(0, 1, 0), 1.0, &sbl);
    CHECK_TRUE(IsClosedChain(sbl, 0, sbl.l.n));
    sbl.Clear();
}

TEST_CASE(rack_invalid_parameters) {
    CHECK_FALSE(GearRackComputeInfo(RackParams(1.5, 5, 0)).valid);
    CHECK_FALSE(GearRackComputeInfo(RackParams(1.5, 0, 3)).valid);
    CHECK_FALSE(GearRackComputeInfo(RackParams(0, 5, 3)).valid);
    CHECK_TRUE(GearRackComputeInfo(RackParams(1.5, 1, 3)).valid);
}

TEST_CASE(rack_foot_position) {
    GearParams gp = Params(2, 20);
    double p = PI * 2;
    // A tooth pointing at the rack faces a tooth space.
    CHECK_EQ_EPS(GearRackFootPosition(gp, 0.1, 0), 0.0);
    CHECK_EQ_EPS(GearRackFootPosition(gp, 3 * p + 0.2, 0), 3 * p);
    // Turning the gear by one tooth moves the rack by one pitch.
    CHECK_EQ_EPS(GearRackFootPosition(gp, 0, 2 * PI / 20), 0.0);
    CHECK_EQ_EPS(GearRackFootPosition(gp, 0, 0.05), -20 * 0.05);
    CHECK_EQ_EPS(GearRackDistance(Params(2, 20, 0.3)), 20.6);
}

static double RackPitchLineDistance(hRequest hr) {
    Vector a = SK.GetEntity(hr.entity(1))->PointGetNum(),
           b = SK.GetEntity(hr.entity(2))->PointGetNum();
    Vector d = b.Minus(a).WithMagnitude(1);
    Vector c = SK.GetEntity(GEAR_REQUEST.entity(1))->PointGetNum();
    Vector f = a.Plus(d.ScaledBy(c.Minus(a).Dot(d)));
    return c.Minus(f).Magnitude();
}

TEST_CASE(rack) {
    CHECK_LOAD("normal.slvs");
    hRequest hr = GearAddRack(GEAR_REQUEST, 12, 3);
    CHECK_TRUE(hr.v != 0);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Request *rack = SK.GetRequest(hr);
    CHECK_TRUE(rack->type == Request::Type::GEAR_RACK);
    CHECK_TRUE(rack->gearDriver == GEAR_REQUEST);
    CHECK_TRUE(rack->gear.teeth == 12);
    CHECK_EQ_EPS(rack->gear.rackBase, 3.0);
    CHECK_EQ_EPS(rack->gear.module, 1.5);
    Group *g = SK.GetGroup(rack->group);
    CHECK_TRUE(g->name == "rack");
    CHECK_TRUE(g->solved.how == SolveResult::OKAY);
    CHECK_TRUE(SS.GW.activeGroup == g->h);

    hRequest hl = GearFindRackLine(hr);
    CHECK_TRUE(hl.v != 0);
    CHECK_TRUE(SK.GetRequest(hl)->construction);
    hConstraint hc = GearFindRackConstraint(GEAR_REQUEST, hr);
    CHECK_TRUE(hc != Constraint::NO_CONSTRAINT);
    CHECK_EQ_EPS(fabs(SK.GetConstraint(hc)->valA), 15.0);
    CHECK_EQ_EPS(RackPitchLineDistance(hr), 15.0);
    Vector a = SK.GetEntity(hr.entity(1))->PointGetNum(),
           b = SK.GetEntity(hr.entity(2))->PointGetNum();
    CHECK_EQ_EPS(a.Minus(b).Magnitude(), 12 * PI * 1.5);
    CHECK_TRUE(Mesh(GearEntity(), SK.GetEntity(hr.entity(0)), 0.15));

    // A change of the gear carries over to the rack, which moves to the new
    // distance and still meshes.
    Request *gear = SK.GetRequest(GEAR_REQUEST);
    gear->gear.module = 2;
    gear->gear.profileShift = 0.3;
    gear->gear.backlash = 0.1;
    GearSyncLinked(GEAR_REQUEST);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    rack = SK.GetRequest(hr);
    CHECK_EQ_EPS(rack->gear.module, 2.0);
    CHECK_EQ_EPS(rack->gear.backlash, 0.1);
    CHECK_EQ_EPS(rack->gear.profileShift, 0.0);
    CHECK_EQ_EPS(fabs(SK.GetConstraint(hc)->valA), 20.6);
    CHECK_EQ_EPS(RackPitchLineDistance(hr), 20.6);
    CHECK_TRUE(Mesh(GearEntity(), SK.GetEntity(hr.entity(0)), 0.1));

    // So does a new number of teeth of the rack.
    rack->gear.teeth = 7;
    GearSyncLinked(hr);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    a = SK.GetEntity(hr.entity(1))->PointGetNum();
    b = SK.GetEntity(hr.entity(2))->PointGetNum();
    CHECK_EQ_EPS(a.Minus(b).Magnitude(), 7 * PI * 2);
    CHECK_TRUE(Mesh(GearEntity(), SK.GetEntity(hr.entity(0)), 0.1));
    CHECK_TRUE(SK.GetGroup(SK.GetRequest(hr)->group)->solved.how == SolveResult::OKAY);
}

// A gear turned by an odd angle still gets a meshing rack, also after the
// rack was moved along and around the gear.
TEST_CASE(rack_turned) {
    CHECK_LOAD("normal.slvs");
    for(Constraint &c : SK.constraint) {
        if(c.type == Constraint::Type::HORIZONTAL) c.tag = 1;
    }
    SK.constraint.RemoveTagged();
    SK.GetEntity(GEAR_REQUEST.entity(2))->PointForceTo(
        Vector::From(15 * cos(0.37), 15 * sin(0.37), 0));
    SS.MarkGroupDirty(SS.GW.activeGroup);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    hGroup sketch = SS.GW.activeGroup;

    hRequest hr = GearAddRack(GEAR_REQUEST, 9, 2);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(Mesh(GearEntity(), SK.GetEntity(hr.entity(0)), 0.15));

    // Turn the rack around the gear and slide it along, as a drag would,
    // then bring it back into mesh.
    hRequest hl = GearFindRackLine(hr);
    Quaternion turn = Quaternion::From(Vector::From(0, 0, 1), 1.1);
    for(int i = 1; i <= 2; i++) {
        Vector q = turn.Rotate(SK.GetEntity(hr.entity(i))->PointGetNum())
                       .Plus(Vector::From(0.8, -0.3, 0));
        SK.GetEntity(hr.entity(i))->PointForceTo(q);
        SK.GetEntity(hl.entity(i))->PointForceTo(q);
    }
    GearSyncLinked(GEAR_REQUEST);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(RackPitchLineDistance(hr), 15.0);
    CHECK_TRUE(Mesh(GearEntity(), SK.GetEntity(hr.entity(0)), 0.15));

    // A group before the sketch of the gear cannot take a rack.
    if(GEAR_RACK_AFTER_ACTIVE_GROUP) {
        SS.GW.activeGroup = Group::HGROUP_REFERENCES;
        CHECK_TRUE(GearAddRack(GEAR_REQUEST, 9, 2).v == 0);
        SS.GW.activeGroup = sketch;
    }
    CHECK_TRUE(GearAddRack(GEAR_REQUEST, 9, 0).v == 0);
    CHECK_TRUE(GearAddRack(GEAR_REQUEST, 0, 2).v == 0);
}

// The extruded rack is a closed solid with the volume of its outline times
// the depth.
TEST_CASE(rack_extrude) {
    CHECK_LOAD("normal.slvs");
    hRequest hr = GearAddRack(GEAR_REQUEST, 6, 3);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    std::vector<Vector> pts;
    Polyline(SK.GetEntity(hr.entity(0)), &pts);
    double area = 0;
    for(size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++) {
        area += pts[j].x * pts[i].y - pts[i].x * pts[j].y;
    }
    area = fabs(area) / 2;
    AddExtrusion(5);
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    g->GenerateDisplayItems();
    CHECK_TRUE(IsWatertight(&g->displayMesh));
    CHECK_TRUE(fabs(g->displayMesh.CalculateVolume() / (area * 5) - 1) < 2e-3);
}

TEST_CASE(rack_roundtrip) {
    CHECK_LOAD("rack.slvs");
    bool found = false;
    for(Request &r : SK.request) {
        if(r.type != Request::Type::GEAR_RACK) continue;
        found = true;
        CHECK_TRUE(r.gearDriver == GEAR_REQUEST);
        CHECK_TRUE(r.gear.teeth == 12);
        CHECK_EQ_EPS(r.gear.rackBase, 3.0);
        CHECK_TRUE(Mesh(GearEntity(), SK.GetEntity(r.h.entity(0)), 0.15));
    }
    CHECK_TRUE(found);
    CHECK_SAVE("rack.slvs");
}

// Area of the outline of a rack.
static double RackArea(hRequest hr) {
    std::vector<Vector> pts;
    Polyline(SK.GetEntity(hr.entity(0)), &pts);
    double area = 0;
    for(size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++) {
        area += pts[j].x * pts[i].y - pts[i].x * pts[j].y;
    }
    return fabs(area) / 2;
}

TEST_CASE(rack_solid_defaults) {
    using Mount = GearRackSolidParams::Mount;
    GearParams gp = RackParams(1.5, 12, 3);
    GearRackSolidParams sp = GearRackSolidParams::Default(gp);
    CHECK_TRUE(sp.mount == Mount::INSERT);
    CHECK_EQ_EPS(sp.width, 7.0);
    CHECK_EQ_EPS(sp.base, 7.5);
    CHECK_EQ_EPS(sp.holeDiameter, 4.0);
    CHECK_EQ_EPS(sp.holeDepth, GEAR_INSERT_LENGTH + 0.5);
    CHECK_TRUE(sp.holes == 2);
    CHECK_TRUE(sp.Problem(gp).empty());

    sp = GearRackSolidParams::Default(gp, Mount::CROSS);
    CHECK_EQ_EPS(sp.width, 6.0);
    CHECK_EQ_EPS(sp.holeDiameter, 3.4);
    CHECK_EQ_EPS(sp.base, 6.0);
    CHECK_TRUE(sp.Problem(gp).empty());

    sp = GearRackSolidParams::Default(gp, Mount::NONE);
    CHECK_EQ_EPS(sp.base, 3.0);
    CHECK_TRUE(sp.Problem(gp).empty());

    // Too thin a back, or too many holes.
    sp = GearRackSolidParams::Default(gp);
    sp.base = 6;
    CHECK_FALSE(sp.Problem(gp).empty());
    sp.base = 7.5;
    sp.holes = 12;
    CHECK_FALSE(sp.Problem(gp).empty());
    CHECK_TRUE(GearRackSolidParams::Default(RackParams(1.5, 4, 3)).holes == 1);
}

static hRequest SolidRack(GearRackSolidParams::Mount mount, GearRackSolidParams *sp,
                          std::string *error) {
    hRequest hr = GearAddRack(GEAR_REQUEST, 12, 3);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    *sp = GearRackSolidParams::Default(SK.GetRequest(hr)->gear, mount);
    if(!GearAddRackSolid(hr, *sp, error)) return {};
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return hr;
}

TEST_CASE(rack_solid_inserts) {
    CHECK_LOAD("normal.slvs");
    GearRackSolidParams sp;
    std::string error;
    hRequest hr = SolidRack(GearRackSolidParams::Mount::INSERT, &sp, &error);
    CHECK_TRUE(hr.v != 0);
    CHECK_EQ_EPS(SK.GetRequest(hr)->gear.rackBase, 7.5);
    CHECK_TRUE(GearFindRackBackLine(hr).v != 0);
    for(const char *name : { "rack", "rack-insert-sketch" }) {
        Group *g = GroupNamed(name);
        CHECK_TRUE(g != NULL);
        CHECK_TRUE(g->solved.how == SolveResult::OKAY);
    }
    CHECK_TRUE(GroupNamed("rack-insert-sketch")->solved.dof == 0);
    Group *last = GroupNamed("rack-insert-holes");
    CHECK_TRUE(last != NULL);
    CHECK_TRUE(SS.GW.activeGroup == last->h);
    last->GenerateDisplayItems();
    SMesh *m = &last->displayMesh;
    CHECK_TRUE(IsWatertight(m));
    double r = sp.holeDiameter / 2;
    double expected = RackArea(hr) * sp.width - sp.holes * PI * r * r * sp.holeDepth;
    CHECK_TRUE(fabs(m->CalculateVolume() / expected - 1) < 3e-3);

    // The holes go in from the back, which lies 1.875 + 7.5 mm below the
    // pitch line, 15 mm below the gear center, and end at their depth
    // around the middle of the width.
    double floorY = -15 - 1.875 - 7.5 + sp.holeDepth;
    int onFloor = 0;
    for(const STriangle &t : m->l) {
        for(const Vector &p : { t.a, t.b, t.c }) {
            if(fabs(p.y - floorY) < 1e-3 && fabs(p.z - sp.width / 2) <= r + 1e-3) onFloor++;
        }
    }
    CHECK_TRUE(onFloor > 0);
    // The depth dimension is the depth of the holes.
    for(const Constraint &c : SK.constraint) {
        if(c.group == last->h && c.type == Constraint::Type::PT_PT_DISTANCE) {
            CHECK_EQ_EPS(c.valA, sp.holeDepth);
        }
    }

    // A new back thickness moves the line to the back along.
    SK.GetRequest(hr)->gear.rackBase = 9;
    GearSyncLinked(hr);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Vector b = SK.GetEntity(GearFindRackBackLine(hr).entity(2))->PointGetNum();
    CHECK_EQ_EPS(b.y, -15 - 1.875 - 9);
}

TEST_CASE(rack_solid_cross_holes) {
    CHECK_LOAD("normal.slvs");
    GearRackSolidParams sp;
    std::string error;
    hRequest hr = SolidRack(GearRackSolidParams::Mount::CROSS, &sp, &error);
    CHECK_TRUE(hr.v != 0);
    CHECK_EQ_EPS(SK.GetRequest(hr)->gear.rackBase, 6.0);
    CHECK_TRUE(GearFindRackBackLine(hr).v == 0);
    CHECK_TRUE(GroupNamed("rack-holes-sketch")->solved.dof == 0);
    Group *last = GroupNamed("rack-holes");
    last->GenerateDisplayItems();
    SMesh *m = &last->displayMesh;
    CHECK_TRUE(IsWatertight(m));
    double r = sp.holeDiameter / 2;
    double expected = (RackArea(hr) - sp.holes * PI * r * r) * sp.width;
    CHECK_TRUE(fabs(m->CalculateVolume() / expected - 1) < 3e-3);
    CHECK_TRUE(last->solved.how == SolveResult::OKAY);
    CHECK_TRUE(last->solved.dof == 0);

    // A wider rack still has the holes through.
    for(Constraint &c : SK.constraint) {
        if(c.group == GroupNamed("rack-body")->h && c.type == Constraint::Type::PT_PT_DISTANCE) {
            c.valA = 11;
        }
    }
    SS.MarkGroupDirty(GroupNamed("rack-body")->h);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    last = GroupNamed("rack-holes");
    last->GenerateDisplayItems();
    m = &last->displayMesh;
    CHECK_TRUE(IsWatertight(m));
    expected = (RackArea(hr) - sp.holes * PI * r * r) * 11;
    CHECK_TRUE(fabs(m->CalculateVolume() / expected - 1) < 3e-3);
}

TEST_CASE(rack_solid_plain) {
    CHECK_LOAD("normal.slvs");
    GearRackSolidParams sp;
    std::string error;
    hRequest hr = SolidRack(GearRackSolidParams::Mount::NONE, &sp, &error);
    CHECK_TRUE(hr.v != 0);
    Group *body = GroupNamed("rack-body");
    CHECK_TRUE(SS.GW.activeGroup == body->h);
    CHECK_TRUE(GroupNamed("rack-holes-sketch") == NULL);
    body->GenerateDisplayItems();
    CHECK_TRUE(IsWatertight(&body->displayMesh));
    CHECK_TRUE(fabs(body->displayMesh.CalculateVolume() / (RackArea(hr) * sp.width) - 1) < 2e-3);

    // A second solid of the same rack is refused.
    CHECK_FALSE(GearAddRackSolid(hr, sp, &error));
    CHECK_FALSE(error.empty());
}
