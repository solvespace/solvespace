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

void SolveSpaceUI::Init() {
#if !defined(HEADLESS)
    // Check that the resource system works.
    dbp("%s", LoadString("banner.txt").data());
#endif

    Platform::SettingsRef settings = Platform::GetSettings();

    SS.tangentArcRadius = 10.0;

    // Then, load the registry settings.
    // Default list of colors for the model material
    modelColor[0] = settings->ThawColor("ModelColor_0", RGBi(150, 150, 150));
    modelColor[1] = settings->ThawColor("ModelColor_1", RGBi(100, 100, 100));
    modelColor[2] = settings->ThawColor("ModelColor_2", RGBi( 30,  30,  30));
    modelColor[3] = settings->ThawColor("ModelColor_3", RGBi(150,   0,   0));
    modelColor[4] = settings->ThawColor("ModelColor_4", RGBi(  0, 100,   0));
    modelColor[5] = settings->ThawColor("ModelColor_5", RGBi(  0,  80,  80));
    modelColor[6] = settings->ThawColor("ModelColor_6", RGBi(  0,   0, 130));
    modelColor[7] = settings->ThawColor("ModelColor_7", RGBi( 80,   0,  80));
    // Light intensities
    lightIntensity[0] = settings->ThawFloat("LightIntensity_0", 1.0);
    lightIntensity[1] = settings->ThawFloat("LightIntensity_1", 0.5);
    ambientIntensity = 0.3; // no setting for that yet
    // Light positions
    lightDir[0].x = settings->ThawFloat("LightDir_0_Right",   -1.0);
    lightDir[0].y = settings->ThawFloat("LightDir_0_Up",       1.0);
    lightDir[0].z = settings->ThawFloat("LightDir_0_Forward",  0.0);
    lightDir[1].x = settings->ThawFloat("LightDir_1_Right",    1.0);
    lightDir[1].y = settings->ThawFloat("LightDir_1_Up",       0.0);
    lightDir[1].z = settings->ThawFloat("LightDir_1_Forward",  0.0);

    exportMode = false;
    // Chord tolerance
    chordTol = settings->ThawFloat("ChordTolerancePct", 0.5);
    // Max pwl segments to generate
    maxSegments = settings->ThawInt("MaxSegments", 10);
    // Chord tolerance
    exportChordTol = settings->ThawFloat("ExportChordTolerance", 0.1);
    // Max pwl segments to generate
    exportMaxSegments = settings->ThawInt("ExportMaxSegments", 64);
    // View units
    viewUnits = (Unit)settings->ThawInt("ViewUnits", (uint32_t)Unit::MM);
    // Number of digits after the decimal point
    afterDecimalMm = settings->ThawInt("AfterDecimalMm", 2);
    afterDecimalInch = settings->ThawInt("AfterDecimalInch", 3);
    afterDecimalDegree = settings->ThawInt("AfterDecimalDegree", 2);
    useSIPrefixes = settings->ThawBool("UseSIPrefixes", false);
    // Camera tangent (determines perspective)
    cameraTangent = settings->ThawFloat("CameraTangent", 0.3f/1e3);
    // Grid spacing
    gridSpacing = settings->ThawFloat("GridSpacing", 5.0);
    // Export scale factor
    exportScale = settings->ThawFloat("ExportScale", 1.0);
    // Export offset (cutter radius comp)
    exportOffset = settings->ThawFloat("ExportOffset", 0.0);
    // Rewrite exported colors close to white into black (assuming white bg)
    fixExportColors = settings->ThawBool("FixExportColors", true);
    // Draw back faces of triangles (when mesh is leaky/self-intersecting)
    drawBackFaces = settings->ThawBool("DrawBackFaces", true);
    // Use turntable mouse navigation
    turntableNav = settings->ThawBool("TurntableNav", false);
    // Check that contours are closed and not self-intersecting
    checkClosedContour = settings->ThawBool("CheckClosedContour", true);
    // Enable automatic constrains for lines
    automaticLineConstraints = settings->ThawBool("AutomaticLineConstraints", true);
    // Draw closed polygons areas
    showContourAreas = settings->ThawBool("ShowContourAreas", false);
    // Export shaded triangles in a 2d view
    exportShadedTriangles = settings->ThawBool("ExportShadedTriangles", true);
    // Export pwl curves (instead of exact) always
    exportPwlCurves = settings->ThawBool("ExportPwlCurves", false);
    // Background color on-screen
    backgroundColor = settings->ThawColor("BackgroundColor", RGBi(0, 0, 0));
    // Whether export canvas size is fixed or derived from bbox
    exportCanvasSizeAuto = settings->ThawBool("ExportCanvasSizeAuto", true);
    // Margins for automatic canvas size
    exportMargin.left   = settings->ThawFloat("ExportMargin_Left",   5.0);
    exportMargin.right  = settings->ThawFloat("ExportMargin_Right",  5.0);
    exportMargin.bottom = settings->ThawFloat("ExportMargin_Bottom", 5.0);
    exportMargin.top    = settings->ThawFloat("ExportMargin_Top",    5.0);
    // Dimensions for fixed canvas size
    exportCanvas.width  = settings->ThawFloat("ExportCanvas_Width",  100.0);
    exportCanvas.height = settings->ThawFloat("ExportCanvas_Height", 100.0);
    exportCanvas.dx     = settings->ThawFloat("ExportCanvas_Dx",     5.0);
    exportCanvas.dy     = settings->ThawFloat("ExportCanvas_Dy",     5.0);
    // Extra parameters when exporting G code
    gCode.depth         = settings->ThawFloat("GCode_Depth", 10.0);
    gCode.passes        = settings->ThawInt("GCode_Passes", 1);
    gCode.feed          = settings->ThawFloat("GCode_Feed", 10.0);
    gCode.plungeFeed    = settings->ThawFloat("GCode_PlungeFeed", 10.0);
    // Show toolbar in the graphics window
    showToolbar = settings->ThawBool("ShowToolbar", true);
    // Recent files menus
    for(size_t i = 0; i < MAX_RECENT; i++) {
        std::string rawPath = settings->ThawString("RecentFile_" + std::to_string(i), "");
        if(rawPath.empty()) continue;
        recentFiles.push_back(Platform::Path::From(rawPath));
    }
    // Autosave timer
    autosaveInterval = settings->ThawInt("AutosaveInterval", 5);
    // Locale
    std::string locale = settings->ThawString("Locale", "");
    if(!locale.empty()) {
        SetLocale(locale);
    }

    generateAllTimer = Platform::CreateTimer();
    generateAllTimer->onTimeout = std::bind(&SolveSpaceUI::GenerateAll, &SS, Generate::DIRTY,
                                            /*andFindFree=*/false, /*genForBBox=*/false);

    showTWTimer = Platform::CreateTimer();
    showTWTimer->onTimeout = std::bind(&TextWindow::Show, &TW);

    autosaveTimer = Platform::CreateTimer();
    autosaveTimer->onTimeout = std::bind(&SolveSpaceUI::Autosave, &SS);

    // The default styles (colors, line widths, etc.) are also stored in the
    // configuration file, but we will automatically load those as we need
    // them.

    ScheduleAutosave();

    NewFile();
    AfterNewFile();

    if(TW.window && GW.window) {
        TW.window->ThawPosition(settings, "TextWindow");
        GW.window->ThawPosition(settings, "GraphicsWindow");
        TW.window->SetVisible(true);
        GW.window->SetVisible(true);
        GW.window->Focus();

        // Do this once the window is created.
        Request3DConnexionEventsForWindow(GW.window);
    }
}

bool SolveSpaceUI::LoadAutosaveFor(const Platform::Path &filename) {
    Platform::Path autosaveFile = filename.WithExtension(AUTOSAVE_EXT);

    FILE *f = OpenFile(autosaveFile, "rb");
    if(!f)
        return false;
    fclose(f);

    Platform::MessageDialogRef dialog = CreateMessageDialog(GW.window);

    using Platform::MessageDialog;
    dialog->SetType(MessageDialog::Type::QUESTION);
    dialog->SetTitle(C_("title", "Autosave Available"));
    dialog->SetMessage(C_("dialog", "An autosave file is available for this sketch."));
    dialog->SetDescription(C_("dialog", "Do you want to load the autosave file instead?"));
    dialog->AddButton(C_("button", "&Load autosave"), MessageDialog::Response::YES,
                      /*isDefault=*/true);
    dialog->AddButton(C_("button", "Do&n't Load"), MessageDialog::Response::NO);

    // FIXME(async): asyncify this call
    if(dialog->RunModal() == MessageDialog::Response::YES) {
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
    Platform::SettingsRef settings = Platform::GetSettings();

    GW.window->FreezePosition(settings, "GraphicsWindow");
    TW.window->FreezePosition(settings, "TextWindow");

    // Recent files
    for(size_t i = 0; i < MAX_RECENT; i++) {
        std::string rawPath;
        if(recentFiles.size() > i) {
            rawPath = recentFiles[i].raw;
        }
        settings->FreezeString("RecentFile_" + std::to_string(i), rawPath);
    }
    // Model colors
    for(size_t i = 0; i < MODEL_COLORS; i++)
        settings->FreezeColor("ModelColor_" + std::to_string(i), modelColor[i]);
    // Light intensities
    settings->FreezeFloat("LightIntensity_0", (float)lightIntensity[0]);
    settings->FreezeFloat("LightIntensity_1", (float)lightIntensity[1]);
    // Light directions
    settings->FreezeFloat("LightDir_0_Right",   (float)lightDir[0].x);
    settings->FreezeFloat("LightDir_0_Up",      (float)lightDir[0].y);
    settings->FreezeFloat("LightDir_0_Forward", (float)lightDir[0].z);
    settings->FreezeFloat("LightDir_1_Right",   (float)lightDir[1].x);
    settings->FreezeFloat("LightDir_1_Up",      (float)lightDir[1].y);
    settings->FreezeFloat("LightDir_1_Forward", (float)lightDir[1].z);
    // Chord tolerance
    settings->FreezeFloat("ChordTolerancePct", (float)chordTol);
    // Max pwl segments to generate
    settings->FreezeInt("MaxSegments", (uint32_t)maxSegments);
    // Export Chord tolerance
    settings->FreezeFloat("ExportChordTolerance", (float)exportChordTol);
    // Export Max pwl segments to generate
    settings->FreezeInt("ExportMaxSegments", (uint32_t)exportMaxSegments);
    // View units
    settings->FreezeInt("ViewUnits", (uint32_t)viewUnits);
    // Number of digits after the decimal point
    settings->FreezeInt("AfterDecimalMm",   (uint32_t)afterDecimalMm);
    settings->FreezeInt("AfterDecimalInch", (uint32_t)afterDecimalInch);
    settings->FreezeInt("AfterDecimalDegree", (uint32_t)afterDecimalDegree);
    settings->FreezeBool("UseSIPrefixes", useSIPrefixes);
    // Camera tangent (determines perspective)
    settings->FreezeFloat("CameraTangent", (float)cameraTangent);
    // Grid spacing
    settings->FreezeFloat("GridSpacing", gridSpacing);
    // Export scale
    settings->FreezeFloat("ExportScale", exportScale);
    // Export offset (cutter radius comp)
    settings->FreezeFloat("ExportOffset", exportOffset);
    // Rewrite exported colors close to white into black (assuming white bg)
    settings->FreezeBool("FixExportColors", fixExportColors);
    // Draw back faces of triangles (when mesh is leaky/self-intersecting)
    settings->FreezeBool("DrawBackFaces", drawBackFaces);
    // Draw closed polygons areas
    settings->FreezeBool("ShowContourAreas", showContourAreas);
    // Check that contours are closed and not self-intersecting
    settings->FreezeBool("CheckClosedContour", checkClosedContour);
    // Use turntable mouse navigation
    settings->FreezeBool("TurntableNav", turntableNav);
    // Enable automatic constrains for lines
    settings->FreezeBool("AutomaticLineConstraints", automaticLineConstraints);
    // Export shaded triangles in a 2d view
    settings->FreezeBool("ExportShadedTriangles", exportShadedTriangles);
    // Export pwl curves (instead of exact) always
    settings->FreezeBool("ExportPwlCurves", exportPwlCurves);
    // Background color on-screen
    settings->FreezeColor("BackgroundColor", backgroundColor);
    // Whether export canvas size is fixed or derived from bbox
    settings->FreezeBool("ExportCanvasSizeAuto", exportCanvasSizeAuto);
    // Margins for automatic canvas size
    settings->FreezeFloat("ExportMargin_Left",   exportMargin.left);
    settings->FreezeFloat("ExportMargin_Right",  exportMargin.right);
    settings->FreezeFloat("ExportMargin_Bottom", exportMargin.bottom);
    settings->FreezeFloat("ExportMargin_Top",    exportMargin.top);
    // Dimensions for fixed canvas size
    settings->FreezeFloat("ExportCanvas_Width",  exportCanvas.width);
    settings->FreezeFloat("ExportCanvas_Height", exportCanvas.height);
    settings->FreezeFloat("ExportCanvas_Dx",     exportCanvas.dx);
    settings->FreezeFloat("ExportCanvas_Dy",     exportCanvas.dy);
     // Extra parameters when exporting G code
    settings->FreezeFloat("GCode_Depth", gCode.depth);
    settings->FreezeInt("GCode_Passes", gCode.passes);
    settings->FreezeFloat("GCode_Feed", gCode.feed);
    settings->FreezeFloat("GCode_PlungeFeed", gCode.plungeFeed);
    // Show toolbar in the graphics window
    settings->FreezeBool("ShowToolbar", showToolbar);
    // Autosave timer
    settings->FreezeInt("AutosaveInterval", autosaveInterval);

    // And the default styles, colors and line widths and such.
    Style::FreezeDefaultStyles(settings);

    Platform::ExitGui();
}

void SolveSpaceUI::ScheduleGenerateAll() {
    generateAllTimer->RunAfterProcessingEvents();
}

void SolveSpaceUI::ScheduleShowTW() {
    showTWTimer->RunAfterProcessingEvents();
}

void SolveSpaceUI::ScheduleAutosave() {
    autosaveTimer->RunAfter(autosaveInterval * 60 * 1000);
}

double SolveSpaceUI::MmPerUnit() {
    switch(viewUnits) {
        case Unit::INCHES: return 25.4;
        case Unit::METERS: return 1000.0;
        case Unit::MM: return 1.0;
    }
    return 1.0;
}
const char *SolveSpaceUI::UnitName() {
    switch(viewUnits) {
        case Unit::INCHES: return "in";
        case Unit::METERS: return "m";
        case Unit::MM: return "mm";
    }
    return "";
}

std::string SolveSpaceUI::MmToString(double v) {
    v /= MmPerUnit();
    switch(viewUnits) {
        case Unit::INCHES:
            return ssprintf("%.*f", afterDecimalInch, v);
        case Unit::METERS:
        case Unit::MM:
            return ssprintf("%.*f", afterDecimalMm, v);
    }
    return "";
}
static const char *DimToString(int dim) {
    switch(dim) {
        case 3: return "³";
        case 2: return "²";
        case 1: return "";
        default: ssassert(false, "Unexpected dimension");
    }
}
static std::pair<int, std::string> SelectSIPrefixMm(int deg) {
         if(deg >=  3) return {  3, "km" };
    else if(deg >=  0) return {  0, "m"  };
    else if(deg >= -2) return { -2, "cm" };
    else if(deg >= -3) return { -3, "mm" };
    else if(deg >= -6) return { -6, "µm" };
    else               return { -9, "nm" };
}
static std::pair<int, std::string> SelectSIPrefixInch(int deg) {
         if(deg >=  0) return {  0, "in"  };
    else if(deg >= -3) return { -3, "mil" };
    else               return { -6, "µin" };
}
std::string SolveSpaceUI::MmToStringSI(double v, int dim) {
    bool compact = false;
    if(dim == 0) {
        if(!useSIPrefixes) return MmToString(v);
        compact = true;
        dim = 1;
    }

    v /= pow((viewUnits == Unit::INCHES) ? 25.4 : 1000, dim);
    int vdeg = floor((log10(fabs(v))) / dim);
    std::string unit;
    if(fabs(v) > 0.0) {
        int sdeg = 0;
        std::tie(sdeg, unit) =
            (viewUnits == Unit::INCHES)
            ? SelectSIPrefixInch(vdeg)
            : SelectSIPrefixMm(vdeg);
        v /= pow(10.0, sdeg * dim);
    }
    int pdeg = ceil(log10(fabs(v) + 1e-10));
    return ssprintf("%#.*g%s%s%s", pdeg + UnitDigitsAfterDecimal(), v,
                    compact ? "" : " ", unit.c_str(), DimToString(dim));
}
std::string SolveSpaceUI::DegreeToString(double v) {
    if(fabs(v - floor(v)) > 1e-10) {
        return ssprintf("%.*f", afterDecimalDegree, v);
    } else {
        return ssprintf("%.0f", v);
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

    GW.Init();
    TW.Init();

    unsaved = false;

    GW.ZoomToFit();

    // Create all the default styles; they'll get created on the fly anyways,
    // but can't hurt to do it now.
    Style::CreateAllDefaultStyles();

    UpdateWindowTitles();
}

void SolveSpaceUI::AddToRecentList(const Platform::Path &filename) {
    auto it = std::find_if(recentFiles.begin(), recentFiles.end(),
                           [&](const Platform::Path &p) { return p.Equals(filename); });
    if(it != recentFiles.end()) {
        recentFiles.erase(it);
    }

    if(recentFiles.size() > MAX_RECENT) {
        recentFiles.erase(recentFiles.begin() + MAX_RECENT);
    }

    recentFiles.insert(recentFiles.begin(), filename);
    GW.PopulateRecentFiles();
}

bool SolveSpaceUI::GetFilenameAndSave(bool saveAs) {
    Platform::SettingsRef settings = Platform::GetSettings();
    Platform::Path newSaveFile = saveFile;

    if(saveAs || saveFile.IsEmpty()) {
        Platform::FileDialogRef dialog = Platform::CreateSaveFileDialog(GW.window);
        dialog->AddFilter(C_("file-type", "SolveSpace models"), { "slvs" });
        dialog->ThawChoices(settings, "Sketch");
        if(!newSaveFile.IsEmpty()) {
            dialog->SetFilename(newSaveFile);
        }
        if(dialog->RunModal()) {
            dialog->FreezeChoices(settings, "Sketch");
            newSaveFile = dialog->GetFilename();
        } else {
            return false;
        }
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

void SolveSpaceUI::Autosave()
{
    ScheduleAutosave();

    if(!saveFile.IsEmpty() && unsaved) {
        SaveToFile(saveFile.WithExtension(AUTOSAVE_EXT));
    }
}

void SolveSpaceUI::RemoveAutosave()
{
    Platform::Path autosaveFile = saveFile.WithExtension(AUTOSAVE_EXT);
    RemoveFile(autosaveFile);
}

bool SolveSpaceUI::OkayToStartNewFile() {
    if(!unsaved) return true;

    Platform::MessageDialogRef dialog = CreateMessageDialog(GW.window);

    using Platform::MessageDialog;
    dialog->SetType(MessageDialog::Type::QUESTION);
    dialog->SetTitle(C_("title", "Modified File"));
    if(!SolveSpace::SS.saveFile.IsEmpty()) {
        dialog->SetMessage(ssprintf(C_("dialog", "Do you want to save the changes you made to "
                                                 "the sketch “%s”?"), saveFile.raw.c_str()));
    } else {
        dialog->SetMessage(C_("dialog", "Do you want to save the changes you made to "
                                        "the new sketch?"));
    }
    dialog->SetDescription(C_("dialog", "Your changes will be lost if you don't save them."));
    dialog->AddButton(C_("button", "&Save"), MessageDialog::Response::YES,
                      /*isDefault=*/true);
    dialog->AddButton(C_("button", "Do&n't Save"), MessageDialog::Response::NO);
    dialog->AddButton(C_("button", "&Cancel"), MessageDialog::Response::CANCEL);

    // FIXME(async): asyncify this call
    switch(dialog->RunModal()) {
        case MessageDialog::Response::YES:
            return GetFilenameAndSave(/*saveAs=*/false);

        case MessageDialog::Response::NO:
            RemoveAutosave();
            return true;

        default:
            return false;
    }
}

void SolveSpaceUI::UpdateWindowTitles() {
    if(!GW.window || !TW.window) return;

    if(saveFile.IsEmpty()) {
        GW.window->SetTitle(C_("title", "(new sketch)"));
    } else {
        if(!GW.window->SetTitleForFilename(saveFile)) {
            GW.window->SetTitle(saveFile.raw);
        }
    }

    TW.window->SetTitle(C_("title", "Property Browser"));
}

void SolveSpaceUI::MenuFile(Command id) {
    Platform::SettingsRef settings = Platform::GetSettings();

    switch(id) {
        case Command::NEW:
            if(!SS.OkayToStartNewFile()) break;

            SS.saveFile.Clear();
            SS.NewFile();
            SS.AfterNewFile();
            break;

        case Command::OPEN: {
            if(!SS.OkayToStartNewFile()) break;

            Platform::FileDialogRef dialog = Platform::CreateOpenFileDialog(SS.GW.window);
            dialog->AddFilters(Platform::SolveSpaceModelFileFilters);
            dialog->ThawChoices(settings, "Sketch");
            if(dialog->RunModal()) {
                dialog->FreezeChoices(settings, "Sketch");
                SS.Load(dialog->GetFilename());
            }
            break;
        }

        case Command::SAVE:
            SS.GetFilenameAndSave(/*saveAs=*/false);
            break;

        case Command::SAVE_AS:
            SS.GetFilenameAndSave(/*saveAs=*/true);
            break;

        case Command::EXPORT_IMAGE: {
            Platform::FileDialogRef dialog = Platform::CreateSaveFileDialog(SS.GW.window);
            dialog->AddFilters(Platform::RasterFileFilters);
            dialog->ThawChoices(settings, "ExportImage");
            if(dialog->RunModal()) {
                dialog->FreezeChoices(settings, "ExportImage");
                SS.ExportAsPngTo(dialog->GetFilename());
            }
            break;
        }

        case Command::EXPORT_VIEW: {
            Platform::FileDialogRef dialog = Platform::CreateSaveFileDialog(SS.GW.window);
            dialog->AddFilters(Platform::VectorFileFilters);
            dialog->ThawChoices(settings, "ExportView");
            if(!dialog->RunModal()) break;
            dialog->FreezeChoices(settings, "ExportView");

            // If the user is exporting something where it would be
            // inappropriate to include the constraints, then warn.
            if(SS.GW.showConstraints &&
                (dialog->GetFilename().HasExtension("txt") ||
                 fabs(SS.exportOffset) > LENGTH_EPS))
            {
                Message(_("Constraints are currently shown, and will be exported "
                          "in the toolpath. This is probably not what you want; "
                          "hide them by clicking the link at the top of the "
                          "text window."));
            }

            SS.ExportViewOrWireframeTo(dialog->GetFilename(), /*exportWireframe=*/false);
            break;
        }

        case Command::EXPORT_WIREFRAME: {
            Platform::FileDialogRef dialog = Platform::CreateSaveFileDialog(SS.GW.window);
            dialog->AddFilters(Platform::Vector3dFileFilters);
            dialog->ThawChoices(settings, "ExportWireframe");
            if(!dialog->RunModal()) break;
            dialog->FreezeChoices(settings, "ExportWireframe");

            SS.ExportViewOrWireframeTo(dialog->GetFilename(), /*exportWireframe*/true);
            break;
        }

        case Command::EXPORT_SECTION: {
            Platform::FileDialogRef dialog = Platform::CreateSaveFileDialog(SS.GW.window);
            dialog->AddFilters(Platform::VectorFileFilters);
            dialog->ThawChoices(settings, "ExportSection");
            if(!dialog->RunModal()) break;
            dialog->FreezeChoices(settings, "ExportSection");

            SS.ExportSectionTo(dialog->GetFilename());
            break;
        }

        case Command::EXPORT_MESH: {
            Platform::FileDialogRef dialog = Platform::CreateSaveFileDialog(SS.GW.window);
            dialog->AddFilters(Platform::MeshFileFilters);
            dialog->ThawChoices(settings, "ExportMesh");
            if(!dialog->RunModal()) break;
            dialog->FreezeChoices(settings, "ExportMesh");

            SS.ExportMeshTo(dialog->GetFilename());
            break;
        }

        case Command::EXPORT_SURFACES: {
            Platform::FileDialogRef dialog = Platform::CreateSaveFileDialog(SS.GW.window);
            dialog->AddFilters(Platform::SurfaceFileFilters);
            dialog->ThawChoices(settings, "ExportSurfaces");
            if(!dialog->RunModal()) break;
            dialog->FreezeChoices(settings, "ExportSurfaces");

            StepFileWriter sfw = {};
            sfw.ExportSurfacesTo(dialog->GetFilename());
            break;
        }

        case Command::IMPORT: {
            Platform::FileDialogRef dialog = Platform::CreateOpenFileDialog(SS.GW.window);
            dialog->AddFilters(Platform::ImportFileFilters);
            dialog->ThawChoices(settings, "Import");
            if(!dialog->RunModal()) break;
            dialog->FreezeChoices(settings, "Import");

            Platform::Path importFile = dialog->GetFilename();
            if(importFile.HasExtension("dxf")) {
                ImportDxf(importFile);
            } else if(importFile.HasExtension("dwg")) {
                ImportDwg(importFile);
            } else {
                Error(_("Can't identify file type from file extension of "
                        "filename '%s'; try .dxf or .dwg."), importFile.raw.c_str());
                break;
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

    SS.UpdateWindowTitles();
}

void SolveSpaceUI::MenuAnalyze(Command id) {
    Platform::SettingsRef settings = Platform::GetSettings();

    SS.GW.GroupSelection();
    auto const &gs = SS.GW.gs;

    switch(id) {
        case Command::STEP_DIM:
            if(gs.constraints == 1 && gs.n == 0) {
                Constraint *c = SK.GetConstraint(gs.constraint[0]);
                if(c->HasLabel() && !c->reference) {
                    SS.TW.stepDim.finish = c->valA;
                    SS.TW.stepDim.steps = 10;
                    SS.TW.stepDim.isDistance =
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

            SS.GW.Invalidate();

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
            SS.GW.Invalidate();
            break;
        }

        case Command::VOLUME: {
            SMesh *m = &(SK.GetGroup(SS.GW.activeGroup)->displayMesh);

            double vol = 0;
            int i;
            for(i = 0; i < m->l.n; i++) {
                STriangle tr = m->l[i];

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
            Message(_("The volume of the solid model is:\n\n"
                      "    %s\n\n"
                      "Curved surfaces have been approximated as triangles.\n"
                      "This introduces error, typically of around 1%%."),
                SS.MmToStringSI(vol, /*dim=*/3).c_str());
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
            Message(_("The area of the region sketched in this group is:\n\n"
                      "    %s\n\n"
                      "Curves have been approximated as piecewise linear.\n"
                      "This introduces error, typically of around 1%%."),
                SS.MmToStringSI(area, /*dim=*/2).c_str());
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
                Message(_("The total length of the selected entities is:\n\n"
                          "    %s\n\n"
                          "Curves have been approximated as piecewise linear.\n"
                          "This introduces error, typically of around 1%%."),
                    SS.MmToStringSI(perimeter, /*dim=*/1).c_str());
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
            Platform::FileDialogRef dialog = Platform::CreateSaveFileDialog(SS.GW.window);
            dialog->AddFilters(Platform::CsvFileFilters);
            dialog->ThawChoices(settings, "Trace");
            if(dialog->RunModal()) {
                dialog->FreezeChoices(settings, "Trace");

                FILE *f = OpenFile(dialog->GetFilename(), "wb");
                if(f) {
                    int i;
                    SContour *sc = &(SS.traced.path);
                    for(i = 0; i < sc->l.n; i++) {
                        Vector p = sc->l[i].p;
                        double s = SS.exportScale;
                        fprintf(f, "%.10f, %.10f, %.10f\r\n",
                            p.x/s, p.y/s, p.z/s);
                    }
                    fclose(f);
                } else {
                    Error(_("Couldn't write to '%s'"), dialog->GetFilename().raw.c_str());
                }
            }
            // Clear the trace, and stop tracing
            SS.traced.point = Entity::NO_ENTITY;
            SS.traced.path.l.Clear();
            SS.GW.Invalidate();
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

    if(reportOnlyWhenNotOkay && !inters && !leaks && SS.nakedEdges.l.IsEmpty()) {
        return;
    }
    SS.GW.Invalidate();

    const char *intersMsg = inters ?
        _("The mesh is self-intersecting (NOT okay, invalid).") :
        _("The mesh is not self-intersecting (okay, valid).");
    const char *leaksMsg = leaks ?
        _("The mesh has naked edges (NOT okay, invalid).") :
        _("The mesh is watertight (okay, valid).");

    std::string cntMsg = ssprintf(
        _("\n\nThe model contains %d triangles, from %d surfaces."),
        g->displayMesh.l.n, g->runningShell.surface.n);

    if(SS.nakedEdges.l.IsEmpty()) {
        Message(_("%s\n\n%s\n\nZero problematic edges, good.%s"),
            intersMsg, leaksMsg, cntMsg.c_str());
    } else {
        Error(_("%s\n\n%s\n\n%d problematic edges, bad.%s"),
            intersMsg, leaksMsg, SS.nakedEdges.l.n, cntMsg.c_str());
    }
}

void SolveSpaceUI::MenuHelp(Command id) {
    switch(id) {
        case Command::WEBSITE:
            Platform::OpenInBrowser("http://solvespace.com/helpmenu");
            break;

        case Command::ABOUT:
            Message(_(
"This is SolveSpace version %s.\n"
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
"© 2008-%d Jonathan Westhues and other authors.\n"),
PACKAGE_VERSION, 2019);
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
    TW.window = NULL;
    GW.openRecentMenu = NULL;
    GW.linkRecentMenu = NULL;
    GW.showGridMenuItem = NULL;
    GW.perspectiveProjMenuItem = NULL;
    GW.showToolbarMenuItem = NULL;
    GW.showTextWndMenuItem = NULL;
    GW.fullScreenMenuItem = NULL;
    GW.unitsMmMenuItem = NULL;
    GW.unitsMetersMenuItem = NULL;
    GW.unitsInchesMenuItem = NULL;
    GW.inWorkplaneMenuItem = NULL;
    GW.in3dMenuItem = NULL;
    GW.undoMenuItem = NULL;
    GW.redoMenuItem = NULL;
    GW.window = NULL;
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

        // arc center point shouldn't be included in bounding box calculation
        if(e.IsPoint() && e.h.isFromRequest()) {
            Request *r = SK.GetRequest(e.h.request());
            if(r->type == Request::Type::ARC_OF_CIRCLE && e.h == r->h.entity(1)) {
                continue;
            }
        }

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
