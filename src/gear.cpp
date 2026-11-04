//-----------------------------------------------------------------------------
// Involute spur gear outlines.
//
// The tooth flank is what a basic rack (DIN 867 / ISO 53 type) with a
// rounded tip cuts into the blank while the blank rolls on the pitch line.
// The working part of the flank is the involute of the base circle. The root
// fillet is the envelope of the rounded rack tip, which also produces the
// undercut of gears with few teeth. Both parts are approximated by cubic
// Beziers within a small tolerance, tip and root by exact circular arcs.
//-----------------------------------------------------------------------------
#include <array>
#include <vector>
#include "solvespace.h"

namespace SolveSpace {

GearParams GearParams::Default() {
    GearParams gp = {};
    // A gear of 20 teeth with module 1.5 is 33 mm across and prints well
    // with a 0.4 mm nozzle; smaller modules quickly lose the tooth shape.
    gp.module        = 1.5;
    gp.teeth         = 20;
    gp.pressureAngle = 20.0;
    gp.profileShift  = 0.0;
    gp.clearance     = 0.25;
    gp.backlash      = 0.15;
    gp.bore          = 0.0;
    return gp;
}

bool GearParams::Equals(const GearParams &o) const {
    return module == o.module && teeth == o.teeth &&
           pressureAngle == o.pressureAngle && profileShift == o.profileShift &&
           clearance == o.clearance && backlash == o.backlash && bore == o.bore;
}

namespace {

struct Vec2 {
    double x, y;
    Vec2 operator+(const Vec2 &b) const { return { x + b.x, y + b.y }; }
    Vec2 operator-(const Vec2 &b) const { return { x - b.x, y - b.y }; }
    Vec2 operator*(double s)      const { return { x * s, y * s }; }
    double Len() const { return sqrt(x*x + y*y); }
};

double Involute(double a) { return tan(a) - a; }

// Everything needed to evaluate one flank. The flank is described in a
// frame with the gear center at the origin and the tooth middle along +y;
// the flank that is generated lies at x > 0. Angles theta are measured from
// the tooth middle toward that flank.
struct Flank {
    double alpha;   // pressure angle, radians
    double r;       // pitch radius
    double rb;      // base radius
    double ra;      // tip radius
    double rf;      // root radius
    double rho;     // rack tip radius
    double sc, tc;  // center of the rack tip rounding, in rack coordinates
    double psi;     // half tooth angle at the pitch circle
    double halfPitchAngle;

    // Point of the gear cut by the rack point (s, t) with normal (ns, nt).
    // The rack moves along the pitch line, which is tangent to the pitch
    // circle at (0, r); t points away from the gear center.
    Vec2 Generated(double s, double t, double ns, double nt) const {
        double phi = (s - t*ns/nt) / r;
        double X = s - r*phi, Y = r + t;
        double c = cos(phi), sn = sin(phi);
        return { c*X + sn*Y, -sn*X + c*Y };
    }

    // The root fillet, cut by the rack tip rounding; beta runs from 3*PI/2,
    // where it touches the root circle, to PI + alpha, where the rounding
    // meets the straight rack flank.
    Vec2 Fillet(double beta) const {
        double cb = cos(beta), sb = sin(beta);
        return Generated(sc + rho*cb, tc + rho*sb, cb, sb);
    }

    double InvoluteTheta(double R) const {
        return psi + Involute(alpha) - Involute(acos(rb/R));
    }
    // The involute, parametrized by its roll angle so that it is smooth down
    // to the base circle.
    Vec2 InvoluteAt(double tau) const {
        double R = rb*sqrt(1 + tau*tau);
        double th = psi + Involute(alpha) - (tau - atan(tau));
        return { R*sin(th), R*cos(th) };
    }
    static double RollAngle(double rb, double R) {
        double q = R/rb;
        return (q <= 1) ? 0 : sqrt(q*q - 1);
    }
};

double Theta(Vec2 p) { return atan2(p.x, p.y); }

// Appends cubic Beziers for the curve f on [a, b], subdividing until the
// curve is within tol of it.
template<class F>
void FitCubics(const F &f, double a, double b, double tol,
               std::vector<std::array<Vec2, 4>> *out, int depth = 0) {
    double lo = std::min(a, b), hi = std::max(a, b);
    auto deriv = [&](double t) {
        double h = (hi - lo) * 1e-6;
        double t0 = std::max(lo, t - h), t1 = std::min(hi, t + h);
        return (f(t1) - f(t0)) * (1.0 / (t1 - t0));
    };
    Vec2 p0 = f(a), p3 = f(b);
    Vec2 d0 = deriv(a) * (b - a), d3 = deriv(b) * (b - a);
    std::array<Vec2, 4> c = {{ p0, p0 + d0*(1.0/3), p3 - d3*(1.0/3), p3 }};

    // Distance of points on the curve from the cubic, which is measured
    // against a fine polyline of the cubic.
    const int M = 64;
    Vec2 poly[M + 1];
    for(int i = 0; i <= M; i++) {
        double u = (double)i / M, v = 1 - u;
        poly[i] = c[0]*(v*v*v) + c[1]*(3*u*v*v) + c[2]*(3*u*u*v) + c[3]*(u*u*u);
    }
    double err = 0;
    for(int i = 1; i < 8; i++) {
        Vec2 p = f(a + (b - a)*(i / 8.0));
        double best = VERY_POSITIVE;
        for(int j = 0; j < M; j++) {
            Vec2 d = poly[j + 1] - poly[j], w = p - poly[j];
            double dd = d.x*d.x + d.y*d.y;
            double s = (dd > 0) ? std::max(0.0, std::min(1.0, (w.x*d.x + w.y*d.y) / dd)) : 0;
            best = std::min(best, (w - d*s).Len());
        }
        err = std::max(err, best);
    }
    if(err > tol && depth < 12) {
        double m = (a + b) / 2;
        FitCubics(f, a, m, tol, out, depth + 1);
        FitCubics(f, m, b, tol, out, depth + 1);
    } else {
        out->push_back(c);
    }
}

bool MakeFlank(const GearParams &gp, Flank *fl, GearInfo *info) {
    *info = {};
    if(gp.teeth < GEAR_MIN_TEETH || gp.teeth > GEAR_MAX_TEETH) return false;
    if(!(gp.module > LENGTH_EPS)) return false;
    if(!(gp.pressureAngle >= 5 && gp.pressureAngle <= 35)) return false;
    if(!(gp.clearance >= 0 && gp.clearance <= 1)) return false;

    double m = gp.module, x = gp.profileShift;
    double alpha = gp.pressureAngle * PI / 180;
    double p = PI * m;

    fl->alpha = alpha;
    fl->r  = m * gp.teeth / 2;
    fl->rb = fl->r * cos(alpha);
    fl->ra = fl->r + (1 + x) * m;
    double hf = (1 + gp.clearance - x) * m;
    fl->rf = fl->r - hf;
    if(fl->rf <= LENGTH_EPS) return false;

    // Half the tooth thickness at the pitch circle, as an arc length.
    double h = (p/2 + 2*x*m*tan(alpha) - gp.backlash/2) / 2;
    if(h <= 0) return false;
    fl->psi = h / fl->r;
    fl->halfPitchAngle = PI / gp.teeth;

    // Largest rounding that stays inside the clearance below the working
    // depth, and that still fits into the tooth space of the rack tip.
    double sa = sin(alpha), ca = cos(alpha);
    double rho = gp.clearance * m / (1 - sa);
    double rhoMax = (p/2 - h - hf*tan(alpha)) * ca / (1 - sa);
    if(rhoMax <= 0) return false;
    rho = std::max(0.0, std::min(rho, rhoMax));
    fl->rho = rho;
    fl->tc = -hf + rho;
    fl->sc = h - fl->tc*tan(alpha) + rho/ca;

    info->pitchRadius  = fl->r;
    info->baseRadius   = fl->rb;
    info->tipRadius    = fl->ra;
    info->rootRadius   = fl->rf;
    info->filletRadius = rho;
    return true;
}

// The flank of one tooth from the root circle to the tip, as Beziers in the
// frame of Flank. Also returns the angle where the flank leaves the root
// circle and the angle where it reaches the tip.
bool MakeFlankCurves(const GearParams &gp, std::vector<std::array<Vec2, 4>> *curves,
                     double *rootTheta, double *tipTheta, GearInfo *info) {
    Flank fl;
    if(!MakeFlank(gp, &fl, info)) return false;
    double tol = std::max(1e-5, 1e-3 * gp.module);

    // Where does the rack tip envelope meet the involute? Without undercut
    // that is where the rounding meets the straight rack flank. With
    // undercut the envelope dips under the involute and crosses it further
    // out; that crossing is where the flank changes over.
    double b0 = 1.5*PI, b1 = PI + fl.alpha;
    auto dist = [&](double beta, bool *defined) {
        Vec2 q = fl.Fillet(beta);
        double R = q.Len();
        *defined = (R >= fl.rb);
        return *defined ? Theta(q) - fl.InvoluteTheta(R) : 0.0;
    };
    double betaEnd = b1;
    bool sawNegative = false;
    const int N = 400;
    double prevBeta = b0;
    for(int i = 0; i <= N; i++) {
        double beta = b0 + (b1 - b0) * i / N;
        bool defined;
        double d = dist(beta, &defined);
        if(defined && d < -1e-12) {
            sawNegative = true;
        } else if(defined && sawNegative) {
            double lo = prevBeta, hi = beta;
            for(int j = 0; j < 60; j++) {
                double mid = (lo + hi) / 2;
                bool dm;
                if(dist(mid, &dm) < 0) lo = mid; else hi = mid;
            }
            betaEnd = hi;
            break;
        }
        prevBeta = beta;
    }
    info->undercut = sawNegative;

    Vec2 filletEnd = fl.Fillet(betaEnd);
    double tau0 = Flank::RollAngle(fl.rb, filletEnd.Len());
    double tau1 = Flank::RollAngle(fl.rb, fl.ra);
    info->involuteStartRadius = filletEnd.Len();

    // Pointed teeth end where the flank reaches the tooth middle.
    auto thetaAtTau = [&](double tau) {
        return fl.psi + Involute(fl.alpha) - (tau - atan(tau));
    };
    if(thetaAtTau(tau1) <= 0) {
        double lo = tau0, hi = tau1;
        for(int j = 0; j < 80; j++) {
            double mid = (lo + hi) / 2;
            if(thetaAtTau(mid) > 0) lo = mid; else hi = mid;
        }
        tau1 = lo;
        info->pointed = true;
    }
    if(tau1 <= tau0 + 1e-9) return false;

    curves->clear();
    if(fl.rho > LENGTH_EPS) {
        FitCubics([&](double beta) { return fl.Fillet(beta); }, b0, betaEnd, tol, curves);
    }
    size_t firstInvolute = curves->size();
    FitCubics([&](double tau) { return fl.InvoluteAt(tau); }, tau0, tau1, tol, curves);
    // Join the pieces exactly.
    if(firstInvolute > 0) {
        Vec2 d = (*curves)[firstInvolute - 1][3] - (*curves)[firstInvolute][0];
        (*curves)[firstInvolute][0] = (*curves)[firstInvolute][0] + d;
        (*curves)[firstInvolute][1] = (*curves)[firstInvolute][1] + d;
    }

    *rootTheta = Theta((*curves).front()[0]);
    *tipTheta  = info->pointed ? 0.0 : Theta((*curves).back()[3]);
    info->tipThickness = 2 * fl.ra * *tipTheta;
    info->valid = true;
    return true;
}

}

GearInfo GearComputeInfo(const GearParams &gp) {
    GearInfo info;
    std::vector<std::array<Vec2, 4>> curves;
    double rootTheta, tipTheta;
    if(!MakeFlankCurves(gp, &curves, &rootTheta, &tipTheta, &info)) {
        info.valid = false;
    }
    return info;
}

void GearMakeOutline(const GearParams &gp, Vector c, Vector xd, Vector yd,
                     double scale, SBezierList *sbl) {
    GearInfo info;
    std::vector<std::array<Vec2, 4>> flank;
    double rootTheta, tipTheta;
    if(!MakeFlankCurves(gp, &flank, &rootTheta, &tipTheta, &info)) return;

    // The flank frame has the tooth middle along +y and the flank on the
    // side of negative angles; map it so that tooth k has its middle at
    // angle phi.
    auto toPlane = [&](Vec2 p, double phi, bool mirror) {
        double th = Theta(p), R = p.Len();
        double a = mirror ? phi + th : phi - th;
        return c.Plus(xd.ScaledBy(scale*R*cos(a))).Plus(yd.ScaledBy(scale*R*sin(a)));
    };
    auto addArc = [&](double R, double a0, double a1) {
        double span = a1 - a0;
        if(fabs(span) < 1e-9) return;
        int n = std::max(1, (int)ceil(fabs(span) / (PI/2) - 1e-9));
        double da = span / n;
        for(int i = 0; i < n; i++) {
            double t0 = a0 + da*i, t1 = t0 + da;
            auto pt = [&](double a, double rr) {
                return c.Plus(xd.ScaledBy(scale*rr*cos(a))).Plus(yd.ScaledBy(scale*rr*sin(a)));
            };
            Vector p0 = pt(t0, R), p2 = pt(t1, R);
            Vector p1 = pt((t0 + t1)/2, R / cos(da/2));
            SBezier sb = SBezier::From(p0, p1, p2);
            sb.weight[1] = cos(da/2);
            sbl->l.Add(&sb);
        }
    };

    int z = gp.teeth;
    double R_root = info.rootRadius, R_tip = info.tipRadius;
    for(int k = 0; k < z; k++) {
        double phi = 2*PI*k/z;
        // Counterclockwise: the flank at phi - theta from root to tip, the
        // tip, the other flank from tip to root, then the root up to the
        // next tooth.
        for(const auto &cv : flank) {
            SBezier sb = SBezier::From(toPlane(cv[0], phi, false), toPlane(cv[1], phi, false),
                                       toPlane(cv[2], phi, false), toPlane(cv[3], phi, false));
            sbl->l.Add(&sb);
        }
        if(!info.pointed) {
            addArc(R_tip, phi - tipTheta, phi + tipTheta);
        }
        for(auto it = flank.rbegin(); it != flank.rend(); ++it) {
            const auto &cv = *it;
            SBezier sb = SBezier::From(toPlane(cv[3], phi, true), toPlane(cv[2], phi, true),
                                       toPlane(cv[1], phi, true), toPlane(cv[0], phi, true));
            sbl->l.Add(&sb);
        }
        addArc(R_root, phi + rootTheta, phi + 2*PI/z - rootTheta);
    }

    double rBore = gp.bore / 2;
    if(rBore > LENGTH_EPS && rBore < R_root - LENGTH_EPS) {
        for(int i = 0; i < 4; i++) addArc(rBore, i*PI/2, (i + 1)*PI/2);
    }
}

double GearCenterDistance(const GearParams &a, const GearParams &b) {
    double alpha = a.pressureAngle * PI / 180;
    int zs = a.teeth + b.teeth;
    double a0 = a.module * zs / 2;
    double xs = a.profileShift + b.profileShift;
    if(zs <= 0 || fabs(xs) < 1e-12) return a0;

    // Working pressure angle from inv(aw) = inv(a) + 2 tan(a) (x1 + x2) / (z1 + z2).
    double target = Involute(alpha) + 2*tan(alpha)*xs/zs;
    if(target <= 0) return a0;
    double aw = alpha;
    for(int i = 0; i < 50; i++) {
        double f = Involute(aw) - target;
        double df = tan(aw)*tan(aw);
        if(df < 1e-12) break;
        double step = f / df;
        aw -= step;
        if(aw < 1e-6) aw = 1e-6;
        if(aw > PI/2 - 1e-6) aw = PI/2 - 1e-6;
        if(fabs(step) < 1e-15) break;
    }
    return a0 * cos(alpha) / cos(aw);
}

double GearMatePhase(int driverTeeth, int mateTeeth, double driverPhase) {
    return PI/mateTeeth - driverPhase * driverTeeth / mateTeeth;
}

//-----------------------------------------------------------------------------
// Gears in the sketch.
//-----------------------------------------------------------------------------

hConstraint GearFindCenterConstraint(hRequest a, hRequest b) {
    hEntity ca = a.entity(1), cb = b.entity(1);
    for(const Constraint &c : SK.constraint) {
        if(c.type != Constraint::Type::PT_PT_DISTANCE) continue;
        if((c.ptA == ca && c.ptB == cb) || (c.ptA == cb && c.ptB == ca)) return c.h;
    }
    return Constraint::NO_CONSTRAINT;
}

namespace {

// Angle of a direction in the plane of the workplane wrkpl.
double AngleInWorkplane(hEntity wrkpl, Vector d) {
    EntityBase *n = SK.GetEntity(wrkpl)->Normal();
    return atan2(d.Dot(n->NormalV()), d.Dot(n->NormalU()));
}

// Turns the mate so that it meshes with the driver, from the current
// positions of their centers and of the tooth marker of the driver.
void PhaseMate(Request *driver, Request *mate) {
    Vector c1 = SK.GetEntity(driver->h.entity(1))->PointGetNum(),
           p1 = SK.GetEntity(driver->h.entity(2))->PointGetNum(),
           c2 = SK.GetEntity(mate->h.entity(1))->PointGetNum();
    hEntity wrkpl = mate->workplane;
    if(c2.Minus(c1).Magnitude() < LENGTH_EPS || p1.Minus(c1).Magnitude() < LENGTH_EPS) return;
    double psi  = AngleInWorkplane(wrkpl, c2.Minus(c1));
    double phi1 = AngleInWorkplane(wrkpl, p1.Minus(c1));
    double phi2 = psi + PI + GearMatePhase(driver->gear.teeth, mate->gear.teeth, phi1 - psi);
    double r2 = mate->gear.module * mate->gear.teeth / 2;
    EntityBase *n = SK.GetEntity(wrkpl)->Normal();
    Vector p2 = c2.Plus(n->NormalU().ScaledBy(r2*cos(phi2)))
                  .Plus(n->NormalV().ScaledBy(r2*sin(phi2)));
    SK.GetEntity(mate->h.entity(2))->PointForceTo(p2);
}

}

void GearSyncLinked(hRequest hr) {
    Request *src = SK.request.FindByIdNoOops(hr);
    if(src == NULL || src->type != Request::Type::GEAR) return;

    // All gears connected to this one through gearDriver, in the order
    // they are reached from it.
    std::vector<hRequest> todo = { hr }, seen = { hr };
    while(!todo.empty()) {
        hRequest cur = todo.back();
        todo.pop_back();
        Request *rc = SK.GetRequest(cur);
        for(Request &r : SK.request) {
            if(r.type != Request::Type::GEAR) continue;
            bool linked = (r.gearDriver == cur) || (rc->gearDriver == r.h);
            if(!linked) continue;
            if(std::find(seen.begin(), seen.end(), r.h) != seen.end()) continue;
            seen.push_back(r.h);
            todo.push_back(r.h);
        }
    }

    for(hRequest h : seen) {
        Request *r = SK.GetRequest(h);
        r->gear.module        = src->gear.module;
        r->gear.pressureAngle = src->gear.pressureAngle;
        r->gear.clearance     = src->gear.clearance;
        r->gear.backlash      = src->gear.backlash;
        SS.MarkGroupDirty(r->group);
    }
    // Drivers before their mates, so that each mate follows a driver that
    // is in place already.
    for(hRequest h : seen) {
        Request *mate = SK.GetRequest(h);
        Request *driver = SK.request.FindByIdNoOops(mate->gearDriver);
        if(driver == NULL || driver->type != Request::Type::GEAR) continue;
        hConstraint hc = GearFindCenterConstraint(driver->h, mate->h);
        if(hc != Constraint::NO_CONSTRAINT) {
            Constraint *c = SK.GetConstraint(hc);
            c->valA = GearCenterDistance(driver->gear, mate->gear);
            SS.MarkGroupDirty(c->group);
            // Move the mate out to the new distance already, so that the
            // solver does not turn it on the way there.
            Vector c1 = SK.GetEntity(driver->h.entity(1))->PointGetNum(),
                   c2 = SK.GetEntity(mate->h.entity(1))->PointGetNum();
            Vector d = c2.Minus(c1);
            if(d.Magnitude() > LENGTH_EPS) {
                SK.GetEntity(mate->h.entity(1))->PointForceTo(
                    c1.Plus(d.WithMagnitude(c->valA)));
            }
        }
        PhaseMate(driver, mate);
    }
}

hRequest GearAddMate(hRequest hd, int teeth) {
    Request *driver = SK.request.FindByIdNoOops(hd);
    if(driver == NULL || driver->type != Request::Type::GEAR) return {};
    if(driver->group != SS.GW.activeGroup || driver->workplane != SS.GW.ActiveWorkplane()) {
        Error(_("Activate the group and workplane of the gear to add a mating gear."));
        return {};
    }
    if(teeth < GEAR_MIN_TEETH || teeth > GEAR_MAX_TEETH) {
        Error(_("The number of teeth must be between %d and %d."),
              GEAR_MIN_TEETH, GEAR_MAX_TEETH);
        return {};
    }

    GearParams gp = driver->gear;
    gp.teeth = teeth;
    gp.profileShift = 0;
    hEntity wrkpl = driver->workplane;
    Vector c1 = SK.GetEntity(hd.entity(1))->PointGetNum();

    hRequest hm = SS.GW.AddRequest(Request::Type::GEAR, /*rememberForUndo=*/false);
    // AddRequest may have moved the request list.
    driver = SK.GetRequest(hd);
    Request *mate = SK.GetRequest(hm);
    mate->gear = gp;
    mate->gearDriver = hd;
    mate->construction = driver->construction;
    mate->style = driver->style;

    double a = GearCenterDistance(driver->gear, gp);
    Vector u = SK.GetEntity(wrkpl)->Normal()->NormalU();
    Vector c2 = c1.Plus(u.ScaledBy(a));
    SK.GetEntity(hm.entity(1))->PointForceTo(c2);
    SK.GetEntity(hm.entity(2))->PointForceTo(c2.Plus(u.ScaledBy(gp.module * teeth / 2)));
    PhaseMate(driver, mate);

    Constraint c = {};
    c.group = driver->group;
    c.workplane = wrkpl;
    c.type = Constraint::Type::PT_PT_DISTANCE;
    c.ptA = hd.entity(1);
    c.ptB = hm.entity(1);
    c.valA = a;
    c.disp.offset = SK.GetEntity(wrkpl)->Normal()->NormalV().ScaledBy(-gp.module * 3);
    Constraint::AddConstraint(&c, /*rememberForUndo=*/false);

    SS.MarkGroupDirty(driver->group);
    return hm;
}

}
