//-----------------------------------------------------------------------------
// The user-visible undo/redo operation; whenever they change something, we
// record our state and push it on a stack, and we pop the stack when they
// select undo.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

void SolveSpaceUI::UndoRemember() {
    unsaved = true;
    PushFromCurrentOnto(&undo);
    UndoClearStack(&redo);
    UndoEnableMenus();
}

void SolveSpaceUI::UndoUndo() {
    if(undo.cnt <= 0) return;

    PushFromCurrentOnto(&redo);
    PopOntoCurrentFrom(&undo);
    UndoEnableMenus();
}

void SolveSpaceUI::UndoRedo() {
    if(redo.cnt <= 0) return;

    PushFromCurrentOnto(&undo);
    PopOntoCurrentFrom(&redo);
    UndoEnableMenus();
}

void SolveSpaceUI::UndoEnableMenus() {
    SS.GW.undoMenuItem->SetEnabled(undo.cnt > 0);
    SS.GW.redoMenuItem->SetEnabled(redo.cnt > 0);
}

void SolveSpaceUI::PushFromCurrentOnto(UndoStack *uk) {
    if(uk->cnt == MAX_UNDO) {
        UndoClearState(&(uk->d[uk->write]));
        // And then write in to this one again
    } else {
        (uk->cnt)++;
    }

    UndoState *ut = &(uk->d[uk->write]);
    *ut = {};
    ut->group.ReserveMore(SK.group.n);
    for(Group &src : SK.group) {
        // Shallow copy
        Group dest(src);
        // And then clean up all the stuff that needs to be a deep copy,
        // and zero out all the dynamic stuff that will get regenerated.
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

        dest.remap = src.remap;

        dest.impMesh = {};
        dest.impShell = {};
        dest.impEntity = {};
        ut->group.Add(&dest);
    }
    for(auto &src : SK.groupOrder) { ut->groupOrder.Add(&src); }
    ut->request.ReserveMore(SK.request.n);
    for(auto &src : SK.request) { ut->request.Add(&src); }
    ut->constraint.ReserveMore(SK.constraint.n);
    for(auto &src : SK.constraint) {
        // Shallow copy
        Constraint dest(src);
        ut->constraint.Add(&dest);
    }
    ut->param.ReserveMore(SK.param.n);
    for(auto &src : SK.param) { ut->param.Add(&src); }
    ut->style.ReserveMore(SK.style.n);
    for(auto &src : SK.style) { ut->style.Add(&src); }
    ut->activeGroup = SS.GW.activeGroup;

    uk->write = WRAP(uk->write + 1, MAX_UNDO);
}

void SolveSpaceUI::PopOntoCurrentFrom(UndoStack *uk) {
    ssassert(uk->cnt > 0, "Cannot pop from empty undo stack");
    (uk->cnt)--;
    uk->write = WRAP(uk->write - 1, MAX_UNDO);

    UndoState *ut = &(uk->d[uk->write]);

    // Free everything in the main copy of the program before replacing it
    for(hGroup hg : SK.groupOrder) {
        Group *g = SK.GetGroup(hg);
        g->Clear();
    }
    SK.group.Clear();
    SK.groupOrder.Clear();
    SK.request.Clear();
    SK.constraint.Clear();
    SK.param.Clear();
    SK.style.Clear();

    // And then do a shallow copy of the state from the undo list
    ut->group.MoveSelfInto(&(SK.group));
    for(auto &gh : ut->groupOrder) { SK.groupOrder.Add(&gh); }
    ut->request.MoveSelfInto(&(SK.request));
    ut->constraint.MoveSelfInto(&(SK.constraint));
    ut->param.MoveSelfInto(&(SK.param));
    ut->style.MoveSelfInto(&(SK.style));
    SS.GW.activeGroup = ut->activeGroup;

    // No need to free it, since a shallow copy was made above
    *ut = {};

    // And reset the state everywhere else in the program, since the
    // sketch just changed a lot.
    SS.GW.ClearSuper();
    SS.TW.ClearSuper();
    SS.ReloadAllLinked(SS.saveFile);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SS.ScheduleShowTW();

    // Activate the group that was active before.
    Group *activeGroup = SK.GetGroup(SS.GW.activeGroup);
    activeGroup->Activate();
}

void SolveSpaceUI::UndoClearStack(UndoStack *uk) {
    while(uk->cnt > 0) {
        uk->write = WRAP(uk->write - 1, MAX_UNDO);
        (uk->cnt)--;
        UndoClearState(&(uk->d[uk->write]));
    }
    *uk = {}; // for good measure
}

void SolveSpaceUI::UndoClearState(UndoState *ut) {
    for(auto &g : ut->group) { g.remap.clear(); }
    ut->group.Clear();
    ut->request.Clear();
    ut->constraint.Clear();
    ut->param.Clear();
    ut->style.Clear();
    *ut = {};
}

