//-----------------------------------------------------------------------------
// Entry point in to the program, our registry-stored settings and top-level
// housekeeping when we open, save, and create new files.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

SolveSpaceUI SolveSpace::SS = {};
Sketch SolveSpace::SK = {};

std::string SolveSpace::RecentFile[MAX_RECENT] = {};

void SolveSpaceUI::Init() {
    SS.tangentArcRadius = 10.0;

    // Then, load the registry settings.
    int i;
    // Default list of colors for the model material
    modelColor[0] = CnfThawColor(RGBi(150, 150, 150), "ModelColor_0");
    modelColor[1] = CnfThawColor(RGBi(100, 100, 100), "ModelColor_1");
    modelColor[2] = CnfThawColor(RGBi( 30,  30,  30), "ModelColor_2");
    modelColor[3] = CnfThawColor(RGBi(150,   0,   0), "ModelColor_3");
    modelColor[4] = CnfThawColor(RGBi(  0, 100,   0), "ModelColor_4");
    modelColor[5] = CnfThawColor(RGBi(  0,  80,  80), "ModelColor_5");
    modelColor[6] = CnfThawColor(RGBi(  0,   0, 130), "ModelColor_6");
    modelColor[7] = CnfThawColor(RGBi( 80,   0,  80), "ModelColor_7");
    // Light intensities
    lightIntensity[0] = CnfThawFloat(1.0f, "LightIntensity_0");
    lightIntensity[1] = CnfThawFloat(0.5f, "LightIntensity_1");
    ambientIntensity = 0.3; // no setting for that yet
    // Light positions
    lightDir[0].x = CnfThawFloat(-1.0f, "LightDir_0_Right"     );
    lightDir[0].y = CnfThawFloat( 1.0f, "LightDir_0_Up"        );
    lightDir[0].z = CnfThawFloat( 0.0f, "LightDir_0_Forward"   );
    lightDir[1].x = CnfThawFloat( 1.0f, "LightDir_1_Right"     );
    lightDir[1].y = CnfThawFloat( 0.0f, "LightDir_1_Up"        );
    lightDir[1].z = CnfThawFloat( 0.0f, "LightDir_1_Forward"   );
    // Chord tolerance
    chordTol = CnfThawFloat(2.0f, "ChordTolerance");
    // Max pwl segments to generate
    maxSegments = CnfThawInt(10, "MaxSegments");
    // View units
    viewUnits = (Unit)CnfThawInt((uint32_t)UNIT_MM, "ViewUnits");
    // Number of digits after the decimal point
    afterDecimalMm = CnfThawInt(2, "AfterDecimalMm");
    afterDecimalInch = CnfThawInt(3, "AfterDecimalInch");
    // Camera tangent (determines perspective)
    cameraTangent = CnfThawFloat(0.3f/1e3f, "CameraTangent");
    // Grid spacing
    gridSpacing = CnfThawFloat(5.0f, "GridSpacing");
    // Export scale factor
    exportScale = CnfThawFloat(1.0f, "ExportScale");
    // Export offset (cutter radius comp)
    exportOffset = CnfThawFloat(0.0f, "ExportOffset");
    // Rewrite exported colors close to white into black (assuming white bg)
    fixExportColors = CnfThawBool(true, "FixExportColors");
    // Draw back faces of triangles (when mesh is leaky/self-intersecting)
    drawBackFaces = CnfThawBool(true, "DrawBackFaces");
    // Check that contours are closed and not self-intersecting
    checkClosedContour = CnfThawBool(true, "CheckClosedContour");
    // Export shaded triangles in a 2d view
    exportShadedTriangles = CnfThawBool(true, "ExportShadedTriangles");
    // Export pwl curves (instead of exact) always
    exportPwlCurves = CnfThawBool(false, "ExportPwlCurves");
    // Background color on-screen
    backgroundColor = CnfThawColor(RGBi(0, 0, 0), "BackgroundColor");
    // Whether export canvas size is fixed or derived from bbox
    exportCanvasSizeAuto = CnfThawBool(true, "ExportCanvasSizeAuto");
    // Margins for automatic canvas size
    exportMargin.left   = CnfThawFloat(5.0f, "ExportMargin_Left");
    exportMargin.right  = CnfThawFloat(5.0f, "ExportMargin_Right");
    exportMargin.bottom = CnfThawFloat(5.0f, "ExportMargin_Bottom");
    exportMargin.top    = CnfThawFloat(5.0f, "ExportMargin_Top");
    // Dimensions for fixed canvas size
    exportCanvas.width  = CnfThawFloat(100.0f, "ExportCanvas_Width");
    exportCanvas.height = CnfThawFloat(100.0f, "ExportCanvas_Height");
    exportCanvas.dx     = CnfThawFloat(  5.0f, "ExportCanvas_Dx");
    exportCanvas.dy     = CnfThawFloat(  5.0f, "ExportCanvas_Dy");
    // Extra parameters when exporting G code
    gCode.depth         = CnfThawFloat(10.0f, "GCode_Depth");
    gCode.passes        = CnfThawInt(1, "GCode_Passes");
    gCode.feed          = CnfThawFloat(10.0f, "GCode_Feed");
    gCode.plungeFeed    = CnfThawFloat(10.0f, "GCode_PlungeFeed");
    // Show toolbar in the graphics window
    showToolbar = CnfThawBool(true, "ShowToolbar");
    // Recent files menus
    for(i = 0; i < MAX_RECENT; i++) {
        RecentFile[i] = CnfThawString("", "RecentFile_" + std::to_string(i));
    }
    RefreshRecentMenus();
    // Autosave timer
    autosaveInterval = CnfThawInt(5, "AutosaveInterval");

    // The default styles (colors, line widths, etc.) are also stored in the
    // configuration file, but we will automatically load those as we need
    // them.

    SetAutosaveTimerFor(autosaveInterval);

    NewFile();
    AfterNewFile();
}

bool SolveSpaceUI::LoadAutosaveFor(const std::string &filename) {
    std::string autosaveFile = filename + AUTOSAVE_SUFFIX;

    FILE *f = ssfopen(autosaveFile, "rb");
    if(!f)
        return false;
    fclose(f);

    if(LoadAutosaveYesNo() == SAVE_YES) {
        unsaved = true;
        return LoadFromFile(autosaveFile);
    }

    return false;
}

bool SolveSpaceUI::OpenFile(const std::string &filename) {
    bool autosaveLoaded = LoadAutosaveFor(filename);
    bool success = autosaveLoaded || LoadFromFile(filename);
    if(success) {
        RemoveAutosave();
        AddToRecentList(filename);
        saveFile = filename;
    } else {
        NewFile();
    }
    AfterNewFile();
    unsaved = autosaveLoaded;
    return success;
}

void SolveSpaceUI::Exit(void) {
    // Recent files
    for(int i = 0; i < MAX_RECENT; i++)
        CnfFreezeString(RecentFile[i], "RecentFile_" + std::to_string(i));
    // Model colors
    for(int i = 0; i < MODEL_COLORS; i++)
        CnfFreezeColor(modelColor[i], "ModelColor_" + std::to_string(i));
    // Light intensities
    CnfFreezeFloat((float)lightIntensity[0], "LightIntensity_0");
    CnfFreezeFloat((float)lightIntensity[1], "LightIntensity_1");
    // Light directions
    CnfFreezeFloat((float)lightDir[0].x, "LightDir_0_Right");
    CnfFreezeFloat((float)lightDir[0].y, "LightDir_0_Up");
    CnfFreezeFloat((float)lightDir[0].z, "LightDir_0_Forward");
    CnfFreezeFloat((float)lightDir[1].x, "LightDir_1_Right");
    CnfFreezeFloat((float)lightDir[1].y, "LightDir_1_Up");
    CnfFreezeFloat((float)lightDir[1].z, "LightDir_1_Forward");
    // Chord tolerance
    CnfFreezeFloat((float)chordTol, "ChordTolerance");
    // Max pwl segments to generate
    CnfFreezeInt((uint32_t)maxSegments, "MaxSegments");
    // View units
    CnfFreezeInt((uint32_t)viewUnits, "ViewUnits");
    // Number of digits after the decimal point
    CnfFreezeInt((uint32_t)afterDecimalMm, "AfterDecimalMm");
    CnfFreezeInt((uint32_t)afterDecimalInch, "AfterDecimalInch");
    // Camera tangent (determines perspective)
    CnfFreezeFloat((float)cameraTangent, "CameraTangent");
    // Grid spacing
    CnfFreezeFloat(gridSpacing, "GridSpacing");
    // Export scale
    CnfFreezeFloat(exportScale, "ExportScale");
    // Export offset (cutter radius comp)
    CnfFreezeFloat(exportOffset, "ExportOffset");
    // Rewrite exported colors close to white into black (assuming white bg)
    CnfFreezeBool(fixExportColors, "FixExportColors");
    // Draw back faces of triangles (when mesh is leaky/self-intersecting)
    CnfFreezeBool(drawBackFaces, "DrawBackFaces");
    // Check that contours are closed and not self-intersecting
    CnfFreezeBool(checkClosedContour, "CheckClosedContour");
    // Export shaded triangles in a 2d view
    CnfFreezeBool(exportShadedTriangles, "ExportShadedTriangles");
    // Export pwl curves (instead of exact) always
    CnfFreezeBool(exportPwlCurves, "ExportPwlCurves");
    // Background color on-screen
    CnfFreezeColor(backgroundColor, "BackgroundColor");
    // Whether export canvas size is fixed or derived from bbox
    CnfFreezeBool(exportCanvasSizeAuto, "ExportCanvasSizeAuto");
    // Margins for automatic canvas size
    CnfFreezeFloat(exportMargin.left,   "ExportMargin_Left");
    CnfFreezeFloat(exportMargin.right,  "ExportMargin_Right");
    CnfFreezeFloat(exportMargin.bottom, "ExportMargin_Bottom");
    CnfFreezeFloat(exportMargin.top,    "ExportMargin_Top");
    // Dimensions for fixed canvas size
    CnfFreezeFloat(exportCanvas.width,  "ExportCanvas_Width");
    CnfFreezeFloat(exportCanvas.height, "ExportCanvas_Height");
    CnfFreezeFloat(exportCanvas.dx,     "ExportCanvas_Dx");
    CnfFreezeFloat(exportCanvas.dy,     "ExportCanvas_Dy");
     // Extra parameters when exporting G code
    CnfFreezeFloat(gCode.depth,         "GCode_Depth");
    CnfFreezeInt(gCode.passes,          "GCode_Passes");
    CnfFreezeFloat(gCode.feed,          "GCode_Feed");
    CnfFreezeFloat(gCode.plungeFeed,    "GCode_PlungeFeed");
    // Show toolbar in the graphics window
    CnfFreezeBool(showToolbar, "ShowToolbar");
    // Autosave timer
    CnfFreezeInt(autosaveInterval, "AutosaveInterval");

    // And the default styles, colors and line widths and such.
    Style::FreezeDefaultStyles();

    // Exiting cleanly.
    RemoveAutosave();

    ExitNow();
}

void SolveSpaceUI::ScheduleGenerateAll() {
    if(!later.scheduled) ScheduleLater();
    later.scheduled = true;
    later.generateAll = true;
}

void SolveSpaceUI::ScheduleShowTW() {
    if(!later.scheduled) ScheduleLater();
    later.scheduled = true;
    later.showTW = true;
}

void SolveSpaceUI::DoLater(void) {
    if(later.generateAll) GenerateAll();
    if(later.showTW) TW.Show();
    later = {};
}

double SolveSpaceUI::MmPerUnit(void) {
    if(viewUnits == UNIT_INCHES) {
        return 25.4;
    } else {
        return 1.0;
    }
}
const char *SolveSpaceUI::UnitName(void) {
    if(viewUnits == UNIT_INCHES) {
        return "inch";
    } else {
        return "mm";
    }
}
char *SolveSpaceUI::MmToString(double v) {
    static int WhichBuf;
    static char Bufs[8][128];

    WhichBuf++;
    if(WhichBuf >= 8 || WhichBuf < 0) WhichBuf = 0;

    char *s = Bufs[WhichBuf];
    if(viewUnits == UNIT_INCHES) {
        sprintf(s, "%.*f", afterDecimalInch, v/25.4);
    } else {
        sprintf(s, "%.*f", afterDecimalMm, v);
    }
    return s;
}
double SolveSpaceUI::ExprToMm(Expr *e) {
    return (e->Eval()) * MmPerUnit();
}
double SolveSpaceUI::StringToMm(const char *str) {
    return atof(str) * MmPerUnit();
}
double SolveSpaceUI::ChordTolMm(void) {
    return SS.chordTol / SS.GW.scale;
}
int SolveSpaceUI::UnitDigitsAfterDecimal(void) {
    return (viewUnits == UNIT_INCHES) ? afterDecimalInch : afterDecimalMm;
}
void SolveSpaceUI::SetUnitDigitsAfterDecimal(int v) {
    if(viewUnits == UNIT_INCHES) {
        afterDecimalInch = v;
    } else {
        afterDecimalMm = v;
    }
}

double SolveSpaceUI::CameraTangent(void) {
    if(!usePerspectiveProj) {
        return 0;
    } else {
        return cameraTangent;
    }
}

void SolveSpaceUI::AfterNewFile(void) {
    // Clear out the traced point, which is no longer valid
    traced.point = Entity::NO_ENTITY;
    traced.path.l.Clear();
    // and the naked edges
    nakedEdges.Clear();

    // GenerateAll() expects the view to be valid, because it uses that to
    // fill in default values for extrusion depths etc. (which won't matter
    // here, but just don't let it work on garbage)
    SS.GW.offset    = Vector::From(0, 0, 0);
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);

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
    // roughly in the window, since that sets the mesh tolerance. Consider
    // invisible entities, so we still get something reasonable if the only
    // thing visible is the not-yet-generated surfaces.
    GW.ZoomToFit(true);

    GenerateAll(0, INT_MAX);
    SS.ScheduleShowTW();
    // Then zoom to fit again, to fit the triangles
    GW.ZoomToFit(false);

    // Create all the default styles; they'll get created on the fly anyways,
    // but can't hurt to do it now.
    Style::CreateAllDefaultStyles();

    UpdateWindowTitle();
}

void SolveSpaceUI::RemoveFromRecentList(const std::string &filename) {
    int src, dest;
    dest = 0;
    for(src = 0; src < MAX_RECENT; src++) {
        if(filename != RecentFile[src]) {
            if(src != dest) RecentFile[dest] = RecentFile[src];
            dest++;
        }
    }
    while(dest < MAX_RECENT) RecentFile[dest++].clear();
    RefreshRecentMenus();
}
void SolveSpaceUI::AddToRecentList(const std::string &filename) {
    RemoveFromRecentList(filename);

    int src;
    for(src = MAX_RECENT - 2; src >= 0; src--) {
        RecentFile[src+1] = RecentFile[src];
    }
    RecentFile[0] = filename;
    RefreshRecentMenus();
}

bool SolveSpaceUI::GetFilenameAndSave(bool saveAs) {
    std::string prevSaveFile = saveFile;

    if(saveAs || saveFile.empty()) {
        if(!GetSaveFile(saveFile, SLVS_EXT, SLVS_PATTERN)) return false;
        // need to get new filename directly into saveFile, since that
        // determines impFileRel path
    }

    if(SaveToFile(saveFile)) {
        AddToRecentList(saveFile);
        RemoveAutosave();
        unsaved = false;
        return true;
    } else {
        // don't store an invalid save filename
        saveFile = prevSaveFile;
        return false;
    }
}

bool SolveSpaceUI::Autosave()
{
    SetAutosaveTimerFor(autosaveInterval);

    if(!saveFile.empty() && unsaved)
        return SaveToFile(saveFile + AUTOSAVE_SUFFIX);

    return false;
}

void SolveSpaceUI::RemoveAutosave()
{
    std::string autosaveFile = saveFile + AUTOSAVE_SUFFIX;
    ssremove(autosaveFile.c_str());
}

bool SolveSpaceUI::OkayToStartNewFile(void) {
    if(!unsaved) return true;

    switch(SaveFileYesNoCancel()) {
        case SAVE_YES:
            return GetFilenameAndSave(false);

        case SAVE_NO:
            return true;

        case SAVE_CANCEL:
            return false;

        default: oops(); break;
    }
}

void SolveSpaceUI::UpdateWindowTitle(void) {
    SetCurrentFilename(saveFile);
}

static std::string Extension(const std::string &filename) {
    int dot = filename.rfind('.');
    if(dot >= 0)
        return filename.substr(dot + 1, filename.length());
    return "";
}

void SolveSpaceUI::MenuFile(int id) {
    if(id >= RECENT_OPEN && id < (RECENT_OPEN+MAX_RECENT)) {
        if(!SS.OkayToStartNewFile()) return;

        std::string newFile = RecentFile[id - RECENT_OPEN];
        RemoveFromRecentList(newFile);
        SS.OpenFile(newFile);
        return;
    }

    switch(id) {
        case GraphicsWindow::MNU_NEW:
            if(!SS.OkayToStartNewFile()) break;

            SS.saveFile = "";
            SS.NewFile();
            SS.AfterNewFile();
            break;

        case GraphicsWindow::MNU_OPEN: {
            if(!SS.OkayToStartNewFile()) break;

            std::string newFile;
            if(GetOpenFile(newFile, SLVS_EXT, SLVS_PATTERN)) {
                SS.OpenFile(newFile);
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
            std::string exportFile;
            if(!GetSaveFile(exportFile, PNG_EXT, PNG_PATTERN)) break;
            SS.ExportAsPngTo(exportFile);
            break;
        }

        case GraphicsWindow::MNU_EXPORT_VIEW: {
            std::string exportFile = CnfThawString("", "2DExportFormat");
            if(!GetSaveFile(exportFile, VEC_EXT, VEC_PATTERN)) break;
            CnfFreezeString(Extension(exportFile), "2DExportFormat");

            // If the user is exporting something where it would be
            // inappropriate to include the constraints, then warn.
            if(SS.GW.showConstraints &&
                (FilenameHasExtension(exportFile, ".txt") ||
                 fabs(SS.exportOffset) > LENGTH_EPS))
            {
                Message("Constraints are currently shown, and will be exported "
                        "in the toolpath. This is probably not what you want; "
                        "hide them by clicking the link at the top of the "
                        "text window.");
            }

            SS.ExportViewOrWireframeTo(exportFile, false);
            break;
        }

        case GraphicsWindow::MNU_EXPORT_WIREFRAME: {
            std::string exportFile;
            if(!GetSaveFile(exportFile, V3D_EXT, V3D_PATTERN)) break;
            SS.ExportViewOrWireframeTo(exportFile, true);
            break;
        }

        case GraphicsWindow::MNU_EXPORT_SECTION: {
            std::string exportFile;
            if(!GetSaveFile(exportFile, VEC_EXT, VEC_PATTERN)) break;
            SS.ExportSectionTo(exportFile);
            break;
        }

        case GraphicsWindow::MNU_EXPORT_MESH: {
            std::string exportFile;
            if(!GetSaveFile(exportFile, MESH_EXT, MESH_PATTERN)) break;
            SS.ExportMeshTo(exportFile);
            break;
        }

        case GraphicsWindow::MNU_EXPORT_SURFACES: {
            std::string exportFile;
            if(!GetSaveFile(exportFile, SRF_EXT, SRF_PATTERN)) break;
            StepFileWriter sfw = {};
            sfw.ExportSurfacesTo(exportFile);
            break;
        }

        case GraphicsWindow::MNU_EXIT:
            if(!SS.OkayToStartNewFile()) break;
            SS.Exit();
            break;

        default: oops();
    }

    SS.UpdateWindowTitle();
}

void SolveSpaceUI::MenuAnalyze(int id) {
    SS.GW.GroupSelection();
#define gs (SS.GW.gs)

    switch(id) {
        case GraphicsWindow::MNU_STEP_DIM:
            if(gs.constraints == 1 && gs.n == 0) {
                Constraint *c = SK.GetConstraint(gs.constraint[0]);
                if(c->HasLabel() && !c->reference) {
                    SS.TW.shown.dimFinish = c->valA;
                    SS.TW.shown.dimSteps = 10;
                    SS.TW.shown.dimIsDistance =
                        (c->type != Constraint::ANGLE) &&
                        (c->type != Constraint::LENGTH_RATIO) &&
                        (c->type != Constraint::LENGTH_DIFFERENCE);
                    SS.TW.shown.constraint = c->h;
                    SS.TW.shown.screen = TextWindow::SCREEN_STEP_DIMENSION;

                    // The step params are specified in the text window,
                    // so force that to be shown.
                    SS.GW.ForceTextWindowShown();

                    SS.ScheduleShowTW();
                    SS.GW.ClearSelection();
                } else {
                    Error("Constraint must have a label, and must not be "
                          "a reference dimension.");
                }
            } else {
                Error("Bad selection for step dimension; select a constraint.");
            }
            break;

        case GraphicsWindow::MNU_NAKED_EDGES: {
            SS.nakedEdges.Clear();

            Group *g = SK.GetGroup(SS.GW.activeGroup);
            SMesh *m = &(g->displayMesh);
            SKdNode *root = SKdNode::From(m);
            bool inters, leaks;
            root->MakeCertainEdgesInto(&(SS.nakedEdges),
                SKdNode::NAKED_OR_SELF_INTER_EDGES, true, &inters, &leaks);

            InvalidateGraphics();

            const char *intersMsg = inters ?
                "The mesh is self-intersecting (NOT okay, invalid)." :
                "The mesh is not self-intersecting (okay, valid).";
            const char *leaksMsg = leaks ?
                "The mesh has naked edges (NOT okay, invalid)." :
                "The mesh is watertight (okay, valid).";

            char cntMsg[1024];
            sprintf(cntMsg, "\n\nThe model contains %d triangles, from "
                            "%d surfaces.",
                g->displayMesh.l.n, g->runningShell.surface.n);

            if(SS.nakedEdges.l.n == 0) {
                Message("%s\n\n%s\n\nZero problematic edges, good.%s",
                    intersMsg, leaksMsg, cntMsg);
            } else {
                Error("%s\n\n%s\n\n%d problematic edges, bad.%s",
                    intersMsg, leaksMsg, SS.nakedEdges.l.n, cntMsg);
            }
            break;
        }

        case GraphicsWindow::MNU_INTERFERENCE: {
            SS.nakedEdges.Clear();

            SMesh *m = &(SK.GetGroup(SS.GW.activeGroup)->displayMesh);
            SKdNode *root = SKdNode::From(m);
            bool inters, leaks;
            root->MakeCertainEdgesInto(&(SS.nakedEdges),
                SKdNode::SELF_INTER_EDGES, false, &inters, &leaks);

            InvalidateGraphics();

            if(inters) {
                Error("%d edges interfere with other triangles, bad.",
                    SS.nakedEdges.l.n);
            } else {
                Message("The assembly does not interfere, good.");
            }
            break;
        }

        case GraphicsWindow::MNU_VOLUME: {
            SMesh *m = &(SK.GetGroup(SS.GW.activeGroup)->displayMesh);

            double vol = 0;
            int i;
            for(i = 0; i < m->l.n; i++) {
                STriangle tr = m->l.elem[i];

                // Translate to place vertex A at (x, y, 0)
                Vector trans = Vector::From(tr.a.x, tr.a.y, 0);
                tr.a = (tr.a).Minus(trans);
                tr.b = (tr.b).Minus(trans);
                tr.c = (tr.c).Minus(trans);

                // Rotate to place vertex B on the y-axis. Depending on
                // whether the triangle is CW or CCW, C is either to the
                // right or to the left of the y-axis. This handles the
                // sign of our normal.
                Vector u = Vector::From(-tr.b.y, tr.b.x, 0);
                u = u.WithMagnitude(1);
                Vector v = Vector::From(tr.b.x, tr.b.y, 0);
                v = v.WithMagnitude(1);
                Vector n = Vector::From(0, 0, 1);

                tr.a = (tr.a).DotInToCsys(u, v, n);
                tr.b = (tr.b).DotInToCsys(u, v, n);
                tr.c = (tr.c).DotInToCsys(u, v, n);

                n = tr.Normal().WithMagnitude(1);

                // Triangles on edge don't contribute
                if(fabs(n.z) < LENGTH_EPS) continue;

                // The plane has equation p dot n = a dot n
                double d = (tr.a).Dot(n);
                // nx*x + ny*y + nz*z = d
                // nz*z = d - nx*x - ny*y
                double A = -n.x/n.z, B = -n.y/n.z, C = d/n.z;

                double mac = tr.c.y/tr.c.x, mbc = (tr.c.y - tr.b.y)/tr.c.x;
                double xc = tr.c.x, yb = tr.b.y;

                // I asked Maple for
                //    int(int(A*x + B*y +C, y=mac*x..(mbc*x + yb)), x=0..xc);
                double integral =
                    (1.0/3)*(
                        A*(mbc-mac)+
                        (1.0/2)*B*(mbc*mbc-mac*mac)
                    )*(xc*xc*xc)+
                    (1.0/2)*(A*yb+B*yb*mbc+C*(mbc-mac))*xc*xc+
                    C*yb*xc+
                    (1.0/2)*B*yb*yb*xc;

                vol += integral;
            }

            char msg[1024];
            sprintf(msg, "The volume of the solid model is:\n\n"
                         "    %.3f %s^3",
                vol / pow(SS.MmPerUnit(), 3),
                SS.UnitName());

            if(SS.viewUnits == SolveSpaceUI::UNIT_MM) {
                sprintf(msg+strlen(msg), "\n    %.2f mL", vol/(10*10*10));
            }
            strcpy(msg+strlen(msg),
                "\n\nCurved surfaces have been approximated as triangles.\n"
                "This introduces error, typically of around 1%.");
            Message("%s", msg);
            break;
        }

        case GraphicsWindow::MNU_AREA: {
            Group *g = SK.GetGroup(SS.GW.activeGroup);
            if(g->polyError.how != Group::POLY_GOOD) {
                Error("This group does not contain a correctly-formed "
                      "2d closed area. It is open, not coplanar, or self-"
                      "intersecting.");
                break;
            }
            SEdgeList sel = {};
            g->polyLoops.MakeEdgesInto(&sel);
            SPolygon sp = {};
            sel.AssemblePolygon(&sp, NULL, true);
            sp.normal = sp.ComputeNormal();
            sp.FixContourDirections();
            double area = sp.SignedArea();
            double scale = SS.MmPerUnit();
            Message("The area of the region sketched in this group is:\n\n"
                    "    %.3f %s^2\n\n"
                    "Curves have been approximated as piecewise linear.\n"
                    "This introduces error, typically of around 1%%.",
                area / (scale*scale),
                SS.UnitName());
            sel.Clear();
            sp.Clear();
            break;
        }

        case GraphicsWindow::MNU_SHOW_DOF:
            // This works like a normal solve, except that it calculates
            // which variables are free/bound at the same time.
            SS.GenerateAll(0, INT_MAX, true);
            break;

        case GraphicsWindow::MNU_TRACE_PT:
            if(gs.points == 1 && gs.n == 1) {
                SS.traced.point = gs.point[0];
                SS.GW.ClearSelection();
            } else {
                Error("Bad selection for trace; select a single point.");
            }
            break;

        case GraphicsWindow::MNU_STOP_TRACING: {
            std::string exportFile;
            if(GetSaveFile(exportFile, CSV_EXT, CSV_PATTERN)) {
                FILE *f = ssfopen(exportFile, "wb");
                if(f) {
                    int i;
                    SContour *sc = &(SS.traced.path);
                    for(i = 0; i < sc->l.n; i++) {
                        Vector p = sc->l.elem[i].p;
                        double s = SS.exportScale;
                        fprintf(f, "%.10f, %.10f, %.10f\r\n",
                            p.x/s, p.y/s, p.z/s);
                    }
                    fclose(f);
                } else {
                    Error("Couldn't write to '%s'", exportFile.c_str());
                }
            }
            // Clear the trace, and stop tracing
            SS.traced.point = Entity::NO_ENTITY;
            SS.traced.path.l.Clear();
            InvalidateGraphics();
            break;
        }

        default: oops();
    }
}

void SolveSpaceUI::MenuHelp(int id) {
    switch(id) {
        case GraphicsWindow::MNU_WEBSITE:
            OpenWebsite("http://solvespace.com/helpmenu");
            break;

        case GraphicsWindow::MNU_ABOUT:
            Message(
"This is SolveSpace version " PACKAGE_VERSION ".\n"
"\n"
"Built " __TIME__ " " __DATE__ ".\n"
"\n"
"For more information, see http://solvespace.com/\n"
"\n"
"SolveSpace is free software: you are free to modify\n"
"and/or redistribute it under the terms of the GNU\n"
"General Public License (GPL) version 3 or later.\n"
"\n"
"There is NO WARRANTY, to the extent permitted by\n"
"law. For details, visit http://gnu.org/licenses/\n"
"\n"
#ifdef WIN32
"\xa9 "
#else
"© "
#endif
       "2008-2013 Jonathan Westhues and other authors.\n"
);
            break;

        default: oops();
    }
}

void SolveSpaceUI::Clear(void) {
    sys.Clear();
    for(int i = 0; i < MAX_UNDO; i++) {
        if(i < undo.cnt) undo.d[i].Clear();
        if(i < redo.cnt) redo.d[i].Clear();
    }
}

void Sketch::Clear(void) {
    group.Clear();
    constraint.Clear();
    request.Clear();
    style.Clear();
    entity.Clear();
    param.Clear();
}
