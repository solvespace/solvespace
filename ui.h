
#ifndef __UI_H
#define __UI_H

typedef struct {
    static const int MAX_COLS = 200;
    static const int MAX_ROWS = 500;

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

    // These are called by the platform-specific code.
    void KeyPressed(int c);
    bool IsHyperlink(int width, int height);
} TextWindow;

typedef struct {
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
