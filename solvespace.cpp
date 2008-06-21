#include "solvespace.h"
#include <png.h>

SolveSpace SS;

void SolveSpace::Init(char *cmdLine) {
    int i;
    // Default list of colors for the model material
    modelColor[0] = CnfThawDWORD(RGB(150, 150, 150), "ModelColor_0");
    modelColor[1] = CnfThawDWORD(RGB(100, 100, 100), "ModelColor_1");
    modelColor[2] = CnfThawDWORD(RGB( 30,  30,  30), "ModelColor_2");
    modelColor[3] = CnfThawDWORD(RGB(150,   0,   0), "ModelColor_3");
    modelColor[4] = CnfThawDWORD(RGB(  0, 100,   0), "ModelColor_4");
    modelColor[5] = CnfThawDWORD(RGB(  0,  80,  80), "ModelColor_5");
    modelColor[6] = CnfThawDWORD(RGB(  0,   0, 130), "ModelColor_6");
    modelColor[7] = CnfThawDWORD(RGB( 80,   0,  80), "ModelColor_7");
    // Light intensities
    lightIntensity[0] = ((int)CnfThawDWORD( 700, "LightIntensity_0"))/1000.0;
    lightIntensity[1] = ((int)CnfThawDWORD( 400, "LightIntensity_1"))/1000.0;
    // Light positions
    lightDir[0].x = ((int)CnfThawDWORD(-500, "LightDir_0_Right"     ))/1000.0;
    lightDir[0].y = ((int)CnfThawDWORD( 500, "LightDir_0_Up"        ))/1000.0;
    lightDir[0].z = ((int)CnfThawDWORD(   0, "LightDir_0_Forward"   ))/1000.0;
    lightDir[1].x = ((int)CnfThawDWORD( 500, "LightDir_1_Right"     ))/1000.0;
    lightDir[1].y = ((int)CnfThawDWORD(   0, "LightDir_1_Up"        ))/1000.0;
    lightDir[1].z = ((int)CnfThawDWORD(   0, "LightDir_1_Forward"   ))/1000.0;
    // Mesh tolerance
    meshTol = ((int)CnfThawDWORD(1000, "MeshTolerance"))/1000.0;
    // View units
    viewUnits = (Unit)CnfThawDWORD((DWORD)UNIT_MM, "ViewUnits");
    // Camera tangent (determines perspective)
    cameraTangent = ((int)CnfThawDWORD(0, "CameraTangent"))/1e6;
    // Recent files menus
    for(i = 0; i < MAX_RECENT; i++) {
        char name[100];
        sprintf(name, "RecentFile_%d", i);
        strcpy(RecentFile[i], "");
        CnfThawString(RecentFile[i], MAX_PATH, name);
    }
    RefreshRecentMenus();

    // Start with either an empty file, or the file specified on the
    // command line.
    NewFile();
    AfterNewFile();
    if(strlen(cmdLine) != 0) {
        if(LoadFromFile(cmdLine)) {
            strcpy(saveFile, cmdLine);
        } else {
            NewFile();
        }
    }
    AfterNewFile();
}

void SolveSpace::Exit(void) {
    int i;
    char name[100];
    // Recent files
    for(i = 0; i < MAX_RECENT; i++) {
        sprintf(name, "RecentFile_%d", i);
        CnfFreezeString(RecentFile[i], name);
    }
    // Model colors
    for(i = 0; i < MODEL_COLORS; i++) {
        sprintf(name, "ModelColor_%d", i);
        CnfFreezeDWORD(modelColor[i], name);
    }
    // Light intensities
    CnfFreezeDWORD((int)(lightIntensity[0]*1000), "LightIntensity_0");
    CnfFreezeDWORD((int)(lightIntensity[1]*1000), "LightIntensity_1");
    // Light directions
    CnfFreezeDWORD((int)(lightDir[0].x*1000), "LightDir_0_Right");
    CnfFreezeDWORD((int)(lightDir[0].y*1000), "LightDir_0_Up");
    CnfFreezeDWORD((int)(lightDir[0].z*1000), "LightDir_0_Forward");
    CnfFreezeDWORD((int)(lightDir[1].x*1000), "LightDir_1_Right");
    CnfFreezeDWORD((int)(lightDir[1].y*1000), "LightDir_1_Up");
    CnfFreezeDWORD((int)(lightDir[1].z*1000), "LightDir_1_Forward");
    // Mesh tolerance
    CnfFreezeDWORD((int)(meshTol*1000), "MeshTolerance");
    // Display/entry units
    CnfFreezeDWORD((int)viewUnits, "ViewUnits");
    // Camera tangent (determines perspective)
    CnfFreezeDWORD((int)(cameraTangent*1e6), "CameraTangent");
    ExitNow();
}

void SolveSpace::DoLater(void) {
    if(later.generateAll) GenerateAll();
    if(later.showTW) TW.Show();
    ZERO(&later);
}

int SolveSpace::CircleSides(double r) {
    int s = 7 + (int)(sqrt(r*SS.GW.scale/meshTol));
    return min(s, 40);
}

char *SolveSpace::MmToString(double v) {
    static int WhichBuf;
    static char Bufs[8][128];

    WhichBuf++;
    if(WhichBuf >= 8 || WhichBuf < 0) WhichBuf = 0;

    char *s = Bufs[WhichBuf];
    if(viewUnits == UNIT_INCHES) {
        sprintf(s, "%.3f", v/25.4);
    } else {
        sprintf(s, "%.2f", v);
    }
    return s;
}
double SolveSpace::ExprToMm(Expr *e) {
    if(viewUnits == UNIT_INCHES) {
        return (e->Eval())*25.4;
    } else {
        return e->Eval();
    }
}


void SolveSpace::AfterNewFile(void) {
    ReloadAllImported();
    GenerateAll(-1, -1);

    TW.Init();
    GW.Init();

    unsaved = false;

    int w, h;
    GetGraphicsWindowSize(&w, &h);
    GW.width = w;
    GW.height = h;

    // The triangles haven't been generated yet, but zoom to fit the entities
    // roughly in the window, since that sets the mesh tolerance.
    GW.ZoomToFit();

    GenerateAll(0, INT_MAX);
    later.showTW = true;
    // Then zoom to fit again, to fit the triangles
    GW.ZoomToFit();
}

void SolveSpace::MarkGroupDirtyByEntity(hEntity he) {
    Entity *e = SS.GetEntity(he);
    MarkGroupDirty(e->group);
}

void SolveSpace::MarkGroupDirty(hGroup hg) {
    int i;
    bool go = false;
    for(i = 0; i < group.n; i++) {
        Group *g = &(group.elem[i]);
        if(g->h.v == hg.v) {
            go = true;
        }
        if(go) {
            g->clean = false;
        }
    }
    unsaved = true;
}

bool SolveSpace::PruneOrphans(void) {
    int i;
    for(i = 0; i < request.n; i++) {
        Request *r = &(request.elem[i]);
        if(GroupExists(r->group)) continue;

        (deleted.requests)++;
        request.RemoveById(r->h);
        return true;
    }

    for(i = 0; i < constraint.n; i++) {
        Constraint *c = &(constraint.elem[i]);
        if(GroupExists(c->group)) continue;

        (deleted.constraints)++;
        constraint.RemoveById(c->h);
        return true;
    }
    return false;
}

bool SolveSpace::GroupsInOrder(hGroup before, hGroup after) {
    if(before.v == 0) return true;
    if(after.v  == 0) return true;

    int beforep = -1, afterp = -1;
    int i;
    for(i = 0; i < group.n; i++) {
        Group *g = &(group.elem[i]);
        if(g->h.v == before.v) beforep = i;
        if(g->h.v == after.v)  afterp  = i;
    }
    if(beforep < 0 || afterp < 0) return false;
    if(beforep >= afterp) return false;
    return true;
}

bool SolveSpace::GroupExists(hGroup hg) {
    // A nonexistent group is not acceptable
    return group.FindByIdNoOops(hg) ? true : false;
}
bool SolveSpace::EntityExists(hEntity he) {
    // A nonexstient entity is acceptable, though, usually just means it
    // doesn't apply.
    if(he.v == Entity::NO_ENTITY.v) return true;
    return entity.FindByIdNoOops(he) ? true : false;
}

bool SolveSpace::PruneGroups(hGroup hg) {
    Group *g = GetGroup(hg);
    if(GroupsInOrder(g->opA, hg) &&
       EntityExists(g->predef.origin) &&
       EntityExists(g->predef.entityB) &&
       EntityExists(g->predef.entityC))
    {
        return false;
    }
    (deleted.groups)++;
    group.RemoveById(g->h);
    return true;
}

bool SolveSpace::PruneRequests(hGroup hg) {
    int i;
    for(i = 0; i < entity.n; i++) {
        Entity *e = &(entity.elem[i]);
        if(e->group.v != hg.v) continue;

        if(EntityExists(e->workplane)) continue;

        if(!e->h.isFromRequest()) oops();

        (deleted.requests)++;
        request.RemoveById(e->h.request());
        return true;
    }
    return false;
}

bool SolveSpace::PruneConstraints(hGroup hg) {
    int i;
    for(i = 0; i < constraint.n; i++) {
        Constraint *c = &(constraint.elem[i]);
        if(c->group.v != hg.v) continue;

        if(EntityExists(c->workplane) &&
           EntityExists(c->ptA) &&
           EntityExists(c->ptB) &&
           EntityExists(c->ptC) &&
           EntityExists(c->entityA) &&
           EntityExists(c->entityB))
        {
            continue;
        }

        (deleted.constraints)++;
        constraint.RemoveById(c->h);
        return true;
    }
    return false;
}

void SolveSpace::GenerateAll(void) {
    int i;
    int firstDirty = INT_MAX, lastVisible = 0;
    // Start from the first dirty group, and solve until the active group,
    // since all groups after the active group are hidden.
    for(i = 0; i < group.n; i++) {
        Group *g = &(group.elem[i]);
        if((!g->clean) || (g->solved.how != Group::SOLVED_OKAY)) {
            firstDirty = min(firstDirty, i);
        }
        if(g->h.v == SS.GW.activeGroup.v) {
            lastVisible = i;
        }
    }
    if(firstDirty == INT_MAX || lastVisible == 0) {
        // All clean; so just regenerate the entities, and don't solve anything.
        GenerateAll(-1, -1);
    } else {
        GenerateAll(firstDirty, lastVisible);
    }
}

void SolveSpace::GenerateAll(int first, int last) {
    int i, j;

    while(PruneOrphans())
        ;

    // Don't lose our numerical guesses when we regenerate.
    IdList<Param,hParam> prev;
    param.MoveSelfInto(&prev);
    entity.Clear();

    for(i = 0; i < group.n; i++) {
        Group *g = &(group.elem[i]);

        // The group may depend on entities or other groups, to define its
        // workplane geometry or for its operands. Those must already exist
        // in a previous group, so check them before generating.
        if(PruneGroups(g->h))
            goto pruned;

        for(j = 0; j < request.n; j++) {
            Request *r = &(request.elem[j]);
            if(r->group.v != g->h.v) continue;

            r->Generate(&entity, &param);
        }
        g->Generate(&entity, &param);

        // The requests and constraints depend on stuff in this or the
        // previous group, so check them after generating.
        if(PruneRequests(g->h) || PruneConstraints(g->h))
            goto pruned;

        // Use the previous values for params that we've seen before, as
        // initial guesses for the solver.
        for(j = 0; j < param.n; j++) {
            Param *newp = &(param.elem[j]);
            if(newp->known) continue;

            Param *prevp = prev.FindByIdNoOops(newp->h);
            if(prevp) newp->val = prevp->val;
        }

        if(g->h.v == Group::HGROUP_REFERENCES.v) {
            ForceReferences();
            g->solved.how = Group::SOLVED_OKAY;
            g->clean = true;
        } else {
            if(i >= first && i <= last) {
                // The group falls inside the range, so really solve it,
                // and then regenerate the mesh based on the solved stuff.
                SolveGroup(g->h);
                g->GeneratePolygon();
                g->GenerateMesh();
                g->clean = true;
            } else {
                // The group falls outside the range, so just assume that
                // it's good wherever we left it. The mesh is unchanged,
                // and the parameters must be marked as known.
                for(j = 0; j < param.n; j++) {
                    Param *newp = &(param.elem[j]);

                    Param *prevp = prev.FindByIdNoOops(newp->h);
                    if(prevp) newp->known = true;
                }
            }
        }
    }

    // And update any reference dimensions with their new values
    for(i = 0; i < constraint.n; i++) {
        Constraint *c = &(constraint.elem[i]);
        if(c->reference) {
            c->ModifyToSatisfy();
        }
    }

    prev.Clear();
    InvalidateGraphics();

    // Remove nonexistent selection items, for same reason we waited till
    // the end to put up a dialog box.
    GW.ClearNonexistentSelectionItems();

    if(deleted.requests > 0 || deleted.constraints > 0 || deleted.groups > 0) {
        // All sorts of interesting things could have happened; for example,
        // the active group or active workplane could have been deleted. So
        // clear all that out.
        if(deleted.groups > 0) {
            SS.TW.ClearSuper();
        }
        later.showTW = true;
        GW.ClearSuper();
        // Don't display any errors until we've regenerated fully. The
        // sketch is not necessarily in a consistent state until we've
        // pruned any orphaned etc. objects, and the message loop for the
        // messagebox could allow us to repaint and crash. But now we must
        // be fine.
        Error("Additional sketch elements were deleted, because they depend "
              "on the element that was just deleted explicitly. These "
              "include: \r\n"
              "     %d request%s\r\n"
              "     %d constraint%s\r\n"
              "     %d group%s\r\n\r\n"
              "Choose Edit -> Undo to undelete all elements.",
                deleted.requests, deleted.requests == 1 ? "" : "s",
                deleted.constraints, deleted.constraints == 1 ? "" : "s",
                deleted.groups, deleted.groups == 1 ? "" : "s");
        memset(&deleted, 0, sizeof(deleted));
    }
    return;

pruned:
    // Restore the numerical guesses
    param.Clear();
    prev.MoveSelfInto(&param);
    // Try again
    GenerateAll(first, last);
}

void SolveSpace::ForceReferences(void) {
    // Force the values of the paramters that define the three reference
    // coordinate systems.
    static const struct {
        hRequest    hr;
        Quaternion  q;
    } Quat[] = {
        { Request::HREQUEST_REFERENCE_XY, { 1,    0,    0,    0,   } },
        { Request::HREQUEST_REFERENCE_YZ, { 0.5,  0.5,  0.5,  0.5, } },
        { Request::HREQUEST_REFERENCE_ZX, { 0.5, -0.5, -0.5, -0.5, } },
    };
    for(int i = 0; i < 3; i++) {
        hRequest hr = Quat[i].hr;
        Entity *wrkpl = GetEntity(hr.entity(0));
        // The origin for our coordinate system, always zero
        Entity *origin = GetEntity(wrkpl->point[0]);
        origin->PointForceTo(Vector::From(0, 0, 0));
        GetParam(origin->param[0])->known = true;
        GetParam(origin->param[1])->known = true;
        GetParam(origin->param[2])->known = true;
        // The quaternion that defines the rotation, from the table.
        Entity *normal = GetEntity(wrkpl->normal); 
        normal->NormalForceTo(Quat[i].q);
        GetParam(normal->param[0])->known = true;
        GetParam(normal->param[1])->known = true;
        GetParam(normal->param[2])->known = true;
        GetParam(normal->param[3])->known = true;
    }
}

void SolveSpace::SolveGroup(hGroup hg) {
    int i;
    // Clear out the system to be solved.
    sys.entity.Clear();
    sys.param.Clear();
    sys.eq.Clear();
    // And generate all the params for requests in this group
    for(i = 0; i < request.n; i++) {
        Request *r = &(request.elem[i]);
        if(r->group.v != hg.v) continue;

        r->Generate(&(sys.entity), &(sys.param));
    }
    // And for the group itself
    Group *g = SS.GetGroup(hg);
    g->Generate(&(sys.entity), &(sys.param));
    // Set the initial guesses for all the params
    for(i = 0; i < sys.param.n; i++) {
        Param *p = &(sys.param.elem[i]);
        p->known = false;
        p->val = GetParam(p->h)->val;
    }

    sys.Solve(g);
    FreeAllTemporary();
}

void SolveSpace::ExportAsPngTo(char *filename) {
    int w = (int)SS.GW.width, h = (int)SS.GW.height;
    // No guarantee that the back buffer contains anything valid right now,
    // so repaint the scene.
    SS.GW.Paint(w, h);
    
    FILE *f = fopen(filename, "wb");
    if(!f) goto err;

    png_struct *png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
        NULL, NULL, NULL);
    if(!png_ptr) goto err;

    png_info *info_ptr = png_create_info_struct(png_ptr);
    if(!png_ptr) goto err;

    if(setjmp(png_jmpbuf(png_ptr))) goto err;

    png_init_io(png_ptr, f);

    // glReadPixels wants to align things on 4-boundaries, and there's 3
    // bytes per pixel. As long as the row width is divisible by 4, all
    // works out.
    w &= ~3; h &= ~3;

    png_set_IHDR(png_ptr, info_ptr, w, h,
        8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);
    
    png_write_info(png_ptr, info_ptr);

    // Get the pixel data from the framebuffer
    BYTE *pixels = (BYTE *)AllocTemporary(3*w*h);
    BYTE **rowptrs = (BYTE **)AllocTemporary(h*sizeof(BYTE *));
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    int y;
    for(y = 0; y < h; y++) {
        // gl puts the origin at lower left, but png puts it top left
        rowptrs[y] = pixels + ((h - 1) - y)*(3*w);
    }
    png_write_image(png_ptr, rowptrs);

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return;

err:    
    Error("Error writing PNG file '%s'", filename);
    if(f) fclose(f);
    return;
}

void SolveSpace::RemoveFromRecentList(char *file) {
    int src, dest;
    dest = 0;
    for(src = 0; src < MAX_RECENT; src++) {
        if(strcmp(file, RecentFile[src]) != 0) {
            if(src != dest) strcpy(RecentFile[dest], RecentFile[src]);
            dest++;
        }
    }
    while(dest < MAX_RECENT) strcpy(RecentFile[dest++], "");
    RefreshRecentMenus();
}
void SolveSpace::AddToRecentList(char *file) {
    RemoveFromRecentList(file);

    int src;
    for(src = MAX_RECENT - 2; src >= 0; src--) {
        strcpy(RecentFile[src+1], RecentFile[src]);
    }
    strcpy(RecentFile[0], file);
    RefreshRecentMenus();
}

bool SolveSpace::GetFilenameAndSave(bool saveAs) {
    char newFile[MAX_PATH];
    strcpy(newFile, saveFile);
    if(saveAs || strlen(newFile)==0) {
        if(!GetSaveFile(newFile, SLVS_EXT, SLVS_PATTERN)) return false;
    }

    if(SaveToFile(newFile)) {
        AddToRecentList(newFile);
        strcpy(saveFile, newFile);
        unsaved = false;
        return true;
    } else {
        return false;
    }
}

bool SolveSpace::OkayToStartNewFile(void) {
    if(!unsaved) return true;

    switch(SaveFileYesNoCancel()) {
        case IDYES:
            return GetFilenameAndSave(false);

        case IDNO:
            return true;

        case IDCANCEL:
            return false;
        
        default: oops();
    }
}

void SolveSpace::MenuFile(int id) {

    if(id >= RECENT_OPEN && id < (RECENT_OPEN+MAX_RECENT)) {
        char newFile[MAX_PATH];
        strcpy(newFile, RecentFile[id-RECENT_OPEN]);
        RemoveFromRecentList(newFile);
        if(SS.LoadFromFile(newFile)) {
            strcpy(SS.saveFile, newFile);
            AddToRecentList(newFile);
        } else {
            strcpy(SS.saveFile, "");
            SS.NewFile();
        }
        SS.AfterNewFile();
        return;
    }

    switch(id) {
        case GraphicsWindow::MNU_NEW:
            if(!SS.OkayToStartNewFile()) break;

            strcpy(SS.saveFile, "");
            SS.NewFile();
            SS.AfterNewFile();
            break;

        case GraphicsWindow::MNU_OPEN: {
            if(!SS.OkayToStartNewFile()) break;

            char newFile[MAX_PATH] = "";
            if(GetOpenFile(newFile, SLVS_EXT, SLVS_PATTERN)) {
                if(SS.LoadFromFile(newFile)) {
                    strcpy(SS.saveFile, newFile);
                    AddToRecentList(newFile);
                } else {
                    strcpy(SS.saveFile, "");
                    SS.NewFile();
                }
                SS.AfterNewFile();
            }
            break;
        }

        case GraphicsWindow::MNU_SAVE:
            SS.GetFilenameAndSave(false);
            break;

        case GraphicsWindow::MNU_SAVE_AS:
            SS.GetFilenameAndSave(true);
            break;

        case GraphicsWindow::MNU_EXPORT_PNG: {
            char exportFile[MAX_PATH] = "";
            if(!GetSaveFile(exportFile, PNG_EXT, PNG_PATTERN)) break;
            SS.ExportAsPngTo(exportFile); 
            break;
        }

        case GraphicsWindow::MNU_EXIT:
            if(!SS.OkayToStartNewFile()) break;
            SS.Exit();
            break;

        default: oops();
    }
}
