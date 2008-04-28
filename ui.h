
#ifndef __UI_H
#define __UI_H

class TextWindow {
public:
    static const int MAX_COLS = 100;
    static const int MAX_ROWS = 200;

#ifndef RGB
#define RGB(r, g, b) ((r) | ((g) << 8) | ((b) << 16))
#endif
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
        char            bg;
        int             link;
        DWORD           data;
        LinkFunction   *f;
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
    static const int SCREEN_REQUEST_INFO        = 2;
    static const int SCREEN_ENTIY_INFO          = 3;
    static const int SCREEN_CONSTRAINT_INFO     = 4;
    typedef struct {
        int         screen;
        hGroup      group;
        hRequest    request;
        hConstraint constraint;
    } ShownState;
    static const int HISTORY_LEN = 16;
    ShownState showns[HISTORY_LEN];
    int shownIndex;
    int history;
    ShownState *shown;

    void ShowHeader(void);
    // These are self-contained screens, that show some information about
    // the sketch.
    void ShowListOfGroups(void);
    void ShowGroupInfo(void);
    void ShowRequestInfo(void);
    void ShowEntityInfo(void);
    void ShowConstraintInfo(void);

    void OneScreenForwardTo(int screen);
    static void ScreenSelectGroup(int link, DWORD v);
    static void ScreenActivateGroup(int link, DWORD v);
    static void ScreenToggleGroupShown(int link, DWORD v);

    static void ScreenSelectRequest(int link, DWORD v);
    static void ScreenSelectConstraint(int link, DWORD v);
    static void ScreenNavigation(int link, DWORD v);
};

class GraphicsWindow {
public:
    void Init(void);

    // This table describes the top-level menus in the graphics winodw.
    typedef enum {
        // File
        MNU_NEW = 100,
        MNU_OPEN,
        MNU_SAVE,
        MNU_SAVE_AS,
        MNU_EXIT,
        // View
        MNU_ZOOM_IN,
        MNU_ZOOM_OUT,
        MNU_ZOOM_TO_FIT,
        MNU_SHOW_TEXT_WND,
        MNU_UNITS_INCHES,
        MNU_UNITS_MM,
        // Edit
        MNU_DELETE,
        MNU_UNSELECT_ALL,
        // Request
        MNU_SEL_WORKPLANE,
        MNU_FREE_IN_3D,
        MNU_DATUM_POINT,
        MNU_LINE_SEGMENT,
        MNU_RECTANGLE,
        MNU_CUBIC,
        // Group
        MNU_GROUP_DRAWING,
        MNU_GROUP_EXTRUDE,
        // Constrain
        MNU_DISTANCE_DIA,
        MNU_EQUAL,
        MNU_ON_ENTITY,
        MNU_HORIZONTAL,
        MNU_VERTICAL,
        MNU_SOLVE_AUTO,
        MNU_SOLVE_NOW,
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
    }       orig;

    // When the user is dragging a point, don't solve multiple times without
    // allowing a paint in between. The extra solves are wasted if they're
    // not displayed.
    bool    havePainted;

    void NormalizeProjectionVectors(void);
    Point2d ProjectPoint(Vector p);
    void AnimateOnto(Quaternion quatf, Vector offsetf);

    typedef enum {
        UNIT_MM = 0,
        UNIT_INCHES,
    } Unit;
    Unit    viewUnits;

    hGroup  activeGroup;
    hEntity activeWorkplane;
    void EnsureValidActives();

    // Operations that must be completed by doing something with the mouse
    // are noted here.
    static const int    DRAGGING_POINT              = 0x0f000000;
    static const int    DRAGGING_NEW_POINT          = 0x0f000001;
    static const int    DRAGGING_NEW_LINE_POINT     = 0x0f000002;
    static const int    DRAGGING_NEW_CUBIC_POINT    = 0x0f000003;
    static const int    DRAGGING_CONSTRAINT         = 0x0f000004;
    hEntity     pendingPoint;
    hConstraint pendingConstraint;
    int         pendingOperation;
    char       *pendingDescription;
    hRequest AddRequest(int type);

    // The constraint that is being edited with the on-screen textbox.
    hConstraint constraintBeingEdited;
    
    // The current selection.
    class Selection {
    public:
        hEntity     entity;
        hConstraint constraint;

        void Draw(void);

        void Clear(void);
        bool IsEmpty(void);
        bool Equals(Selection *b);
    };
    Selection hover;
    static const int MAX_SELECTED = 32;
    Selection selection[MAX_SELECTED];
    void HitTestMakeSelection(Point2d mp);
    void ClearSelection(void);
    struct {
        hEntity     point[MAX_SELECTED];
        hEntity     entity[MAX_SELECTED];
        int         points;
        int         entities;
        int         workplanes;
        int         planes;
        int         lineSegments;
        int         n;
    } gs;
    void GroupSelection(void);

    // This sets what gets displayed.
    bool    showWorkplanes;
    bool    showAxes;
    bool    showPoints;
    bool    showAllGroups;
    bool    showConstraints;
    bool    showTextWindow;
    static void ToggleBool(int link, DWORD v);
    static void ToggleAnyDatumShown(int link, DWORD v);

    static const int DONT_SOLVE = 0;
    static const int SOLVE_ALWAYS = 1;
    int     solving;

    void UpdateDraggedPoint(Vector *pos, double mx, double my);
    void UpdateDraggedEntity(hEntity hp, double mx, double my);

    // These are called by the platform-specific code.
    void Paint(int w, int h);
    void MouseMoved(double x, double y, bool leftDown, bool middleDown,
                                bool rightDown, bool shiftDown, bool ctrlDown);
    void MouseLeftDown(double x, double y);
    void MouseLeftUp(double x, double y);
    void MouseLeftDoubleClick(double x, double y);
    void MouseMiddleDown(double x, double y);
    void MouseScroll(double x, double y, int delta);
    void EditControlDone(char *s);
};


#endif
