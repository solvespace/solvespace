//-----------------------------------------------------------------------------
// Offscreen rendering in OpenGL using EGL and framebuffer objects.
//
// Copyright 2015-2016 whitequark
//-----------------------------------------------------------------------------
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#endif

#include "solvespace.h"

void GlOffscreen::Clear() {
    glDeleteRenderbuffersEXT(1, &depthRenderbuffer);
    glDeleteRenderbuffersEXT(1, &colorRenderbuffer);
    glDeleteFramebuffersEXT(1, &framebuffer);
    *this = {};
}

bool GlOffscreen::Render(int width, int height, std::function<void()> renderFn) {
    data.resize(width * height * 4);

    if(framebuffer == 0)
        glGenFramebuffersEXT(1, &framebuffer);
    if(colorRenderbuffer == 0)
        glGenRenderbuffersEXT(1, &colorRenderbuffer);
    if(depthRenderbuffer == 0)
        glGenRenderbuffersEXT(1, &depthRenderbuffer);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);

    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, colorRenderbuffer);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, width, height);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                 GL_RENDERBUFFER_EXT, colorRenderbuffer);

    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depthRenderbuffer);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                                 GL_RENDERBUFFER_EXT, depthRenderbuffer);

    bool framebufferComplete =
        glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT;
    if(framebufferComplete) {
        renderFn();
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, &data[0]);
#else
        glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, &data[0]);
#endif
    }

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    return framebufferComplete;
}
