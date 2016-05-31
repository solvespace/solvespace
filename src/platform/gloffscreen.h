//-----------------------------------------------------------------------------
// Offscreen rendering in OpenGL using framebuffer objects.
//
// Copyright 2015 <whitequark@whitequark.org>
//-----------------------------------------------------------------------------
#ifndef __GLOFFSCREEN_H
#define __GLOFFSCREEN_H

#include <stdint.h>

class GLOffscreen {
public:
    /* these allocate and deallocate OpenGL resources.
       an OpenGL context /must/ be current. */
    GLOffscreen();
    ~GLOffscreen();

    /* prepare for drawing a frame of specified size.
       returns true if OpenGL likes our configuration, false
       otherwise. if it returns false, the OpenGL state is restored. */
    bool begin(int width, int height);

    /* get pixels out of the frame and restore OpenGL state.
       the pixel format is ARGB32 with top row at index 0 if
       flip is true and bottom row at index 0 if flip is false.
       the returned array is valid until the next call to begin() */
    uint8_t *end();

private:
    unsigned int _framebuffer;
    unsigned int _color_renderbuffer, _depth_renderbuffer;
    uint8_t *_pixels;
    int _width, _height;
};

#endif
