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
           clearance == o.clearance && backlash == o.backlash && bore == o.bore &&
           rackBase == o.rackBase;
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
                     double scale, SBezierList *sbl, Vector boreU) {
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
        double a0 = (boreU.Magnitude() > LENGTH_EPS) ? atan2(boreU.Dot(yd), boreU.Dot(xd)) : 0;
        for(int i = 0; i < 4; i++) addArc(rBore, a0 + i*PI/2, a0 + (i + 1)*PI/2);
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
// Racks.
//-----------------------------------------------------------------------------

namespace {

// The profile of a rack tooth in a frame with s along the pitch line and t
// toward the tooth tips. The tooth space around s = 0 is bounded by the
// root line t = -hf and the flank s = g + t tan(alpha), joined by a fillet
// of radius rho centered at (sc, tc).
struct RackProfile {
    double p, alpha, ha, hf, g, rho, sc, tc, base;
};

bool MakeRackProfile(const GearParams &gp, RackProfile *rp, GearRackInfo *info) {
    *info = {};
    if(gp.teeth < GEAR_RACK_MIN_TEETH || gp.teeth > GEAR_RACK_MAX_TEETH) return false;
    if(!(gp.module > LENGTH_EPS)) return false;
    if(!(gp.pressureAngle >= 5 && gp.pressureAngle <= 35)) return false;
    if(!(gp.clearance >= 0 && gp.clearance <= 1)) return false;
    if(!(gp.backlash >= 0)) return false;

    double m = gp.module;
    double alpha = gp.pressureAngle * PI / 180;
    double sa = sin(alpha), ca = cos(alpha), ta = tan(alpha);
    rp->p = PI * m;
    rp->alpha = alpha;
    rp->ha = m;
    rp->hf = (1 + gp.clearance) * m;
    // Half the tooth space at the pitch line; each tooth is thinner by half
    // the backlash.
    double h = (rp->p/2 - gp.backlash/2) / 2;
    rp->g = rp->p/2 - h;
    double tipHalf = h - rp->ha * ta;

    // The same root rounding as the basic rack that cuts the gears, as far
    // as it fits into the tooth space.
    double rho = gp.clearance * m / (1 - sa);
    double rhoMax = (rp->g - rp->hf * ta) * ca / (1 - sa);
    if(rhoMax < 0) return false;
    rho = std::max(0.0, std::min(rho, rhoMax));
    rp->rho = rho;
    rp->tc = -rp->hf + rho;
    rp->sc = std::max(0.0, rp->g + rp->tc * ta - rho / ca);
    rp->base = gp.rackBase;

    info->pitch        = rp->p;
    info->length       = rp->p * gp.teeth;
    info->addendum     = rp->ha;
    info->dedendum     = rp->hf;
    info->height       = rp->ha + rp->hf + rp->base;
    info->filletRadius = rho;
    info->tipWidth     = 2 * tipHalf;
    if(tipHalf <= LENGTH_EPS) return false;
    if(!(rp->base > LENGTH_EPS)) return false;
    info->valid = true;
    return true;
}

}

GearRackInfo GearRackComputeInfo(const GearParams &gp) {
    RackProfile rp;
    GearRackInfo info;
    MakeRackProfile(gp, &rp, &info);
    return info;
}

void GearMakeRackOutline(const GearParams &gp, Vector p0, Vector xd, Vector yd,
                         double scale, SBezierList *sbl) {
    RackProfile rp;
    GearRackInfo info;
    if(!MakeRackProfile(gp, &rp, &info)) return;

    auto toPlane = [&](Vec2 q) {
        return p0.Plus(xd.ScaledBy(scale*q.x)).Plus(yd.ScaledBy(scale*q.y));
    };
    auto addLine = [&](Vec2 a, Vec2 b) {
        if((b - a).Len() < LENGTH_EPS) return;
        SBezier sb = SBezier::From(toPlane(a), toPlane(b));
        sbl->l.Add(&sb);
    };
    auto addArc = [&](Vec2 a, Vec2 m, Vec2 b, double w) {
        SBezier sb = SBezier::From(toPlane(a), toPlane(m), toPlane(b));
        sb.weight[1] = w;
        sbl->l.Add(&sb);
    };

    // The fillet on the left side of the tooth after the tooth space at
    // s = 0, from the root line to the flank, as a rational quadratic.
    double ca = cos(rp.alpha), sa = sin(rp.alpha), ta = tan(rp.alpha);
    double span = PI/2 - rp.alpha, mid = -PI/2 + span/2;
    Vec2 fc = { rp.sc, rp.tc };
    Vec2 fa = { rp.sc, -rp.hf };
    Vec2 fb = fc + Vec2{ ca, -sa } * rp.rho;
    Vec2 fm = fc + Vec2{ cos(mid), sin(mid) } * (rp.rho / cos(span/2));
    double fw = cos(span/2);
    bool fillet = rp.rho > LENGTH_EPS;
    Vec2 tip = { rp.g + rp.ha * ta, rp.ha };

    int n = gp.teeth;
    double p = rp.p, L = p * n;
    auto at = [](Vec2 q, double ds) { return Vec2{ q.x + ds, q.y }; };
    // Mirror about the middle of the tooth that starts at s0.
    auto mirror = [&](Vec2 q, double s0) { return Vec2{ 2*s0 + p - q.x, q.y }; };

    // The top from left to right: root, then each tooth with its fillets,
    // with the root between two teeth as one line.
    addLine({ 0, -rp.hf }, fa);
    for(int k = 0; k < n; k++) {
        double s0 = p * k;
        if(fillet) addArc(at(fa, s0), at(fm, s0), at(fb, s0), fw);
        addLine(at(fb, s0), at(tip, s0));
        addLine(at(tip, s0), mirror(at(tip, s0), s0));
        addLine(mirror(at(tip, s0), s0), mirror(at(fb, s0), s0));
        if(fillet) {
            addArc(mirror(at(fb, s0), s0), mirror(at(fm, s0), s0),
                   mirror(at(fa, s0), s0), fw);
        }
        Vec2 next = (k + 1 < n) ? at(fa, s0 + p) : Vec2{ L, -rp.hf };
        addLine(mirror(at(fa, s0), s0), next);
    }
    // Down the right end, along the back, and up the left end.
    double back = -rp.hf - rp.base;
    addLine({ L, -rp.hf }, { L, back });
    addLine({ L, back }, { 0, back });
    addLine({ 0, back }, { 0, -rp.hf });
}

double GearRackDistance(const GearParams &gear) {
    return gear.module * (gear.teeth / 2.0 + gear.profileShift);
}

double GearRackFootPosition(const GearParams &gear, double footPos, double toothAngle) {
    // A tooth of the gear that points at the rack faces the middle of a tooth
    // space, which lies at a multiple of the pitch from the start. Turning
    // the gear counterclockwise moves the rack forward by the pitch radius
    // times the angle.
    double r = gear.module * gear.teeth / 2;
    double p = PI * gear.module;
    double back = -r * toothAngle;
    return back + p * floor((footPos - back) / p + 0.5);
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

// Distance of the point c from the line through a and b, with the sign that
// a pt-line-distance constraint in the workplane wrkpl gives it.
double SignedLineDistance(hEntity wrkpl, Vector c, Vector a, Vector b) {
    EntityBase *n = SK.GetEntity(wrkpl)->Normal();
    Vector u = n->NormalU(), v = n->NormalV();
    double du = a.Minus(b).Dot(u), dv = a.Minus(b).Dot(v);
    double m = sqrt(du*du + dv*dv);
    if(m < LENGTH_EPS) return 0;
    return (dv * a.Minus(c).Dot(u) - du * a.Minus(c).Dot(v)) / m;
}

// Moves the rack to the distance from the driver at which it meshes, keeps
// its direction, and slides it along its pitch line so that its teeth mesh
// with those of the driver. The construction line on the pitch line and
// the distance constraint follow.
void PlaceRack(Request *driver, Request *rack) {
    Vector c  = SK.GetEntity(driver->h.entity(1))->PointGetNum(),
           pm = SK.GetEntity(driver->h.entity(2))->PointGetNum(),
           p0 = SK.GetEntity(rack->h.entity(1))->PointGetNum(),
           p1 = SK.GetEntity(rack->h.entity(2))->PointGetNum();
    hEntity wrkpl = rack->workplane;
    EntityBase *n = SK.GetEntity(wrkpl)->Normal();
    Vector xd = p1.Minus(p0);
    if(xd.Magnitude() < LENGTH_EPS || pm.Minus(c).Magnitude() < LENGTH_EPS) return;
    xd = xd.WithMagnitude(1);
    Vector yd = n->NormalN().Cross(xd).WithMagnitude(1);
    // The teeth must face the gear; a rack moved to the other side of it
    // is left alone.
    if(c.Minus(p0).Dot(yd) <= 0) return;

    const GearParams &gp = driver->gear;
    double dist = GearRackDistance(gp);
    double foot = c.Minus(p0).Dot(xd);
    double toothAngle = AngleInWorkplane(wrkpl, pm.Minus(c)) -
                        AngleInWorkplane(wrkpl, yd.Negated());
    foot = GearRackFootPosition(gp, foot, toothAngle);
    double L = PI * rack->gear.module * rack->gear.teeth;
    Vector q0 = c.Minus(yd.ScaledBy(dist)).Minus(xd.ScaledBy(foot));
    Vector q1 = q0.Plus(xd.ScaledBy(L));
    SK.GetEntity(rack->h.entity(1))->PointForceTo(q0);
    SK.GetEntity(rack->h.entity(2))->PointForceTo(q1);

    // The construction line to the back, if any, and its length.
    hRequest hb = GearFindRackBackLine(rack->h);
    if(hb.v != 0) {
        double depth = (1 + rack->gear.clearance) * rack->gear.module + rack->gear.rackBase;
        SK.GetEntity(hb.entity(1))->PointForceTo(q0);
        SK.GetEntity(hb.entity(2))->PointForceTo(q0.Minus(yd.ScaledBy(depth)));
        for(Constraint &cn : SK.constraint) {
            if(cn.type != Constraint::Type::PT_PT_DISTANCE) continue;
            if(!((cn.ptA == hb.entity(1) && cn.ptB == hb.entity(2)) ||
                 (cn.ptA == hb.entity(2) && cn.ptB == hb.entity(1)))) continue;
            cn.valA = depth;
            SS.MarkGroupDirty(cn.group);
        }
    }

    hRequest hl = GearFindRackLine(rack->h);
    if(hl.v == 0) return;
    SK.GetEntity(hl.entity(1))->PointForceTo(q0);
    SK.GetEntity(hl.entity(2))->PointForceTo(q1);
    hConstraint hc = GearFindRackConstraint(driver->h, rack->h);
    if(hc == Constraint::NO_CONSTRAINT) return;
    Constraint *cn = SK.GetConstraint(hc);
    double sign = (SignedLineDistance(cn->workplane, c, q0, q1) < 0) ? -1 : 1;
    cn->valA = sign * dist;
    SS.MarkGroupDirty(cn->group);
}

bool IsGearOrRack(const Request &r) {
    return r.type == Request::Type::GEAR || r.type == Request::Type::GEAR_RACK;
}

}

namespace {

bool PointsCoincident(hEntity a, hEntity b) {
    for(const Constraint &c : SK.constraint) {
        if(c.type != Constraint::Type::POINTS_COINCIDENT) continue;
        if((c.ptA == a && c.ptB == b) || (c.ptA == b && c.ptB == a)) return true;
    }
    return false;
}

// A construction line in the sketch of the rack that starts at the start
// of its pitch line, and that ends at the end of the pitch line or not.
hRequest FindRackLine(hRequest rack, bool toEnd) {
    Request *rr = SK.request.FindByIdNoOops(rack);
    if(rr == NULL) return {};
    for(const Request &r : SK.request) {
        if(r.type != Request::Type::LINE_SEGMENT || r.group != rr->group) continue;
        if(!PointsCoincident(r.h.entity(1), rack.entity(1))) continue;
        if(PointsCoincident(r.h.entity(2), rack.entity(2)) == toEnd) return r.h;
    }
    return {};
}

}

hRequest GearFindRackLine(hRequest rack) {
    return FindRackLine(rack, /*toEnd=*/true);
}

hRequest GearFindRackBackLine(hRequest rack) {
    return FindRackLine(rack, /*toEnd=*/false);
}

hConstraint GearFindRackConstraint(hRequest gear, hRequest rack) {
    hRequest hl = GearFindRackLine(rack);
    if(hl.v == 0) return Constraint::NO_CONSTRAINT;
    for(const Constraint &c : SK.constraint) {
        if(c.type != Constraint::Type::PT_LINE_DISTANCE) continue;
        if(c.ptA == gear.entity(1) && c.entityA == hl.entity(0)) return c.h;
    }
    return Constraint::NO_CONSTRAINT;
}

void GearSyncLinked(hRequest hr) {
    Request *src = SK.request.FindByIdNoOops(hr);
    if(src == NULL || !IsGearOrRack(*src)) return;

    // All gears and racks connected to this one through gearDriver, in the
    // order they are reached from it.
    std::vector<hRequest> todo = { hr }, seen = { hr };
    while(!todo.empty()) {
        hRequest cur = todo.back();
        todo.pop_back();
        Request *rc = SK.GetRequest(cur);
        for(Request &r : SK.request) {
            if(!IsGearOrRack(r)) continue;
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
        if(mate->type == Request::Type::GEAR_RACK) {
            PlaceRack(driver, mate);
            continue;
        }
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

namespace {

// Adds the group after the active one and makes it active, as the Group
// menu does, then generates it so that its entities exist.
Group *InsertGroup(Group g) {
    if(g.IsMeshGroup()) {
        Group *running = SK.GetRunningMeshGroupFor(SS.GW.activeGroup);
        if(running != NULL) g.color = running->color;
    }
    bool afterActive = false;
    for(hGroup hg : SK.groupOrder) {
        Group *gi = SK.GetGroup(hg);
        if(afterActive) gi->order += 1;
        if(gi->h == SS.GW.activeGroup) {
            g.order = gi->order + 1;
            afterActive = true;
        }
    }
    g.clean = false;
    SK.group.AddAndAssignId(&g);
    SS.GW.activeGroup = g.h;
    SS.GenerateAll(SolveSpaceUI::Generate::DIRTY);
    Group *gg = SK.GetGroup(g.h);
    if(gg->type == Group::Type::DRAWING_WORKPLANE) {
        gg->activeWorkplane = gg->h.entity(0);
    }
    return gg;
}

Group NewGroup(Group::Type type, const char *name) {
    Group g = {};
    g.visible = true;
    g.color = RGBi(100, 100, 100);
    g.scale = 1;
    g.type = type;
    g.name = name;
    return g;
}

Group *AddSketch(Quaternion q, hEntity origin, const char *name) {
    Group g = NewGroup(Group::Type::DRAWING_WORKPLANE, name);
    g.subtype = Group::Subtype::WORKPLANE_BY_POINT_ORTHO;
    g.predef.q = q;
    g.predef.origin = origin;
    return InsertGroup(g);
}

}

hRequest GearAddMate(hRequest hd, int teeth) {
    Request *driver = SK.request.FindByIdNoOops(hd);
    if(driver == NULL || driver->type != Request::Type::GEAR) return {};
    if(teeth < GEAR_MIN_TEETH || teeth > GEAR_MAX_TEETH) {
        Error(_("The number of teeth must be between %d and %d."),
              GEAR_MIN_TEETH, GEAR_MAX_TEETH);
        return {};
    }

    GearParams gp = driver->gear;
    gp.teeth = teeth;
    gp.profileShift = 0;
    Quaternion q = SK.GetEntity(driver->workplane)->Normal()->NormalGetNum();
    Vector u = q.RotationU(), v = q.RotationV();
    Vector c1 = SK.GetEntity(hd.entity(1))->PointGetNum();

    // The mate gets a sketch of its own in the plane of the driver, so that
    // it can be extruded and get a hub of its own. The sketch follows the
    // active group, as with the Group menu. Set GEAR_MATE_AFTER_ACTIVE_GROUP
    // to false to put it after all other groups instead.
    if(GEAR_MATE_AFTER_ACTIVE_GROUP) {
        Group *active = SK.group.FindByIdNoOops(SS.GW.activeGroup);
        if(active == NULL || active->order < SK.GetGroup(driver->group)->order) {
            Error(_("Activate the sketch of the gear or a later group to add a "
                    "mating gear."));
            return {};
        }
    } else {
        SS.GW.activeGroup = SK.groupOrder[SK.groupOrder.n - 1];
    }
    Group *sketch = AddSketch(q, hd.entity(1), "mating-gear");
    hGroup hs = sketch->h;
    hEntity wrkpl = sketch->activeWorkplane;

    hRequest hm = SS.GW.AddRequest(Request::Type::GEAR, /*rememberForUndo=*/false);
    driver = SK.GetRequest(hd);
    Request *mate = SK.GetRequest(hm);
    mate->gear = gp;
    mate->gearDriver = hd;
    mate->style = driver->style;

    double a = GearCenterDistance(driver->gear, gp);
    Vector c2 = c1.Plus(u.ScaledBy(a));
    SK.GetEntity(hm.entity(1))->PointForceTo(c2);
    SK.GetEntity(hm.entity(2))->PointForceTo(c2.Plus(u.ScaledBy(gp.module * teeth / 2)));
    PhaseMate(driver, mate);

    Constraint c = {};
    c.group = hs;
    c.workplane = wrkpl;
    c.type = Constraint::Type::PT_PT_DISTANCE;
    c.ptA = hd.entity(1);
    c.ptB = hm.entity(1);
    c.valA = a;
    c.disp.offset = v.ScaledBy(-gp.module * 3);
    Constraint::AddConstraint(&c, /*rememberForUndo=*/false);

    SS.MarkGroupDirty(hs);
    return hm;
}

hRequest GearAddRack(hRequest hd, int teeth, double base) {
    Request *driver = SK.request.FindByIdNoOops(hd);
    if(driver == NULL || driver->type != Request::Type::GEAR) return {};
    if(teeth < GEAR_RACK_MIN_TEETH || teeth > GEAR_RACK_MAX_TEETH) {
        Error(_("The number of teeth must be between %d and %d."),
              GEAR_RACK_MIN_TEETH, GEAR_RACK_MAX_TEETH);
        return {};
    }
    if(!(base > LENGTH_EPS)) {
        Error(_("The back of the rack must be thicker than zero."));
        return {};
    }

    GearParams gp = driver->gear;
    gp.teeth = teeth;
    gp.profileShift = 0;
    gp.bore = 0;
    gp.rackBase = base;
    if(!GearRackComputeInfo(gp).valid) {
        Error(_("No rack tooth shape is possible for these values."));
        return {};
    }
    Quaternion q = SK.GetEntity(driver->workplane)->Normal()->NormalGetNum();
    Vector u = q.RotationU(), v = q.RotationV();
    Vector c = SK.GetEntity(hd.entity(1))->PointGetNum();

    // The rack gets a sketch of its own in the plane of the gear, so that
    // it can be extruded on its own, after the active group as with the
    // Group menu, or after all other groups.
    if(GEAR_RACK_AFTER_ACTIVE_GROUP) {
        Group *active = SK.group.FindByIdNoOops(SS.GW.activeGroup);
        if(active == NULL || active->order < SK.GetGroup(driver->group)->order) {
            Error(_("Activate the sketch of the gear or a later group to add a rack."));
            return {};
        }
    } else {
        SS.GW.activeGroup = SK.groupOrder[SK.groupOrder.n - 1];
    }
    Group *sketch = AddSketch(q, hd.entity(1), "rack");
    hGroup hs = sketch->h;
    hEntity wrkpl = sketch->activeWorkplane;

    hRequest hr = SS.GW.AddRequest(Request::Type::GEAR_RACK, /*rememberForUndo=*/false);
    hRequest hl = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    driver = SK.GetRequest(hd);
    Request *rack = SK.GetRequest(hr);
    rack->gear = gp;
    rack->gearDriver = hd;
    rack->style = driver->style;
    SK.GetRequest(hl)->construction = true;
    SS.MarkGroupDirty(hs);
    SS.GenerateAll(SolveSpaceUI::Generate::DIRTY);

    // Below the gear, with the teeth up, centered on it before it is moved
    // into mesh.
    double L = PI * gp.module * teeth;
    Vector p0 = c.Minus(v.ScaledBy(GearRackDistance(driver->gear))).Minus(u.ScaledBy(L/2));
    Vector p1 = p0.Plus(u.ScaledBy(L));
    SK.GetEntity(hr.entity(1))->PointForceTo(p0);
    SK.GetEntity(hr.entity(2))->PointForceTo(p1);
    SK.GetEntity(hl.entity(1))->PointForceTo(p0);
    SK.GetEntity(hl.entity(2))->PointForceTo(p1);

    for(int i = 1; i <= 2; i++) {
        Constraint cc = {};
        cc.group = hs;
        cc.workplane = wrkpl;
        cc.type = Constraint::Type::POINTS_COINCIDENT;
        cc.ptA = hr.entity(i);
        cc.ptB = hl.entity(i);
        Constraint::AddConstraint(&cc, /*rememberForUndo=*/false);
    }
    Constraint cd = {};
    cd.group = hs;
    cd.workplane = wrkpl;
    cd.type = Constraint::Type::PT_LINE_DISTANCE;
    cd.ptA = hd.entity(1);
    cd.entityA = hl.entity(0);
    cd.valA = SignedLineDistance(wrkpl, c, p0, p1);
    cd.disp.offset = u.ScaledBy(-gp.module * 4);
    Constraint::AddConstraint(&cd, /*rememberForUndo=*/false);

    PlaceRack(SK.GetRequest(hd), SK.GetRequest(hr));
    SS.MarkGroupDirty(hs);
    return hr;
}

//-----------------------------------------------------------------------------
// Hub with clamp screw.
//-----------------------------------------------------------------------------

GearHubParams GearHubParams::Default(const GearParams &gp) {
    GearHubParams hp = {};
    // Four modules make a sturdy printed gear, but not under 5 mm.
    hp.width = std::max(5.0, floor(4 * gp.module * 2 + 0.5) / 2);
    hp.shaft = (gp.bore > 0) ? gp.bore : 5.0;
    hp.clamp = Clamp::INSERT;
    hp.SuggestHub();
    return hp;
}

void GearHubParams::SuggestHub() {
    // Wall around the shaft that holds the clamp screw, and a hub long
    // enough to have 2 mm of material above and below the clamp hole.
    double wall;
    switch(clamp) {
        case Clamp::INSERT:
            clampHole = 4.0;
            wall = GEAR_INSERT_LENGTH + 1.0;
            break;
        case Clamp::TAPPED:
            clampHole = 2.5;
            wall = 5.0;
            break;
        default:
            clampHole = 0;
            wall = 3.0;
            break;
    }
    // Rounded up to half millimeters.
    hubDiameter = ceil((shaft + 2 * wall) * 2 - 1e-9) / 2;
    hubLength = std::max(5.0, ceil((clampHole + 4.0) * 2 - 1e-9) / 2);
}

std::string GearHubParams::Problem(const GearParams &gp) const {
    GearInfo info = GearComputeInfo(gp);
    if(!info.valid) return _("The gear has no valid tooth shape.");
    if(!(width > LENGTH_EPS)) return _("The gear width must be greater than zero.");
    if(!(shaft > LENGTH_EPS)) return _("The shaft diameter must be greater than zero.");
    if(shaft / 2 > info.rootRadius - 1.0) {
        return _("The shaft hole leaves less than 1 mm of material below the teeth.");
    }
    if(!(hubLength > LENGTH_EPS)) return _("The hub length must be greater than zero.");
    if(hubDiameter < shaft + 2.0) {
        return _("The hub must be at least 2 mm larger than the shaft.");
    }
    if(clamp != Clamp::NONE) {
        if(!(clampHole > LENGTH_EPS)) return _("The clamp hole diameter must be greater than zero.");
        if(clampHole > hubLength - 1.0) {
            return _("The hub must be at least 1 mm longer than the clamp hole diameter.");
        }
    }
    return "";
}

std::string GearHubParams::Warning() const {
    double wall = (hubDiameter - shaft) / 2;
    if(clamp == Clamp::INSERT && wall < GEAR_INSERT_LENGTH + 0.5) {
        return ssprintf(_("The hub wall of %.1f mm is too thin for a %.1f mm insert."),
                        wall, GEAR_INSERT_LENGTH);
    }
    if(clamp == Clamp::TAPPED && wall < 3.0) {
        return ssprintf(_("The hub wall of %.1f mm gives the screw little thread."), wall);
    }
    return "";
}

namespace {

// Extrudes the sketch by depth, with a dimension between the two copies of
// the point pt so that the depth can be changed later. Given a face, the
// far copy of pt lies on that face instead, so that the extrusion ends
// where the face is.
Group *AddExtrusion(hGroup sketch, hEntity pt, Vector depth, Group::CombineAs combine,
                    const char *name, hEntity toFace = Entity::NO_ENTITY) {
    Group g = NewGroup(Group::Type::EXTRUDE, name);
    g.opA = sketch;
    g.predef.entityB = SK.GetGroup(sketch)->activeWorkplane;
    g.subtype = Group::Subtype::ONE_SIDED;
    g.meshCombine = combine;
    // No solid until the depth is set; the default one only costs time.
    g.suppress = true;
    Group *gg = InsertGroup(g);
    // A one-sided extrusion goes twice as far as its vector.
    gg->ExtrusionForceVectorTo(depth.ScaledBy(0.5));
    gg->suppress = false;

    Constraint c = {};
    c.group = gg->h;
    c.workplane = Entity::FREE_IN_3D;
    if(toFace.v != 0) {
        c.type = Constraint::Type::PT_ON_FACE;
        c.ptA = gg->Remap(pt, Group::REMAP_TOP);
        c.entityA = toFace;
    } else {
        c.type = Constraint::Type::PT_PT_DISTANCE;
        c.ptA = gg->Remap(pt, Group::REMAP_BOTTOM);
        c.ptB = gg->Remap(pt, Group::REMAP_TOP);
        c.valA = depth.Magnitude();
        c.disp.offset = depth.Normal(0).WithMagnitude(depth.Magnitude());
    }
    Constraint::AddConstraint(&c, /*rememberForUndo=*/false);
    SS.MarkGroupDirty(gg->h);
    return SK.GetGroup(c.group);
}

void AddConstraint(Constraint::Type type, hEntity ptA, hEntity ptB, hEntity entityA,
                   double valA = 0) {
    Constraint c = {};
    c.group = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();
    c.type = type;
    c.ptA = ptA;
    c.ptB = ptB;
    c.entityA = entityA;
    c.valA = valA;
    Constraint::AddConstraint(&c, /*rememberForUndo=*/false);
}

// A circle in the active sketch, centered on the point center and fixed by a
// diameter dimension.
hRequest AddCircle(hEntity center, double diameter) {
    hRequest hr = SS.GW.AddRequest(Request::Type::CIRCLE, /*rememberForUndo=*/false);
    SK.GetEntity(hr.entity(1))->PointForceTo(SK.GetEntity(center)->PointGetNum());
    SK.GetEntity(hr.entity(64))->DistanceForceTo(diameter / 2);
    AddConstraint(Constraint::Type::POINTS_COINCIDENT, hr.entity(1), center,
                  Entity::NO_ENTITY);
    AddConstraint(Constraint::Type::DIAMETER, Entity::NO_ENTITY, Entity::NO_ENTITY,
                  hr.entity(0), diameter);
    return hr;
}

}

bool GearAddHub(hRequest hg, const GearHubParams &hp, std::string *error) {
    Request *r = SK.request.FindByIdNoOops(hg);
    if(r == NULL || r->type != Request::Type::GEAR) {
        *error = _("The gear no longer exists.");
        return false;
    }
    std::string problem = hp.Problem(r->gear);
    if(!problem.empty()) {
        *error = problem;
        return false;
    }
    hGroup sketch = r->group;
    if(SK.GetGroup(sketch)->type != Group::Type::DRAWING_WORKPLANE) {
        *error = _("The gear must be drawn in a sketch in a workplane.");
        return false;
    }
    for(const Group &g : SK.group) {
        if(g.opA == sketch && g.type == Group::Type::EXTRUDE) {
            *error = _("The sketch of the gear is extruded already. Delete that "
                       "extrusion to add the gear with a hub.");
            return false;
        }
    }

    // The groups of the hub follow the active group, as with the Group
    // menu, so that they end up where the user works, usually at the end.
    // Set GEAR_HUB_AFTER_ACTIVE_GROUP to false to put them right after the
    // sketch of the gear instead.
    hGroup insertAfter = sketch;
    if(GEAR_HUB_AFTER_ACTIVE_GROUP) {
        Group *active = SK.group.FindByIdNoOops(SS.GW.activeGroup);
        if(active == NULL || active->order < SK.GetGroup(sketch)->order) {
            *error = _("Activate the sketch of the gear or a later group to add "
                       "the gear with a hub.");
            return false;
        }
        insertAfter = active->h;
    }

    hEntity wrkpl = r->workplane;
    hEntity center = hg.entity(1);
    Quaternion q = SK.GetEntity(wrkpl)->Normal()->NormalGetNum();
    Vector u = q.RotationU(), v = q.RotationV(), n = q.RotationN();

    r->gear.bore = hp.shaft;
    SS.MarkGroupDirty(sketch);
    SS.GW.activeGroup = insertAfter;

    // The gear, as thick as the face width.
    Group *gear = AddExtrusion(sketch, center, n.ScaledBy(hp.width), Group::CombineAs::UNION,
                               "gear");
    hEntity faceCenter = gear->Remap(center, Group::REMAP_TOP);

    // The hub, a ring on the gear face around the shaft hole.
    Group *hubSketch = AddSketch(q, faceCenter, "hub-sketch");
    hEntity hubOrigin = hubSketch->h.entity(2);
    hRequest outer = AddCircle(hubOrigin, hp.hubDiameter);
    AddCircle(hubOrigin, hp.shaft);
    hGroup hubSketchH = hubSketch->h;
    Group *hub = AddExtrusion(hubSketchH, outer.entity(1), n.ScaledBy(hp.hubLength),
                              Group::CombineAs::UNION, "hub");

    if(hp.clamp != GearHubParams::Clamp::NONE) {
        // The clamp hole lies along u, halfway along the hub. It is drawn in
        // a plane through the axis, centered on a construction line along
        // the axis of the hub, so that it follows the hub.
        hEntity hubBottom = outer.entity(1);
        hEntity hubTop = hub->Remap(hubBottom, Group::REMAP_TOP);
        Group *clampSketch = AddSketch(Quaternion::From(v, n), hubBottom, "clamp-sketch");
        (void)clampSketch;

        hRequest axis = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
        SK.GetRequest(axis)->construction = true;
        SK.GetEntity(axis.entity(1))->PointForceTo(SK.GetEntity(hubBottom)->PointGetNum());
        SK.GetEntity(axis.entity(2))->PointForceTo(SK.GetEntity(hubBottom)->PointGetNum()
                                                       .Plus(n.ScaledBy(hp.hubLength)));
        AddConstraint(Constraint::Type::POINTS_COINCIDENT, axis.entity(1), hubBottom,
                      Entity::NO_ENTITY);
        AddConstraint(Constraint::Type::POINTS_COINCIDENT, axis.entity(2), hubTop,
                      Entity::NO_ENTITY);

        hRequest hole = SS.GW.AddRequest(Request::Type::CIRCLE, /*rememberForUndo=*/false);
        SK.GetEntity(hole.entity(1))->PointForceTo(SK.GetEntity(hubBottom)->PointGetNum()
                                                       .Plus(n.ScaledBy(hp.hubLength / 2)));
        SK.GetEntity(hole.entity(64))->DistanceForceTo(hp.clampHole / 2);
        AddConstraint(Constraint::Type::AT_MIDPOINT, hole.entity(1), Entity::NO_ENTITY,
                      axis.entity(0));
        AddConstraint(Constraint::Type::DIAMETER, Entity::NO_ENTITY, Entity::NO_ENTITY,
                      hole.entity(0), hp.clampHole);

        // From the axis out through the wall of the hub.
        AddExtrusion(SS.GW.activeGroup, hole.entity(1), u.ScaledBy(hp.hubDiameter / 2 + 1.0),
                     Group::CombineAs::DIFFERENCE, "clamp-hole");
    }

    SS.MarkGroupDirty(sketch);
    SS.GenerateAll(SolveSpaceUI::Generate::DIRTY);
    return true;
}

//-----------------------------------------------------------------------------
// Rack made a solid, with mounting holes.
//-----------------------------------------------------------------------------

GearRackSolidParams GearRackSolidParams::Default(const GearParams &rack, Mount mount) {
    GearRackSolidParams sp = {};
    sp.width = std::max(5.0, floor(4 * rack.module * 2 + 0.5) / 2);
    sp.base = rack.rackBase;
    sp.mount = mount;
    // One hole for short racks, else two or about one each 50 mm.
    double len = PI * rack.module * rack.teeth;
    sp.holes = (len < 30) ? 1 : std::max(2, (int)floor(len / 50 + 0.5));

    // The back and width grow to hold the holes, rounded up to half
    // millimeters.
    auto up = [](double x) { return ceil(x * 2 - 1e-9) / 2; };
    switch(mount) {
        case Mount::INSERT:
            sp.holeDiameter = 4.0;
            sp.holeDepth = GEAR_INSERT_LENGTH + 0.5;
            sp.base = std::max(sp.base, up(sp.holeDepth + 1.0));
            sp.width = std::max(sp.width, up(sp.holeDiameter + 3.0));
            break;
        case Mount::CROSS:
            sp.holeDiameter = 3.4;
            sp.base = std::max(sp.base, up(sp.holeDiameter + 2.4));
            break;
        default:
            break;
    }
    return sp;
}

std::string GearRackSolidParams::Problem(const GearParams &rack) const {
    GearParams gp = rack;
    gp.rackBase = base;
    if(!(width > LENGTH_EPS)) return _("The rack width must be greater than zero.");
    if(!(base > LENGTH_EPS)) return _("The back of the rack must be thicker than zero.");
    if(!GearRackComputeInfo(gp).valid) return _("The rack has no valid tooth shape.");
    if(mount == Mount::NONE) return "";

    if(holes < 1) return _("There must be at least one hole.");
    if(!(holeDiameter > LENGTH_EPS)) return _("The hole diameter must be greater than zero.");
    double len = PI * gp.module * gp.teeth;
    if(len / (2 * holes) < holeDiameter / 2 + 1.0) {
        return _("The holes do not fit along the rack with 1 mm between them.");
    }
    if(mount == Mount::INSERT) {
        if(!(holeDepth > LENGTH_EPS)) return _("The hole depth must be greater than zero.");
        if(base < holeDepth + 1.0) {
            return _("The back must be at least 1 mm thicker than the holes are deep.");
        }
        if(width < holeDiameter + 2.0) {
            return _("The rack must be at least 2 mm wider than the holes.");
        }
    } else {
        if(base < holeDiameter + 2.0) {
            return _("The back must be at least 2 mm thicker than the holes.");
        }
    }
    return "";
}

std::string GearRackSolidParams::Warning() const {
    if(mount == Mount::INSERT && width < holeDiameter + 3.0) {
        return ssprintf(_("The wall of %.1f mm beside the inserts is thin."),
                        (width - holeDiameter) / 2);
    }
    if(mount == Mount::CROSS && base < holeDiameter + 2.4) {
        return ssprintf(_("The wall of %.1f mm around the holes is thin."),
                        (base - holeDiameter) / 2);
    }
    return "";
}

namespace {

// Adds a constraint between two lines in the active sketch.
void AddLineConstraint(Constraint::Type type, hEntity a, hEntity b) {
    Constraint c = {};
    c.group = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();
    c.type = type;
    c.entityA = a;
    c.entityB = b;
    Constraint::AddConstraint(&c, /*rememberForUndo=*/false);
}

// A circle of the given diameter in the active sketch, placed by its
// distances along and from the line, from the point start of that line.
hRequest AddPlacedCircle(hEntity line, hEntity start, Vector center, double diameter) {
    hRequest hr = SS.GW.AddRequest(Request::Type::CIRCLE, /*rememberForUndo=*/false);
    SK.GetEntity(hr.entity(1))->PointForceTo(center);
    SK.GetEntity(hr.entity(64))->DistanceForceTo(diameter / 2);
    EntityBase *ln = SK.GetEntity(line);
    Vector a = SK.GetEntity(ln->point[0])->PointGetNum(),
           b = SK.GetEntity(ln->point[1])->PointGetNum();
    Vector s = SK.GetEntity(start)->PointGetNum();
    double along = center.Minus(s).Dot(ln->VectorGetNum().WithMagnitude(1));
    AddConstraint(Constraint::Type::PROJ_PT_DISTANCE, start, hr.entity(1), line, along);
    AddConstraint(Constraint::Type::PT_LINE_DISTANCE, hr.entity(1), Entity::NO_ENTITY, line,
                  SignedLineDistance(SS.GW.ActiveWorkplane(), center, a, b));
    AddConstraint(Constraint::Type::DIAMETER, Entity::NO_ENTITY, Entity::NO_ENTITY,
                  hr.entity(0), diameter);
    return hr;
}

}

bool GearAddRackSolid(hRequest hr, const GearRackSolidParams &sp, std::string *error) {
    using Mount = GearRackSolidParams::Mount;
    Request *r = SK.request.FindByIdNoOops(hr);
    if(r == NULL || r->type != Request::Type::GEAR_RACK) {
        *error = _("The rack no longer exists.");
        return false;
    }
    std::string problem = sp.Problem(r->gear);
    if(!problem.empty()) {
        *error = problem;
        return false;
    }
    hGroup sketch = r->group;
    if(SK.GetGroup(sketch)->type != Group::Type::DRAWING_WORKPLANE) {
        *error = _("The rack must be drawn in a sketch in a workplane.");
        return false;
    }
    for(const Group &g : SK.group) {
        if(g.opA == sketch && g.type == Group::Type::EXTRUDE) {
            *error = _("The sketch of the rack is extruded already. Delete that "
                       "extrusion to make the rack a solid.");
            return false;
        }
    }
    hRequest pitch = GearFindRackLine(hr);
    if(pitch.v == 0) {
        *error = _("The pitch line of the rack is missing.");
        return false;
    }
    hGroup insertAfter = sketch;
    if(GEAR_HUB_AFTER_ACTIVE_GROUP) {
        Group *active = SK.group.FindByIdNoOops(SS.GW.activeGroup);
        if(active == NULL || active->order < SK.GetGroup(sketch)->order) {
            *error = _("Activate the sketch of the rack or a later group to make "
                       "it a solid.");
            return false;
        }
        insertAfter = active->h;
    }

    // The new back thickness moves the construction line to the back, if
    // there is one.
    r->gear.rackBase = sp.base;
    SS.MarkGroupDirty(sketch);
    GearSyncLinked(hr);
    SS.GenerateAll(SolveSpaceUI::Generate::DIRTY);

    r = SK.GetRequest(hr);
    Quaternion q = SK.GetEntity(r->workplane)->Normal()->NormalGetNum();
    Vector n = q.RotationN();
    Vector p0 = SK.GetEntity(hr.entity(1))->PointGetNum(),
           p1 = SK.GetEntity(hr.entity(2))->PointGetNum();
    Vector xd = p1.Minus(p0).WithMagnitude(1);
    Vector yd = n.Cross(xd).WithMagnitude(1);
    double len = PI * r->gear.module * r->gear.teeth;
    double hf = (1 + r->gear.clearance) * r->gear.module;

    if(sp.mount == Mount::INSERT && GearFindRackBackLine(hr).v == 0) {
        // A construction line from the start of the pitch line to the back,
        // as the origin of the sketch of the holes.
        SS.GW.activeGroup = sketch;
        hRequest hb = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
        SK.GetRequest(hb)->construction = true;
        double depth = hf + sp.base;
        SK.GetEntity(hb.entity(1))->PointForceTo(p0);
        SK.GetEntity(hb.entity(2))->PointForceTo(p0.Minus(yd.ScaledBy(depth)));
        AddConstraint(Constraint::Type::POINTS_COINCIDENT, hr.entity(1), hb.entity(1),
                      Entity::NO_ENTITY);
        AddLineConstraint(Constraint::Type::PERPENDICULAR, hb.entity(0), pitch.entity(0));
        AddConstraint(Constraint::Type::PT_PT_DISTANCE, hb.entity(1), hb.entity(2),
                      Entity::NO_ENTITY, depth);
        SS.MarkGroupDirty(sketch);
        SS.GenerateAll(SolveSpaceUI::Generate::DIRTY);
    }
    SS.GW.activeGroup = insertAfter;

    Group *body = AddExtrusion(sketch, hr.entity(1), n.ScaledBy(sp.width),
                               Group::CombineAs::UNION, "rack-body");
    hEntity bodyTop = body->Remap(Entity::NO_ENTITY, Group::REMAP_TOP);

    auto holeAt = [&](int i) { return len * (2*i + 1) / (2 * sp.holes); };
    if(sp.mount == Mount::INSERT) {
        // Drawn on the back, with u across the rack and v along it, and cut
        // in from there by the depth of the holes.
        hEntity back = GearFindRackBackLine(hr).entity(2);
        Group *hs = AddSketch(Quaternion::From(n, xd), back, "rack-insert-sketch");
        hEntity origin = hs->h.entity(2);
        Vector o = SK.GetEntity(origin)->PointGetNum();

        hRequest axis = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
        SK.GetRequest(axis)->construction = true;
        SK.GetEntity(axis.entity(1))->PointForceTo(o);
        SK.GetEntity(axis.entity(2))->PointForceTo(o.Plus(xd.ScaledBy(len)));
        AddConstraint(Constraint::Type::POINTS_COINCIDENT, axis.entity(1), origin,
                      Entity::NO_ENTITY);
        AddConstraint(Constraint::Type::VERTICAL, Entity::NO_ENTITY, Entity::NO_ENTITY,
                      axis.entity(0));
        AddConstraint(Constraint::Type::PT_PT_DISTANCE, axis.entity(1), axis.entity(2),
                      Entity::NO_ENTITY, len);
        hRequest first = {};
        for(int i = 0; i < sp.holes; i++) {
            Vector c = o.Plus(xd.ScaledBy(holeAt(i))).Plus(n.ScaledBy(sp.width / 2));
            hRequest hole = AddPlacedCircle(axis.entity(0), axis.entity(1), c, sp.holeDiameter);
            if(i == 0) first = hole;
        }
        AddExtrusion(SS.GW.activeGroup, first.entity(1), yd.ScaledBy(sp.holeDepth),
                     Group::CombineAs::DIFFERENCE, "rack-insert-holes");
    } else if(sp.mount == Mount::CROSS) {
        // Drawn in the plane of the rack, on the bottom face of the body, in
        // the middle of the back, and cut up to the top face, so that the
        // holes go through also when the width changes.
        AddSketch(q, hr.entity(1), "rack-holes-sketch");
        hRequest first = {};
        for(int i = 0; i < sp.holes; i++) {
            Vector c = p0.Plus(xd.ScaledBy(holeAt(i))).Minus(yd.ScaledBy(hf + sp.base / 2));
            hRequest hole = AddPlacedCircle(pitch.entity(0), hr.entity(1), c, sp.holeDiameter);
            if(i == 0) first = hole;
        }
        AddExtrusion(SS.GW.activeGroup, first.entity(1), n.ScaledBy(sp.width),
                     Group::CombineAs::DIFFERENCE, "rack-holes", bodyTop);
    }

    SS.MarkGroupDirty(sketch);
    SS.GenerateAll(SolveSpaceUI::Generate::DIRTY);
    return true;
}

}
