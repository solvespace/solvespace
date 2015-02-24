//
// "$Id$"
//
// OpenGL window group widget for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2010 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     http://www.fltk.org/COPYING.php
//
// Please report all bugs and problems on the following page:
//
//     http://www.fltk.org/str.php
//

#include <config.h>
#include <stdio.h>
#include <stdlib.h>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>

#include <FL/fl_draw.H>
#include <FL/gl.h>

#include <fltk/xFl_Gl_Window_Group.H>

#if 1 //HAVE_GL

#if !defined(GL_TEXTURE_RECTANGLE) && defined(WIN32)
#  define GL_TEXTURE_RECTANGLE 0x84F5
#endif

#define Gl_Stand_In Fl_Gl_Window_Group_INTERNAL

#define RESET_FIELDS() \
  imgbuf = NULL; \
  offscr_w = -1; \
  offscr_h = -1

class Gl_Stand_In : public Fl_Box {

  Fl_Gl_Window_Group *glwg;

public:

  Gl_Stand_In(int W, int H, Fl_Gl_Window_Group *w)
  : Fl_Box(0, 0, W, H) {
    glwg = w;
  }

  int handle(int event) { return glwg->handle_gl(event); }

protected:

  void draw(void) { Fl::fatal("Never call this"); }

  virtual void dummy(void);
};

void Gl_Stand_In::dummy(void) {}

void Fl_Gl_Window_Group::init(void) {
  begin();  // Revert the end() in the Fl_Gl_Window constructor
  glstandin = new Gl_Stand_In(w(), h(), this);
  texid = 0;
  RESET_FIELDS();
}

Fl_Gl_Window_Group::~Fl_Gl_Window_Group(void) {
  delete glstandin;
}

void Fl_Gl_Window_Group::adjust_offscr(int w, int h) {
  if (imgbuf == NULL) {
    // Find maximum width and height across all visible child widgets
    // (except for the GL stand-in widget)
    Fl_Widget*const* a = array();
    for (int i = children(); i--;) {
      Fl_Widget* o = a[i];
      if (o == glstandin) continue;
      if (!o->visible()) continue;
      int cw = o->w();
      int ch = o->h();
      if (offscr_w < cw) offscr_w = cw;
      if (offscr_h < ch) offscr_h = ch;
    }
  } else {
    fl_delete_offscreen(offscr);
  }

  if (w > offscr_w) offscr_w = w;
  if (h > offscr_h) offscr_h = h;

  offscr = fl_create_offscreen(offscr_w, offscr_h);

  int imgbuf_size = offscr_w * offscr_h * 3;  // GL_RGB
  imgbuf = (uchar *)realloc(imgbuf, (size_t)imgbuf_size);
}

void Fl_Gl_Window_Group::show() {
  Fl_Gl_Window::show();

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glEnable(GL_TEXTURE_RECTANGLE);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glGenTextures(1, &texid);
  glBindTexture(GL_TEXTURE_RECTANGLE, texid);
  make_current();
}

void Fl_Gl_Window_Group::hide() {
  glDeleteTextures(1, &texid);
  texid = 0;
  make_current();

  Fl_Gl_Window::hide();
}

/**
  Deletes all child widgets from memory recursively.

  This method differs from the remove() method in that it
  affects all child widgets and deletes them from memory.
*/
void Fl_Gl_Window_Group::clear(void) {
  if (imgbuf != NULL) {
    fl_delete_offscreen(offscr);
    free(imgbuf);
    RESET_FIELDS();
  }
  remove(glstandin);
  Fl_Gl_Window::clear();
  add(glstandin);
}

int Fl_Gl_Window_Group::handle_gl(int event) {
  // Override me
  return 0;
}

void Fl_Gl_Window_Group::resize(int x, int y,int w, int h)
{
    glstandin->resize(x,y,w,h);
    Fl_Window::resize(x,y,w,h);
}

void Fl_Gl_Window_Group::flush(void) {
  // Fl_Window::make_current() does this, but not
  // Fl_Gl_Window::make_current(), and we can't override make_current()
  // and have Fl_Gl_Window::flush() call us

  Fl_Window::make_current();
  Fl_Gl_Window::make_current();

  Fl_Gl_Window::flush();
}

void Fl_Gl_Window_Group::draw(void) {
  if (damage()) {
    draw_gl();
    draw_children();
  }
}

/**
  Draws all children of the group.
*/
void Fl_Gl_Window_Group::draw_children(void) {
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glOrtho(0, w(), 0, h(), -1.0, 1.0);
  glTranslatef(0.0, h(), 0.0);
  glScalef(1.0, -1.0, 1.0);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glViewport(0, 0, w(), h());

  Fl_Widget*const* a = array();
  for (int i = children(); i--;) draw_child(**a++);

#if 1
  int err = glGetError();
  if (err != GL_NO_ERROR) {
    Fl::warning("OpenGL error after drawing Fl_Gl_Window_Group children: 0x0%X", err);
  }
#endif

  make_current();
}

/**
  This draws a child widget, if it is not clipped.
  The damage bits are cleared after drawing.
*/
void Fl_Gl_Window_Group::draw_child(Fl_Widget& widget) {
  if (&widget == glstandin) return;

  if (!widget.visible() || widget.type() >= FL_WINDOW ||
      !fl_not_clipped(widget.x(), widget.y(), widget.w(), widget.h())) return;

  if (widget.w() > offscr_w || widget.h() > offscr_h) {
    adjust_offscr(widget.w(), widget.h());
  }

  int widget_x = widget.x();
  int widget_y = widget.y();
  int widget_w = widget.w();
  int widget_h = widget.h();

  widget.position(0, 0);

  fl_begin_offscreen(offscr);
  fl_rectf(0, 0, widget_w, widget_h, FL_MAGENTA);
  widget.clear_damage(FL_DAMAGE_ALL);
  widget.draw();
  widget.clear_damage();
  fl_read_image(imgbuf, 0, 0, widget_w, widget_h);
  fl_end_offscreen();

  widget.position(widget_x, widget_y);

#ifdef USE_GLDRAWPIXELS  // Note: glDrawPixels() is deprecated

  glRasterPos2i(widget_x, widget_y);
  glPixelZoom(1.0, -1.0);
  glDrawPixels(widget_w, widget_h, GL_RGB, GL_UNSIGNED_BYTE, imgbuf);

#else // ! USE_GLDRAWPIXELS

  glTexImage2D(
    GL_TEXTURE_RECTANGLE,
    0,
    GL_RGB,
    widget_w, widget_h,
    0,
    GL_RGB,
    GL_UNSIGNED_BYTE,
    imgbuf);

#define CORNER(x,y) glTexCoord2f(x, y); glVertex2f(widget_x + x, widget_y + y)
  glBegin(GL_QUADS);
  CORNER(0, 0);
  CORNER(widget_w, 0);
  CORNER(widget_w, widget_h);
  CORNER(0, widget_h);
  glEnd();
#undef CORNER

#endif // ! USE_GLDRAWPIXELS
}

void Fl_Gl_Window_Group::draw_gl(void) {
  Fl::fatal("Fl_Gl_Window_Group::draw_gl() *must* be overriden. Please refer to the documentation.");
}

#else // ! HAVE_GL

typedef int no_opengl_support;

#endif // ! HAVE_GL

//
// End of "$Id:$".
//
