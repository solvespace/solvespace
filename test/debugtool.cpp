//-----------------------------------------------------------------------------
// Our entry point for exposing various internal mechanisms.
//
// Copyright 2017 whitequark
//-----------------------------------------------------------------------------

#include "solvespace.h"
#include "expr.h"
#include "platform/platform.h"

using namespace SolveSpace;

static const char *SolveResultName(SolveResult r) {
    switch(r) {
        case SolveResult::OKAY:                     return "OKAY";
        case SolveResult::DIDNT_CONVERGE:           return "DIDNT_CONVERGE";
        case SolveResult::REDUNDANT_OKAY:           return "REDUNDANT_OKAY";
        case SolveResult::REDUNDANT_DIDNT_CONVERGE: return "REDUNDANT_DIDNT_CONVERGE";
        case SolveResult::TOO_MANY_UNKNOWNS:        return "TOO_MANY_UNKNOWNS";
    }
    return "?";
}

struct StepResult {
    SolveResult how;
    double      signedAngle;
};

// Set constraint `hc` to `value`, regenerate, and report how the group solved.
static StepResult SetAndSolve(uint32_t hcv, double value, bool verbose) {
    hConstraint hc = {hcv};
    Constraint *c = SK.GetConstraint(hc);
    hGroup hg = c->group;
    c->valA = value;
    SS.MarkGroupDirty(hg);
    // This is what the GUI does after an edit: MarkGroupDirty() schedules a
    // Generate::DIRTY pass, which is run from the event loop.
    if(getenv("SS_GENERATE_ALL")) {
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    } else {
        SS.GenerateAll();
    }

    Group *g = SK.GetGroup(hg);
    StepResult sr = { g->solved.how, 0.0 };

    // Report the geometry too, not just the result code: converging to a
    // mirrored or otherwise different solution is not a success.
    c = SK.constraint.FindByIdNoOops(hc);
    if(c && c->type == Constraint::Type::ANGLE) {
        Vector a = SK.GetEntity(c->entityA)->VectorGetNum();
        Vector b = SK.GetEntity(c->entityB)->VectorGetNum();
        if(c->other) a = a.ScaledBy(-1);
        a = a.ProjectVectorInto(c->workplane);
        b = b.ProjectVectorInto(c->workplane);
        Vector n = SK.GetEntity(c->workplane)->Normal()->NormalN();
        double dot = a.Dot(b) / (a.Magnitude() * b.Magnitude());
        double crs = a.Cross(b).Dot(n) / (a.Magnitude() * b.Magnitude());
        sr.signedAngle = atan2(crs, dot) * 180 / PI;
    }

    if(verbose) {
        fprintf(stderr, "  set c%u = %.10g -> %s (dof=%d, bad=%d, signed angle=%.9g)\n",
                hcv, value, SolveResultName(sr.how), g->solved.dof, g->solved.remove.n,
                sr.signedAngle);
        fprintf(stderr, "      requests=%d constraints=%d entities=%d params=%d\n",
                SK.request.n, SK.constraint.n, SK.entity.n, SK.param.n);
        for(int i = 0; i < g->solved.remove.n; i++) {
            Constraint *bc = SK.constraint.FindByIdNoOops(g->solved.remove[i]);
            fprintf(stderr, "      bad constraint %u type %d\n", g->solved.remove[i].v,
                    bc ? (int)bc->type : -1);
        }
        for(auto &e : SK.entity) {
            if(e.group != hg) continue;
            if(e.type != Entity::Type::POINT_IN_2D && e.type != Entity::Type::POINT_IN_3D)
                continue;
            Vector p = e.PointGetNum();
            fprintf(stderr, "      pt %08x (%.6f, %.6f, %.6f)\n", e.h.v, p.x, p.y, p.z);
        }
    }
    return sr;
}

static bool LoadSketch(const std::string &file) {
    SS.Init();
    SS.showToolbar = false;
    SS.checkClosedContour = false;
    if(!SS.LoadFromFile(Platform::Path::From(file))) {
        fprintf(stderr, "cannot load %s\n", file.c_str());
        return false;
    }
    SS.AfterNewFile();
    return true;
}

int main(int argc, char **argv) {
    std::vector<std::string> args = Platform::InitCli(argc, argv);

    if(args.size() == 3 && args[1] == "expr") {
        std::string expr = args[2], err;
        Expr *e = Expr::Parse(expr.c_str(), &err);
        if(e == NULL) {
            fprintf(stderr, "cannot parse: %s\n", err.c_str());
        } else {
            fprintf(stderr, "%g\n", e->Eval());
        }
        Platform::FreeAllTemporary();
    } else if(args.size() >= 5 && args[1] == "solve") {
        // solve <file.slvs> <constraint-handle> <value> [<value> ...]
        if(!LoadSketch(args[2])) return 1;
        uint32_t hcv = (uint32_t)strtoul(args[3].c_str(), NULL, 0);
        for(size_t i = 4; i < args.size(); i++) {
            SetAndSolve(hcv, strtod(args[i].c_str(), NULL), /*verbose=*/true);
        }
        Platform::FreeAllTemporary();
    } else if(args.size() == 8 && args[1] == "sweep") {
        // sweep <file.slvs> <constraint-handle> <start> <from> <to> <step>
        uint32_t hcv = (uint32_t)strtoul(args[3].c_str(), NULL, 0);
        double start = strtod(args[4].c_str(), NULL);
        double from  = strtod(args[5].c_str(), NULL);
        double to    = strtod(args[6].c_str(), NULL);
        double step  = strtod(args[7].c_str(), NULL);
        for(double target = from; target <= to + step/2; target += step) {
            if(!LoadSketch(args[2])) return 1;
            StepResult a = SetAndSolve(hcv, start, /*verbose=*/false);
            StepResult b = SetAndSolve(hcv, target, /*verbose=*/false);
            printf("%.6g %.6g %s %s %.9g\n", start, target,
                   SolveResultName(a.how), SolveResultName(b.how), b.signedAngle);
            fflush(stdout);
            SK.Clear();
            SS.Clear();
        }
        Platform::FreeAllTemporary();
    } else {
        fprintf(stderr, "Usage: %s <command> <options>\n", args[0].c_str());
//-----------------------------------------------------------------------------> 80 col */
        fprintf(stderr, R"(
Commands:
    expr [expr]
        Evaluate an expression.
    solve [file.slvs] [constraint-handle] [value]...
        Load a sketch, then repeatedly set a constraint's value and re-solve,
        reporting the solve result of each step.
    sweep [file.slvs] [constraint-handle] [start] [from] [to] [step]
        For each target value in [from, to], load the sketch, solve it at
        [start], then at the target, and report both solve results.
)");
    }

    return 0;
}
