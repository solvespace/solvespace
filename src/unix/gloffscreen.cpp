//-----------------------------------------------------------------------------
// Offscreen rendering in OpenGL using framebuffer objects.
//
// Copyright 2015 <whitequark@whitequark.org>
//-----------------------------------------------------------------------------
#include "gloffscreen.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <GL/glew.h>

GLOffscreen::GLOffscreen() : _pixels(NULL), _pixels_inv(NULL) {
    if(glewInit() != GLEW_OK)
        abort();

    glGenFramebuffers(1, &_framebuffer);
    glGenRenderbuffers(1, &_color_renderbuffer);
    glGenRenderbuffers(1, &_depth_renderbuffer);
}

GLOffscreen::~GLOffscreen() {
    glDeleteRenderbuffers(1, &_depth_renderbuffer);
    glDeleteRenderbuffers(1, &_color_renderbuffer);
    glDeleteFramebuffers(1, &_framebuffer);
}

bool GLOffscreen::begin(int width, int height) {
    if(_width != width || _height != height) {
        delete[] _pixels;
        delete[] _pixels_inv;

        _pixels = new uint32_t[width * height * 4];
        _pixels_inv = new uint32_t[width * height * 4];

        _width = width;
        _height = height;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);

    glBindRenderbuffer(GL_RENDERBUFFER, _color_renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, _width, _height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, _color_renderbuffer);

    glBindRenderbuffer(GL_RENDERBUFFER, _depth_renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, _width, _height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, _depth_renderbuffer);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        return true;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return false;
}

uint8_t *GLOffscreen::end() {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    glReadPixels(0, 0, _width, _height,
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, _pixels_inv);
#else
    glReadPixels(0, 0, _width, _height,
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, _pixels_inv);
#endif

    /* in OpenGL coordinates, bottom is zero Y */
    for(int i = 0; i < _height; i++)
        memcpy(&_pixels[_width * i], &_pixels_inv[_width * (_height - i - 1)], _width * 4);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return (uint8_t*) _pixels;
}
