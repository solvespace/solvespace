#include "solvespace.h"

void SolveSpace::UndoRemember(void) {
    unsaved = true;
    PushFromCurrentOnto(&undo);
    UndoClearStack(&redo);
    UndoEnableMenus();
}

void SolveSpace::UndoUndo(void) {
    if(undo.cnt <= 0) return;

    PushFromCurrentOnto(&redo);
    PopOntoCurrentFrom(&undo);
    UndoEnableMenus();
}

void SolveSpace::UndoRedo(void) {
    if(redo.cnt <= 0) return;

    PushFromCurrentOnto(&undo);
    PopOntoCurrentFrom(&redo);
    UndoEnableMenus();
}

void SolveSpace::UndoEnableMenus(void) {
    EnableMenuById(GraphicsWindow::MNU_UNDO, undo.cnt > 0);
    EnableMenuById(GraphicsWindow::MNU_REDO, redo.cnt > 0);
}

void SolveSpace::PushFromCurrentOnto(UndoStack *uk) {
    int i;

    if(uk->cnt == MAX_UNDO) {
        UndoClearState(&(uk->d[uk->write]));
        // And then write in to this one again
    } else {
        (uk->cnt)++;
    }

    UndoState *ut = &(uk->d[uk->write]);
    ZERO(ut);
    for(i = 0; i < SK.group.n; i++) {
        Group *src = &(SK.group.elem[i]);
        Group dest = *src;
        // And then clean up all the stuff that needs to be a deep copy,
        // and zero out all the dynamic stuff that will get regenerated.
        dest.clean = false;
        ZERO(&(dest.solved));
        ZERO(&(dest.poly));
        ZERO(&(dest.bezierLoopSet));
        ZERO(&(dest.polyError));
        ZERO(&(dest.runningMesh));
        ZERO(&(dest.thisShell));
        ZERO(&(dest.runningShell));
        ZERO(&(dest.runningEdges));

        ZERO(&(dest.remap));
        src->remap.DeepCopyInto(&(dest.remap));

        ZERO(&(dest.impMesh));
        ZERO(&(dest.impEntity));
        ut->group.Add(&dest);
    }
    for(i = 0; i < SK.request.n; i++) {
        ut->request.Add(&(SK.request.elem[i]));
    }
    for(i = 0; i < SK.constraint.n; i++) {
        Constraint *src = &(SK.constraint.elem[i]);
        Constraint dest = *src;
        ZERO(&(dest.dogd));
        ut->constraint.Add(&dest);
    }
    for(i = 0; i < SK.param.n; i++) {
        ut->param.Add(&(SK.param.elem[i]));
    }
    ut->activeGroup = SS.GW.activeGroup;

    uk->write = WRAP(uk->write + 1, MAX_UNDO);
}

void SolveSpace::PopOntoCurrentFrom(UndoStack *uk) {
    if(uk->cnt <= 0) oops();
    (uk->cnt)--;
    uk->write = WRAP(uk->write - 1, MAX_UNDO);

    UndoState *ut = &(uk->d[uk->write]);

    int i;
    // Free everything in the main copy of the program before replacing it
    for(i = 0; i < SK.group.n; i++) {
        Group *g = &(SK.group.elem[i]);
        g->poly.Clear();
        g->bezierLoopSet.Clear();
        g->runningMesh.Clear();
        g->thisShell.Clear();
        g->runningShell.Clear();
        g->runningEdges.Clear();
        g->remap.Clear();
        g->impMesh.Clear();
        g->impEntity.Clear();
    }
    SK.group.Clear();
    SK.request.Clear();
    SK.constraint.Clear();
    SK.param.Clear();

    // And then do a shallow copy of the state from the undo list
    ut->group.MoveSelfInto(&(SK.group));
    ut->request.MoveSelfInto(&(SK.request));
    ut->constraint.MoveSelfInto(&(SK.constraint));
    ut->param.MoveSelfInto(&(SK.param));
    SS.GW.activeGroup = ut->activeGroup;

    // No need to free it, since a shallow copy was made above
    ZERO(ut);

    // And reset the state everywhere else in the program, since the
    // sketch just changed a lot.
    SS.GW.ClearSuper();
    SS.TW.ClearSuper();
    SS.ReloadAllImported();
    SS.GenerateAll(0, INT_MAX);
    later.showTW = true;
}

void SolveSpace::UndoClearStack(UndoStack *uk) {
    while(uk->cnt > 0) {
        uk->write = WRAP(uk->write - 1, MAX_UNDO);
        (uk->cnt)--;
        UndoClearState(&(uk->d[uk->write]));
    }
    ZERO(uk); // for good measure
}

void SolveSpace::UndoClearState(UndoState *ut) {
    int i;
    for(i = 0; i < ut->group.n; i++) {
        Group *g = &(ut->group.elem[i]);

        g->remap.Clear();
    }
    ut->group.Clear();
    ut->request.Clear();
    ut->constraint.Clear();
    ut->param.Clear();
    ZERO(ut);
}

