//-----------------------------------------------------------------------------
// Declarations relating to our user interface, in both the graphics and
// text browser window.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef __UI_H
#define __UI_H

class TextWindow {
public:
    static const int MAX_COLS = 100;
    static const int MIN_COLS = 45;
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

    float bgColorTable[256*3];
    float fgColorTable[256*3];

    static const int CHAR_WIDTH     = 9;
    static const int CHAR_HEIGHT    = 16;
    static const int LINE_HEIGHT    = 20;
    static const int LEFT_MARGIN    = 6;

    static const int CHECK_FALSE    = 0x80;
    static const int CHECK_TRUE     = 0x81;
    static const int RADIO_FALSE    = 0x82;
    static const int RADIO_TRUE     = 0x83;

    int scrollPos;      // The scrollbar position, in half-row units
    int halfRows;       // The height of our window, in half-row units

    uint8_t    text[MAX_ROWS][MAX_COLS];
    typedef void LinkFunction(int link, uint32_t v);
    static const int NOT_A_LINK = 0;
    struct {
        char            fg;
        int             bg;
        int             link;
        uint32_t           data;
        LinkFunction   *f;
        LinkFunction   *h;
    }       meta[MAX_ROWS][MAX_COLS];
    int hoveredRow, hoveredCol;


    int top[MAX_ROWS]; // in half-line units, or -1 for unused
    int rows;

    // The row of icons at the top of the text window, to hide/show things
    typedef struct {
        bool    *var;
        uint8_t    *icon;
        char    *tip;
    } HideShowIcon;
    static HideShowIcon hideShowIcons[];
    static bool SPACER;

    // These are called by the platform-specific code.
    void Paint(void);
    void MouseEvent(bool isClick, bool leftDown, double x, double y);
    void MouseScroll(double x, double y, int delta);
    void MouseLeave(void);
    void ScrollbarEvent(int newPos);

    static const int PAINT = 0;
    static const int HOVER = 1;
    static const int CLICK = 2;
    void DrawOrHitTestIcons(int how, double mx, double my);
    void TimerCallback(void);
    Point2d oldMousePos;
    HideShowIcon *hoveredIcon, *tooltippedIcon;

    Vector HsvToRgb(Vector hsv);
    uint8_t *HsvPattern2d(void);
    uint8_t *HsvPattern1d(double h, double s);
    void ColorPickerDone(void);
    bool DrawOrHitTestColorPicker(int how, bool leftDown, double x, double y);

    void Init(void);
    void MakeColorTable(const Color *in, float *out);
    void Printf(bool half, char *fmt, ...);
    void ClearScreen(void);

    void Show(void);

    // State for the screen that we are showing in the text window.
    static const int SCREEN_LIST_OF_GROUPS      = 0;
    static const int SCREEN_GROUP_INFO          = 1;
    static const int SCREEN_GROUP_SOLVE_INFO    = 2;
    static const int SCREEN_CONFIGURATION       = 3;
    static const int SCREEN_STEP_DIMENSION      = 4;
    static const int SCREEN_LIST_OF_STYLES      = 5;
    static const int SCREEN_STYLE_INFO          = 6;
    static const int SCREEN_PASTE_TRANSFORMED   = 7;
    static const int SCREEN_EDIT_VIEW           = 8;
    static const int SCREEN_TANGENT_ARC         = 9;
    typedef struct {
        int         screen;

        hGroup      group;
        hStyle      style;

        hConstraint constraint;
        bool        dimIsDistance;
        double      dimFinish;
        int         dimSteps;

        struct {
            int         times;
            Vector      trans;
            double      theta;
            Vector      origin;
            double      scale;
        }           paste;
    } ShownState;
    ShownState shown;

    static const int EDIT_NOTHING               = 0;
    // For multiple groups
    static const int EDIT_TIMES_REPEATED        = 1;
    static const int EDIT_GROUP_NAME            = 2;
    static const int EDIT_GROUP_SCALE           = 3;
    static const int EDIT_GROUP_COLOR           = 4;
    // For the configuraiton screen
    static const int EDIT_LIGHT_DIRECTION       = 100;
    static const int EDIT_LIGHT_INTENSITY       = 101;
    static const int EDIT_COLOR                 = 102;
    static const int EDIT_CHORD_TOLERANCE       = 103;
    static const int EDIT_MAX_SEGMENTS          = 104;
    static const int EDIT_CAMERA_TANGENT        = 105;
    static const int EDIT_GRID_SPACING          = 106;
    static const int EDIT_DIGITS_AFTER_DECIMAL  = 107;
    static const int EDIT_EXPORT_SCALE          = 108;
    static const int EDIT_EXPORT_OFFSET         = 109;
    static const int EDIT_CANVAS_SIZE           = 110;
    static const int EDIT_G_CODE_DEPTH          = 120;
    static const int EDIT_G_CODE_PASSES         = 121;
    static const int EDIT_G_CODE_FEED           = 122;
    static const int EDIT_G_CODE_PLUNGE_FEED    = 123;
    // For TTF text
    static const int EDIT_TTF_TEXT              = 300;
    // For the step dimension screen
    static const int EDIT_STEP_DIM_FINISH       = 400;
    static const int EDIT_STEP_DIM_STEPS        = 401;
    // For the styles stuff
    static const int EDIT_STYLE_WIDTH           = 500;
    static const int EDIT_STYLE_TEXT_HEIGHT     = 501;
    static const int EDIT_STYLE_TEXT_ANGLE      = 502;
    static const int EDIT_STYLE_COLOR           = 503;
    static const int EDIT_STYLE_FILL_COLOR      = 504;
    static const int EDIT_STYLE_NAME            = 505;
    static const int EDIT_BACKGROUND_COLOR      = 506;
    static const int EDIT_BACKGROUND_IMG_SCALE  = 507;
    // For paste transforming
    static const int EDIT_PASTE_TIMES_REPEATED  = 600;
    static const int EDIT_PASTE_ANGLE           = 601;
    static const int EDIT_PASTE_SCALE           = 602;
    // For view
    static const int EDIT_VIEW_SCALE            = 700;
    static const int EDIT_VIEW_ORIGIN           = 701;
    static const int EDIT_VIEW_PROJ_RIGHT       = 702;
    static const int EDIT_VIEW_PROJ_UP          = 703;
    // For tangent arc
    static const int EDIT_TANGENT_ARC_RADIUS    = 800;
    struct {
        bool        showAgain;
        int         meaning;
        int         i;
        hGroup      group;
        hRequest    request;
        hStyle      style;
    } edit;

    static void ReportHowGroupSolved(hGroup hg);

    struct {
        int     halfRow;
        int     col;

        struct {
            uint32_t   rgb;
            double  h, s, v;
            bool    show;
            bool    picker1dActive;
            bool    picker2dActive;
        }       colorPicker;
    } editControl;

    void HideEditControl(void);
    void ShowEditControl(int halfRow, int col, char *s);
    void ShowEditControlWithColorPicker(int halfRow, int col, uint32_t rgb);

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
    void ShowPasteTransformed(void);
    void ShowEditView(void);
    void ShowTangentArc(void);
    // Special screen, based on selection
    void DescribeSelection(void);

    void GoToScreen(int screen);

    // All of these are callbacks from the GUI code; first from when
    // we're describing an entity
    static void ScreenEditTtfText(int link, uint32_t v);
    static void ScreenSetTtfFont(int link, uint32_t v);
    static void ScreenUnselectAll(int link, uint32_t v);

    // and the rest from the stuff in textscreens.cpp
    static void ScreenSelectGroup(int link, uint32_t v);
    static void ScreenActivateGroup(int link, uint32_t v);
    static void ScreenToggleGroupShown(int link, uint32_t v);
    static void ScreenHowGroupSolved(int link, uint32_t v);
    static void ScreenShowGroupsSpecial(int link, uint32_t v);
    static void ScreenDeleteGroup(int link, uint32_t v);

    static void ScreenHoverConstraint(int link, uint32_t v);
    static void ScreenHoverRequest(int link, uint32_t v);
    static void ScreenSelectRequest(int link, uint32_t v);
    static void ScreenSelectConstraint(int link, uint32_t v);

    static void ScreenChangeGroupOption(int link, uint32_t v);
    static void ScreenColor(int link, uint32_t v);

    static void ScreenShowListOfStyles(int link, uint32_t v);
    static void ScreenShowStyleInfo(int link, uint32_t v);
    static void ScreenDeleteStyle(int link, uint32_t v);
    static void ScreenChangeStyleYesNo(int link, uint32_t v);
    static void ScreenCreateCustomStyle(int link, uint32_t v);
    static void ScreenLoadFactoryDefaultStyles(int link, uint32_t v);
    static void ScreenAssignSelectionToStyle(int link, uint32_t v);
    static void ScreenBackgroundImage(int link, uint32_t v);

    static void ScreenShowConfiguration(int link, uint32_t v);
    static void ScreenShowEditView(int link, uint32_t v);
    static void ScreenGoToWebsite(int link, uint32_t v);

    static void ScreenChangeFixExportColors(int link, uint32_t v);
    static void ScreenChangeBackFaces(int link, uint32_t v);
    static void ScreenChangeCheckClosedContour(int link, uint32_t v);
    static void ScreenChangePwlCurves(int link, uint32_t v);
    static void ScreenChangeCanvasSizeAuto(int link, uint32_t v);
    static void ScreenChangeCanvasSize(int link, uint32_t v);
    static void ScreenChangeShadedTriangles(int link, uint32_t v);

    static void ScreenAllowRedundant(int link, uint32_t v);

    static void ScreenStepDimSteps(int link, uint32_t v);
    static void ScreenStepDimFinish(int link, uint32_t v);
    static void ScreenStepDimGo(int link, uint32_t v);

    static void ScreenChangeTangentArc(int link, uint32_t v);

    static void ScreenPasteTransformed(int link, uint32_t v);

    static void ScreenHome(int link, uint32_t v);

    // These ones do stuff with the edit control
    static void ScreenChangeExprA(int link, uint32_t v);
    static void ScreenChangeGroupName(int link, uint32_t v);
    static void ScreenChangeGroupScale(int link, uint32_t v);
    static void ScreenChangeLightDirection(int link, uint32_t v);
    static void ScreenChangeLightIntensity(int link, uint32_t v);
    static void ScreenChangeColor(int link, uint32_t v);
    static void ScreenChangeChordTolerance(int link, uint32_t v);
    static void ScreenChangeMaxSegments(int link, uint32_t v);
    static void ScreenChangeCameraTangent(int link, uint32_t v);
    static void ScreenChangeGridSpacing(int link, uint32_t v);
    static void ScreenChangeDigitsAfterDecimal(int link, uint32_t v);
    static void ScreenChangeExportScale(int link, uint32_t v);
    static void ScreenChangeExportOffset(int link, uint32_t v);
    static void ScreenChangeGCodeParameter(int link, uint32_t v);
    static void ScreenChangeStyleName(int link, uint32_t v);
    static void ScreenChangeStyleWidthOrTextHeight(int link, uint32_t v);
    static void ScreenChangeStyleTextAngle(int link, uint32_t v);
    static void ScreenChangeStyleColor(int link, uint32_t v);
    static void ScreenChangeBackgroundColor(int link, uint32_t v);
    static void ScreenChangeBackgroundImageScale(int link, uint32_t v);
    static void ScreenChangePasteTransformed(int link, uint32_t v);
    static void ScreenChangeViewScale(int link, uint32_t v);
    static void ScreenChangeViewOrigin(int link, uint32_t v);
    static void ScreenChangeViewProjection(int link, uint32_t v);

    bool EditControlDoneForStyles(char *s);
    bool EditControlDoneForConfiguration(char *s);
    bool EditControlDoneForPaste(char *s);
    bool EditControlDoneForView(char *s);
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
        MNU_EXPORT_WIREFRAME,
        MNU_EXIT,
        // View
        MNU_ZOOM_IN,
        MNU_ZOOM_OUT,
        MNU_ZOOM_TO_FIT,
        MNU_SHOW_GRID,
        MNU_PERSPECTIVE_PROJ,
        MNU_ONTO_WORKPLANE,
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
        MNU_CUT,
        MNU_COPY,
        MNU_PASTE,
        MNU_PASTE_TRANSFORM,
        MNU_DELETE,
        MNU_SELECT_CHAIN,
        MNU_SELECT_ALL,
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
        MNU_TANGENT_ARC,
        MNU_CONSTRUCTION,
        // Group
        MNU_GROUP_3D,
        MNU_GROUP_WRKPL,
        MNU_GROUP_EXTRUDE,
        MNU_GROUP_LATHE,
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
        MNU_WHERE_DRAGGED,
        MNU_COMMENT,
        // Analyze
        MNU_VOLUME,
        MNU_AREA,
        MNU_INTERFERENCE,
        MNU_NAKED_EDGES,
        MNU_SHOW_DOF,
        MNU_TRACE_PT,
        MNU_STOP_TRACING,
        MNU_STEP_DIM,
        // Help,
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
    void DeleteSelection(void);
    void CopySelection(void);
    void PasteClipboard(Vector trans, double theta, double scale);
    static void MenuClipboard(int id);

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
        bool    mouseDown;
        Vector  offset;
        Vector  projRight;
        Vector  projUp;
        Point2d mouse;
        Point2d mouseOnButtonDown;
        Vector  marqueePoint;
        bool    startedMoving;
    }       orig;

    // Most recent mouse position, updated every time the mouse moves.
    Point2d currentMousePosition;

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
    Vector UnProjectPoint(Point2d p);
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
    static const int    DRAGGING_POINTS             = 0x0f000000;
    static const int    DRAGGING_NEW_POINT          = 0x0f000001;
    static const int    DRAGGING_NEW_LINE_POINT     = 0x0f000002;
    static const int    DRAGGING_NEW_CUBIC_POINT    = 0x0f000003;
    static const int    DRAGGING_NEW_ARC_POINT      = 0x0f000004;
    static const int    DRAGGING_CONSTRAINT         = 0x0f000005;
    static const int    DRAGGING_RADIUS             = 0x0f000006;
    static const int    DRAGGING_NORMAL             = 0x0f000007;
    static const int    DRAGGING_NEW_RADIUS         = 0x0f000008;
    static const int    DRAGGING_MARQUEE            = 0x0f000009;
    struct {
        int             operation;

        hEntity         point;
        List<hEntity>   points;
        hEntity         circle;
        hEntity         normal;
        hConstraint     constraint;

        char            *description;
    } pending;
    void ClearPending(void);
    // The constraint that is being edited with the on-screen textbox.
    hConstraint constraintBeingEdited;

    Vector SnapToGrid(Vector p);
    bool ConstrainPointByHovered(hEntity pt);
    void DeleteTaggedRequests(void);
    hRequest AddRequest(int type, bool rememberForUndo);
    hRequest AddRequest(int type);

    class ParametricCurve {
    public:
        bool isLine; // else circle
        Vector p0, p1;
        Vector u, v;
        double r, theta0, theta1, dtheta;

        void MakeFromEntity(hEntity he, bool reverse);
        Vector PointAt(double t);
        Vector TangentAt(double t);
        double LengthForAuto(void);

        hRequest CreateRequestTrimmedTo(double t, bool extraConstraints,
            hEntity orig, hEntity arc, bool arcFinish);
        void ConstrainPointIfCoincident(hEntity hpt);
    };
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
        int         tag;

        hEntity     entity;
        hConstraint constraint;
        bool        emphasized;

        void Draw(void);

        void Clear(void);
        bool IsEmpty(void);
        bool Equals(Selection *b);
        bool IsStylable(void);
        bool HasEndpoints(void);
    };
    Selection hover;
    bool hoverWasSelectedOnMousedown;
    List<Selection> selection;
    void HitTestMakeSelection(Point2d mp);
    void ClearSelection(void);
    void ClearNonexistentSelectionItems(void);
    static const int MAX_SELECTED = 32;
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
        int         periodicCubics;
        int         anyNormals;
        int         vectors;
        int         constraints;
        int         stylables;
        int         comments;
        int         withEndpoints;
        int         n;
    } gs;
    void GroupSelection(void);
    bool IsSelected(Selection *s);
    bool IsSelected(hEntity he);
    void MakeSelected(hEntity he);
    void MakeSelected(Selection *s);
    void MakeUnselected(hEntity he, bool coincidentPointTrick);
    void MakeUnselected(Selection *s, bool coincidentPointTrick);
    void SelectByMarquee(void);
    void ClearSuper(void);

    static const int CMNU_UNSELECT_ALL     = 0x100;
    static const int CMNU_UNSELECT_HOVERED = 0x101;
    static const int CMNU_CUT_SEL          = 0x102;
    static const int CMNU_COPY_SEL         = 0x103;
    static const int CMNU_PASTE_SEL        = 0x104;
    static const int CMNU_DELETE_SEL       = 0x105;
    static const int CMNU_SELECT_CHAIN     = 0x106;
    static const int CMNU_NEW_CUSTOM_STYLE = 0x110;
    static const int CMNU_NO_STYLE         = 0x111;
    static const int CMNU_GROUP_INFO       = 0x120;
    static const int CMNU_STYLE_INFO       = 0x121;
    static const int CMNU_REFERENCE_DIM    = 0x130;
    static const int CMNU_OTHER_ANGLE      = 0x131;
    static const int CMNU_DEL_COINCIDENT   = 0x132;
    static const int CMNU_SNAP_TO_GRID     = 0x140;
    static const int CMNU_FIRST_STYLE      = 0x40000000;
    void ContextMenuListStyles(void);
    int32_t contextMenuCancelTime;

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
    void ToggleBool(bool *v);

    bool    showSnapGrid;

    void AddPointToDraggedList(hEntity hp);
    void StartDraggingByEntity(hEntity he);
    void StartDraggingBySelection(void);
    void UpdateDraggedNum(Vector *pos, double mx, double my);
    void UpdateDraggedPoint(hEntity hp, double mx, double my);

    // These are called by the platform-specific code.
    void Paint(void);
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

    int32_t lastSpaceNavigatorTime;
    hGroup lastSpaceNavigatorGroup;
    void SpaceNavigatorMoved(double tx, double ty, double tz,
                             double rx, double ry, double rz, bool shiftDown);
    void SpaceNavigatorButtonUp(void);
};


#endif
