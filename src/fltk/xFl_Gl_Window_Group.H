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

/* \file
   Fl_Gl_Window_Group widget . */

#ifndef Fl_Gl_Window_Group_H
#define Fl_Gl_Window_Group_H

#include <FL/Fl_Gl_Window.H> //#include "Fl_Gl_Window.H"
#include <FL/x.H> //#include "x.H"

#define Fl_Gl_Window_Group xFl_Gl_Window_Group

class Fl_Gl_Window_Group_INTERNAL;

/**
  The Fl_Gl_Window_Group widget is an extended form of Fl_Gl_Window that
  can contain child widgets not specially modified to use OpenGL calls.
  After the main OpenGL area is drawn, child widgets are each drawn into an
  offscreen buffer (using the standard FLTK drawing routines), and then
  copied into the OpenGL window using a textured quad.
*/
class FL_EXPORT Fl_Gl_Window_Group : public Fl_Gl_Window {

  class Fl_Gl_Window_Group_INTERNAL *glstandin;

  Fl_Offscreen offscr;
  int offscr_w, offscr_h;
  uchar *imgbuf;

  unsigned int texid;

  void init(void);
  void adjust_offscr(int w, int h);

public:

  ~Fl_Gl_Window_Group(void);

  /**
    Creates a new Fl_Gl_Window_Group widget using the given size and label string.
  */
  Fl_Gl_Window_Group(int W, int H, const char *l=0)
    : Fl_Gl_Window(W,H,l) {init();}

  /**
    Creates a new Fl_Gl_Window_Group widget using the given position,
    size, and label string.
  */
  Fl_Gl_Window_Group(int X, int Y, int W, int H, const char *l=0)
    : Fl_Gl_Window(X,Y,W,H,l) {init();}

  void show(void);
  void hide(void);
  void clear(void);

  void flush(void);

  /**
    Handles the specified event.

    This method only receives events that were not used by any of the child
    widgets. Unlike most handle() methods, this should not call the
    inherited handle() method, or else an infinite loop will result.
  */
  virtual int handle_gl(int event);

  // override resize
  virtual void resize(int x, int y,int w, int h);

protected:

  void draw(void);
  void draw_children(void);
  void draw_child(Fl_Widget& widget);

  /**
    Draws the main OpenGL area of the Fl_Gl_Window_Group.

    You \e \b must override the draw_gl() method.
  */
  virtual void draw_gl(void);
};

#endif

//
// End of "$Id:$".
//
