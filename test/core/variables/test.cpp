#include "solvespace.h"

#include "harness.h"

// distance.slvs has two datum points in group 2, with a 10 mm distance
// between them (constraint 1).
static const hGroup      HGROUP = { 2 };
static const hConstraint HDIST  = { 1 };

// Add a named parameter to group 2, regenerate so that it exists, and give
// it a value. Returns its request.
static hRequest AddParam(const char *name, double value, bool locked) {
    Request r = {};
    r.group = HGROUP;
    r.type = locked ? Request::Type::NAMED_CONST_PARAM : Request::Type::NAMED_PARAMETER;
    r.str = name;
    SK.request.AddAndAssignId(&r);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetParam(r.NamedParam())->val = value;
    return r.h;
}

// Set the distance to an expression, typed in mm, and regenerate.
static void SetExpression(const char *expression) {
    Constraint *c = SK.GetConstraint(HDIST);
    c->expression = expression;
    c->exprScale = 1.0;
    SS.MarkGroupDirty(HGROUP);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
}

static double MeasuredDistance() {
    Constraint *c = SK.GetConstraint(HDIST);
    Vector a = SK.GetEntity(c->ptA)->PointGetNum();
    Vector b = SK.GetEntity(c->ptB)->PointGetNum();
    return a.Minus(b).Magnitude();
}

TEST_CASE(parse_names) {
    hParam hw = { 0x00100040 };
    ParamNames names;
    names["w"] = hw;
    names["Width"] = hw;

    std::string error;
    int used = 0;
    Expr *e = Expr::Parse("w*2 + Width", &error, &names, &used);
    CHECK_TRUE(e != NULL);
    CHECK_TRUE(used == 2);

    // The functions and constants still work alongside names.
    e = Expr::Parse("sqrt(4) + pi", &error, &names, &used);
    CHECK_TRUE(e != NULL);
    CHECK_TRUE(used == 0);

    // Unknown names are errors, capitalized or not.
    CHECK_TRUE(Expr::Parse("x", &error, &names) == NULL);
    CHECK_TRUE(Expr::Parse("Height", &error, &names) == NULL);
    CHECK_TRUE(Expr::IsReservedName("sqrt"));
    CHECK_FALSE(Expr::IsReservedName("w"));
}

TEST_CASE(new_param_has_default_value) {
    CHECK_LOAD("distance.slvs");
    Request r = {};
    r.group = HGROUP;
    r.type = Request::Type::NAMED_PARAMETER;
    r.str = "w";
    SK.request.AddAndAssignId(&r);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(SK.GetParam(r.NamedParam())->val, Request::NAMED_PARAM_DEFAULT);

    // Once it has a value, regenerating keeps it.
    SK.GetParam(r.NamedParam())->val = 2.0;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(SK.GetParam(r.NamedParam())->val, 2.0);
}

TEST_CASE(locked_param_sets_dimension) {
    CHECK_LOAD("distance.slvs");
    hRequest hr = AddParam("w", 7.0, /*locked=*/true);
    SetExpression("w*2");

    Constraint *c = SK.GetConstraint(HDIST);
    CHECK_TRUE(SK.GetGroup(HGROUP)->solved.how == SolveResult::OKAY);
    CHECK_EQ_EPS(MeasuredDistance(), 14.0);
    // valA follows the expression, so everything that reads it sees 14.
    CHECK_EQ_EPS(c->valA, 14.0);
    // The solver must not change a locked parameter, and regenerating must
    // not lose its value.
    CHECK_EQ_EPS(SK.GetParam(SK.GetRequest(hr)->NamedParam())->val, 7.0);
}

TEST_CASE(unknown_name_keeps_constraint) {
    CHECK_LOAD("distance.slvs");
    hRequest hr = AddParam("w", 7.0, /*locked=*/true);
    SetExpression("w*2");

    // Deleting the parameter leaves the dimension using an unknown name. It
    // is reported, but the constraint and its expression are kept.
    SK.request.RemoveById(hr);
    SS.MarkGroupDirty(HGROUP);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);

    Group *g = SK.GetGroup(HGROUP);
    CHECK_TRUE(g->solved.how == SolveResult::UNKNOWN_NAME);
    CHECK_TRUE(g->solved.remove.n == 1);
    CHECK_TRUE(g->solved.remove[0] == HDIST);
    Constraint *c = SK.constraint.FindByIdNoOops(HDIST);
    CHECK_TRUE(c != NULL);
    CHECK_EQ_STR(c->expression, "w*2");
    CHECK_EQ_EPS(c->valA, 14.0);
}

TEST_CASE(reference_measures_and_keeps_expression) {
    CHECK_LOAD("distance.slvs");
    hRequest hr = AddParam("w", 7.0, /*locked=*/true);
    SetExpression("w*2");

    // As a reference, the dimension shows what it measures, and nothing
    // moves when the parameter changes.
    SK.GetConstraint(HDIST)->reference = true;
    SK.GetParam(SK.GetRequest(hr)->NamedParam())->val = 5.0;
    SS.MarkGroupDirty(HGROUP);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Constraint *c = SK.GetConstraint(HDIST);
    CHECK_EQ_EPS(c->valA, 14.0);
    CHECK_EQ_EPS(c->exprScale, 1.0);
    CHECK_EQ_STR(c->expression, "w*2");

    // Back to a driving dimension, the expression applies again.
    c->reference = false;
    SS.MarkGroupDirty(HGROUP);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(SK.GetConstraint(HDIST)->valA, 10.0);
    CHECK_EQ_EPS(MeasuredDistance(), 10.0);
}

TEST_CASE(later_definition_replaces_earlier) {
    CHECK_LOAD("distance.slvs");
    // A name defined in the references group is visible in group 2, until
    // group 2 defines it again.
    Request r = {};
    r.group = Group::HGROUP_REFERENCES;
    r.type = Request::Type::NAMED_CONST_PARAM;
    r.str = "w";
    SK.request.AddAndAssignId(&r);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetParam(r.NamedParam())->val = 3.0;
    SetExpression("w*2");
    CHECK_EQ_EPS(MeasuredDistance(), 6.0);

    AddParam("w", 4.0, /*locked=*/true);
    SS.MarkGroupDirty(HGROUP);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(MeasuredDistance(), 8.0);
}

TEST_CASE(save_and_load_expression) {
    CHECK_LOAD("distance.slvs");
    AddParam("w", 7.0, /*locked=*/true);
    SetExpression("w*2");

    Platform::Path path = helper->GetAssetPath(__FILE__, "expression.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    SK.GetConstraint(HDIST)->expression.clear();
    CHECK_TRUE(SS.LoadFromFile(path));
    Platform::RemoveFile(path);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);

    Constraint *c = SK.GetConstraint(HDIST);
    CHECK_EQ_STR(c->expression, "w*2");
    CHECK_EQ_EPS(c->exprScale, 1.0);
    CHECK_EQ_EPS(c->valA, 14.0);
    CHECK_EQ_EPS(MeasuredDistance(), 14.0);
}

// Add a locked parameter to the references group, with a value.
static hRequest AddReferenceParam(const char *name, double value) {
    Request r = {};
    r.group = Group::HGROUP_REFERENCES;
    r.type = Request::Type::NAMED_CONST_PARAM;
    r.str = name;
    SK.request.AddAndAssignId(&r);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetParam(r.NamedParam())->val = value;
    return r.h;
}

// Give a parameter an expression, and regenerate.
static void SetParamExpression(hRequest hr, const char *expression) {
    SK.GetRequest(hr)->expression = expression;
    SS.MarkGroupDirty(HGROUP);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
}

TEST_CASE(param_expression_follows_earlier_group) {
    CHECK_LOAD("distance.slvs");
    hRequest ha = AddReferenceParam("a", 3.0);
    hRequest hw = AddParam("w", 1.0, /*locked=*/true);
    SetParamExpression(hw, "a*2");
    SetExpression("w");

    CHECK_EQ_EPS(SK.GetParam(SK.GetRequest(hw)->NamedParam())->val, 6.0);
    CHECK_EQ_EPS(MeasuredDistance(), 6.0);

    // Changing the earlier parameter changes this one, and the dimension.
    SK.GetParam(SK.GetRequest(ha)->NamedParam())->val = 4.0;
    SS.MarkGroupDirty(Group::HGROUP_REFERENCES);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(SK.GetParam(SK.GetRequest(hw)->NamedParam())->val, 8.0);
    CHECK_EQ_EPS(MeasuredDistance(), 8.0);
}

TEST_CASE(param_expression_cant_use_same_group) {
    CHECK_LOAD("distance.slvs");
    AddParam("b", 3.0, /*locked=*/true);
    hRequest hw = AddParam("w", 7.0, /*locked=*/true);
    SetParamExpression(hw, "b*2");

    // b is in the same group, so it isn't defined for the expression; the
    // group fails, and the parameter keeps its value.
    CHECK_TRUE(SK.GetGroup(HGROUP)->solved.how == SolveResult::UNKNOWN_NAME);
    CHECK_EQ_EPS(SK.GetParam(SK.GetRequest(hw)->NamedParam())->val, 7.0);
}

TEST_CASE(param_expression_unknown_name) {
    CHECK_LOAD("distance.slvs");
    hRequest ha = AddReferenceParam("a", 3.0);
    hRequest hw = AddParam("w", 1.0, /*locked=*/true);
    SetParamExpression(hw, "a*2");
    CHECK_TRUE(SK.GetGroup(HGROUP)->solved.how == SolveResult::OKAY);

    SK.request.RemoveById(ha);
    SS.MarkGroupDirty(Group::HGROUP_REFERENCES);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(SK.GetGroup(HGROUP)->solved.how == SolveResult::UNKNOWN_NAME);
    CHECK_EQ_EPS(SK.GetParam(SK.GetRequest(hw)->NamedParam())->val, 6.0);
    CHECK_EQ_STR(SK.GetRequest(hw)->expression, "a*2");
}

TEST_CASE(save_and_load_param_expression) {
    CHECK_LOAD("distance.slvs");
    AddReferenceParam("a", 3.0);
    hRequest hw = AddParam("w", 1.0, /*locked=*/true);
    SetParamExpression(hw, "a*2");

    Platform::Path path = helper->GetAssetPath(__FILE__, "param_expression.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    Platform::RemoveFile(path);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);

    CHECK_EQ_STR(SK.GetRequest(hw)->expression, "a*2");
    CHECK_EQ_EPS(SK.GetParam(SK.GetRequest(hw)->NamedParam())->val, 6.0);
}

TEST_CASE(unknown_name_reported_once) {
    CHECK_LOAD("distance.slvs");
    hRequest ha = AddReferenceParam("width", 3.0);
    SetExpression("width");

    // Deleting the name fails the group, and shows why.
    SK.request.RemoveById(ha);
    SS.MarkGroupDirty(Group::HGROUP_REFERENCES);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(SK.GetGroup(HGROUP)->solved.how == SolveResult::UNKNOWN_NAME);
    CHECK_TRUE(SS.TW.shown.screen == TextWindow::Screen::GROUP_SOLVE_INFO);

    // Adding a parameter back, from the references group's page, leaves the
    // user on that page, although the group still fails until it's named.
    SS.TW.shown.screen = TextWindow::Screen::GROUP_INFO;
    SS.TW.shown.group = Group::HGROUP_REFERENCES;
    hRequest hb = AddReferenceParam("param1", 5.0);
    CHECK_TRUE(SK.GetGroup(HGROUP)->solved.how == SolveResult::UNKNOWN_NAME);
    CHECK_TRUE(SS.TW.shown.screen == TextWindow::Screen::GROUP_INFO);
    CHECK_TRUE(SS.TW.shown.group == Group::HGROUP_REFERENCES);

    // Naming it fixes the group.
    SK.GetRequest(hb)->str = "width";
    SS.MarkGroupDirty(Group::HGROUP_REFERENCES);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(SK.GetGroup(HGROUP)->solved.how == SolveResult::OKAY);
    CHECK_EQ_EPS(MeasuredDistance(), 5.0);
}
