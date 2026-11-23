#include "stepnurbs.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace SolveSpace {
namespace Step {
namespace Nurbs {
namespace {

void Check(bool okay, int entity, bool unsupported, const std::string &message) {
    if(!okay) throw Error{entity, unsupported, message};
}

// One Boehm insertion in homogeneous coordinates. Inserting the active end
// knots also handles non-clamped finite B-splines, without extending their
// parameter domains or manufacturing endpoint controls.
void Insert(int degree, double value, std::vector<double> *knots,
            std::vector<Vector4> *controls) {
    const auto &u = *knots;
    const auto &p = *controls;
    int k = (int)(std::upper_bound(u.begin(), u.end(), value) - u.begin()) - 1;
    int s = (int)std::count(u.begin(), u.end(), value);
    std::vector<Vector4> q(p.size() + 1);
    for(int i = 0; i <= k - degree; ++i) q[i] = p[i];
    for(int i = k - s; i < (int)p.size(); ++i) q[i + 1] = p[i];
    for(int i = k - degree + 1; i <= k - s; ++i) {
        double alpha = (value - u[i]) / (u[i + degree] - u[i]);
        q[i] = Vector4::Blend(p[i - 1], p[i], alpha);
    }
    knots->insert(knots->begin() + k + 1, value);
    *controls = std::move(q);
}

struct HomogeneousSpan {
    Vector4 controls[4];
    double first, last;
};

std::vector<HomogeneousSpan> Decompose(const Knots &source,
                                      std::vector<Vector4> controls) {
    auto knots = source.full;
    for(double value : source.breaks) {
        int multiplicity = (int)std::count(knots.begin(), knots.end(), value);
        for(int i = multiplicity; i < source.degree; ++i) {
            Insert(source.degree, value, &knots, &controls);
        }
    }
    std::vector<HomogeneousSpan> result;
    for(size_t i = source.degree; i < controls.size(); ++i) {
        if(knots[i] < 0 || knots[i + 1] > 1 || knots[i + 1] <= knots[i]) continue;
        HomogeneousSpan span = {};
        span.first = knots[i];
        span.last = knots[i + 1];
        for(int j = 0; j <= source.degree; ++j) span.controls[j] = controls[i - source.degree + j];
        result.push_back(span);
    }
    Check(!result.empty() && result.size() < MAX_PATCHES, source.entity, true,
          "B-spline decomposition exceeds the native span limit.");
    return result;
}

double Evaluate(std::vector<double> coefficients, double t) {
    for(size_t n = coefficients.size(); n > 1; --n) {
        for(size_t i = 0; i + 1 < n; ++i) {
            coefficients[i] = (1 - t) * coefficients[i] + t * coefficients[i + 1];
        }
    }
    return coefficients.front();
}

std::vector<double> Derivative(const std::vector<double> &coefficients) {
    std::vector<double> result;
    for(size_t i = 1; i < coefficients.size(); ++i) {
        result.push_back((coefficients.size() - 1) * (coefficients[i] - coefficients[i - 1]));
    }
    return result;
}

double Binomial(int n, int k) {
    double result = 1;
    for(int i = 1; i <= k; ++i) result *= (n - i + 1) / (double)i;
    return result;
}

std::vector<double> Product(const std::vector<double> &a, const std::vector<double> &b) {
    int n = (int)a.size() - 1, m = (int)b.size() - 1;
    std::vector<double> result(n + m + 1, 0);
    for(int i = 0; i <= n; ++i) for(int j = 0; j <= m; ++j) {
        result[i + j] += a[i] * b[j] * Binomial(n, i) * Binomial(m, j) / Binomial(n + m, i + j);
    }
    return result;
}

// Small polynomials in Bernstein form (degree <= 5 here). Derivative roots partition the interval
// into monotone pieces, so even double roots/tangencies are not missed by a
// sign-change-only search. The arithmetic here is parameter root finding,
// not tolerance-based modification of source controls or weights.
std::vector<double> Roots(const std::vector<double> &coefficients) {
    std::vector<double> critical{0, 1}, roots;
    if(coefficients.size() > 2) {
        auto stationary = Roots(Derivative(coefficients));
        critical.insert(critical.end(), stationary.begin(), stationary.end());
    }
    std::sort(critical.begin(), critical.end());
    double scale = 0;
    for(double coefficient : coefficients) scale = std::max(scale, std::abs(coefficient));
    if(scale == 0) return {}; // callers choose a non-constant coordinate
    double eps = 32 * std::numeric_limits<double>::epsilon() * scale;
    for(double t : critical) if(std::abs(Evaluate(coefficients, t)) <= eps) roots.push_back(t);
    for(size_t i = 1; i < critical.size(); ++i) {
        double a = critical[i - 1], b = critical[i];
        double fa = Evaluate(coefficients, a), fb = Evaluate(coefficients, b);
        if((fa > 0) == (fb > 0) || std::abs(fa) <= eps || std::abs(fb) <= eps) continue;
        for(int j = 0; j < 60; ++j) {
            double m = (a + b) * 0.5, fm = Evaluate(coefficients, m);
            if((fa > 0) == (fm > 0)) { a = m; fa = fm; }
            else b = m;
        }
        roots.push_back((a + b) * 0.5);
    }
    std::sort(roots.begin(), roots.end());
    roots.erase(std::unique(roots.begin(), roots.end(), [](double a, double b) {
        return std::abs(a - b) < 1e-12;
    }), roots.end());
    return roots;
}

struct Box {
    Vector min, max;
    explicit Box(const SBezier &curve) {
        min = max = curve.ctrl[0];
        for(int i = 1; i <= curve.deg; ++i) curve.ctrl[i].MakeMaxMin(&max, &min);
    }
    bool Overlaps(const Box &b) const {
        const double eps = LENGTH_EPS * 0.01;
        return min.x <= b.max.x + eps && max.x >= b.min.x - eps &&
               min.y <= b.max.y + eps && max.y >= b.min.y - eps &&
               min.z <= b.max.z + eps && max.z >= b.min.z - eps;
    }
    double Size() const { return max.Minus(min).Magnitude(); }
};

} // namespace

Knots::Knots(int entity, int degree, size_t controls,
             const std::vector<int> &multiplicities, const std::vector<double> &values)
    : entity(entity), degree(degree), controls(controls) {
    Check(degree >= 1 && degree <= 3, entity, true,
          "B-spline degree is outside the native Bezier range 1..3; no degree fitting is performed.");
    Check(controls >= (size_t)degree + 1, entity, false,
          "B-spline has fewer control points than its degree requires.");
    Check(controls <= MAX_CONTROLS, entity, true, "B-spline control-point limit exceeded.");
    Check(values.size() >= 2 && values.size() == multiplicities.size(), entity, false,
          "B-spline knots and multiplicities must have matching, nonempty intervals.");
    for(size_t i = 0; i < values.size(); ++i) {
        Check(std::isfinite(values[i]) && (i == 0 || values[i] > values[i - 1]), entity, false,
              "B-spline distinct knots must be finite and strictly increasing.");
        Check(multiplicities[i] >= 1 && multiplicities[i] <= degree + 1, entity, false,
              "B-spline knot multiplicity is outside 1..degree+1.");
        Check(full.size() + multiplicities[i] <= controls + degree + 1, entity, false,
              "B-spline knot multiplicity sum does not match its control-point count and degree.");
        full.insert(full.end(), multiplicities[i], values[i]);
    }
    Check(full.size() == controls + degree + 1, entity, false,
          "B-spline knot multiplicity sum does not match its control-point count and degree.");
    double first = full[degree], last = full[controls], range = last - first;
    Check(std::isfinite(range) && range > 0, entity, false,
          "B-spline active knots must define a finite, positive parameter interval.");
    double previousSource = 0, previousNormalized = 0;
    bool havePrevious = false;
    for(double &value : full) {
        double sourceValue = value;
        value = (value - first) / range;
        Check(std::isfinite(value), entity, true,
              "B-spline knot normalization is numerically unrepresentable.");
        Check(!havePrevious || sourceValue == previousSource || value > previousNormalized,
              entity, true, "Distinct B-spline knots collapse under native parameter normalization; no interval was dropped.");
        previousSource = sourceValue; previousNormalized = value; havePrevious = true;
        if(value >= 0 && value <= 1 && (breaks.empty() || value != breaks.back())) {
            breaks.push_back(value);
        }
    }
}

void CheckWeights(int entity, const std::vector<double> &weights) {
    Check(!weights.empty(), entity, false, "Empty rational B-spline weight list.");
    double min = 1e100, max = 1e-100;
    for(double weight : weights) {
        Check(std::isfinite(weight) && weight > 0, entity, false,
              "Rational B-spline weights must be finite and strictly positive.");
        Check(weight >= 1e-100 && weight <= 1e100, entity, true,
              "Rational weights exceed the native numerical envelope; no weights were changed.");
        min = std::min(min, weight); max = std::max(max, weight);
    }
    Check(max / min <= 1e12, entity, true,
          "Rational weight ratio exceeds the native numerical envelope; no weights were changed.");
}

std::vector<CurveSpan> Curve(const Knots &knots, const std::vector<Vector> &controls,
                             const std::vector<double> &weights) {
    Check(controls.size() == knots.controls && weights.size() == controls.size(),
          knots.entity, false, "B-spline curve controls/weights do not match its knot definition.");
    for(Vector point : controls) Check(std::isfinite(point.x) && std::isfinite(point.y) &&
          std::isfinite(point.z), knots.entity, false, "Non-finite B-spline curve control point.");
    CheckWeights(knots.entity, weights);
    if(controls.size() == (size_t)knots.degree + 1 && knots.breaks.size() == 2 &&
       knots.full.front() == 0 && knots.full.back() == 1) {
        SBezier bezier = {};
        bezier.deg = knots.degree;
        for(int i = 0; i <= bezier.deg; ++i) {
            bezier.ctrl[i] = controls[i];
            bezier.weight[i] = weights[i];
        }
        return {{bezier, 0, 1}};
    }
    std::vector<Vector4> homogeneous;
    for(size_t i = 0; i < controls.size(); ++i) {
        homogeneous.push_back(Vector4::From(weights[i], controls[i]));
    }
    std::vector<CurveSpan> result;
    for(const auto &span : Decompose(knots, std::move(homogeneous))) {
        SBezier bezier = {};
        bezier.deg = knots.degree;
        for(int i = 0; i <= bezier.deg; ++i) {
            bezier.ctrl[i] = span.controls[i].PerspectiveProject();
            bezier.weight[i] = span.controls[i].w;
        }
        result.push_back({bezier, span.first, span.last});
    }
    return result;
}

std::vector<SurfacePatch> Surface(const Knots &u, const Knots &v,
                                 const std::vector<std::vector<Vector>> &controls,
                                 const std::vector<std::vector<double>> &weights) {
    Check(controls.size() == u.controls && weights.size() == u.controls, u.entity, false,
          "B-spline surface controls/weights do not match its u knot definition.");
    Check(u.controls * v.controls <= MAX_CONTROLS, u.entity, true,
          "B-spline surface control-point limit exceeded.");
    for(size_t i = 0; i < controls.size(); ++i) {
        Check(controls[i].size() == v.controls && weights[i].size() == v.controls, u.entity, false,
              "B-spline surface controls/weights do not match its v knot definition.");
        for(Vector point : controls[i]) Check(std::isfinite(point.x) && std::isfinite(point.y) &&
              std::isfinite(point.z), u.entity, false, "Non-finite B-spline surface control point.");
    }
    Check((u.breaks.size() - 1) * (v.breaks.size() - 1) <= MAX_PATCHES, u.entity, true,
          "B-spline surface decomposition exceeds the native patch limit.");
    std::vector<double> allWeights;
    for(const auto &row : weights) allWeights.insert(allWeights.end(), row.begin(), row.end());
    CheckWeights(u.entity, allWeights);
    if(u.controls == (size_t)u.degree + 1 && v.controls == (size_t)v.degree + 1 &&
       u.breaks.size() == 2 && v.breaks.size() == 2 &&
       u.full.front() == 0 && u.full.back() == 1 && v.full.front() == 0 && v.full.back() == 1) {
        SSurface surface = {};
        surface.degm = u.degree; surface.degn = v.degree;
        for(int i = 0; i <= surface.degm; ++i) for(int j = 0; j <= surface.degn; ++j) {
            surface.ctrl[i][j] = controls[i][j];
            surface.weight[i][j] = weights[i][j];
        }
        return {{surface, 0, 1, 0, 1, 0, 0}};
    }
    std::vector<std::vector<HomogeneousSpan>> columns;
    for(size_t j = 0; j < v.controls; ++j) {
        std::vector<Vector4> column;
        for(size_t i = 0; i < u.controls; ++i) {
            column.push_back(Vector4::From(weights[i][j], controls[i][j]));
        }
        columns.push_back(Decompose(u, std::move(column)));
    }
    std::vector<SurfacePatch> result;
    for(size_t iu = 0; iu < columns.front().size(); ++iu) {
        std::vector<std::vector<HomogeneousSpan>> rows;
        for(int i = 0; i <= u.degree; ++i) {
            std::vector<Vector4> row;
            for(size_t j = 0; j < v.controls; ++j) row.push_back(columns[j][iu].controls[i]);
            rows.push_back(Decompose(v, std::move(row)));
        }
        for(size_t iv = 0; iv < rows.front().size(); ++iv) {
            SSurface surface = {};
            surface.degm = u.degree; surface.degn = v.degree;
            for(int i = 0; i <= u.degree; ++i) for(int j = 0; j <= v.degree; ++j) {
                auto point = rows[i][iv].controls[j];
                surface.ctrl[i][j] = point.PerspectiveProject();
                surface.weight[i][j] = point.w;
            }
            result.push_back({surface, columns[0][iu].first, columns[0][iu].last,
                              rows[0][iv].first, rows[0][iv].last, (int)iu, (int)iv});
        }
    }
    return result;
}

SBezier Restrict(const SBezier &bezier, double first, double last) {
    if(first == 0 && last == 1) return bezier; // preserve all serialized controls/weights
    SBezier before, after, result = bezier;
    if(first != 0) {
        result.SplitAt(first, &before, &after);
        result = after;
    }
    if(last != 1) {
        result.SplitAt((last - first) / (1 - first), &before, &after);
        result = before;
    }
    return result;
}

SBezier IsoCurve(const SSurface &surface, bool constantU, bool atFinish) {
    SBezier curve = {};
    curve.deg = constantU ? surface.degn : surface.degm;
    for(int k = 0; k <= curve.deg; ++k) {
        int i = constantU ? (atFinish ? surface.degm : 0) : k;
        int j = constantU ? k : (atFinish ? surface.degn : 0);
        curve.ctrl[k] = surface.ctrl[i][j];
        curve.weight[k] = surface.weight[i][j];
    }
    return curve;
}

void Transpose(SSurface *surface) {
    for(int i = 0; i < 4; ++i) for(int j = i + 1; j < 4; ++j) {
        std::swap(surface->ctrl[i][j], surface->ctrl[j][i]);
        std::swap(surface->weight[i][j], surface->weight[j][i]);
    }
    std::swap(surface->degm, surface->degn);
}

std::vector<double> Parameters(const SBezier &bezier, Vector point, double tolerance) {
    double scale = 0;
    int axis = 0;
    for(int k = 0; k < 3; ++k) {
        double variation = 0;
        for(int i = 1; i <= bezier.deg; ++i) {
            variation = std::max(variation, std::abs(bezier.ctrl[i].Element(k) -
                                                    bezier.ctrl[0].Element(k)));
        }
        if(variation > scale) { scale = variation; axis = k; }
    }
    std::vector<double> coefficients;
    for(int i = 0; i <= bezier.deg; ++i) {
        coefficients.push_back(bezier.weight[i] * (bezier.ctrl[i].Element(axis) - point.Element(axis)));
    }
    std::vector<double> result;
    if(bezier.Start().Equals(point, tolerance)) result.push_back(0);
    if(bezier.Finish().Equals(point, tolerance)) result.push_back(1);
    for(double t : Roots(coefficients)) {
        if(bezier.PointAt(t).Equals(point, tolerance)) result.push_back(t);
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end(), [](double a, double b) {
        return std::abs(a - b) < 1e-11;
    }), result.end());
    return result;
}

std::vector<double> SortedCuts(const SBezier &bezier, std::vector<double> cuts) {
    cuts.push_back(0); cuts.push_back(1);
    std::sort(cuts.begin(), cuts.end());
    std::vector<double> result;
    for(double t : cuts) {
        if(t < 0 || t > 1) continue;
        if(result.empty() || (t != result.back() &&
           (t - result.back() > 1e-10 ||
            !bezier.PointAt(t).Equals(bezier.PointAt(result.back()), LENGTH_EPS * 0.001)))) {
            result.push_back(t);
        } else if(t == 1) result.back() = t;
    }
    return result;
}

bool IsMonotoneLine(const SBezier &bezier, Vector start, Vector finish, double tolerance) {
    Vector direction = finish.Minus(start);
    double length = direction.Magnitude();
    if(length <= LENGTH_EPS) return false;
    direction = direction.ScaledBy(1 / length);
    double first = bezier.Start().Minus(start).Dot(direction);
    double last = bezier.Finish().Minus(start).Dot(direction);
    if(first < -tolerance || last > length + tolerance || last <= first) return false;
    double largestWeight = 0;
    for(int i = 0; i <= bezier.deg; ++i) largestWeight = std::max(largestWeight, bezier.weight[i]);
    std::vector<double> numerator, denominator;
    for(int i = 0; i <= bezier.deg; ++i) {
        Vector delta = bezier.ctrl[i].Minus(start);
        if(delta.Cross(direction).Magnitude() > tolerance) return false;
        // Common scaling is for recognition arithmetic only; all source/native
        // controls and weights remain untouched, including their common scale.
        double weight = bezier.weight[i] / largestWeight;
        numerator.push_back(weight * delta.Dot(direction) / length);
        denominator.push_back(weight);
    }
    auto a = Product(Derivative(numerator), denominator);
    auto b = Product(numerator, Derivative(denominator));
    double scale = 0;
    for(size_t i = 0; i < a.size(); ++i) {
        scale = std::max({scale, std::abs(a[i]), std::abs(b[i])});
        a[i] -= b[i]; // derivative numerator N'W - NW', degree at most five
    }
    auto critical = Roots(Derivative(a));
    critical.push_back(0); critical.push_back(1);
    double eps = 128 * std::numeric_limits<double>::epsilon() * scale;
    for(double t : critical) if(Evaluate(a, t) < -eps) return false;
    return true;
}

std::vector<std::pair<double, double>> TorusSeamIntersections(const SBezier &boundary,
                                                             const SBezier &seam) {
    Vector origin = seam.Start();
    Vector normal = seam.ctrl[1].Minus(origin).Cross(seam.Finish().Minus(origin)).WithMagnitude(1);
    double largest = 1;
    for(const auto *curve : {&boundary, &seam}) for(int i = 0; i <= curve->deg; ++i) {
        Vector p = curve->ctrl[i];
        largest = std::max({largest, std::abs(p.x), std::abs(p.y), std::abs(p.z)});
    }
    double eps = std::min(LENGTH_EPS * 0.01, 64 * std::numeric_limits<double>::epsilon() * largest);
    bool coplanar = true;
    std::vector<double> coefficients, weights;
    for(int i = 0; i <= boundary.deg; ++i) {
        double distance = boundary.ctrl[i].Minus(origin).Dot(normal);
        if(std::abs(distance) > eps) coplanar = false;
        coefficients.push_back(distance * boundary.weight[i]);
        weights.push_back(boundary.weight[i]);
    }
    std::vector<std::pair<double, double>> result;
    std::vector<double> candidates{0, 1};
    if(!coplanar) {
        // The positive denominator cannot introduce or remove plane roots.
        auto roots = Roots(coefficients);
        candidates.insert(candidates.end(), roots.begin(), roots.end());
        // Translating/rotating a double plane root can leave its extremum a
        // few ulps off zero. Test those isolated stationary points too, bounded
        // by arithmetic roundoff and still by the original seam's XYZ check.
        for(double t : Roots(Derivative(coefficients))) {
            if(std::abs(Evaluate(coefficients, t)) <= eps * Evaluate(weights, t)) candidates.push_back(t);
        }
    }
    for(double a : candidates) for(double b : Parameters(seam, boundary.PointAt(a), LENGTH_EPS * 0.01)) {
        result.emplace_back(a, b);
    }
    for(double b : {0.0, 1.0}) for(double a : Parameters(boundary, seam.PointAt(b), LENGTH_EPS * 0.01)) {
        result.emplace_back(a, b);
    }
    return result;
}

std::vector<std::pair<double, double>> Intersections(int entity, const SBezier &a,
                                                    const SBezier &b) {
    std::vector<std::pair<double, double>> result;
    auto add = [&](double ta, double tb) {
        for(const auto &hit : result) {
            if(std::abs(hit.first - ta) < 1e-9 && std::abs(hit.second - tb) < 1e-9) return;
        }
        result.emplace_back(ta, tb);
    };
    for(double ta : {0.0, 1.0}) for(double tb : Parameters(b, a.PointAt(ta), LENGTH_EPS * 0.01)) add(ta, tb);
    for(double tb : {0.0, 1.0}) for(double ta : Parameters(a, b.PointAt(tb), LENGTH_EPS * 0.01)) add(ta, tb);
    // Coincident intervals are split at their endpoints. They are not a cloud
    // of intersection points, and must not exhaust the subdivision budget.
    auto contained = [](const SBezier &x, const SBezier &y) {
        for(double t : {0.0, 0.25, 0.5, 0.75, 1.0}) {
            if(Parameters(y, x.PointAt(t), LENGTH_EPS * 0.01).empty()) return false;
        }
        return true;
    };
    if(contained(a, b) || contained(b, a)) return result;

    struct Pair { SBezier a, b; double a0, a1, b0, b1; int depth; };
    std::vector<Pair> pending{{a, b, 0, 1, 0, 1, 0}};
    size_t work = 0;
    while(!pending.empty()) {
        Check(++work <= 65536, entity, true,
              "Cannot isolate a boundary/patch-seam intersection within the native work limit.");
        auto pair = pending.back(); pending.pop_back();
        Box abox(pair.a), bbox(pair.b);
        if(!abox.Overlaps(bbox)) continue;
        if((abox.Size() < LENGTH_EPS * 0.01 && bbox.Size() < LENGTH_EPS * 0.01) || pair.depth >= 64) {
            double ta = (pair.a0 + pair.a1) * 0.5, tb = (pair.b0 + pair.b1) * 0.5;
            for(int i = 0; i < 20; ++i) {
                Vector delta = b.PointAt(tb).Minus(a.PointAt(ta));
                Vector da = a.TangentAt(ta), db = b.TangentAt(tb);
                double aa = da.MagSquared(), bb = db.MagSquared(), ab = da.Dot(db);
                double det = aa * bb - ab * ab;
                if(det <= 1e-16 * aa * bb) break;
                double ca = delta.Dot(da), cb = delta.Dot(db);
                double dta = (bb * ca - ab * cb) / det;
                double dtb = (ab * ca - aa * cb) / det;
                ta = std::max(0.0, std::min(1.0, ta + dta));
                tb = std::max(0.0, std::min(1.0, tb + dtb));
                if(std::abs(dta) + std::abs(dtb) < 1e-14) break;
            }
            Check(a.PointAt(ta).Equals(b.PointAt(tb), LENGTH_EPS * 0.05), entity, true,
                  "A boundary/patch-seam intersection is numerically ambiguous; no trim was guessed.");
            add(ta, tb);
            continue;
        }
        SBezier before, after;
        if(abox.Size() >= bbox.Size()) {
            pair.a.SplitAt(0.5, &before, &after);
            double m = (pair.a0 + pair.a1) * 0.5;
            pending.push_back({before, pair.b, pair.a0, m, pair.b0, pair.b1, pair.depth + 1});
            pending.push_back({after, pair.b, m, pair.a1, pair.b0, pair.b1, pair.depth + 1});
        } else {
            pair.b.SplitAt(0.5, &before, &after);
            double m = (pair.b0 + pair.b1) * 0.5;
            pending.push_back({pair.a, before, pair.a0, pair.a1, pair.b0, m, pair.depth + 1});
            pending.push_back({pair.a, after, pair.a0, pair.a1, m, pair.b1, pair.depth + 1});
        }
    }
    return result;
}

} // namespace Nurbs
} // namespace Step
} // namespace SolveSpace
