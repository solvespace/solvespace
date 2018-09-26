//
// Created by benjamin on 9/25/18.
//

#ifndef SOLVESPACE_EVENTS_H
#define SOLVESPACE_EVENTS_H

namespace Platform{

// A mouse input event.
class MouseEvent {
public:
    enum class Type {
        MOTION,
        PRESS,
        DBL_PRESS,
        RELEASE,
        SCROLL_VERT,
        LEAVE,
    };

    enum class Button {
        NONE,
        LEFT,
        MIDDLE,
        RIGHT,
    };

    Type        type;
    double      x;
    double      y;
    bool        shiftDown;
    bool        controlDown;
    union {
        Button      button;       // for Type::{MOTION,PRESS,DBL_PRESS,RELEASE}
        double      scrollDelta;  // for Type::SCROLL_VERT
    };
};

// A 3-DOF input event.
struct SixDofEvent {
    enum class Type {
        MOTION,
        PRESS,
        RELEASE,
    };

    enum class Button {
        FIT,
    };

    Type   type;
    bool   shiftDown;
    bool   controlDown;
    double translationX, translationY, translationZ; // for Type::MOTION
    double rotationX,    rotationY,    rotationZ;    // for Type::MOTION
    Button button;                                   // for Type::{PRESS,RELEASE}
};

// A keyboard input event.
struct KeyboardEvent {
    enum class Type {
        PRESS,
        RELEASE,
    };

    enum class Key {
        CHARACTER,
        FUNCTION,
    };

    Type        type;
    Key         key;
    union {
        char32_t    chr; // for Key::CHARACTER
        int         num; // for Key::FUNCTION
    };
    bool        shiftDown;
    bool        controlDown;

    bool Equals(const KeyboardEvent &other) {
        return type == other.type && key == other.key && num == other.num &&
               shiftDown == other.shiftDown && controlDown == other.controlDown;
    }
};


}

#endif //SOLVESPACE_EVENTS_H
