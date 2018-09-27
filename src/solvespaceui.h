//
// Created by benjamin on 9/26/18.
//

#ifndef SOLVESPACE_SOLVESPACEUI_H
#define SOLVESPACE_SOLVESPACEUI_H

#include <map>
#include <memory>

#include "ui.h"
#include "sketch.h"
#include "platform.h"
#include "resource.h"
#include "list.h"
#include "system.h"

namespace SolveSpace {

enum class Unit : uint32_t {
    MM = 0,
    INCHES,
    METERS
};

class SolveSpaceUI {
public:
    TextWindow *pTW;
    TextWindow &TW;
    GraphicsWindow GW;

    // The state for undo/redo
    typedef struct {
        IdList <Group, hGroup> group;
        List <hGroup> groupOrder;
        IdList <Request, hRequest> request;
        IdList <Constraint, hConstraint> constraint;
        IdList <Param, hParam> param;
        IdList <Style, hStyle> style;
        hGroup activeGroup;

        void Clear() {
            group.Clear();
            request.Clear();
            constraint.Clear();
            param.Clear();
            style.Clear();
        }
    } UndoState;
    enum {
        MAX_UNDO = 16
    };
    typedef struct {
        UndoState d[MAX_UNDO];
        int cnt;
        int write;
    } UndoStack;
    UndoStack undo;
    UndoStack redo;

    std::map<Platform::Path, std::shared_ptr<Pixmap>, Platform::PathLess> images;

    bool ReloadLinkedImage(const Platform::Path &saveFile, Platform::Path *filename,
                           bool canCancel);

    void UndoEnableMenus();

    void UndoRemember();

    void UndoUndo();

    void UndoRedo();

    void PushFromCurrentOnto(UndoStack *uk);

    void PopOntoCurrentFrom(UndoStack *uk);

    void UndoClearState(UndoState *ut);

    void UndoClearStack(UndoStack *uk);

    // Little bits of extra configuration state
    enum {
        MODEL_COLORS = 8
    };
    RgbaColor modelColor[MODEL_COLORS];
    Vector lightDir[2];
    double lightIntensity[2];
    double ambientIntensity;
    double chordTol;
    double chordTolCalculated;
    int maxSegments;
    double exportChordTol;
    int exportMaxSegments;
    double cameraTangent;
    double gridSpacing;
    double exportScale;
    double exportOffset;
    bool fixExportColors;
    bool drawBackFaces;
    bool showContourAreas;
    bool checkClosedContour;
    bool showToolbar;
    Platform::Path screenshotFile;
    RgbaColor backgroundColor;
    bool exportShadedTriangles;
    bool exportPwlCurves;
    bool exportCanvasSizeAuto;
    bool exportMode;
    struct {
        double left;
        double right;
        double bottom;
        double top;
    } exportMargin;
    struct {
        double width;
        double height;
        double dx;
        double dy;
    } exportCanvas;
    struct {
        double depth;
        int passes;
        double feed;
        double plungeFeed;
    } gCode;

    Unit viewUnits;
    int afterDecimalMm;
    int afterDecimalInch;
    int autosaveInterval; // in minutes

    std::string MmToString(double v);

    double ExprToMm(Expr *e);

    double StringToMm(const std::string &s);

    const char *UnitName();

    double MmPerUnit();

    int UnitDigitsAfterDecimal();

    void SetUnitDigitsAfterDecimal(int v);

    double ChordTolMm();

    double ExportChordTolMm();

    int GetMaxSegments();

    bool usePerspectiveProj;

    double CameraTangent();

    // Some stuff relating to the tangent arcs created non-parametrically
    // as special requests.
    double tangentArcRadius;
    bool tangentArcManual;
    bool tangentArcDeleteOld;

    // The platform-dependent code calls this before entering the msg loop
    void Init();

    void Exit();

    // File load/save routines, including the additional files that get
    // loaded when we have link groups.
    FILE *fh;

    void AfterNewFile();

    void AddToRecentList(const Platform::Path &filename);

    Platform::Path saveFile;
    bool fileLoadError;
    bool unsaved;
    typedef struct {
        char type;
        const char *desc;
        char fmt;
        void *ptr;
    } SaveTable;
    static const SaveTable SAVED[];

    void SaveUsingTable(const Platform::Path &filename, int type);

    void LoadUsingTable(const Platform::Path &filename, char *key, char *val);

    struct {
        Group g;
        Request r;
        Entity e;
        Param p;
        Constraint c;
        Style s;
    } sv;

    static void MenuFile(Command id);

    void Autosave();

    void RemoveAutosave();

    static const size_t MAX_RECENT = 8;
    std::vector<Platform::Path> recentFiles;

    bool Load(const Platform::Path &filename);

    bool GetFilenameAndSave(bool saveAs);

    bool OkayToStartNewFile();

    hGroup CreateDefaultDrawingGroup();

    void UpdateWindowTitles();

    void ClearExisting();

    void NewFile();

    bool SaveToFile(const Platform::Path &filename);

    bool LoadAutosaveFor(const Platform::Path &filename);

    bool LoadFromFile(const Platform::Path &filename, bool canCancel = false);

    void UpgradeLegacyData();

    bool LoadEntitiesFromFile(const Platform::Path &filename, EntityList *le,
                              SMesh *m, SShell *sh);

    bool ReloadAllLinked(const Platform::Path &filename, bool canCancel = false);

    // And the various export options
    void ExportAsPngTo(const Platform::Path &filename);

    void ExportMeshTo(const Platform::Path &filename);

    void ExportMeshAsStlTo(FILE *f, SMesh *sm);

    void ExportMeshAsObjTo(FILE *fObj, FILE *fMtl, SMesh *sm);

    void ExportMeshAsThreeJsTo(FILE *f, const Platform::Path &filename,
                               SMesh *sm, SOutlineList *sol);

    void ExportViewOrWireframeTo(const Platform::Path &filename, bool exportWireframe);

    void ExportSectionTo(const Platform::Path &filename);

    void ExportWireframeCurves(SEdgeList *sel, SBezierList *sbl,
                               VectorFileWriter *out);

    void ExportLinesAndMesh(SEdgeList *sel, SBezierList *sbl, SMesh *sm,
                            Vector u, Vector v,
                            Vector n, Vector origin,
                            double cameraTan,
                            VectorFileWriter *out);

    static void MenuAnalyze(Command id);

    // Additional display stuff
    struct {
        SContour path;
        hEntity point;
    } traced;
    SEdgeList nakedEdges;
    struct {
        bool draw;
        Vector ptA;
        Vector ptB;
    } extraLine;
    struct {
        bool draw, showOrigin;
        Vector pt, u, v;
    } justExportedInfo;
    struct {
        bool draw;
        bool dirty;
        Vector position;
    } centerOfMass;

    class Clipboard {
    public:
        List <ClipboardRequest> r;
        List <Constraint> c;

        void Clear();

        bool ContainsEntity(hEntity old);

        hEntity NewEntityFor(hEntity old);
    };

    Clipboard clipboard;

    void MarkGroupDirty(hGroup hg, bool onlyThis = false);

    void MarkGroupDirtyByEntity(hEntity he);

    // Consistency checking on the sketch: stuff with missing dependencies
    // will get deleted automatically.
    struct {
        int requests;
        int groups;
        int constraints;
        int nonTrivialConstraints;
    } deleted;

    bool GroupExists(hGroup hg);

    bool PruneOrphans();

    bool EntityExists(hEntity he);

    bool GroupsInOrder(hGroup before, hGroup after);

    bool PruneGroups(hGroup hg);

    bool PruneRequests(hGroup hg);

    bool PruneConstraints(hGroup hg);

    static void ShowNakedEdges(bool reportOnlyWhenNotOkay);

    enum class Generate : uint32_t {
        DIRTY,
        ALL,
        REGEN,
        UNTIL_ACTIVE,
    };

    void GenerateAll(Generate type = Generate::DIRTY, bool andFindFree = false,
                     bool genForBBox = false);

    void SolveGroup(hGroup hg, bool andFindFree);

    void SolveGroupAndReport(hGroup hg, bool andFindFree);

    SolveResult TestRankForGroup(hGroup hg);

    void WriteEqSystemForGroup(hGroup hg);

    void MarkDraggedParams();

    void ForceReferences();

    void UpdateCenterOfMass();

    bool ActiveGroupsOkay();

    // The system to be solved.
    System *pSys;
    System &sys;

    // All the TrueType fonts in memory
    TtfFontList fonts;

    // Everything has been pruned, so we know there's no dangling references
    // to entities that don't exist. Before that, we mustn't try to display
    // the sketch!
    bool allConsistent;

    Platform::TimerRef showTWTimer;
    Platform::TimerRef generateAllTimer;
    Platform::TimerRef autosaveTimer;

    void ScheduleShowTW();

    void ScheduleGenerateAll();

    void ScheduleAutosave();

    static void MenuHelp(Command id);

    void Clear();

    // We allocate TW and sys on the heap to work around an MSVC problem
    // where it puts zero-initialized global data in the binary (~30M of zeroes)
    // in release builds.
    SolveSpaceUI()
            : pTW(new TextWindow({})), TW(*pTW),
              pSys(new System({})), sys(*pSys) {}

    ~SolveSpaceUI() {
        delete pTW;
        delete pSys;
    }
};
}


#endif //SOLVESPACE_SOLVESPACEUI_H
