//-----------------------------------------------------------------------------
// The Qt-based implementation of platform-dependent GUI functionality.
//
// Copyright 2024 Karl Robillard
// Copyright 2023 Raed Marwan
// SPDX-License-Identifier: GPL-3.0-or-later
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_GUIQT_H
#define SOLVESPACE_GUIQT_H

#include <solvespace.h>

#include <QDockWidget>
#include <QLineEdit>
#include <QMainWindow>
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <QScrollBar>


namespace SolveSpace {
namespace Platform {

class SSView : public QOpenGLWidget
{
    Q_OBJECT

public:
    SSView(QWidget* parent = 0);

    void startEditing(int x, int y, int fontHeight, int minWidth,
                      bool isMonoSpace, const std::string& val);
    void stopEditing();

    QSize minimumSizeHint() const {
        return QSize(50, 50);
    }

    QSize sizeHint() const {
        return QSize(400, 400);
    }

protected slots:
    void entryFinished();

protected:
    //void initializeGL() {}
    //void resizeGL(int width, int height) {}

    void paintGL();

    //----Mouse Events--------
    void wheelEvent(QWheelEvent* event);

    void mouseReleaseEvent(QMouseEvent* event) override{
        updateSlvSpaceMouseEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override{
        updateSlvSpaceMouseEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override{
        updateSlvSpaceMouseEvent(event);
    }

    void updateSlvSpaceMouseEvent(QMouseEvent* event);

    //----Keyboard Events--------

    void keyPressEvent(QKeyEvent* keyEvent)
    {
        slvKeyEvent.type = KeyboardEvent::Type::PRESS;
        updateSlvSpaceKeyEvent(keyEvent);
        QWidget::keyPressEvent(keyEvent);
    }

    void keyReleaseEvent(QKeyEvent* keyEvent)
    {
        slvKeyEvent.type = KeyboardEvent::Type::RELEASE;
        updateSlvSpaceKeyEvent(keyEvent);
        QWidget::keyReleaseEvent(keyEvent);
    }

    void updateSlvSpaceKeyEvent(QKeyEvent* event);

public:
    Platform::Window* receiver;
    QLineEdit* entry;
    double pixelRatio;
    double pixelRatioI;
    MouseEvent    slvMouseEvent;
    KeyboardEvent slvKeyEvent;
};

class SSTextWindow : public QDockWidget
{
    Q_OBJECT

public:
    SSTextWindow(QWidget* parent = 0);

protected slots:
    void sliderSlot(int value);

protected:
    void closeEvent(QCloseEvent* ev);

public:
    SSView* ssView;
    QScrollBar* scrollBar;
};

class SSMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    SSMainWindow();

protected:
    void closeEvent(QCloseEvent* ev);

public:
    SSView* ssView;
};

}
}

#endif
