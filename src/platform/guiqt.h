#ifndef SOLVESPACE_GUIQT_H
#define SOLVESPACE_GUIQT_H


#include <solvespace.h>

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
    SSView(QWidget* parent = 0)
        : QOpenGLWidget(parent) {

        entry = new QLineEdit(this);
        entry->setGeometry(QRect(0, 0, 200, 40));
        QFont entryFont("Arial", 16);
        entry->setFont(entryFont);
        entry->setVisible(false);
        connect(entry, SIGNAL(returnPressed()), this, SLOT(entryFinished()));
        setFocusPolicy(Qt::FocusPolicy::StrongFocus);
        setMouseTracking(true);
    }

    ~SSView()
    {
        delete entry;
    }

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

    void entryFinished()
    {
        std::string entryText =  entry->text().toStdString();
        emit editingDoneSignal(entryText);
    }

protected:
    //void initializeGL() {}

    void paintGL() {
        emit renderOccuredSignal();
    }

    void resizeGL(int width, int height) {
        emit renderOccuredSignal();
    }

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

    bool event(QEvent* e);

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

signals:
    void mouseEventOccuredSignal(MouseEvent mouseEvent);
    void keyEventOccuredSignal(KeyboardEvent keyEvent);
    void renderOccuredSignal();
    void editingDoneSignal(std::string);
    void tabKeyPressed();

public:

    QLineEdit* entry;
    MouseEvent    slvMouseEvent;
    KeyboardEvent slvKeyEvent;
};


class SSMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    SSMainWindow(Platform::Window* pwin, Platform::Window::Kind kind,
                 QWidget* parent = 0);

protected slots:
    void sliderSlot(int value);

protected:
    void closeEvent(QCloseEvent* ev);

signals:
    void windowClosedSignal();

public:
    Platform::Window* receiver;
    SSView* ssView;
    QScrollBar* scrollBar;
};

}
}

#endif
