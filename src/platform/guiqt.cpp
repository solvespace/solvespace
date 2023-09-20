
#include <tinyxml2.h>
#include <platform/qglmainwindow.h>
#include "solvespace.h"
#include "platform.h"

#include <QFileDialog>
#include <QTimer>
#include <QEventLoop>
#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QApplication>
#include <QMessageBox>
#include <QDesktopServices>


#include <fstream>

// FOR TESTING
#include <iostream>


namespace SolveSpace
{
    namespace Platform
    {
        /*********** Fatal Error */
        
        void FatalError(const std::string& message)
        {
            QMessageBox::StandardButton buttonPressed = (QMessageBox::StandardButton)QMessageBox::critical(NULL, QString("Fatal Error"), QString::fromStdString(message),
                QMessageBox::StandardButton::Ok);

            abort();
        }
        /*********** Message Dialog */

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


        /*********** File Dilaog */
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
                else
                {
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
                }
                else {
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

        /*******Timer Class ***************************/

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

        /**********Menu Item*****************/

        class MenuItemImplQt final : public MenuItem , public QObject {

        public:
            QAction* actionItemQ;
            // MenuItemImplQt must set this pointer in order to control exculsivity 
            QActionGroup* actionGroupItemQ;

            MenuItemImplQt() {
                actionItemQ = new QAction;
                actionGroupItemQ = 0;
                connect(actionItemQ, &QAction::triggered, this, &MenuItemImplQt:: onTriggered);
            }

            ~MenuItemImplQt() {
                actionItemQ->disconnect();
                onTrigger = nullptr;
            }

            void Clear() {

                actionItemQ->disconnect();
             }

            void SetAccelerator(KeyboardEvent accel) override {

                QString accelKey;

                if (accel.key == KeyboardEvent::Key::CHARACTER) {
                    if (accel.chr == '\t') {
                        accelKey = "Tab";
                    }
                    else if (accel.chr == '\x1b') {
                        accelKey = "Escape";
                    }
                    else if (accel.chr == '\x7f') {
                        accelKey = "Del";
                    }
                    else {
                        accelKey = accel.chr;
                    }
                }
                else if (accel.key == KeyboardEvent::Key::FUNCTION) {
                    accelKey = "F" + QString::number(accel.num );
                }

                QString accelMods;

                if (accel.controlDown) {
                    accelMods = "Ctrl";
                }

                if (accel.shiftDown) {
                    accelMods += (true == accelMods.isEmpty()) ? "Shift" : "+Shift";
                }

                QKeySequence keySequence;

                if (false == accelMods.isEmpty()) {
                    keySequence = QString(accelMods + "+" + accelKey);
                }
                else {
                    keySequence = QString(accelKey);
                }

                actionItemQ->setShortcut(keySequence);
            }

            void SetIndicator(Indicator type) override {

                switch (type)
                {
                case Indicator::NONE:
                    actionItemQ->setCheckable(false);
                    actionGroupItemQ->removeAction(actionItemQ);
                    break;
                case Indicator::CHECK_MARK:
                    actionItemQ->setCheckable(true);
                    actionGroupItemQ->setExclusive(false);
                    actionGroupItemQ->addAction(actionItemQ);
                    break;
                case Indicator::RADIO_MARK:
                    actionItemQ->setCheckable(true);
                    actionGroupItemQ->setExclusive(true);
                    actionGroupItemQ->addAction(actionItemQ);
                    break;

                }
            }

            void SetEnabled(bool enabled) override {
                actionItemQ->setEnabled(enabled);
            }

            void SetActive(bool active) override {
                actionItemQ->setChecked(active);
            }

        public slots:
            void onTriggered(bool value)
            {
                if (onTrigger)
                    this->onTrigger();

            }

        };

        /**********************Menu *************************/
        class MenuImplQt final : public Menu, public QObject {
            
        public:
            QMenu* menuQ;
            QActionGroup* menuActionGroupQ;
            bool  hasParent;
            std::vector<std::shared_ptr<MenuItemImplQt>>   menuItems;
            std::vector<std::shared_ptr<MenuImplQt>>       subMenus;

            MenuImplQt(bool hasParentParam = true) {
                menuQ = new QMenu();

                connect(menuQ, &QMenu::aboutToShow, this, &MenuImplQt::menuAboutToShowEvent);

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

            ~MenuImplQt()
            {
                menuQ->disconnect();
                this->Clear();
            }


            std::shared_ptr<MenuItem> AddItem(const std::string& label, std::function<void()> onTrigger,
                bool /*mnemonics*/) override {

                std::shared_ptr<MenuItemImplQt> menuItem = std::make_shared<MenuItemImplQt>();
                menuItem->actionGroupItemQ = menuActionGroupQ;
                menuItem->onTrigger = onTrigger;
                menuItem->actionItemQ->setText(QString::fromStdString(label));
                // I do not think anything is needed for mnemonics flag. 
                // the shortcut key sequence is set in the menuItem class and Qt acts accordingly 
                // with no further activation ... I think 
                menuItems.push_back(menuItem);
                menuQ->addAction(menuItem->actionItemQ);
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

                menuQ->disconnect();
                menuQ->clear();
                
                for(auto menuItem : menuItems) {
                   menuItem->Clear();
                }

                for(auto subMenu : subMenus) {
                   subMenu->Clear();
                }

            }

        public slots:
            void menuAboutToShowEvent()
            {
                std::cout.imbue(std::locale::classic());
                this->menuQ->repaint();
                QRect rect = this->menuQ->geometry();
                QRect rect2 = rect;                
            }
        };

        MenuRef CreateMenu() {
            return std::make_shared<MenuImplQt>(false); // false says -> menu has not Qtwidget as a parent
        }

        /******************Menu Bar********************************/

        class MenuBarImpQt final : public MenuBar {

        public:
            QMenuBar* menuBarQ;
            std::vector<std::shared_ptr<MenuImplQt>>  subMenus;
            std::shared_ptr<MenuItemImplQt>           showPropertyBrowserMenuItem;

            MenuBarImpQt() {
                menuBarQ = new QMenuBar;
                this->menuBarQ->setNativeMenuBar(false);
                showPropertyBrowserMenuItem = nullptr;
            }


            void findShowPropertyBrowserMenuItem() {
                // loop through the menus 
                // go thorugh each item and check the key sequence 
                // it should be the one with the tab key sequence
                for(size_t ii = 0; ii < subMenus.size(); ++ii) {

                    std::vector<std::shared_ptr<MenuItemImplQt>> menuItems = subMenus[ii]->menuItems;

                    for(size_t jj = 0; jj < menuItems.size(); ++jj) {

                        std::shared_ptr<MenuItemImplQt> menuItem = menuItems[jj];

                        if(menuItem->actionItemQ->shortcut() == Qt::Key_Tab) {

                              showPropertyBrowserMenuItem = menuItem;
                        }
                    }
                }
                        
            }

            ~MenuBarImpQt() {
                this->Clear();
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
                for(auto subMenu : subMenus) {
                    subMenu->Clear();
                }
            }
        };
        
        std::shared_ptr<MenuBarImpQt> gMenuBarQt = nullptr;

        MenuBarRef GetOrCreateMainMenu(bool* unique) {

            *unique = false;
            //return std::make_shared<MenuBarImpQt>();
            if(gMenuBarQt == nullptr) {
                gMenuBarQt = std::make_shared<MenuBarImpQt>();
                
            } 
            else {
                gMenuBarQt->menuBarQ->clear();
                gMenuBarQt->Clear();
           }

            return gMenuBarQt;
        }

        /***********Window ***************/


        class WindowImplQt final : public Window, public QObject {
        public:
            QtGLMainWindow* windowGLQ;
            Window::Kind windowKind;
            std::shared_ptr<WindowImplQt> parentWindow;
            //std::shared_ptr<MenuBarImpQt> menuBarQt;

            WindowImplQt(Window::Kind kind , std::shared_ptr<WindowImplQt> parent = nullptr)
                : windowKind(kind), 
                  parentWindow(parent)
            {
                windowGLQ = new QtGLMainWindow((parentWindow == nullptr)? nullptr : parentWindow->windowGLQ);
                connect(windowGLQ->GetGlWidget(), &GLWidget::mouseEventOccuredSignal, this, &WindowImplQt::runOnMouseEvent);
                connect(windowGLQ->GetGlWidget(), &GLWidget::keyEventOccuredSignal, this, &WindowImplQt::runOnKeyboardEvent);
                connect(windowGLQ->GetGlWidget(), &GLWidget::renderOccuredSignal, this, &WindowImplQt::runOnRender);
                connect(windowGLQ->GetGlWidget(), &GLWidget::editingDoneSignal, this, &WindowImplQt::runOnEditingDone);
                connect(windowGLQ, &QtGLMainWindow::windowClosedSignal, this, &WindowImplQt::runOnClose);
                connect(windowGLQ->GetGlWidget(), &GLWidget::tabKeyPressed, this, &WindowImplQt::processTabKeyPressed);
                
            }

            ~WindowImplQt() {

            }

            void runOnMouseEvent(SolveSpace::Platform::MouseEvent mouseEvent){
                onMouseEvent(mouseEvent);
            }

            void runOnKeyboardEvent(SolveSpace::Platform::KeyboardEvent keyEvent){
                onKeyboardEvent(keyEvent);
            }

            void runOnRender(){
                onRender();
            }

            void runOnEditingDone(std::string text) {
                onEditingDone(text);
            }

            void runOnClose(){
                onClose();
            }

            // Returns physical display DPI.
            double GetPixelDensity() override {
                return windowGLQ->GetPixelDensity();
            }

            // Returns raster graphics and coordinate scale (already applied on the platform side),
            // i.e. size of logical pixel in physical pixels, or device pixel ratio.
            double GetDevicePixelRatio() override {
                return windowGLQ->GetDevicePixelRatio();
            }

            // Returns (fractional) font scale, to be applied on top of (integral) device pixel ratio.
            double GetDeviceFontScale() {
                return GetPixelDensity() / GetDevicePixelRatio() / 96.0;
            }

            bool IsVisible() override {
                return windowGLQ->IsVisible();
            }

            void SetVisible(bool visible) override {
                windowGLQ->setVisible(visible);
                windowGLQ->raise();
            }

            void Focus() override {
                windowGLQ->setFocus();
            }

            bool IsFullScreen() override {
                return windowGLQ->isFullScreen();
            }

            void SetFullScreen(bool fullScreen) override {
                if (true == fullScreen)
                    windowGLQ->setWindowState(Qt::WindowFullScreen);
                else
                    windowGLQ->setWindowState(Qt::WindowNoState); //The window has no state set (in normal state).

            }

            void SetTitle(const std::string& title) override {
                windowGLQ->setWindowTitle(QString::fromStdString(title));
            }

            void SetMenuBar(MenuBarRef menuBar) override {
                // Cast this menu bar to a Qt
                //menuBarQt = std::static_pointer_cast<MenuBarImpQt>(menuBar);
                //windowGLQ->setMenuBar(menuBarQt->menuBarQ);

                if(windowGLQ->menuBar() != gMenuBarQt->menuBarQ) {
                    windowGLQ->setMenuBar(gMenuBarQt->menuBarQ);
                    gMenuBarQt->findShowPropertyBrowserMenuItem();
                }


            }

            void GetContentSize(double* width, double* height) override {

                windowGLQ->GetContentSize(width, height);
            }

            void SetMinContentSize(double width, double height) override {
                int w = width;
                int h = height;
                windowGLQ->setGeometry(50,50,w, h);
            }

            void FreezePosition(SettingsRef settings, const std::string& key) override {

                if (!(windowGLQ->isVisible())) return;

                int left, top, width, height;
                #if 0
                QPoint topLeftPoint = windowGLQ->geometry().topLeft();
                left = topLeftPoint.x();
                top = topLeftPoint.y();
                #endif
                QPoint windowPos = windowGLQ->pos();
                left = windowPos.x();
                top = windowPos.y();
                width = windowGLQ->geometry().width();
                height = windowGLQ->geometry().height();
                bool isMaximized = windowGLQ->isMaximized();

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
                    windowGLQ->move(left, top);
                    windowGLQ->resize(width, height);
                }

                if (settings->ThawBool(key + "_Maximized", false)) {
//                  windowGLQ->SetFullScreen(true);
                    windowGLQ->setWindowState(Qt::WindowMaximized);
                }
            }

            void SetCursor(Cursor cursor) override {

                switch (cursor)
                {
                case Cursor::POINTER:
                    windowGLQ->setCursor(Qt::ArrowCursor);
                    break;
                case Cursor::HAND:
                    windowGLQ->setCursor(Qt::PointingHandCursor);
                    break;
                default:
                    windowGLQ->setCursor(Qt::ArrowCursor);
                }
            }

            void SetTooltip(const std::string& text, double x, double y,
                double width, double height) override {
                windowGLQ->SetTooltip(text, x, y, width, height);
            }

            bool IsEditorVisible() override {
                return windowGLQ->IsEditorVisible();
            }

            void ShowEditor(double x, double y, double fontHeight, double minWidth,
                bool isMonospace, const std::string& text) override {
                windowGLQ->ShowEditor(x, y, fontHeight, minWidth, isMonospace, text);
            }

            void HideEditor() override {
                windowGLQ->HideEditor();
            }

            void SetScrollbarVisible(bool visible) override {
                windowGLQ->SetScrollbarVisible(visible);
            }

            void ConfigureScrollbar(double min, double max, double pageSize) override {
                windowGLQ->ConfigureScrollbar(min, max, pageSize);
            }

            double GetScrollbarPosition() override {
                return windowGLQ->GetScrollbarPosition();
            }

            void SetScrollbarPosition(double pos) override {
                windowGLQ->SetScrollbarPosition(pos);
            }

            void Invalidate() override {
                windowGLQ->Invalidate();
            }

            QtGLMainWindow* getQtGLMainWindow() {
                return windowGLQ;
            }

            public slots:

            void processTabKeyPressed() {

                if(windowGLQ->menuBar() != gMenuBarQt->menuBarQ && gMenuBarQt->showPropertyBrowserMenuItem != nullptr) {
                      gMenuBarQt->showPropertyBrowserMenuItem->actionItemQ->setChecked(false);
                      gMenuBarQt->showPropertyBrowserMenuItem->onTriggered(false);
                }
            }

        };

        std::shared_ptr<WindowImplQt> gGrahicWindowQt = nullptr;

#undef CreateWindow
        WindowRef CreateWindow(Window::Kind kind,
            WindowRef parentWindow) {

            std::shared_ptr<Window> window;
            
            if(Window::Kind::TOOL == kind) {
                window = std::make_shared<WindowImplQt>(kind, gGrahicWindowQt);
            } else {
                window = std::make_shared<WindowImplQt>(kind, nullptr);
                gGrahicWindowQt =std::static_pointer_cast<WindowImplQt>(window);
            }
            return window;
        }

        // 3DConnexion support.
        void Open3DConnexion() {

        }
        void Close3DConnexion() {

        }
        void Request3DConnexionEventsForWindow(WindowRef window) {

        }

        /************** Settings *******************************/
//#ifndef XMLCheckResult
//  #define XMLCheckResult(a_eResult) if (a_eResult != tinyxml2::XMLError::XML_SUCCESS) { printf("Error: %i\n", a_eResult); }
//#endif

        class SettingsImpTinyXml final : public Settings {
        public:
            tinyxml2::XMLDocument xmlDoc;
            tinyxml2::XMLNode* pRoot;

            SettingsImpTinyXml(){
                tinyxml2::XMLError eResult = xmlDoc.LoadFile("solvespace.conf");
 //               XMLCheckResult(eResult);
                if (eResult != tinyxml2::XMLError::XML_SUCCESS) {
                    pRoot = xmlDoc.NewElement("Root");
                    xmlDoc.InsertFirstChild(pRoot);
                    tinyxml2::XMLError eResult = xmlDoc.SaveFile("solvespace.conf");
                }
                else
                    pRoot = xmlDoc.FirstChild();
            }

            
            ~SettingsImpTinyXml(){
            }

            void FreezeInt(const std::string& key, uint32_t value) override {
                tinyxml2::XMLElement* pElement = pRoot->FirstChildElement(key.c_str());
                if (pElement == nullptr) {
                    pElement = xmlDoc.NewElement(key.c_str());
                    pRoot->InsertEndChild(pElement);
                }
                pElement->SetText(value);
                tinyxml2::XMLError eResult = xmlDoc.SaveFile("solvespace.conf");
  //              XMLCheckResult(eResult);
            }

            uint32_t ThawInt(const std::string& key, uint32_t defaultValue = 0) override {
                int iOutInt = defaultValue;
                tinyxml2::XMLNode* pRoot2 = xmlDoc.FirstChild();
                if(pRoot2 != nullptr) {
                    tinyxml2::XMLElement* pElement2 = pRoot2->FirstChildElement(key.c_str());
                    if (pElement2 != nullptr) {
                        tinyxml2::XMLError eResult = pElement2->QueryIntText(&iOutInt);
                    } 
                    else {
        //                std::cout << "Error: " << tinyxml2::XMLError::XML_ERROR_PARSING_ELEMENT;
        //             XMLCheckResult(eResult);
                    }
                }
                else {
       //             std::cout << "Error: " << tinyxml2::XMLError::XML_ERROR_FILE_READ_ERROR;
                }
                return iOutInt;
            }

            void FreezeFloat(const std::string& key, double value) override{
                tinyxml2::XMLElement* pElement = pRoot->FirstChildElement(key.c_str());
                if (pElement == nullptr) {
                    pElement = xmlDoc.NewElement(key.c_str());
                    pRoot->InsertEndChild(pElement);
                }
                pElement->SetText(value);
                tinyxml2::XMLError eResult = xmlDoc.SaveFile("solvespace.conf");
  //              XMLCheckResult(eResult);
            }

            double ThawFloat(const std::string& key, double defaultValue = 0.0) override {
                double iOutFloat = defaultValue;
                tinyxml2::XMLNode* pRoot2 = xmlDoc.FirstChild();
                if(pRoot2 != nullptr) {
                    tinyxml2::XMLElement* pElement2 = pRoot2->FirstChildElement(key.c_str());
                    if (pElement2 != nullptr) {
                        tinyxml2::XMLError eResult = pElement2->QueryDoubleText(&iOutFloat);
                    } 
                    else {
        //                std::cout << "Error: " << tinyxml2::XMLError::XML_ERROR_PARSING_ELEMENT;
        //             XMLCheckResult(eResult);
                    }
                }
                else {
       //             std::cout << "Error: " << tinyxml2::XMLError::XML_ERROR_FILE_READ_ERROR;
                }
                return iOutFloat;
            }

            void FreezeString(const std::string& key, const std::string& value) override {
                tinyxml2::XMLElement* pElement = pRoot->FirstChildElement(key.c_str());
                if (pElement == nullptr) {
                    pElement = xmlDoc.NewElement(key.c_str());
                    pRoot->InsertEndChild(pElement);
                }
                pElement->SetText(value.c_str());
                tinyxml2::XMLError eResult = xmlDoc.SaveFile("solvespace.conf");
  //              XMLCheckResult(eResult);
            }

            std::string ThawString(const std::string& key, const std::string& defaultValue = "") override {
                std::string iOutString = defaultValue;
                tinyxml2::XMLNode* pRoot2 = xmlDoc.FirstChild();
                if(pRoot2 != nullptr) {
                    tinyxml2::XMLElement* pElement2 = pRoot2->FirstChildElement(key.c_str());
                    if(pElement2 != nullptr && pElement2->GetText() != nullptr) {
                        iOutString = pElement2->GetText();
                    } 
                    else {
                        //std::cout << "Error: " << tinyxml2::XMLError::XML_ERROR_PARSING_ELEMENT;
                    }
                }
                else {
                  //  std::cout << "Error: " << tinyxml2::XMLError::XML_ERROR_FILE_READ_ERROR;
                }
                return iOutString;
                #if 0
                std::string iOutString = defaultValue;
                tinyxml2::XMLNode* pRoot2 = xmlDoc.FirstChild();
                if (pRoot2 == nullptr)
                    std::cout << "Error: " << tinyxml2::XMLError::XML_ERROR_FILE_READ_ERROR;
                tinyxml2::XMLElement* pElement2 = pRoot2->FirstChildElement(key.c_str());
                if (pElement2 == nullptr)
                    std::cout << "Error: " << tinyxml2::XMLError::XML_ERROR_PARSING_ELEMENT;
                iOutString = pElement2->GetText();
                return iOutString;
                #endif
            }

        };

        SettingsRef GetSettings()
        {
            static std::shared_ptr<SettingsImpTinyXml> settings;
            if (!settings) {
                settings = std::make_shared<SettingsImpTinyXml>();
            }
            return settings;
        }

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

        QApplication* gQApp;

        std::vector<std::string> InitGui(int argc, char** argv) {
            gQApp = new QApplication(argc, argv);
            std::vector<std::string> args = SolveSpace::Platform::InitCli(argc, argv);
            return args;
        }

        void RunGui() {
            QString filePath = QString("./styleSheets/darkorange.qss");
            QFile File(filePath);
            bool opened = File.open(QFile::ReadOnly);

            if (opened == false)
            {
                std::cout << "failed to open qss" << std::endl;
            }
            else
            {
                QString StyleSheet = QLatin1String(File.readAll());
                gQApp->setStyleSheet(StyleSheet);
            }
            gQApp->exec();
        }
        void ExitGui() {
//          gQApp->exit;
            gQApp->quit();
        }
        void ClearGui() {
            delete gQApp;
        }
    }
}
