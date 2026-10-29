#include "solvespace.h"

#include "harness.h"

// A sketch can be generated on its own, while SK holds a different sketch:
// nothing that generating it uses may look in SK. solid.slvs has extrusions
// and Booleans; points.slvs has two points and a distance.

// A separate copy of what is in SK, made the way undo copies it: the parts
// of the groups that generating makes again aren't copied.
static Sketch *CopyOfSK() {
    Sketch *s = new Sketch();
    for(Group &src : SK.group) {
        Group dest(src);
        dest.clean = false;
        dest.solved = {};
        dest.polyLoops = {};
        dest.bezierLoops = {};
        dest.bezierOpens = {};
        dest.polyError = {};
        dest.thisMesh = {};
        dest.runningMesh = {};
        dest.thisShell = {};
        dest.runningShell = {};
        dest.displayMesh = {};
        dest.displayOutlines = {};
        dest.impMesh = {};
        dest.impShell = {};
        dest.impEntity = {};
        s->group.Add(&dest);
    }
    for(Request &r : SK.request) s->request.Add(&r);
    for(Constraint &c : SK.constraint) {
        Constraint dest(c);
        s->constraint.Add(&dest);
    }
    for(Param &p : SK.param) s->param.Add(&p);
    for(Style &st : SK.style) s->style.Add(&st);
    s->RegenerateGroupOrder();
    return s;
}

// Solve every group of the sketch, and generate its shells and meshes.
static void GenerateSketch(Sketch *s) {
    Sketch::GenerateOptions opts = {};
    opts.first = 0;
    opts.last = INT_MAX;
    opts.timeoutRedundantConstr = SS.timeoutRedundantConstr;
    opts.sys = &SS.sys;
    Sketch::GenerateResult result;
    s->GenerateEntities(opts, &result);
    s->GenerateShellsAndMeshes(result.first, result.last, s->ChordTolForSize(SS.chordTol));
}

// What generating a sketch made, to compare.
struct Generated {
    int                  entities;
    std::vector<hEntity> points;
    std::vector<Vector>  positions;
    std::vector<int>     dof;
    std::vector<int>     triangles;
    std::vector<int>     surfaces;
};

static Generated What(Sketch *s) {
    Generated g = {};
    g.entities = s->entity.n;
    for(Entity &e : s->entity) {
        if(!e.IsPoint()) continue;
        g.points.push_back(e.h);
        g.positions.push_back(e.PointGetNum());
    }
    for(hGroup hg : s->groupOrder) {
        Group *grp = s->GetGroup(hg);
        g.dof.push_back(grp->solved.dof);
        g.triangles.push_back(grp->runningMesh.l.n);
        g.surfaces.push_back(grp->runningShell.surface.n);
    }
    return g;
}

static bool Same(const Generated &a, const Generated &b) {
    if(a.entities != b.entities || a.points != b.points || a.dof != b.dof ||
       a.triangles != b.triangles || a.surfaces != b.surfaces) {
        return false;
    }
    for(size_t i = 0; i < a.positions.size(); i++) {
        if(!a.positions[i].Equals(b.positions[i])) return false;
    }
    return true;
}

// Generate the sketch in the fixture in SK, as usual, then load a different
// sketch into SK, and generate a copy of the first on its own. It must come
// out the same, and SK must be left as it was.
static bool GeneratesAlone(Test::Helper *helper, const char *file, const char *fixture) {
    if(!helper->CheckLoad(file, __LINE__, fixture)) return false;
    Generated inSK = What(&SK);
    Sketch *copy = CopyOfSK();

    if(!helper->CheckLoad(file, __LINE__, "points.slvs")) {
        copy->Clear();
        delete copy;
        return false;
    }
    Generated pointsBefore = What(&SK);
    double chordTolBefore = SS.chordTolCalculated;
    GenerateSketch(copy);

    bool ok = Same(What(copy), inSK);
    for(hGroup hg : copy->groupOrder) {
        ok = ok && copy->GetGroup(hg)->IsSolvedOkay();
    }
    for(Entity &e : copy->entity) {
        ok = ok && (e.Owner() == copy);
    }
    ok = ok && Same(What(&SK), pointsBefore);
    ok = ok && (SS.chordTolCalculated == chordTolBefore);
    for(Entity &e : SK.entity) {
        ok = ok && (e.Owner() == &SK);
    }

    copy->Clear();
    delete copy;
    return ok;
}

TEST_CASE(generate_extrusions_and_booleans_alone) {
    CHECK_TRUE(GeneratesAlone(helper, __FILE__, "solid.slvs"));
}

TEST_CASE(generate_step_and_repeat_alone) {
    CHECK_TRUE(GeneratesAlone(helper, __FILE__, "repeat.slvs"));
}
