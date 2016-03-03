//-----------------------------------------------------------------------------
// Offscreen rendering in OpenGL using framebuffer objects.
//
// Copyright 2015 <whitequark@whitequark.org>
//-----------------------------------------------------------------------------
#ifdef __APPLE__
#include <OpenGL/GL.h>
#else
#include <GL/glew.h>
#endif

#include "gloffscreen.h"
#include "solvespace.h"

GLOffscreen::GLOffscreen() : _pixels(NULL), _pixels_inv(NULL), _width(0), _height(0) {
#ifndef __APPLE__
    if(glewInit() != GLEW_OK)
        oops();
#endif

    if(!GL_EXT_framebuffer_object)
        oops();

    glGenFramebuffersEXT(1, &_framebuffer);
    glGenRenderbuffersEXT(1, &_color_renderbuffer);
    glGenRenderbuffersEXT(1, &_depth_renderbuffer);
}

GLOffscreen::~GLOffscreen() {
    delete[] _pixels;
    delete[] _pixels_inv;
    glDeleteRenderbuffersEXT(1, &_depth_renderbuffer);
    glDeleteRenderbuffersEXT(1, &_color_renderbuffer);
    glDeleteFramebuffersEXT(1, &_framebuffer);
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

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _framebuffer);

    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, _color_renderbuffer);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, _width, _height);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                              GL_RENDERBUFFER_EXT, _color_renderbuffer);

    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, _depth_renderbuffer);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, _width, _height);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                              GL_RENDERBUFFER_EXT, _depth_renderbuffer);

    if(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT)
        return true;

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    return false;
}

uint8_t *GLOffscreen::end(bool flip) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    glReadPixels(0, 0, _width, _height,
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, _pixels_inv);
#else
    glReadPixels(0, 0, _width, _height,
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, _pixels_inv);
#endif

    if(flip) {
        /* in OpenGL coordinates, bottom is zero Y */
        for(int i = 0; i < _height; i++)
            memcpy(&_pixels[_width * i], &_pixels_inv[_width * (_height - i - 1)], _width * 4);
    }

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    return (uint8_t*) (flip ? _pixels : _pixels_inv);
}
