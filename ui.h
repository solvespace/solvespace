
#ifndef __UI_H
#define __UI_H

class TextWindow {
public:
    static const int MAX_COLS = 100;
    static const int MAX_ROWS = 2000;

#ifndef RGB
#define RGB(r, g, b) ((r) | ((g) << 8) | ((b) << 16))
#endif
#define RGBf(r, g, b) RGB((int)(255*(r)), (int)(255*(g)), (int)(255*(b)))
#define RED(v)      (((v) >>  0) & 0xff)
#define GREEN(v)    (((v) >>  8) & 0xff)
#define BLUE(v)     (((v) >> 16) & 0xff)
#define REDf(v)     (RED  (v) / 255.0f)
#define GREENf(v)   (GREEN(v) / 255.0f)
#define BLUEf(v)    (BLUE (v) / 255.0f)
    typedef struct {
        char    c;
        int     color;
    } Color;
    static const Color fgColors[];
    static const Color bgColors[];

    BYTE    text[MAX_ROWS][MAX_COLS];
    typedef void LinkFunction(int link, DWORD v);
    static const int NOT_A_LINK = 0;
    struct {
        char            fg;
        int             bg;
        int             link;
        DWORD           data;
        LinkFunction   *f;
        LinkFunction   *h;
    }       meta[MAX_ROWS][MAX_COLS];
    int top[MAX_ROWS]; // in half-line units, or -1 for unused

    int rows;
   
    void Init(void);
    void Printf(bool half, char *fmt, ...);
    void ClearScreen(void);
   
    void Show(void);

    // State for the screen that we are showing in the text window.
    static const int SCREEN_LIST_OF_GROUPS      = 0;
    static const int SCREEN_GROUP_INFO          = 1;
    static const int SCREEN_GROUP_SOLVE_INFO    = 2;
    static const int SCREEN_CONFIGURATION       = 3;
    static const int SCREEN_STEP_DIMENSION      = 4;
    static const int SCREEN_MESH_VOLUME         = 5;
    static const int SCREEN_LIST_OF_STYLES      = 6;
    static const int SCREEN_STYLE_INFO          = 7;
    typedef struct {
        int         screen;

        hGroup      group;
        hStyle      style;

        hConstraint constraint;
        bool        dimIsDistance;
        double      dimFinish;
        int         dimSteps;
        
        double      volume;
    } ShownState;
    ShownState shown;

    static const int EDIT_NOTHING               = 0;
    // For multiple groups
    static const int EDIT_TIMES_REPEATED        = 1;
    static const int EDIT_GROUP_NAME            = 2;
    // For the configuraiton screen
    static const int EDIT_LIGHT_DIRECTION       = 10;
    static const int EDIT_LIGHT_INTENSITY       = 11;
    static const int EDIT_COLOR                 = 12;
    static const int EDIT_CHORD_TOLERANCE       = 13;
    static const int EDIT_MAX_SEGMENTS          = 14;
    static const int EDIT_CAMERA_TANGENT        = 15;
    static const int EDIT_GRID_SPACING          = 16;
    static const int EDIT_EXPORT_SCALE          = 17;
    static const int EDIT_EXPORT_OFFSET         = 18;
    static const int EDIT_CANVAS_SIZE           = 19;
    // For the helical sweep
    static const int EDIT_HELIX_TURNS           = 20;
    static const int EDIT_HELIX_PITCH           = 21;
    static const int EDIT_HELIX_DRADIUS         = 22;
    // For TTF text
    static const int EDIT_TTF_TEXT              = 30;
    // For the step dimension screen
    static const int EDIT_STEP_DIM_FINISH       = 40;
    static const int EDIT_STEP_DIM_STEPS        = 41;
    // For the styles stuff
    static const int EDIT_STYLE_WIDTH           = 50;
    static const int EDIT_STYLE_TEXT_HEIGHT     = 51;
    static const int EDIT_STYLE_TEXT_ANGLE      = 52;
    static const int EDIT_STYLE_COLOR           = 53;
    static const int EDIT_STYLE_NAME            = 54;
    static const int EDIT_BACKGROUND_COLOR      = 55;
    struct {
        int         meaning;
        int         i;
        hGroup      group;
        hRequest    request;
        hStyle      style;
    } edit;

    static void ReportHowGroupSolved(hGroup hg);

    void ClearSuper(void);

    void ShowHeader(bool withNav);
    // These are self-contained screens, that show some information about
    // the sketch.
    void ShowListOfGroups(void);
    void ShowGroupInfo(void);
    void ShowGroupSolveInfo(void);
    void ShowConfiguration(void);
    void ShowListOfStyles(void);
    void ShowStyleInfo(void);
    void ShowStepDimension(void);
    void ShowMeshVolume(void);
    // Special screen, based on selection
    void DescribeSelection(void);

    void GoToScreen(int screen);

    // All of these are callbacks from the GUI code; first from when
    // we're describing an entity
    static void ScreenEditTtfText(int link, DWORD v);
    static void ScreenSetTtfFont(int link, DWORD v);
    static void ScreenUnselectAll(int link, DWORD v);

    // and the rest from the stuff in textscreens.cpp
    static void ScreenSelectGroup(int link, DWORD v);
    static void ScreenActivateGroup(int link, DWORD v);
    static void ScreenToggleGroupShown(int link, DWORD v);
    static void ScreenHowGroupSolved(int link, DWORD v);
    static void ScreenShowGroupsSpecial(int link, DWORD v);
    static void ScreenDeleteGroup(int link, DWORD v);

    static void ScreenHoverConstraint(int link, DWORD v);
    static void ScreenHoverRequest(int link, DWORD v);
    static void ScreenSelectRequest(int link, DWORD v);
    static void ScreenSelectConstraint(int link, DWORD v);

    static void ScreenChangeOneOrTwoSides(int link, DWORD v);
    static void ScreenChangeSkipFirst(int link, DWORD v);
    static void ScreenChangeMeshCombine(int link, DWORD v);
    static void ScreenChangeMeshOrExact(int link, DWORD v);
    static void ScreenChangeSuppress(int link, DWORD v);
    static void ScreenChangeRightLeftHanded(int link, DWORD v);
    static void ScreenChangeHelixParameter(int link, DWORD v);
    static void ScreenColor(int link, DWORD v);

    static void ScreenShowListOfStyles(int link, DWORD v);
    static void ScreenShowStyleInfo(int link, DWORD v);
    static void ScreenDeleteStyle(int link, DWORD v);
    static void ScreenChangeStyleYesNo(int link, DWORD v);
    static void ScreenCreateCustomStyle(int link, DWORD v);
    static void ScreenLoadFactoryDefaultStyles(int link, DWORD v);
    static void ScreenAssignSelectionToStyle(int link, DWORD v);

    static void ScreenShowConfiguration(int link, DWORD v);
    static void ScreenGoToWebsite(int link, DWORD v);

    static void ScreenChangeFixExportColors(int link, DWORD v);
    static void ScreenChangeBackFaces(int link, DWORD v);
    static void ScreenChangePwlCurves(int link, DWORD v);
    static void ScreenChangeCanvasSizeAuto(int link, DWORD v);
    static void ScreenChangeCanvasSize(int link, DWORD v);
    static void ScreenChangeShadedTriangles(int link, DWORD v);

    static void ScreenStepDimSteps(int link, DWORD v);
    static void ScreenStepDimFinish(int link, DWORD v);
    static void ScreenStepDimGo(int link, DWORD v);

    static void ScreenHome(int link, DWORD v);

    // These ones do stuff with the edit control
    static void ScreenChangeExprA(int link, DWORD v);
    static void ScreenChangeGroupName(int link, DWORD v);
    static void ScreenChangeLightDirection(int link, DWORD v);
    static void ScreenChangeLightIntensity(int link, DWORD v);
    static void ScreenChangeColor(int link, DWORD v);
    static void ScreenChangeChordTolerance(int link, DWORD v);
    static void ScreenChangeMaxSegments(int link, DWORD v);
    static void ScreenChangeCameraTangent(int link, DWORD v);
    static void ScreenChangeGridSpacing(int link, DWORD v);
    static void ScreenChangeExportScale(int link, DWORD v);
    static void ScreenChangeExportOffset(int link, DWORD v);
    static void ScreenChangeStyleName(int link, DWORD v);
    static void ScreenChangeStyleWidthOrTextHeight(int link, DWORD v);
    static void ScreenChangeStyleTextAngle(int link, DWORD v);
    static void ScreenChangeStyleColor(int link, DWORD v);
    static void ScreenChangeBackgroundColor(int link, DWORD v);

    bool EditControlDoneForStyles(char *s);
    void EditControlDone(char *s);
};

class GraphicsWindow {
public:
    void Init(void);

    // This table describes the top-level menus in the graphics winodw.
    typedef enum {
        // File
        MNU_NEW = 100,
        MNU_OPEN,
        MNU_OPEN_RECENT,
        MNU_SAVE,
        MNU_SAVE_AS,
        MNU_EXPORT_PNG,
        MNU_EXPORT_MESH,
        MNU_EXPORT_SURFACES,
        MNU_EXPORT_VIEW,
        MNU_EXPORT_SECTION,
        MNU_EXIT,
        // View
        MNU_ZOOM_IN,
        MNU_ZOOM_OUT,
        MNU_ZOOM_TO_FIT,
        MNU_SHOW_GRID,
        MNU_PARALLEL_PROJ,
        MNU_NEAREST_ORTHO,
        MNU_NEAREST_ISO,
        MNU_CENTER_VIEW,
        MNU_SHOW_TEXT_WND,
        MNU_SHOW_TOOLBAR,
        MNU_UNITS_INCHES,
        MNU_UNITS_MM,
        // Edit
        MNU_UNDO,
        MNU_REDO,
        MNU_DELETE,
        MNU_SNAP_TO_GRID,
        MNU_ROTATE_90,
        MNU_UNSELECT_ALL,
        MNU_REGEN_ALL,
        // Request
        MNU_SEL_WORKPLANE,
        MNU_FREE_IN_3D,
        MNU_DATUM_POINT,
        MNU_WORKPLANE,
        MNU_LINE_SEGMENT,
        MNU_CIRCLE,
        MNU_ARC,
        MNU_RECTANGLE,
        MNU_CUBIC,
        MNU_TTF_TEXT,
        MNU_SPLIT_CURVES,
        MNU_CONSTRUCTION,
        // Group
        MNU_GROUP_3D,
        MNU_GROUP_WRKPL,
        MNU_GROUP_EXTRUDE,
        MNU_GROUP_LATHE,
        MNU_GROUP_SWEEP,
        MNU_GROUP_HELICAL,
        MNU_GROUP_ROT,
        MNU_GROUP_TRANS,
        MNU_GROUP_IMPORT,
        MNU_GROUP_RECENT,
        // Constrain
        MNU_DISTANCE_DIA,
        MNU_ANGLE,
        MNU_OTHER_ANGLE,
        MNU_REFERENCE,
        MNU_EQUAL,
        MNU_RATIO,
        MNU_ON_ENTITY,
        MNU_SYMMETRIC,
        MNU_AT_MIDPOINT,
        MNU_HORIZONTAL,
        MNU_VERTICAL,
        MNU_PARALLEL,
        MNU_PERPENDICULAR,
        MNU_ORIENTED_SAME,
        MNU_COMMENT,
        // Analyze
        MNU_VOLUME,
        MNU_INTERFERENCE,
        MNU_NAKED_EDGES,
        MNU_SHOW_DOF,
        MNU_TRACE_PT,
        MNU_STOP_TRACING,
        MNU_STEP_DIM,
        // Help,
        MNU_LICENSE,
        MNU_WEBSITE,
        MNU_ABOUT,
    } MenuId;
    typedef void MenuHandler(int id);
    typedef struct {
        int         level;          // 0 == on menu bar, 1 == one level down
        char       *label;          // or NULL for a separator
        int         id;             // unique ID
        int         accel;          // keyboard accelerator
        MenuHandler *fn;
    } MenuEntry;
    static const MenuEntry menu[];
    static void MenuView(int id);
    static void MenuEdit(int id);
    static void MenuRequest(int id);

    // The width and height (in pixels) of the window.
    double width, height;
    // These parameters define the map from 2d screen coordinates to the
    // coordinates of the 3d sketch points. We will use an axonometric
    // projection.
    Vector  offset;
    Vector  projRight;
    Vector  projUp;
    double  scale;
    struct {
        Vector  offset;
        Vector  projRight;
        Vector  projUp;
        Point2d mouse;
        bool    startedMoving;
    }       orig;

    // When the user is dragging a point, don't solve multiple times without
    // allowing a paint in between. The extra solves are wasted if they're
    // not displayed.
    bool    havePainted;

    // Some state for the context menu.
    struct {
        bool        active;
    }       context;

    void NormalizeProjectionVectors(void);
    Point2d ProjectPoint(Vector p);
    Vector ProjectPoint3(Vector p);
    Vector ProjectPoint4(Vector p, double *w);
    void AnimateOnto(Quaternion quatf, Vector offsetf);
    void AnimateOntoWorkplane(void);
    Vector VectorFromProjs(Vector rightUpForward);
    void HandlePointForZoomToFit(Vector p, Point2d *pmax, Point2d *pmin,
                                           double *wmin, bool div);
    void LoopOverPoints(Point2d *pmax, Point2d *pmin, double *wmin, bool div,
                            bool includingInvisibles);
    void ZoomToFit(bool includingInvisibles);

    hGroup  activeGroup;
    void EnsureValidActives(void);
    bool LockedInWorkplane(void);
    void SetWorkplaneFreeIn3d(void);
    hEntity ActiveWorkplane(void);
    void ForceTextWindowShown(void);

    // Operations that must be completed by doing something with the mouse
    // are noted here. These occupy the same space as the menu ids.
    static const int    FIRST_PENDING               = 0x0f000000;
    static const int    DRAGGING_POINT              = 0x0f000000;
    static const int    DRAGGING_NEW_POINT          = 0x0f000001;
    static const int    DRAGGING_NEW_LINE_POINT     = 0x0f000002;
    static const int    DRAGGING_NEW_CUBIC_POINT    = 0x0f000003;
    static const int    DRAGGING_NEW_ARC_POINT      = 0x0f000004;
    static const int    DRAGGING_CONSTRAINT         = 0x0f000005;
    static const int    DRAGGING_RADIUS             = 0x0f000006;
    static const int    DRAGGING_NORMAL             = 0x0f000007;
    static const int    DRAGGING_NEW_RADIUS         = 0x0f000008;
    struct {
        int         operation;

        hEntity     point;
        hEntity     circle;
        hEntity     normal;
        hConstraint constraint;

        char        *description;
    } pending;
    void ClearPending(void);
    // The constraint that is being edited with the on-screen textbox.
    hConstraint constraintBeingEdited;

    Vector SnapToGrid(Vector p);
    bool ConstrainPointByHovered(hEntity pt);
    void DeleteTaggedRequests(void);
    hRequest AddRequest(int type, bool rememberForUndo);
    hRequest AddRequest(int type);

    void MakeTangentArc(void);
    void SplitLinesOrCurves(void);
    hEntity SplitEntity(hEntity he, Vector pinter);
    hEntity SplitLine(hEntity he, Vector pinter);
    hEntity SplitCircle(hEntity he, Vector pinter);
    hEntity SplitCubic(hEntity he, Vector pinter);
    void ReplacePointInConstraints(hEntity oldpt, hEntity newpt);
    void FixConstraintsForRequestBeingDeleted(hRequest hr);
    void FixConstraintsForPointBeingDeleted(hEntity hpt);
    
    // The current selection.
    class Selection {
    public:
        hEntity     entity;
        hConstraint constraint;
        
        bool        emphasized;

        void Draw(void);

        void Clear(void);
        bool IsEmpty(void);
        bool Equals(Selection *b);
        bool IsStylable(void);
    };
    Selection hover;
    static const int MAX_SELECTED = 32;
    Selection selection[MAX_SELECTED];
    void HitTestMakeSelection(Point2d mp);
    void ClearSelection(void);
    void ClearNonexistentSelectionItems(void);
    struct {
        hEntity     point[MAX_SELECTED];
        hEntity     entity[MAX_SELECTED];
        hEntity     anyNormal[MAX_SELECTED];
        hEntity     vector[MAX_SELECTED];
        hEntity     face[MAX_SELECTED];
        hConstraint constraint[MAX_SELECTED];
        int         points;
        int         entities;
        int         workplanes;
        int         faces;
        int         lineSegments;
        int         circlesOrArcs;
        int         arcs;
        int         cubics;
        int         anyNormals;
        int         vectors;
        int         constraints;
        int         stylables;
        int         comments;
        int         n;
    } gs;
    void GroupSelection(void);
    void ToggleSelectionStateOfHovered(void);
    void ClearSuper(void);

    static const int CMNU_TOGGLE_SELECTION = 0x100;
    static const int CMNU_UNSELECT_ALL     = 0x101;
    static const int CMNU_DELETE_SEL       = 0x102;
    static const int CMNU_NEW_CUSTOM_STYLE = 0x103;
    static const int CMNU_NO_STYLE         = 0x104;
    static const int CMNU_GROUP_INFO       = 0x105;
    static const int CMNU_REFERENCE_DIM    = 0x106;
    static const int CMNU_OTHER_ANGLE      = 0x107;
    static const int CMNU_STYLE_INFO       = 0x108;
    static const int CMNU_FIRST_STYLE      = 0x40000000;
    void ContextMenuListStyles(void);

    // The toolbar, in toolbar.cpp
    bool ToolbarDrawOrHitTest(int x, int y, bool paint, int *menu);
    void ToolbarDraw(void);
    bool ToolbarMouseMoved(int x, int y);
    bool ToolbarMouseDown(int x, int y);
    static void TimerCallback(void);
    int toolbarHovered;
    int toolbarTooltipped;
    int toolbarMouseX, toolbarMouseY;

    // This sets what gets displayed.
    bool    showWorkplanes;
    bool    showNormals;
    bool    showPoints;
    bool    showConstraints;
    bool    showTextWindow;
    bool    showShaded;
    bool    showEdges;
    bool    showFaces;
    bool    showMesh;
    bool    showHdnLines;
    static void ToggleBool(int link, DWORD v);

    bool    showSnapGrid;

    void UpdateDraggedNum(Vector *pos, double mx, double my);
    void UpdateDraggedPoint(hEntity hp, double mx, double my);

    // These are called by the platform-specific code.
    void Paint(int w, int h);
    void MouseMoved(double x, double y, bool leftDown, bool middleDown,
                                bool rightDown, bool shiftDown, bool ctrlDown);
    void MouseLeftDown(double x, double y);
    void MouseLeftUp(double x, double y);
    void MouseLeftDoubleClick(double x, double y);
    void MouseMiddleOrRightDown(double x, double y);
    void MouseRightUp(double x, double y);
    void MouseScroll(double x, double y, int delta);
    void MouseLeave(void);
    bool KeyDown(int c);
    void EditControlDone(char *s);

    SDWORD lastSpaceNavigatorTime;
    hGroup lastSpaceNavigatorGroup;
    void SpaceNavigatorMoved(double tx, double ty, double tz,
                             double rx, double ry, double rz, bool shiftDown);
    void SpaceNavigatorButtonUp(void);
};


#endif
