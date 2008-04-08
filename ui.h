
#ifndef __UI_H
#define __UI_H

class TextWindow {
public:
    static const int MAX_COLS = 150;
    static const int MAX_ROWS = 300;

#ifndef RGB
#define RGB(r, g, b) ((r) | ((g) << 8) | ((b) << 16))
#endif
    static const int COLOR_BG_DEFAULT       = RGB( 15,  15,   0);
    static const int COLOR_FG_DEFAULT       = RGB(255, 255, 255);
    static const int COLOR_BG_CMDLINE       = RGB(  0,  20,  80);
    static const int COLOR_FG_CMDLINE       = RGB(255, 255, 255);

    typedef struct {
        int     fg;
        int     bg;
    } Color;
    static const Color colors[];

    // The line with the user-typed command, that is currently being edited.
    char    cmd[MAX_COLS];
    int     cmdInsert;
    int     cmdLen;

    // The rest of the window, text displayed in response to typed commands;
    // some of this might do something if you click on it.

    static const int NOT_A_LINK = 0;

    static const int COLOR_NORMAL   = 0;

    BYTE    text[MAX_ROWS][MAX_COLS];
    typedef void LinkFunction(int link, DWORD v);
    struct {
        int             color;
        int             link;
        DWORD           data;
        LinkFunction   *f;
    }       meta[MAX_ROWS][MAX_COLS];

    int row0, rows;
   
    void Init(void);
    void Printf(char *fmt, ...);
    void ClearScreen(void);
    
    void ClearCommand(void);

    void ProcessCommand(char *cmd);

    // These are called by the platform-specific code.
    void KeyPressed(int c);
    bool IsHyperlink(int width, int height);

    // These are self-contained screens, that show some information about
    // the sketch.
    void ShowGroupList(void);
    void ShowRequestList(void);
};

class GraphicsWindow {
public:
    // This table describes the top-level menus in the graphics winodw.
    typedef void MenuHandler(int id);
    typedef struct {
        int         level;          // 0 == on menu bar, 1 == one level down
        char       *label;          // or NULL for a separator
        int         id;             // unique ID
        MenuHandler *fn;
    } MenuEntry;
    static const MenuEntry menu[];

    // The width and height (in pixels) of the window.
    double width, height;
    // These parameters define the map from 2d screen coordinates to the
    // coordinates of the 3d sketch points. We will use an axonometric
    // projection.
    Vector  offset;
    Vector  projRight;
    Vector  projDown;
    double  scale;
    struct {
        Vector  offset;
        Vector  projRight;
        Vector  projDown;
        Point2d mouse;
    }       orig;

    void Init(void);
    void NormalizeProjectionVectors(void);

    // These are called by the platform-specific code.
    void Paint(int w, int h);
    void MouseMoved(double x, double y, bool leftDown, bool middleDown,
                                bool rightDown, bool shiftDown, bool ctrlDown);
    void MouseLeftDown(double x, double y);
    void MouseLeftDoubleClick(double x, double y);
    void MouseMiddleDown(double x, double y);
    void MouseScroll(double x, double y, int delta);
};


#endif
