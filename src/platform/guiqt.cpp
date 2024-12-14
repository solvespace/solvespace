//-----------------------------------------------------------------------------
// The Qt-based implementation of platform-dependent GUI functionality.
//
// Copyright 2024 Karl Robillard
// Copyright 2023 Raed Marwan
// SPDX-License-Identifier: GPL-3.0-or-later
//-----------------------------------------------------------------------------

#include <iostream>
#include "solvespace.h"
#include "platform.h"
#include "guiqt.h"

#include <QApplication>
#include <QAction>
#include <QActionGroup>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QTimer>


namespace SolveSpace {
namespace Platform {

//-----------------------------------------------------------------------------
// Fatal errors
//-----------------------------------------------------------------------------

void FatalError(const std::string& message)
{
    QMessageBox::critical(NULL, QString("Fatal Error"),
                          QString::fromStdString(message),
                          QMessageBox::StandardButton::Ok);
    abort();
}

//-----------------------------------------------------------------------------
// Message dialogs
//-----------------------------------------------------------------------------

class MessageDialogImplQt final : public MessageDialog {
public:
    QMessageBox messageBoxQ;

    MessageDialogImplQt(QWidget* parent) : messageBoxQ(parent) {
    }

#undef ERROR 
    void SetType(Type type) override {
        switch (type) {
        case Type::INFORMATION:
            messageBoxQ.setIcon(QMessageBox::Information);
            break;
        case Type::QUESTION:
            messageBoxQ.setIcon(QMessageBox::Question);
            break;
        case Type::WARNING:
            messageBoxQ.setIcon(QMessageBox::Warning);
            break;
        case Type::ERROR:
            messageBoxQ.setIcon(QMessageBox::Critical);
            break;
        }
    }

    void SetTitle(std::string title) override {
        messageBoxQ.setWindowTitle(QString::fromStdString(title));
    }

    void SetMessage(std::string message) override {
        messageBoxQ.setText(QString::fromStdString(message));
    }

    void SetDescription(std::string description) override {
        messageBoxQ.setInformativeText(QString::fromStdString(description));
    }

    void AddButton(std::string label, Response response, bool isDefault) override {
        QMessageBox::StandardButton std;
        switch (response) {
            case Response::NONE:   ssassert(false, "Invalid response");
            case Response::OK:
                std = QMessageBox::StandardButton::Ok;
                break;
            case Response::YES:
                std = QMessageBox::StandardButton::Yes;
                break;
            case Response::NO:
                std = QMessageBox::StandardButton::No;
                break;
            case Response::CANCEL:
                std = QMessageBox::StandardButton::Cancel;
                break;
        }
        QPushButton* button = messageBoxQ.addButton(std);
        button->setText(QString::fromStdString(label));
        if (isDefault)
            messageBoxQ.setDefaultButton(button);
    }

    Response RunModal() {
        switch (messageBoxQ.exec())
        {
        case QMessageBox::StandardButton::Cancel:
            return Response::CANCEL;
        case QMessageBox::StandardButton::No:
            return Response::NO;
        case QMessageBox::StandardButton::Yes:
            return Response::YES;
        case QMessageBox::StandardButton::Ok:
            return Response::OK;
        default: // NONE
            return Response::NONE;
        }
    }
};

//-----------------------------------------------------------------------------
// File dialogs
//-----------------------------------------------------------------------------

class FileDialogImplQt final : public FileDialog {
public:
    QFileDialog fileDialogQ;
    QStringList filters;
    bool filtersMod;

    FileDialogImplQt(QWidget* parent, bool save) : filtersMod(false) {
        fileDialogQ.setParent(parent);
        fileDialogQ.setWindowFlags(Qt::Dialog); // Reset after setParent.

        // NOTE: The file extension is automatically set by the KDE native
        // dialog but not with the Qt one.
#if 0
        fileDialogQ.setOption(QFileDialog::DontUseNativeDialog);
#endif

        if (save) {
            fileDialogQ.setAcceptMode(QFileDialog::AcceptSave);
        }
    }

    void SetTitle(std::string title) {
        fileDialogQ.setWindowTitle(QString::fromStdString(title));
    }

    void SetCurrentName(std::string name) {
        fileDialogQ.selectFile(QString::fromStdString(name));
    }

    Platform::Path GetFilename() {
        QStringList files = fileDialogQ.selectedFiles();
        return Path::From(files.at(0).toStdString());
    }

    void SetFilename(Platform::Path path) {
        fileDialogQ.selectFile(QString::fromStdString(path.raw));
    }

    void SuggestFilename(Platform::Path path) {
        fileDialogQ.selectFile(QString::fromStdString(path.FileStem()));
    }

    void AddFilter(std::string name, std::vector<std::string> extensions) override {
        QString fline(name.c_str());
        int count = 0;
        for (auto& ext : extensions) {
            fline.append(count ? " *." : " (*.");
            fline.append(ext.c_str());
            ++count;
        }
        fline.append(")");

        //printf("AddFilter \"%s\"\n", fline.toLocal8Bit().data());
        filters.append(fline);
        filtersMod = true;
    }

    void updateFilters() {
        if (filtersMod) {
            filtersMod = false;
            fileDialogQ.setNameFilters(filters);
        }
    }

    void FreezeChoices(SettingsRef settings, const std::string& key)
    {
        settings->FreezeString("Dialog_" + key + "_Folder",
                fileDialogQ.directory().absolutePath().toStdString());
        settings->FreezeString("Dialog_" + key + "_Filter",
                fileDialogQ.selectedNameFilter().toStdString());
    }

    void ThawChoices(SettingsRef settings, const std::string &key)
    {
        updateFilters();

        std::string val;
        val = settings->ThawString("Dialog_" + key + "_Folder");
        fileDialogQ.setDirectory(QDir(QString::fromStdString(val)));
        val = settings->ThawString("Dialog_" + key + "_Filter");
        fileDialogQ.selectNameFilter(QString::fromStdString(val));
    }

    bool RunModal() override {
        updateFilters();
        return fileDialogQ.exec();
    }
};

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------

class SettingsImplQt final : public Settings {
public:
    QSettings qset;

    SettingsImplQt() : qset("SolveSpace", "solvespace") {
    }

    void FreezeInt(const std::string& key, uint32_t value) override {
        qset.setValue(QString::fromStdString(key), value);
    }

    uint32_t ThawInt(const std::string& key, uint32_t defaultValue = 0) override {
        return qset.value(QString::fromStdString(key), defaultValue).toInt();
    }

    void FreezeFloat(const std::string& key, double value) override{
        qset.setValue(QString::fromStdString(key), value);
    }

    double ThawFloat(const std::string& key, double defaultValue = 0.0) override {
        return qset.value(QString::fromStdString(key), defaultValue).toDouble();
    }

    void FreezeString(const std::string& key, const std::string& value) override {
        qset.setValue(QString::fromStdString(key),
                      QString::fromStdString(value));
    }

    std::string ThawString(const std::string& key, const std::string& defaultValue = "") override {
        return qset.value(QString::fromStdString(key),
                          QString::fromStdString(defaultValue)).toString().toStdString();
    }
};

SettingsRef GetSettings()
{
    static std::shared_ptr<SettingsImplQt> settings;
    if (!settings) {
        settings = std::make_shared<SettingsImplQt>();
    }
    return settings;
}

//-----------------------------------------------------------------------------
// Timers
//-----------------------------------------------------------------------------

class TimerImplQt final : public Timer, public QObject
{
public:
    TimerImplQt() {}

    void RunAfter(unsigned milliseconds) override {
        QTimer::singleShot(milliseconds, this, &TimerImplQt::runOnTimeOut);
    }

    void runOnTimeOut() {
        if(onTimeout) {
            onTimeout();
        }
    }
};

TimerRef CreateTimer() {
    std::shared_ptr<TimerImplQt> timer = std::make_shared<TimerImplQt>();
    return timer;
}

//-----------------------------------------------------------------------------
// Menus
//-----------------------------------------------------------------------------

class MenuItemImplQt final : public MenuItem, public QObject {
public:
    QAction action;

    MenuItemImplQt() {
        connect(&action, &QAction::triggered, this, &MenuItemImplQt::onTriggered);
    }

    void SetAccelerator(KeyboardEvent accel) override {
        int key = 0;

        if (accel.key == KeyboardEvent::Key::CHARACTER) {
            if (accel.chr == '\t') {
                key = Qt::Key_Tab;
            } else if (accel.chr == '\x1b') {
                key = Qt::Key_Escape;
            } else if (accel.chr == '\x7f') {
                key = Qt::Key_Delete;
            } else {
                key = std::toupper(accel.chr);
            }
        } else if (accel.key == KeyboardEvent::Key::FUNCTION) {
            key = Qt::Key_F1 + accel.num - 1;
        }

        if (key) {
            if (accel.controlDown) {
                key |= Qt::CTRL;
            }
            if (accel.shiftDown) {
                key |= Qt::SHIFT;
            }
            action.setShortcut(key);
        }
    }

    /*
    RADIO_MARK must be implemented using QActionGroup so we scan for
    sequential actions with the "radio_mark" property to form these groups.
    It would be safer if the Indicator type was an argument to
    Platform::Menu::AddItem rather than a set by a separate method to guarantee
    that the Indicator type can never change.
    */
    void SetIndicator(Indicator type) override {
        switch (type)
        {
        case Indicator::NONE:
            action.setCheckable(false);
            break;
        case Indicator::CHECK_MARK:
            action.setCheckable(true);
            break;
        case Indicator::RADIO_MARK:
            action.setCheckable(true);
            // Added to QActionGroup later by formActionGroups().
            action.setProperty("radio_mark", true);
            break;
        }
    }

    void SetEnabled(bool enabled) override {
        action.setEnabled(enabled);
    }

    void SetActive(bool active) override {
        action.setChecked(active);
    }

public slots:
    void onTriggered(bool value)
    {
        if (onTrigger)
            onTrigger();
    }
};

class MenuImplQt final : public Menu {
public:
    QMenu* menuQ;
    bool  hasParent;
    std::vector<std::shared_ptr<MenuItemImplQt>>   menuItems;
    std::vector<std::shared_ptr<MenuImplQt>>       subMenus;

    MenuImplQt(bool hasParentParam = true) {
        menuQ = new QMenu();
        hasParent = hasParentParam;
    }

    MenuImplQt(QMenu* menuQParam) {
        menuQ = menuQParam;
        hasParent = true;
    }

    std::shared_ptr<MenuItem> AddItem(const std::string& label, std::function<void()> onTrigger,
        bool /*mnemonics*/) override {

        std::shared_ptr<MenuItemImplQt> menuItem = std::make_shared<MenuItemImplQt>();
        menuItem->onTrigger = onTrigger;
        menuItem->action.setText(QString::fromStdString(label));
        // I do not think anything is needed for mnemonics flag.
        // the shortcut key sequence is set in the menuItem class and Qt acts
        // accordingly with no further activation ... I think
        menuItems.push_back(menuItem);

        // The QWidget::addAction method does not take ownership of the action.
        menuQ->addAction(&menuItem->action);
        return menuItem;
    }

    std::shared_ptr<Menu> AddSubMenu(const std::string& label) override {
        std::shared_ptr<MenuImplQt> subMenu = std::make_shared<MenuImplQt>(menuQ->addMenu(QString::fromStdString(label)));
        subMenus.push_back(subMenu);
        return subMenu;
    }

    void AddSeparator() override {
        menuQ->addSeparator();
    }

    void PopUp() override {
        if (true == this->hasParent)
            return;

        menuQ->popup(menuQ->mapFromGlobal(QCursor::pos()));
        menuQ->exec();
    }

    void Clear() override {
        menuQ->clear();
        menuItems.clear();
        subMenus.clear();
    }
};

MenuRef CreateMenu() {
    return std::make_shared<MenuImplQt>(false); // false says -> menu has not Qtwidget as a parent
}

class MenuBarImpQt final : public MenuBar {
public:
    QMenuBar* menuBarQ;
    std::vector<std::shared_ptr<MenuImplQt>> subMenus;

    MenuBarImpQt() {
        // The QMenuBar is deleted by the QMainWindow its attached to.
        menuBarQ = new QMenuBar;
        menuBarQ->setNativeMenuBar(false);
    }

    std::shared_ptr<Menu> AddSubMenu(const std::string& label) override {
        std::shared_ptr<MenuImplQt> menu = std::make_shared<MenuImplQt>();
        menuBarQ->addMenu(menu->menuQ);
        menu->menuQ->setTitle(QString::fromStdString(label));
        subMenus.push_back(menu);
        return menu;
    }

    void Clear() override {
        menuBarQ->clear();
        subMenus.clear();
    }

    static void _formActionGroups(QMenu *menu) {
        QActionGroup* group = nullptr;
        foreach (QAction* act, menu->actions()) {
            if(act->menu()) {
                _formActionGroups(act->menu());
            } else {
                QVariant var = act->property("radio_mark");
                if(var.isValid()) {
                    if(! group)
                        group = new QActionGroup(menu);
                    //printf("radio_mark on menu %p\n", menu);
                    group->addAction(act);
                } else {
                    group = nullptr;    // End current group.
                }
            }
        }
    }

    void formActionGroups() {
        foreach (QAction* act, menuBarQ->actions()) {
            if (act->menu()) {
                _formActionGroups(act->menu());
            }
        }
    }
};

MenuBarRef GetOrCreateMainMenu(bool* unique) {
    *unique = false;
    return std::make_shared<MenuBarImpQt>();
}

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------

class SSApplication : public QApplication {
public:
    SSApplication(int &argc, char **argv) : QApplication(argc, argv)
    {
        // Setup fonts once for SSView QLineEdit.
#ifdef _WIN32
        fontEdit     = QFont("Arial");
        fontEditMono = QFont("Lucida Console");
#else
        fontEdit     = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        fontEditMono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        /*
        printf("Font general %s\n", fontEdit.family().toLocal8Bit().data());
        printf("Font mono %s\n", fontEditMono.family().toLocal8Bit().data());
        */
#endif

        // Set language locale.
        std::string localeStr("en_US");
        QStringList list = QLocale::system().uiLanguages();
        if (! list.empty()) {
            QString lang = list[0];
            if (lang.size() == 5 && lang[2] == '-') {
                lang[2] = '_';
                localeStr = lang.toStdString();
            }
        }
        SetLocale(localeStr);

#ifdef __linux
        QIcon icon("/usr/share/icons/hicolor/48x48/apps/solvespace.png");
        setWindowIcon(icon);
#endif
    }

    QFont fontEdit;
    QFont fontEditMono;
};

SSView::SSView(QWidget* parent) : QOpenGLWidget(parent) {
    entry = new QLineEdit(this);
    entry->setVisible(false);
    connect(entry, SIGNAL(returnPressed()), SLOT(entryFinished()));

    setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    setMouseTracking(true);
    pixelRatio = pixelRatioI = 1.0;
}

void SSView::startEditing(int x, int y, int fontHeight, int minWidth,
                          bool isMonoSpace, const std::string& val)
{
    //printf("startEditing %d,%d, %d,%d %d\n", x, y, fontHeight, minWidth, isMonoSpace);

    SSApplication* app = static_cast<SSApplication*>(qApp);
    QFont font = isMonoSpace ? app->fontEditMono : app->fontEdit;
    font.setPixelSize(fontHeight);
    entry->setFont(font);

    QFontMetrics fm(font);
    int valWidth = fm.averageCharWidth() * val.size();
    if (minWidth < valWidth)
        minWidth = valWidth;
    if (x < 0)
        x = 0;

    entry->setGeometry(QRect(x, y - fm.ascent(), minWidth, fm.height()));
    entry->setText(QString::fromStdString(val));

    if (! entry->isVisible()) {
        entry->selectAll();
        entry->show();
        entry->setFocus();
    }
}

void SSView::stopEditing() {
    if (entry->isVisible()) {
        entry->hide();
        setFocus();
    }
}

void SSView::entryFinished() {
    std::string entryText = entry->text().toStdString();
    receiver->onEditingDone(entryText);
}

void SSView::paintGL() {
    // pixelRatio is set here as devicePixelRatioF has been seen to change
    // after the resizeGL() & showEvent() methods are called.
    pixelRatio = devicePixelRatioF();
    pixelRatioI = floor(pixelRatio);

    receiver->onRender();
}

static inline void setMouseKeyModifiers(MouseEvent& me, const QInputEvent* qe)
{
    Qt::KeyboardModifiers mod = qe->modifiers();
    me.shiftDown   = mod & Qt::ShiftModifier   ? true : false;
    me.controlDown = mod & Qt::ControlModifier ? true : false;
}

void SSView::wheelEvent(QWheelEvent* event)
{
    const double wheelDelta=120.0;
    slvMouseEvent.button = MouseEvent::Button::NONE;
    slvMouseEvent.type = MouseEvent::Type::SCROLL_VERT;
    slvMouseEvent.scrollDelta = (double)event->angleDelta().y() / wheelDelta;
    setMouseKeyModifiers(slvMouseEvent, event);

    receiver->onMouseEvent(slvMouseEvent);
}

void SSView::updateSlvSpaceMouseEvent(QMouseEvent* event)
{
    switch (event->type())
    {
    case QEvent::MouseMove:
        slvMouseEvent.type = MouseEvent::Type::MOTION;
        break;
    case QEvent::MouseButtonPress:
        slvMouseEvent.type = MouseEvent::Type::PRESS;
        break;
    case QEvent::MouseButtonDblClick:
        slvMouseEvent.type = MouseEvent::Type::DBL_PRESS;
        break;
    case QEvent::MouseButtonRelease:
        slvMouseEvent.type = MouseEvent::Type::RELEASE;
        slvMouseEvent.button = MouseEvent::Button::NONE;
        break;
    case QEvent::Wheel:
        slvMouseEvent.type = MouseEvent::Type::SCROLL_VERT;
        //slvMouseEvent.scrollDelta = event->
        break;
    case QEvent::Leave:
        slvMouseEvent.type = MouseEvent::Type::LEAVE;
        slvMouseEvent.button = MouseEvent::Button::NONE;
        break;
    default:
        slvMouseEvent.type = MouseEvent::Type::RELEASE;
        slvMouseEvent.button = MouseEvent::Button::NONE;
    }

    Qt::MouseButtons buttonState = (slvMouseEvent.type == MouseEvent::Type::MOTION) ? event->buttons() : event->button();

    switch (buttonState)
    {
    case Qt::MouseButton::LeftButton:
        slvMouseEvent.button = MouseEvent::Button::LEFT;
        break;
    case Qt::MouseButton::RightButton:
        slvMouseEvent.button = MouseEvent::Button::RIGHT;
        break;
    case Qt::MouseButton::MiddleButton:
        slvMouseEvent.button = MouseEvent::Button::MIDDLE;
        break;
    default:
        slvMouseEvent.button = MouseEvent::Button::NONE;
        //slvMouseEvent.button = slvMouseEvent.button;
    }

#if QT_VERSION >= 0x060000
    QPointF pos = event->position();
    slvMouseEvent.x = pos.x() * pixelRatio;
    slvMouseEvent.y = pos.y() * pixelRatio;
#else
    slvMouseEvent.x = double(event->x());
    slvMouseEvent.y = double(event->y());
#endif

    setMouseKeyModifiers(slvMouseEvent, event);

    receiver->onMouseEvent(slvMouseEvent);
}

void SSView::updateSlvSpaceKeyEvent(QKeyEvent* event)
{
    slvKeyEvent.key = KeyboardEvent::Key(-1);

    Qt::KeyboardModifiers mod = event->modifiers();
    slvKeyEvent.shiftDown   = mod & Qt::ShiftModifier   ? true : false;
    slvKeyEvent.controlDown = mod & Qt::ControlModifier ? true : false;

    if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F35) {
        slvKeyEvent.key = KeyboardEvent::Key::FUNCTION;
        slvKeyEvent.num = event->key() - Qt::Key_F1 + 1;
    }
    if (event->key() >= Qt::Key_Space && event->key() <= Qt::Key_yen) {
        slvKeyEvent.key = KeyboardEvent::Key::CHARACTER;
        QString keyLower = event->text().toLower();
        slvKeyEvent.chr = keyLower.toUcs4().at(0);
    }

    receiver->onKeyboardEvent(slvKeyEvent);
}

SSTextWindow::SSTextWindow(QWidget* parent)
        : QDockWidget(parent)
{
    ssView = new SSView;

    QWidget* group = new QWidget;
    scrollBar = new QScrollBar(Qt::Vertical, group);
    connect(scrollBar, SIGNAL(valueChanged(int)), SLOT(sliderSlot(int)));

    QHBoxLayout* lo = new QHBoxLayout(group);
    lo->setContentsMargins(0, 0, 0, 0);
    lo->setSpacing(2);
    lo->addWidget(ssView);
    lo->addWidget(scrollBar);
    group->setLayout(lo);

    setWidget(group);
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
}

void SSTextWindow::sliderSlot(int value)
{
    std::function<void(double)> adjust = ssView->receiver->onScrollbarAdjusted;
    if(adjust)
       adjust(double(value));
}

void SSTextWindow::closeEvent(QCloseEvent* ev)
{
    ssView->receiver->onClose();
    ev->ignore();
}

SSMainWindow::SSMainWindow()
{
    ssView = new SSView;
    setCentralWidget(ssView);
}

void SSMainWindow::closeEvent(QCloseEvent* ev)
{
    ssView->receiver->onClose();
    ev->ignore();
}

class WindowImplQt final : public Window {
public:
    QWidget* ssWindow;  // SSMainWindow or SSTextWindow.
    SSView* view;       // Copy of ssWindow ssView.

    // NOTE: Only TextWindow has a scroll bar and only GraphicsWindow has menus,
    // but Platform::Window squishes both together in a single interface.

    QScrollBar* scrollBar;      // Copy of SSTextWindow::scrollBar or nullptr.
    std::shared_ptr<MenuBarImpQt> menuBar;

    WindowImplQt(Window::Kind kind, std::shared_ptr<WindowImplQt> parent)
    {
        if(kind == Platform::Window::Kind::TOOL) {
            QWidget* qparent = parent ? parent->ssWindow : nullptr;

            SSTextWindow* twin = new SSTextWindow(qparent);
            ssWindow = twin;
            scrollBar = twin->scrollBar;
            view = twin->ssView;

            // Set object name for saveState settings.
            twin->setObjectName("TextWindow");

            QMainWindow* mwin = qobject_cast<QMainWindow*>(qparent);
            if(mwin) {
                mwin->addDockWidget(Qt::RightDockWidgetArea, twin);
            }
        } else {
            SSMainWindow* mwin = new SSMainWindow;
            ssWindow = mwin;
            scrollBar = nullptr;
            view = mwin->ssView;
        }

        // Have the view call our Platform::Window event methods.
        view->receiver = this;
    }

    // Returns physical display DPI.
    double GetPixelDensity() override {
        return QGuiApplication::primaryScreen()->logicalDotsPerInch();
    }

    // Returns raster graphics and coordinate scale (already applied on the platform side),
    // i.e. size of logical pixel in physical pixels, or device pixel ratio.
    double GetDevicePixelRatio() override {
        return view->pixelRatioI;
    }

    // Returns (fractional) font scale, to be applied on top of (integral) device pixel ratio.
    double GetDeviceFontScale() {
        return GetPixelDensity() / GetDevicePixelRatio() / 96.0;
    }

    bool IsVisible() override {
        return ssWindow->isVisible();
    }

    void SetVisible(bool visible) override {
        ssWindow->setVisible(visible);
        ssWindow->raise();
    }

    void Focus() override {
        ssWindow->setFocus();
    }

    bool IsFullScreen() override {
        return ssWindow->isFullScreen();
    }

    void SetFullScreen(bool fullScreen) override {
        // Qt::WindowNoState is "normal".
        ssWindow->setWindowState(fullScreen ? Qt::WindowFullScreen
                                            : Qt::WindowNoState);
    }

    void SetTitle(const std::string& title) override {
        ssWindow->setWindowTitle(QString::fromStdString(title));
    }

    void SetMenuBar(MenuBarRef menus) override {
        QMainWindow* mwin = qobject_cast<QMainWindow*>(ssWindow);
        if (mwin) {
            // We must take ownership of the new menus.
            menuBar = std::static_pointer_cast<MenuBarImpQt>(menus);
            menuBar->formActionGroups();

            mwin->setMenuBar(menuBar->menuBarQ);
        }
    }

    void GetContentSize(double* width, double* height) override {
#if QT_VERSION >= 0x060000
        double pixelRatio = view->pixelRatio;
        QSize rc = view->size();
        *width = rc.width() * pixelRatio;
        *height = rc.height() * pixelRatio;
        //printf("KR cont %f %d,%d %f,%f\n", pixelRatio, rc.width(), rc.height(), *width, *height);
#else
        *width = view->width();
        *height = view->height();
        //printf("KR cont %f,%f\n", *width, *height);
#endif
    }

    void SetMinContentSize(double width, double height) override {
        ssWindow->resize(int(width), int(height));
    }

    void FreezePosition(SettingsRef settings, const std::string& key) override {
        // Geometry is only saved for the main window; the TextWindow dock
        // widget is part of the saveState().
        QMainWindow* mwin = qobject_cast<QMainWindow*>(ssWindow);
        if (mwin) {
            QSettings& qset = std::static_pointer_cast<SettingsImplQt>(settings)->qset;
            QString qkey = QString::fromStdString(key);
            qset.setValue(qkey + "_Geometry", mwin->saveGeometry());
            qset.setValue(qkey + "_State", mwin->saveState());
        }
    }

    void ThawPosition(SettingsRef settings, const std::string& key) override {
        QMainWindow* mwin = qobject_cast<QMainWindow*>(ssWindow);
        if (mwin) {
            QSettings& qset = std::static_pointer_cast<SettingsImplQt>(settings)->qset;
            QString qkey = QString::fromStdString(key);
            mwin->restoreGeometry(qset.value(qkey + "_Geometry").toByteArray());
            mwin->restoreState(qset.value(qkey + "_State").toByteArray());
        }
    }

    void SetCursor(Cursor cursor) override {

        switch (cursor)
        {
        case Cursor::POINTER:
            ssWindow->setCursor(Qt::ArrowCursor);
            break;
        case Cursor::HAND:
            ssWindow->setCursor(Qt::PointingHandCursor);
            break;
        default:
            ssWindow->setCursor(Qt::ArrowCursor);
        }
    }

    void SetTooltip(const std::string& text, double x, double y,
        double width, double height) override {
        view->setToolTip(QString::fromStdString(text));
    }

    bool IsEditorVisible() override {
        return view->entry->isVisible();
    }

    void ShowEditor(double x, double y, double fontHeight, double minWidth,
        bool isMonospace, const std::string& text) override {
        view->startEditing(x, y, fontHeight, minWidth, isMonospace, text);
    }

    void HideEditor() override {
        view->stopEditing();
    }

    void SetScrollbarVisible(bool visible) override {
        //printf("Scroll vis %d\n", int(visible));
        if (scrollBar)
            scrollBar->setVisible(visible);
    }

    void ConfigureScrollbar(double min, double max, double pageSize) override {
        //printf("Scroll bar %f %f %f\n", min, max, pageSize);
        if (scrollBar) {
            scrollBar->setRange(int(min), int(max - pageSize));
            scrollBar->setPageStep(int(pageSize));
        }
    }

    double GetScrollbarPosition() override {
        if (scrollBar)
            return (double) scrollBar->value();
        return 0.0;
    }

    void SetScrollbarPosition(double pos) override {
        //printf("Scroll pos %f\n", pos);
        if (scrollBar)
            scrollBar->setValue((int) pos);
    }

    void Invalidate() override {
        view->update();
    }
};

#if defined(WIN32)
// This interferes with our identifier.
#    undef CreateWindow
#endif

WindowRef CreateWindow(Window::Kind kind, WindowRef parentWindow) {
    return std::make_shared<WindowImplQt>(kind,
                std::static_pointer_cast<WindowImplQt>(parentWindow));
}

MessageDialogRef CreateMessageDialog(WindowRef parentWindow) {
    return std::make_shared<MessageDialogImplQt>(
                std::static_pointer_cast<WindowImplQt>(parentWindow)->ssWindow);
}

FileDialogRef CreateOpenFileDialog(WindowRef parentWindow) {
    return std::make_shared<FileDialogImplQt>(
                std::static_pointer_cast<WindowImplQt>(parentWindow)->ssWindow,
                false);
}

FileDialogRef CreateSaveFileDialog(WindowRef parentWindow) {
    return std::make_shared<FileDialogImplQt>(
                std::static_pointer_cast<WindowImplQt>(parentWindow)->ssWindow,
                true);
}

//-----------------------------------------------------------------------------
// 3DConnexion support
//-----------------------------------------------------------------------------

void Open3DConnexion() {}
void Close3DConnexion() {}
void Request3DConnexionEventsForWindow(WindowRef window) {}

//-----------------------------------------------------------------------------
// Application-wide APIs
//-----------------------------------------------------------------------------

std::vector<Platform::Path> GetFontFiles() {
#if WIN32
    std::vector<Platform::Path> fonts;

    std::wstring fontsDirW(MAX_PATH, '\0');
    fontsDirW.resize(GetWindowsDirectoryW(&fontsDirW[0], fontsDirW.length()));
    fontsDirW += L"\\fonts\\";
    Platform::Path fontsDir = Platform::Path::From(SolveSpace::Platform::Narrow(fontsDirW));

    WIN32_FIND_DATAW wfd;
    HANDLE h = FindFirstFileW((fontsDirW + L"*").c_str(), &wfd);
    while (h != INVALID_HANDLE_VALUE) {
        fonts.push_back(fontsDir.Join(SolveSpace::Platform::Narrow(wfd.cFileName)));
        if (!FindNextFileW(h, &wfd)) break;
    }

    return fonts;
#endif
#if UNIX
    std::vector<Platform::Path> fonts;

    // fontconfig is already initialized by GTK
    FcPattern* pat = FcPatternCreate();
    FcObjectSet* os = FcObjectSetBuild(FC_FILE, (char*)0);
    FcFontSet* fs = FcFontList(0, pat, os);

    for (int i = 0; i < fs->nfont; i++) {
        FcChar8* filenameFC = FcPatternFormat(fs->fonts[i], (const FcChar8*)"%{file}");
        fonts.push_back(Platform::Path::From((const char*)filenameFC));
        FcStrFree(filenameFC);
    }

    FcFontSetDestroy(fs);
    FcObjectSetDestroy(os);
    FcPatternDestroy(pat);

    return fonts;

#endif

    return std::vector<Platform::Path>();
}

void OpenInBrowser(const std::string& url) {

    std::string urlTemp = url;

    if (urlTemp.find("http") == std::string::npos)
    {
        urlTemp = QApplication::applicationDirPath().toStdString();
        urlTemp += "/" + url;
    }

    if (false == QDesktopServices::openUrl(QUrl(urlTemp.c_str(), QUrl::TolerantMode)))
    {
        std::string errorStr = "Error: unable to open URL path : " + urlTemp;
        QMessageBox::critical(0,QString("Manual Open Error"),QString::fromStdString(errorStr),QMessageBox::Ok);
    }
}

void ExitGui() {
    qApp->quit();
}

}
}

using namespace SolveSpace;

int main(int argc, char** argv) {
    // Must share GL contexts or an undocked QDockWidget is black.
    // See https://bugreports.qt.io/browse/QTBUG-89812
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);

    Platform::SSApplication app(argc, argv);
    Platform::Open3DConnexion();
    SS.Init();

    if(argc >= 2) {
        if(argc > 2) {
            dbp("Only the first file passed on command line will be opened.");
        }
        SS.Load(Platform::Path::From(argv[1]));
    }

    app.exec();

    Platform::Close3DConnexion();
    SS.Clear();
    SK.Clear();
    return 0;
}
