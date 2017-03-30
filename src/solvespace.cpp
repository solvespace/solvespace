//-----------------------------------------------------------------------------
// Entry point in to the program, our registry-stored settings and top-level
// housekeeping when we open, save, and create new files.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "config.h"

SolveSpaceUI SolveSpace::SS = {};
Sketch SolveSpace::SK = {};

Platform::Path SolveSpace::RecentFile[MAX_RECENT] = {};

void SolveSpaceUI::Init() {
#if !defined(HEADLESS)
    // Check that the resource system works.
    dbp("%s", LoadString("banner.txt").data());
#endif

    SS.tangentArcRadius = 10.0;

    // Then, load the registry settings.
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

    exportMode = false;
    // Chord tolerance
    chordTol = CnfThawFloat(0.5f, "ChordTolerancePct");
    // Max pwl segments to generate
    maxSegments = CnfThawInt(10, "MaxSegments");
    // Chord tolerance
    exportChordTol = CnfThawFloat(0.1f, "ExportChordTolerance");
    // Max pwl segments to generate
    exportMaxSegments = CnfThawInt(64, "ExportMaxSegments");
    // View units
    viewUnits = (Unit)CnfThawInt((uint32_t)Unit::MM, "ViewUnits");
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
    // Draw closed polygons areas
    showContourAreas = CnfThawBool(false, "ShowContourAreas");
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
    for(size_t i = 0; i < MAX_RECENT; i++) {
        RecentFile[i] = Platform::Path::From(CnfThawString("", "RecentFile_" + std::to_string(i)));
    }
    RefreshRecentMenus();
    // Autosave timer
    autosaveInterval = CnfThawInt(5, "AutosaveInterval");
    // Locale
    std::string locale = CnfThawString("", "Locale");
    if(!locale.empty()) {
        SetLocale(locale);
    }

    // The default styles (colors, line widths, etc.) are also stored in the
    // configuration file, but we will automatically load those as we need
    // them.

    SetAutosaveTimerFor(autosaveInterval);

    NewFile();
    AfterNewFile();
}

bool SolveSpaceUI::LoadAutosaveFor(const Platform::Path &filename) {
    Platform::Path autosaveFile = filename.WithExtension(AUTOSAVE_EXT);

    FILE *f = OpenFile(autosaveFile, "rb");
    if(!f)
        return false;
    fclose(f);

    if(LoadAutosaveYesNo() == DIALOG_YES) {
        unsaved = true;
        return LoadFromFile(autosaveFile, /*canCancel=*/true);
    }

    return false;
}

bool SolveSpaceUI::Load(const Platform::Path &filename) {
    bool autosaveLoaded = LoadAutosaveFor(filename);
    bool fileLoaded = autosaveLoaded || LoadFromFile(filename, /*canCancel=*/true);
    if(fileLoaded) {
        saveFile = filename;
        AddToRecentList(filename);
    } else {
        saveFile.Clear();
        NewFile();
    }
    AfterNewFile();
    unsaved = autosaveLoaded;
    return fileLoaded;
}

void SolveSpaceUI::Exit() {
    // Recent files
    for(size_t i = 0; i < MAX_RECENT; i++)
        CnfFreezeString(RecentFile[i].raw, "RecentFile_" + std::to_string(i));
    // Model colors
    for(size_t i = 0; i < MODEL_COLORS; i++)
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
    CnfFreezeFloat((float)chordTol, "ChordTolerancePct");
    // Max pwl segments to generate
    CnfFreezeInt((uint32_t)maxSegments, "MaxSegments");
    // Export Chord tolerance
    CnfFreezeFloat((float)exportChordTol, "ExportChordTolerance");
    // Export Max pwl segments to generate
    CnfFreezeInt((uint32_t)exportMaxSegments, "ExportMaxSegments");
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
    // Draw closed polygons areas
    CnfFreezeBool(showContourAreas, "ShowContourAreas");
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

void SolveSpaceUI::DoLater() {
    if(later.generateAll) GenerateAll();
    if(later.showTW) TW.Show();
    later = {};
}

double SolveSpaceUI::MmPerUnit() {
    if(viewUnits == Unit::INCHES) {
        return 25.4;
    } else {
        return 1.0;
    }
}
const char *SolveSpaceUI::UnitName() {
    if(viewUnits == Unit::INCHES) {
        return "inch";
    } else {
        return "mm";
    }
}
std::string SolveSpaceUI::MmToString(double v) {
    if(viewUnits == Unit::INCHES) {
        return ssprintf("%.*f", afterDecimalInch, v/25.4);
    } else {
        return ssprintf("%.*f", afterDecimalMm, v);
    }
}
double SolveSpaceUI::ExprToMm(Expr *e) {
    return (e->Eval()) * MmPerUnit();
}
double SolveSpaceUI::StringToMm(const std::string &str) {
    return std::stod(str) * MmPerUnit();
}
double SolveSpaceUI::ChordTolMm() {
    if(exportMode) return ExportChordTolMm();
    return chordTolCalculated;
}
double SolveSpaceUI::ExportChordTolMm() {
    return exportChordTol / exportScale;
}
int SolveSpaceUI::GetMaxSegments() {
    if(exportMode) return exportMaxSegments;
    return maxSegments;
}
int SolveSpaceUI::UnitDigitsAfterDecimal() {
    return (viewUnits == Unit::INCHES) ? afterDecimalInch : afterDecimalMm;
}
void SolveSpaceUI::SetUnitDigitsAfterDecimal(int v) {
    if(viewUnits == Unit::INCHES) {
        afterDecimalInch = v;
    } else {
        afterDecimalMm = v;
    }
}

double SolveSpaceUI::CameraTangent() {
    if(!usePerspectiveProj) {
        return 0;
    } else {
        return cameraTangent;
    }
}

void SolveSpaceUI::AfterNewFile() {
    // Clear out the traced point, which is no longer valid
    traced.point = Entity::NO_ENTITY;
    traced.path.l.Clear();
    // and the naked edges
    nakedEdges.Clear();

    // Quit export mode
    justExportedInfo.draw = false;
    centerOfMass.draw = false;
    exportMode = false;

    // GenerateAll() expects the view to be valid, because it uses that to
    // fill in default values for extrusion depths etc. (which won't matter
    // here, but just don't let it work on garbage)
    SS.GW.offset    = Vector::From(0, 0, 0);
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);

    GenerateAll(Generate::ALL);

    TW.Init();
    GW.Init();

    unsaved = false;

    int w, h;
    GetGraphicsWindowSize(&w, &h);
    GW.width = w;
    GW.height = h;

    GW.ZoomToFit(/*includingInvisibles=*/false);

    // Create all the default styles; they'll get created on the fly anyways,
    // but can't hurt to do it now.
    Style::CreateAllDefaultStyles();

    UpdateWindowTitle();
}

void SolveSpaceUI::RemoveFromRecentList(const Platform::Path &filename) {
    int dest = 0;
    for(int src = 0; src < (int)MAX_RECENT; src++) {
        if(!filename.Equals(RecentFile[src])) {
            if(src != dest) RecentFile[dest] = RecentFile[src];
            dest++;
        }
    }
    while(dest < (int)MAX_RECENT) RecentFile[dest++].Clear();
    RefreshRecentMenus();
}
void SolveSpaceUI::AddToRecentList(const Platform::Path &filename) {
    RemoveFromRecentList(filename);

    for(int src = MAX_RECENT - 2; src >= 0; src--) {
        RecentFile[src+1] = RecentFile[src];
    }
    RecentFile[0] = filename;
    RefreshRecentMenus();
}

bool SolveSpaceUI::GetFilenameAndSave(bool saveAs) {
    Platform::Path newSaveFile = saveFile;

    if(saveAs || saveFile.IsEmpty()) {
        if(!GetSaveFile(&newSaveFile, "", SlvsFileFilter)) return false;
    }

    if(SaveToFile(newSaveFile)) {
        AddToRecentList(newSaveFile);
        RemoveAutosave();
        saveFile = newSaveFile;
        unsaved = false;
        return true;
    } else {
        return false;
    }
}

bool SolveSpaceUI::Autosave()
{
    SetAutosaveTimerFor(autosaveInterval);

    if(!saveFile.IsEmpty() && unsaved)
        return SaveToFile(saveFile.WithExtension(AUTOSAVE_EXT));

    return false;
}

void SolveSpaceUI::RemoveAutosave()
{
    Platform::Path autosaveFile = saveFile.WithExtension(AUTOSAVE_EXT);
    RemoveFile(autosaveFile);
}

bool SolveSpaceUI::OkayToStartNewFile() {
    if(!unsaved) return true;

    switch(SaveFileYesNoCancel()) {
        case DIALOG_YES:
            return GetFilenameAndSave(/*saveAs=*/false);

        case DIALOG_NO:
            RemoveAutosave();
            return true;

        case DIALOG_CANCEL:
            return false;
    }
    ssassert(false, "Unexpected dialog choice");
}

void SolveSpaceUI::UpdateWindowTitle() {
    SetCurrentFilename(saveFile);
}

void SolveSpaceUI::MenuFile(Command id) {
    if((uint32_t)id >= (uint32_t)Command::RECENT_OPEN &&
       (uint32_t)id < ((uint32_t)Command::RECENT_OPEN+MAX_RECENT)) {
        if(!SS.OkayToStartNewFile()) return;

        Platform::Path newFile = RecentFile[(uint32_t)id - (uint32_t)Command::RECENT_OPEN];
        SS.Load(newFile);
        return;
    }

    switch(id) {
        case Command::NEW:
            if(!SS.OkayToStartNewFile()) break;

            SS.saveFile.Clear();
            SS.NewFile();
            SS.AfterNewFile();
            break;

        case Command::OPEN: {
            if(!SS.OkayToStartNewFile()) break;

            Platform::Path newFile;
            if(GetOpenFile(&newFile, "", SlvsFileFilter)) {
                SS.Load(newFile);
            }
            break;
        }

        case Command::SAVE:
            SS.GetFilenameAndSave(/*saveAs=*/false);
            break;

        case Command::SAVE_AS:
            SS.GetFilenameAndSave(/*saveAs=*/true);
            break;

        case Command::EXPORT_PNG: {
            Platform::Path exportFile = SS.saveFile;
            if(!GetSaveFile(&exportFile, "", RasterFileFilter)) break;
            SS.ExportAsPngTo(exportFile);
            break;
        }

        case Command::EXPORT_VIEW: {
            Platform::Path exportFile = SS.saveFile;
            if(!GetSaveFile(&exportFile, CnfThawString("", "ViewExportFormat"),
                            VectorFileFilter)) break;
            CnfFreezeString(exportFile.Extension(), "ViewExportFormat");

            // If the user is exporting something where it would be
            // inappropriate to include the constraints, then warn.
            if(SS.GW.showConstraints &&
                (exportFile.HasExtension("txt") ||
                 fabs(SS.exportOffset) > LENGTH_EPS))
            {
                Message(_("Constraints are currently shown, and will be exported "
                          "in the toolpath. This is probably not what you want; "
                          "hide them by clicking the link at the top of the "
                          "text window."));
            }

            SS.ExportViewOrWireframeTo(exportFile, /*exportWireframe*/false);
            break;
        }

        case Command::EXPORT_WIREFRAME: {
            Platform::Path exportFile = SS.saveFile;
            if(!GetSaveFile(&exportFile, CnfThawString("", "WireframeExportFormat"),
                            Vector3dFileFilter)) break;
            CnfFreezeString(exportFile.Extension(), "WireframeExportFormat");

            SS.ExportViewOrWireframeTo(exportFile, /*exportWireframe*/true);
            break;
        }

        case Command::EXPORT_SECTION: {
            Platform::Path exportFile = SS.saveFile;
            if(!GetSaveFile(&exportFile, CnfThawString("", "SectionExportFormat"),
                            VectorFileFilter)) break;
            CnfFreezeString(exportFile.Extension(), "SectionExportFormat");

            SS.ExportSectionTo(exportFile);
            break;
        }

        case Command::EXPORT_MESH: {
            Platform::Path exportFile = SS.saveFile;
            if(!GetSaveFile(&exportFile, CnfThawString("", "MeshExportFormat"),
                            MeshFileFilter)) break;
            CnfFreezeString(exportFile.Extension(), "MeshExportFormat");

            SS.ExportMeshTo(exportFile);
            break;
        }

        case Command::EXPORT_SURFACES: {
            Platform::Path exportFile = SS.saveFile;
            if(!GetSaveFile(&exportFile, CnfThawString("", "SurfacesExportFormat"),
                            SurfaceFileFilter)) break;
            CnfFreezeString(exportFile.Extension(), "SurfacesExportFormat");

            StepFileWriter sfw = {};
            sfw.ExportSurfacesTo(exportFile);
            break;
        }

        case Command::IMPORT: {
            Platform::Path importFile;
            if(!GetOpenFile(&importFile, CnfThawString("", "ImportFormat"),
                            ImportableFileFilter)) break;
            CnfFreezeString(importFile.Extension(), "ImportFormat");

            if(importFile.HasExtension("dxf")) {
                ImportDxf(importFile);
            } else if(importFile.HasExtension("dwg")) {
                ImportDwg(importFile);
            } else {
                Error("Can't identify file type from file extension of "
                      "filename '%s'; try .dxf or .dwg.", importFile.raw.c_str());
            }

            SS.GenerateAll(SolveSpaceUI::Generate::UNTIL_ACTIVE);
            SS.ScheduleShowTW();
            break;
        }

        case Command::EXIT:
            if(!SS.OkayToStartNewFile()) break;
            SS.Exit();
            break;

        default: ssassert(false, "Unexpected menu ID");
    }

    SS.UpdateWindowTitle();
}

void SolveSpaceUI::MenuAnalyze(Command id) {
    SS.GW.GroupSelection();
    auto const &gs = SS.GW.gs;

    switch(id) {
        case Command::STEP_DIM:
            if(gs.constraints == 1 && gs.n == 0) {
                Constraint *c = SK.GetConstraint(gs.constraint[0]);
                if(c->HasLabel() && !c->reference) {
                    SS.TW.shown.dimFinish = c->valA;
                    SS.TW.shown.dimSteps = 10;
                    SS.TW.shown.dimIsDistance =
                        (c->type != Constraint::Type::ANGLE) &&
                        (c->type != Constraint::Type::LENGTH_RATIO) &&
                        (c->type != Constraint::Type::LENGTH_DIFFERENCE);
                    SS.TW.shown.constraint = c->h;
                    SS.TW.shown.screen = TextWindow::Screen::STEP_DIMENSION;

                    // The step params are specified in the text window,
                    // so force that to be shown.
                    SS.GW.ForceTextWindowShown();

                    SS.ScheduleShowTW();
                    SS.GW.ClearSelection();
                } else {
                    Error(_("Constraint must have a label, and must not be "
                            "a reference dimension."));
                }
            } else {
                Error(_("Bad selection for step dimension; select a constraint."));
            }
            break;

        case Command::NAKED_EDGES: {
            ShowNakedEdges(/*reportOnlyWhenNotOkay=*/false);
            break;
        }

        case Command::INTERFERENCE: {
            SS.nakedEdges.Clear();

            SMesh *m = &(SK.GetGroup(SS.GW.activeGroup)->displayMesh);
            SKdNode *root = SKdNode::From(m);
            bool inters, leaks;
            root->MakeCertainEdgesInto(&(SS.nakedEdges),
                EdgeKind::SELF_INTER, /*coplanarIsInter=*/false, &inters, &leaks);

            InvalidateGraphics();

            if(inters) {
                Error("%d edges interfere with other triangles, bad.",
                    SS.nakedEdges.l.n);
            } else {
                Message(_("The assembly does not interfere, good."));
            }
            break;
        }

        case Command::CENTER_OF_MASS: {
            SS.UpdateCenterOfMass();
            SS.centerOfMass.draw = true;
            InvalidateGraphics();
            break;
        }

        case Command::VOLUME: {
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

            std::string msg = ssprintf("The volume of the solid model is:\n\n""    %.3f %s^3",
                vol / pow(SS.MmPerUnit(), 3),
                SS.UnitName());

            if(SS.viewUnits == Unit::MM) {
                msg += ssprintf("\n    %.2f mL", vol/(10*10*10));
            }
            msg += "\n\nCurved surfaces have been approximated as triangles.\n"
                   "This introduces error, typically of around 1%.";
            Message("%s", msg.c_str());
            break;
        }

        case Command::AREA: {
            Group *g = SK.GetGroup(SS.GW.activeGroup);
            if(g->polyError.how != PolyError::GOOD) {
                Error(_("This group does not contain a correctly-formed "
                        "2d closed area. It is open, not coplanar, or self-"
                        "intersecting."));
                break;
            }
            SEdgeList sel = {};
            g->polyLoops.MakeEdgesInto(&sel);
            SPolygon sp = {};
            sel.AssemblePolygon(&sp, NULL, /*keepDir=*/true);
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

        case Command::PERIMETER: {
            if(gs.n > 0 && gs.n == gs.entities) {
                double perimeter = 0.0;
                for(int i = 0; i < gs.entities; i++) {
                    Entity *e = SK.entity.FindById(gs.entity[i]);
                    SEdgeList *el = e->GetOrGenerateEdges();
                    for(const SEdge &e : el->l) {
                        perimeter += e.b.Minus(e.a).Magnitude();
                    }
                }

                double scale = SS.MmPerUnit();
                Message("The total length of the selected entities is:\n\n"
                        "    %.3f %s\n\n"
                        "Curves have been approximated as piecewise linear.\n"
                        "This introduces error, typically of around 1%%.",
                    perimeter / scale,
                    SS.UnitName());
            } else {
                Error(_("Bad selection for perimeter; select line segments, arcs, and curves."));
            }
            break;
        }

        case Command::SHOW_DOF:
            // This works like a normal solve, except that it calculates
            // which variables are free/bound at the same time.
            SS.GenerateAll(SolveSpaceUI::Generate::ALL, /*andFindFree=*/true);
            break;

        case Command::TRACE_PT:
            if(gs.points == 1 && gs.n == 1) {
                SS.traced.point = gs.point[0];
                SS.GW.ClearSelection();
            } else {
                Error(_("Bad selection for trace; select a single point."));
            }
            break;

        case Command::STOP_TRACING: {
            Platform::Path exportFile = SS.saveFile;
            if(GetSaveFile(&exportFile, "", CsvFileFilter)) {
                FILE *f = OpenFile(exportFile, "wb");
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
                    Error("Couldn't write to '%s'", exportFile.raw.c_str());
                }
            }
            // Clear the trace, and stop tracing
            SS.traced.point = Entity::NO_ENTITY;
            SS.traced.path.l.Clear();
            InvalidateGraphics();
            break;
        }

        default: ssassert(false, "Unexpected menu ID");
    }
}

void SolveSpaceUI::ShowNakedEdges(bool reportOnlyWhenNotOkay) {
    SS.nakedEdges.Clear();

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    SMesh *m = &(g->displayMesh);
    SKdNode *root = SKdNode::From(m);
    bool inters, leaks;
    root->MakeCertainEdgesInto(&(SS.nakedEdges),
        EdgeKind::NAKED_OR_SELF_INTER, /*coplanarIsInter=*/true, &inters, &leaks);

    if(reportOnlyWhenNotOkay && !inters && !leaks && SS.nakedEdges.l.n == 0) {
        return;
    }
    InvalidateGraphics();

    const char *intersMsg = inters ?
        "The mesh is self-intersecting (NOT okay, invalid)." :
        "The mesh is not self-intersecting (okay, valid).";
    const char *leaksMsg = leaks ?
        "The mesh has naked edges (NOT okay, invalid)." :
        "The mesh is watertight (okay, valid).";

    std::string cntMsg = ssprintf("\n\nThe model contains %d triangles, from "
                    "%d surfaces.", g->displayMesh.l.n, g->runningShell.surface.n);

    if(SS.nakedEdges.l.n == 0) {
        Message("%s\n\n%s\n\nZero problematic edges, good.%s",
            intersMsg, leaksMsg, cntMsg.c_str());
    } else {
        Error("%s\n\n%s\n\n%d problematic edges, bad.%s",
            intersMsg, leaksMsg, SS.nakedEdges.l.n, cntMsg.c_str());
    }
}

void SolveSpaceUI::MenuHelp(Command id) {
    if((uint32_t)id >= (uint32_t)Command::LOCALE &&
       (uint32_t)id < ((uint32_t)Command::LOCALE + Locales().size())) {
        size_t offset = (uint32_t)id - (uint32_t)Command::LOCALE;
        size_t i = 0;
        for(auto locale : Locales()) {
            if(i++ == offset) {
                CnfFreezeString(locale.Culture(), "Locale");
                SetLocale(locale.Culture());
                break;
            }
        }
        return;
    }

    switch(id) {
        case Command::WEBSITE:
            OpenWebsite("http://solvespace.com/helpmenu");
            break;

        case Command::ABOUT:
            Message(
"This is SolveSpace version " PACKAGE_VERSION ".\n"
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
"© 2008-2016 Jonathan Westhues and other authors.\n"
);
            break;

        default: ssassert(false, "Unexpected menu ID");
    }
}

void SolveSpaceUI::Clear() {
    sys.Clear();
    for(int i = 0; i < MAX_UNDO; i++) {
        if(i < undo.cnt) undo.d[i].Clear();
        if(i < redo.cnt) redo.d[i].Clear();
    }
}

void Sketch::Clear() {
    group.Clear();
    groupOrder.Clear();
    constraint.Clear();
    request.Clear();
    style.Clear();
    entity.Clear();
    param.Clear();
}

BBox Sketch::CalculateEntityBBox(bool includingInvisible) {
    BBox box = {};
    bool first = true;

    auto includePoint = [&](const Vector &point) {
        if(first) {
            box.minp = point;
            box.maxp = point;
            first = false;
        } else {
            box.Include(point);
        }
    };

    for(const Entity &e : entity) {
        if(e.construction) continue;
        if(!(includingInvisible || e.IsVisible())) continue;

        if(e.IsPoint()) {
            includePoint(e.PointGetNum());
            continue;
        }

        switch(e.type) {
            // Circles and arcs are special cases. We calculate their bounds
            // based on Bezier curve bounds. This is not exact for arcs,
            // but the implementation is rather simple.
            case Entity::Type::CIRCLE:
            case Entity::Type::ARC_OF_CIRCLE: {
                SBezierList sbl = {};
                e.GenerateBezierCurves(&sbl);

                for(const SBezier &sb : sbl.l) {
                    for(int j = 0; j <= sb.deg; j++) {
                        includePoint(sb.ctrl[j]);
                    }
                }
                sbl.Clear();
                continue;
            }

            default:
                continue;
        }
    }

    return box;
}

Group *Sketch::GetRunningMeshGroupFor(hGroup h) {
    Group *g = GetGroup(h);
    while(g != NULL) {
        if(g->IsMeshGroup()) {
            return g;
        }
        g = g->PreviousGroup();
    }
    return NULL;
}
