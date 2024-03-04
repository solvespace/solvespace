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
#include <QEventLoop>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
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
    QMessageBox* messageBoxQ;

    MessageDialogImplQt() {
        messageBoxQ = new QMessageBox(0);
    }

    ~MessageDialogImplQt() {
        delete messageBoxQ;
    }

#undef ERROR 
    void SetType(Type type) override {
        switch (type) {
        case Type::INFORMATION:
            messageBoxQ->setIcon(QMessageBox::Information);
            break;
        case Type::QUESTION:
            messageBoxQ->setIcon(QMessageBox::Question);
            break;
        case Type::WARNING:
            messageBoxQ->setIcon(QMessageBox::Warning);
            break;
        case Type::ERROR:
            messageBoxQ->setIcon(QMessageBox::Critical);
            break;
        }
    }

    void SetTitle(std::string title) override {
        messageBoxQ->setWindowTitle(QString::fromStdString(title));
    }

    void SetMessage(std::string message) override {
        messageBoxQ->setText(QString::fromStdString(message));
    }

    void SetDescription(std::string description) override {
        messageBoxQ->setInformativeText(QString::fromStdString(description));
    }

    void AddButton(std::string label, Response response, bool isDefault = false) override {
        switch (response)
        {
        case Response::CANCEL:
            messageBoxQ->addButton(QMessageBox::StandardButton::Cancel);
            break;
        case Response::NO:
            messageBoxQ->addButton(QMessageBox::StandardButton::No);
            break;
        case Response::YES:
            messageBoxQ->addButton(QMessageBox::StandardButton::Yes);
            break;
        case Response::OK:
            messageBoxQ->addButton(QMessageBox::StandardButton::Ok);
            break;
        case Response::NONE:
            messageBoxQ->addButton(QMessageBox::StandardButton::Ignore);
            break;
        }
    }

    Response RunModal() {
        //return Response::CANCEL;
        QMessageBox::StandardButton responseQ = (QMessageBox::StandardButton)messageBoxQ->exec();

        switch (responseQ)
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

        return Response::NONE;
    }
};

MessageDialogRef CreateMessageDialog(WindowRef parentWindow) {
    std::shared_ptr<MessageDialogImplQt> dialog = std::make_shared<MessageDialogImplQt>();
    //dialog->mbp.hwndOwner = std::static_pointer_cast<WindowImplWin32>(parentWindow)->hWindow;
    return dialog;
}

//-----------------------------------------------------------------------------
// File dialogs
//-----------------------------------------------------------------------------

class FileDialogImplQt final : public FileDialog, public QObject {
public:
    bool isSaveDialog;
    QStringList  filters;
    QFileDialog* fileDialogQ;

    FileDialogImplQt() {
        fileDialogQ = new QFileDialog(0);
        connect(fileDialogQ, &QFileDialog::filterSelected, this, &FileDialogImplQt::updateDefaultSuffix);
        isSaveDialog = false;
    }

    ~FileDialogImplQt() {
        fileDialogQ->disconnect();
        delete fileDialogQ;
    }

    void updateDefaultSuffix(QString filter) {
        fileDialogQ->setDefaultSuffix(filter);
    }

    std::string replaceSubstringInString(std::string str, std::string substr1,
                        std::string substr2) {
        for(size_t index = str.find(substr1, 0);
            index != std::string::npos && substr1.length();
            index = str.find(substr1, index + substr2.length()))
            str.replace(index, substr1.length(), substr2);
        return str;
    }

    std::vector<std::string> tokanize(const std::string& str, const std::string& delimeters)
    {
        std::vector<std::string>tokens;

        // Skip delimeters at begining
        std::string::size_type lastPos = str.find_first_not_of(delimeters, 0);
        // first non delimeter
        std::string::size_type pos = str.find_first_of(delimeters, lastPos);

        while (pos != std::string::npos || lastPos != std::string::npos)
        {
            // Token found, insert into tokens
            tokens.push_back(str.substr(lastPos, pos - lastPos));
            // Skip delimeters
            lastPos = str.find_first_not_of(delimeters, pos);
            // find the next non delimeter
            pos = str.find_first_of(delimeters, lastPos);
        }
        return tokens;
    }

    void SetTitle(std::string title) {
        fileDialogQ->setWindowTitle(QString::fromStdString(title));
    }

    void SetCurrentName(std::string name) {
        fileDialogQ->selectFile(QString::fromStdString(name));
    }

    Platform::Path GetFilename() {
#if defined WIN32
        std::cout << "IN WINDOWS" << std::endl;
#endif
#if defined UNIX 
        std::cout << "IN UNIX" << std::endl;
#endif
        Platform::Path filePath;
        QString suffix = tokanize(filters[0].toStdString(), ".")[1].c_str();
        fileDialogQ->setDefaultSuffix(suffix);
        QStringList selectedFiles = fileDialogQ->selectedFiles();

        if (true == selectedFiles.isEmpty())
            return filePath;

        std::string pathUnixStyle = selectedFiles.at(0).toStdString(); // return first selection.
        if ('\\' == QDir::separator())
            filePath.raw = replaceSubstringInString(pathUnixStyle, "/", "\\");
        else {
            filePath.raw = pathUnixStyle;
        }
        return filePath;
    }

    void SetFilename(Platform::Path path) {
        fileDialogQ->selectFile(QString::fromStdString(path.raw));
    }

    void SuggestFilename(Platform::Path path) {
        fileDialogQ->selectFile(QString::fromStdString(path.FileStem()));
    }

    void AddFilter(std::string name, std::vector<std::string> extensions) override {
        std::string desc, patterns;
        for (auto& extension : extensions) {
            std::string pattern = "*." + extension;
            if (!desc.empty()) desc += " ";
            desc += pattern;
        }
        filters.append(QString::fromStdString(desc));
    }

    void AddFilter(const FileFilter& filter) {
        AddFilter(filter.name, filter.extensions);
    }

    void AddFilters(const std::vector<FileFilter>& filters) {
        for (auto& filter : filters)
        {
            AddFilter(filter);
        }
    }

    std::string GetExtension()
    {
        return fileDialogQ->selectedNameFilter().toStdString();
    }

    void SetExtension(std::string extension)
    {
        fileDialogQ->selectNameFilter(QString::fromStdString(extension));
    }

    void FreezeChoices(SettingsRef settings, const std::string& key)
    {
        settings->FreezeString("Dialog_" + key + "_Folder",
            fileDialogQ->directory().absolutePath().toStdString());
        settings->FreezeString("Dialog_" + key + "_Filter",GetExtension());
    }

    void ThawChoices(SettingsRef settings, const std::string &key)
    {
        fileDialogQ->setDirectory(QDir(QString::fromStdString(settings->ThawString("Dialog_" + key + "_Folder"))));
        SetExtension(settings->ThawString("Dialog_" + key + "_Filter"));
    }

    bool RunModal() override {
        if (isSaveDialog) {
            SetTitle("Save file");
            fileDialogQ->setAcceptMode(QFileDialog::AcceptSave);
        } else {
            SetTitle("Open file");
            fileDialogQ->setAcceptMode(QFileDialog::AcceptOpen);
        }

        fileDialogQ->setNameFilters(filters);
        return(fileDialogQ->exec());
    }
};

FileDialogRef CreateOpenFileDialog(WindowRef parentWindow) {
    std::shared_ptr<FileDialogImplQt> dialog = std::make_shared<FileDialogImplQt>();
    dialog->isSaveDialog = false;
    return dialog;
}

FileDialogRef CreateSaveFileDialog(WindowRef parentWindow) {
    std::shared_ptr<FileDialogImplQt> dialog = std::make_shared<FileDialogImplQt>();
    dialog->isSaveDialog = true;
    return dialog;
}

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------

class SettingsImplQt final : public Settings {
public:
    QSettings* _qset;

    SettingsImplQt() {
        _qset = new QSettings("SolveSpace", "solvespace");
    }

    ~SettingsImplQt() {
        delete _qset;
    }

    void FreezeInt(const std::string& key, uint32_t value) override {
        _qset->setValue(QString::fromStdString(key), value);
    }

    uint32_t ThawInt(const std::string& key, uint32_t defaultValue = 0) override {
        return _qset->value(QString::fromStdString(key), defaultValue).toInt();
    }

    void FreezeFloat(const std::string& key, double value) override{
        _qset->setValue(QString::fromStdString(key), value);
    }

    double ThawFloat(const std::string& key, double defaultValue = 0.0) override {
        return _qset->value(QString::fromStdString(key), defaultValue).toDouble();
    }

    void FreezeString(const std::string& key, const std::string& value) override {
        _qset->setValue(QString::fromStdString(key),
                        QString::fromStdString(value));
    }

    std::string ThawString(const std::string& key, const std::string& defaultValue = "") override {
        return _qset->value(QString::fromStdString(key),
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
        #if 0
        QEventLoop loop;
        QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
        loop.exec();
        if (onTimeout) {
            onTimeout();
        }
        #endif
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
    // MenuItemImplQt must set this pointer in order to control exculsivity
    QActionGroup* actionGroupItemQ;

    MenuItemImplQt() {
        actionGroupItemQ = 0;
        connect(&action, &QAction::triggered, this, &MenuItemImplQt::onTriggered);
    }

    void SetAccelerator(KeyboardEvent accel) override {
        int key;

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

        if (accel.controlDown) {
            key |= Qt::CTRL;
        }
        if (accel.shiftDown) {
            key |= Qt::SHIFT;
        }
        action.setShortcut(key);
    }

    void SetIndicator(Indicator type) override {
        switch (type)
        {
        case Indicator::NONE:
            action.setCheckable(false);
            actionGroupItemQ->removeAction(&action);
            break;
        case Indicator::CHECK_MARK:
            action.setCheckable(true);
            actionGroupItemQ->setExclusive(false);
            actionGroupItemQ->addAction(&action);
            break;
        case Indicator::RADIO_MARK:
            action.setCheckable(true);
            actionGroupItemQ->setExclusive(true);
            actionGroupItemQ->addAction(&action);
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
    QActionGroup* menuActionGroupQ;
    bool  hasParent;
    std::vector<std::shared_ptr<MenuItemImplQt>>   menuItems;
    std::vector<std::shared_ptr<MenuImplQt>>       subMenus;

    MenuImplQt(bool hasParentParam = true) {
        menuQ = new QMenu();

        // The menu action group is needed for menu items that are checkable and are exclusive
        // The exclusive ( radio button like check, only one the group can be checked at a time)
        menuActionGroupQ = new QActionGroup(menuQ);
        hasParent = hasParentParam;
    }

    MenuImplQt(QMenu* menuQParam) {
        menuQ = menuQParam;
        // The menu action group is needed for menu items that are checkable and are exclusive
        // The exclusive ( radio button like check, only one the group can be checked at a time)
        menuActionGroupQ = new QActionGroup(menuQ);
        hasParent = true;
    }

    std::shared_ptr<MenuItem> AddItem(const std::string& label, std::function<void()> onTrigger,
        bool /*mnemonics*/) override {

        std::shared_ptr<MenuItemImplQt> menuItem = std::make_shared<MenuItemImplQt>();
        menuItem->actionGroupItemQ = menuActionGroupQ;
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
};

MenuBarRef GetOrCreateMainMenu(bool* unique) {
    *unique = false;
    return std::make_shared<MenuBarImpQt>();
}

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------

SSView::SSView(QWidget* parent) : QOpenGLWidget(parent) {
    entry = new QLineEdit(this);
    entry->setGeometry(QRect(0, 0, 200, 40));
    QFont entryFont("Arial", 16);
    entry->setFont(entryFont);
    entry->setVisible(false);
    connect(entry, SIGNAL(returnPressed()), this, SLOT(entryFinished()));
    setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    setMouseTracking(true);
}

void SSView::startEditing(int x, int y, int fontHeight, int minWidth,
                          bool isMonoSpace, const std::string& val)
{
    entry->setGeometry(QRect(x, y, minWidth, fontHeight));
    entry->setText(QString::fromStdString(val));

    if (!entry->isVisible()) {
        entry->show();
        entry->setFocus();
        entry->setCursorPosition(0);
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

void SSView::wheelEvent(QWheelEvent* event)
{
    const double wheelDelta=120.0;
    slvMouseEvent.button = MouseEvent::Button::NONE;
    slvMouseEvent.type = MouseEvent::Type::SCROLL_VERT;
    slvMouseEvent.scrollDelta = (double)event->angleDelta().y() / wheelDelta;

    receiver->onMouseEvent(slvMouseEvent);
}

void SSView::updateSlvSpaceMouseEvent(QMouseEvent* event)
{
    slvMouseEvent.shiftDown = false;
    slvMouseEvent.controlDown = false;

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


    slvMouseEvent.x = event->pos().x();
    slvMouseEvent.y = event->pos().y();
    Qt::KeyboardModifiers keyModifier = QGuiApplication::keyboardModifiers();

    switch (keyModifier)
    {
    case Qt::ShiftModifier:
        slvMouseEvent.shiftDown = true;
        break;
    case Qt::ControlModifier:
        slvMouseEvent.controlDown = true;
        break;
    }

    receiver->onMouseEvent(slvMouseEvent);
}

void SSView::updateSlvSpaceKeyEvent(QKeyEvent* event)
{
    slvKeyEvent.key = KeyboardEvent::Key(-1);
    slvKeyEvent.shiftDown = false;
    slvKeyEvent.controlDown = false;
    if (event->modifiers() == Qt::ShiftModifier)
        slvKeyEvent.shiftDown = true;
    if (event->modifiers() == Qt::ControlModifier)
        slvKeyEvent.controlDown = true;

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
        return view->devicePixelRatio();
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

            mwin->setMenuBar(menuBar->menuBarQ);
        }
    }

    void GetContentSize(double* width, double* height) override {
        double pixelRatio = GetDevicePixelRatio();
        QSize rc = view->size();
        *width = rc.width() / pixelRatio;
        *height = rc.height() / pixelRatio;
    }

    void SetMinContentSize(double width, double height) override {
        ssWindow->resize(int(width), int(height));
    }

    void FreezePosition(SettingsRef settings, const std::string& key) override {
        if (!(ssWindow->isVisible())) return;

        int left, top, width, height;
        #if 0
        QPoint topLeftPoint = ssWindow->geometry().topLeft();
        left = topLeftPoint.x();
        top = topLeftPoint.y();
        #endif
        QPoint windowPos = ssWindow->pos();
        left = windowPos.x();
        top = windowPos.y();
        width = ssWindow->geometry().width();
        height = ssWindow->geometry().height();
        bool isMaximized = ssWindow->isMaximized();

        settings->FreezeInt(key + "_Left", left);
        settings->FreezeInt(key + "_Top", top);
        settings->FreezeInt(key + "_Width", width);
        settings->FreezeInt(key + "_Height", height);
        settings->FreezeBool(key + "_Maximized", isMaximized);
    }

    void ThawPosition(SettingsRef settings, const std::string& key) override {
        int left = 100, top = 100, width = 0, height = 0;

        left = settings->ThawInt(key + "_Left", left);
        top = settings->ThawInt(key + "_Top", top);
        width = settings->ThawInt(key + "_Width", width);
        height = settings->ThawInt(key + "_Height", height);

        if(width != 0 && height != 0) {
            ssWindow->move(left, top);
            ssWindow->resize(width, height);
        }

        if (settings->ThawBool(key + "_Maximized", false)) {
//                  ssWindow->SetFullScreen(true);
            ssWindow->setWindowState(Qt::WindowMaximized);
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
        // font size from Solvespace is very small and hard to see
        // hard coded 20 for height and 100 for width
        // using Arial font of size 16
        // font decalred and set to the entry QLabel in the constructor
        // Raed Marwan
        view->startEditing(x, y, 20, 100, isMonospace, text);
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

WindowRef CreateWindow(Window::Kind kind, WindowRef parentWindow) {
    return std::make_shared<WindowImplQt>(kind,
                std::static_pointer_cast<WindowImplQt>(parentWindow));
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

    QApplication app(argc, argv);
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
