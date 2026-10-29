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

// A linked .slvs file is loaded as a sketch of its own, and generated.
// stale.slvs says that its two points are 20 apart, but was saved with them
// 10 apart, and with the second point hidden.
TEST_CASE(linked_file_is_generated) {
    Platform::Path path = helper->GetAssetPath(__FILE__, "stale.slvs");
    EntityList le = {};
    SMesh m = {};
    SShell sh = {};
    CHECK_TRUE(SS.LoadEntitiesFromFile(path, &le, &m, &sh));

    hEntity ha = { 0x00040000 }, hb = { 0x00050000 };
    Entity *a = le.FindByIdNoOops(ha);
    Entity *b = le.FindByIdNoOops(hb);
    CHECK_TRUE(a != NULL && b != NULL);
    CHECK_EQ_EPS(a->actPoint.Minus(b->actPoint).Magnitude(), 20.0);
    // Whether each is visible is as saved, and they belong to no sketch.
    CHECK_TRUE(a->actVisible);
    CHECK_FALSE(b->actVisible);
    CHECK_TRUE(a->sketch == NULL);

    le.Clear();
    m.Clear();
    sh.Clear();
}

// self.slvs has a linked group that links self.slvs. Generating that would
// never end, so the inner link uses what was saved in the file instead.
TEST_CASE(linked_file_that_links_itself) {
    Platform::Path path = helper->GetAssetPath(__FILE__, "self.slvs");
    EntityList le = {};
    SMesh m = {};
    SShell sh = {};
    CHECK_TRUE(SS.LoadEntitiesFromFile(path, &le, &m, &sh));
    CHECK_TRUE(le.n > 0);
    CHECK_TRUE(SS.linkedFilesLoading.empty());

    le.Clear();
    m.Clear();
    sh.Clear();
}

// The distance between the two points of a linked copy of stale.slvs.
static double LinkedDistance(const Platform::Path &path) {
    EntityList le = {};
    SMesh m = {};
    SShell sh = {};
    double d = -1;
    if(SS.LoadEntitiesFromFile(path, &le, &m, &sh)) {
        hEntity ha = { 0x00040000 }, hb = { 0x00050000 };
        Entity *a = le.FindByIdNoOops(ha);
        Entity *b = le.FindByIdNoOops(hb);
        if(a != NULL && b != NULL) d = a->actPoint.Minus(b->actPoint).Magnitude();
    }
    le.Clear();
    m.Clear();
    sh.Clear();
    return d;
}

// A generated linked sketch is kept, and used again until its file changes.
TEST_CASE(linked_file_is_cached) {
    std::string data;
    CHECK_TRUE(ReadFile(helper->GetAssetPath(__FILE__, "stale.slvs"), &data));
    Platform::Path path = helper->GetAssetPath(__FILE__, "cached.slvs", "out");
    CHECK_TRUE(WriteFile(path, data));

    CHECK_EQ_EPS(LinkedDistance(path), 20.0);
    CHECK_TRUE(SS.linkedSketches.count(path.raw) == 1);
    CHECK_EQ_EPS(LinkedDistance(path), 20.0);

    // Change the distance in the file; the sketch is generated again.
    size_t at = data.find("Constraint.valA=20.");
    CHECK_TRUE(at != std::string::npos);
    data.replace(at, strlen("Constraint.valA=20."), "Constraint.valA=30.");
    CHECK_TRUE(WriteFile(path, data));
    CHECK_EQ_EPS(LinkedDistance(path), 30.0);

    RemoveFile(path);
    SS.ClearLinkedSketches();
}

// part.slvs, written by the test, is points.slvs with a parameter d in its
// references group, which is the distance between its two points.
// assembly.slvs links part.slvs, in group 3.
static const hGroup HLINKED = { 3 };

static bool WritePart(Test::Helper *helper, const char *file, Platform::Path *path) {
    if(!helper->CheckLoad(file, __LINE__, "points.slvs")) return false;
    Request r = {};
    r.group = Group::HGROUP_REFERENCES;
    r.type = Request::Type::NAMED_CONST_PARAM;
    r.str = "d";
    SK.request.AddAndAssignId(&r);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetParam(r.NamedParam())->val = 10.0;
    hConstraint hc = { 1 };
    Constraint *c = SK.GetConstraint(hc);
    c->expression = "d";
    c->exprScale = 1.0;
    SS.MarkGroupDirty(c->group);
    *path = helper->GetAssetPath(file, "part.slvs");
    return SS.SaveToFile(*path);
}

// The distance between the two points that the linked group gets.
static double LinkedGroupDistance() {
    Group *g = SK.GetGroup(HLINKED);
    hEntity ha = { 0x00040000 }, hb = { 0x00050000 };
    Entity *a = g->impEntity.FindByIdNoOops(ha);
    Entity *b = g->impEntity.FindByIdNoOops(hb);
    if(a == NULL || b == NULL) return -1;
    return a->actPoint.Minus(b->actPoint).Magnitude();
}

static hRequest AddRequest(hGroup hg, Request::Type type, const char *name, double value) {
    Request r = {};
    r.group = hg;
    r.type = type;
    r.str = name;
    SK.request.AddAndAssignId(&r);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetParam(r.NamedParam())->val = value;
    SS.MarkGroupDirty(hg);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return r.h;
}

TEST_CASE(linked_parameters) {
    Platform::Path part;
    CHECK_TRUE(WritePart(helper, __FILE__, &part));
    CHECK_LOAD("assembly.slvs");
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);

    // The linked group knows the linked sketch's parameters, with its values.
    Group *g = SK.GetGroup(HLINKED);
    CHECK_TRUE(g->linkParams.size() == 1);
    CHECK_EQ_STR(g->linkParams[0].name, "d");
    CHECK_EQ_EPS(g->linkParams[0].value, 10.0);
    CHECK_EQ_EPS(LinkedGroupDistance(), 10.0);

    // Giving d a value generates the linked sketch with it.
    hRequest hd = AddRequest(HLINKED, Request::Type::NAMED_LINK_PARAM, "d", 30.0);
    CHECK_EQ_EPS(LinkedGroupDistance(), 30.0);
    CHECK_TRUE(SK.GetGroup(HLINKED)->IsSolvedOkay());

    // The value can be an expression, using the parameters of earlier groups.
    hRequest hx = AddRequest(Group::HGROUP_REFERENCES, Request::Type::NAMED_CONST_PARAM,
                             "x", 7.0);
    SK.GetRequest(hd)->expression = "x*2";
    SS.MarkGroupDirty(HLINKED);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(LinkedGroupDistance(), 14.0);

    // When x changes, as it would when dragged, the linked sketch is
    // generated again; and only what is used is kept.
    SK.GetParam(SK.GetRequest(hx)->NamedParam())->val = 9.0;
    SS.MarkGroupDirty(Group::HGROUP_REFERENCES);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(LinkedGroupDistance(), 18.0);
    CHECK_TRUE(SS.linkedSketches.size() == 1);
    CHECK_TRUE(SS.linkedSketchFiles.size() == 1);

    // A value for a parameter that the linked sketch doesn't have is reported.
    AddRequest(HLINKED, Request::Type::NAMED_LINK_PARAM, "q", 1.0);
    CHECK_TRUE(SK.GetGroup(HLINKED)->solved.how == SolveResult::UNKNOWN_NAME);

    RemoveFile(part);
}
