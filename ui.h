
#ifndef __UI_H
#define __UI_H

typedef struct {
    static const int MAX_COLS = 200;
    static const int MAX_ROWS = 500;

#ifndef RGB
#define RGB(r, g, b) ((r) | ((g) << 8) | ((b) << 16))
#endif
    static const int COLOR_BG_DEFAULT       = RGB( 15,  15,   0);
    static const int COLOR_FG_DEFAULT       = RGB(255, 255, 255);
    static const int COLOR_BG_CMDLINE       = RGB(  0,  20,  80);
    static const int COLOR_FG_CMDLINE       = RGB(255, 255, 255);

    // The line with the user-typed command, that is currently being edited.
    char    cmd[MAX_COLS];
    int     cmdInsert;
    int     cmdLen;

    // The rest of the window, text displayed in response to typed commands;
    // some of this might do something if you click on it.

    static const int NOT_A_LINK = 0;

    static const int COLOR_NORMAL   = 0;

    BYTE    text[MAX_ROWS][MAX_COLS];
    struct {
        int     color;
        int     link;
        DWORD   data;
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
} TextWindow;

typedef struct {
    // This table describes the top-level menus in the graphics winodw.
    typedef void MenuHandler(int id);
    typedef struct {
        int         level;          // 0 == on menu bar, 1 == one level down, ...
        char       *label;          // or NULL for a separator
        int         id;             // unique ID
        MenuHandler *fn;
    } MenuEntry;
    static const MenuEntry menu[];


    // These parameters define the map from 2d screen coordinates to the
    // coordinates of the 3d sketch points. We will use an axonometric
    // projection.
    Vector  offset;
    double  scale;
    Vector  projRight;
    Vector  projDown;

    // These are called by the platform-specific code.
    void Paint(void);
    void MouseMoved(double x, double y, bool leftDown, bool middleDown,
                                        bool rightDown);
    void MouseLeftClick(double x, double y);
    void MouseLeftDoubleClick(double x, double y);
    void MouseScroll(int delta);
} GraphicsWindow;


#endif
