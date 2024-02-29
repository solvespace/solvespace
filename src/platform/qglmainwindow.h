#ifndef QGlMainWindow_h
#define QGlMainWindow_h


#include <solvespace.h>

#include <QApplication>
#include <QWidget>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_1_0>
//#include <gl/GLU.h>
//#include <gl/GL.h>


#include <QMainWindow>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QLineEdit>
#include <QToolTip>
#include <QMessageBox>
#include <QScreen>
#include <iostream>


using namespace SolveSpace::Platform;

class GLWidget : public QOpenGLWidget, public QOpenGLFunctions_1_0
{
	Q_OBJECT

public:
	GLWidget(QWidget* parent = 0) 
		: QOpenGLWidget(parent) {

		entry = new QLineEdit(this);
		entry->setGeometry(QRect(0, 0, 200, 40));
        QFont entryFont("Arial", 16);
        entry->setFont(entryFont);
		entry->setVisible(false);
		connect(entry, SIGNAL(returnPressed()), this, SLOT(entryFinished()));
		this->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
        this->setMouseTracking(true);
	}

	~GLWidget()
	{
		delete entry;
	}

	void startEditing(int x, int y, int fontHeight, int minWidth, bool isMonoSpace,
		const std::string& val) {

		entry->setGeometry(QRect(x, y, minWidth, fontHeight));
		entry->setText(QString::fromStdString(val));

		if (!entry->isVisible()) {
			entry->show();
			entry->setFocus();
			entry->setCursorPosition(0);
		}
	}

	void stopEditing() {

		if (entry->isVisible()) {
			entry->hide();
			this->setFocus();
		}
	}


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
	void initializeGL() {
		initializeOpenGLFunctions();
	}

	void paintGL() {

	    if(true == QOpenGLContext::currentContext()->isValid())
			emit renderOccuredSignal();
	}

	void resizeGL(int width, int height) {
		// end of remove
       if(true == QOpenGLContext::currentContext()->isValid())
          emit renderOccuredSignal();
	}

	//----Mouse Events--------
	void wheelEvent(QWheelEvent* event)
	{
         const double wheelDelta=120.0;
		 slvMouseEvent.button = MouseEvent::Button::NONE;
		 slvMouseEvent.type = MouseEvent::Type::SCROLL_VERT;
         slvMouseEvent.scrollDelta = (double)event->angleDelta().y() / wheelDelta;

		 emit mouseEventOccuredSignal(slvMouseEvent);
	}

	void mouseReleaseEvent(QMouseEvent* event) override{
		updateSlvSpaceMouseEvent(event);
	}

	void mousePressEvent(QMouseEvent* event) override{
		updateSlvSpaceMouseEvent(event);
	}

	void mouseMoveEvent(QMouseEvent* event) override{
		updateSlvSpaceMouseEvent(event);
	}

	void updateSlvSpaceMouseEvent(QMouseEvent* event)
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
		case Qt::MouseButton::MidButton:
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

		emit mouseEventOccuredSignal(slvMouseEvent);
	}
	//----Keyboard Events--------

	/*
	* Need to override the event method to catch the tab key during a key press event
	* Qt does not send a key press event when the tab key is presed. To get that 
	* we need to overwrite the event functon and check if the tab key was presed during a key press event
	* otherwise we send the rest of the event back to the widget for further processes
	*/
       
	bool event(QEvent* e) 
	{
        if(e->type() == QEvent::KeyPress) {

            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(e);

            if(keyEvent->key() == Qt::Key_Tab) {
	            emit tabKeyPressed();
            }
        } else {

	       QWidget::event(e);
        }
        
		
	    return true;
	}
	

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

	void updateSlvSpaceKeyEvent(QKeyEvent* event)
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

		emit keyEventOccuredSignal(slvKeyEvent);
	}

	void SetTooltip(const std::string& text, double x, double y,
		double width, double height) {
		this->setToolTip(QString::fromStdString(text));
		QPoint pos(x, y);
		QToolTip::showText(pos, toolTip());
	}
signals:
	void mouseEventOccuredSignal(SolveSpace::Platform::MouseEvent mouseEvent);
	void keyEventOccuredSignal(SolveSpace::Platform::KeyboardEvent keyEvent);
	void renderOccuredSignal();
    void editingDoneSignal(std::string);
    void tabKeyPressed();

public:

	QLineEdit* entry;
	SolveSpace::Platform::MouseEvent   slvMouseEvent;
	SolveSpace::Platform::KeyboardEvent slvKeyEvent;
};


class QtGLMainWindow : public QMainWindow
{
	Q_OBJECT

public:
	QtGLMainWindow(QWidget* parent = 0)
		:QMainWindow(parent) {

		glWidget = new GLWidget;
		scrollArea = new QScrollArea(this);
		gridLayout = new QGridLayout();
		glWidget->setLayout(gridLayout);
		//scrollArea->setWidget(glWidget);
		//scrollArea->setWidgetResizable(true);
		this->setCentralWidget(glWidget);
		//void QLayout::setContentsMargins(int left, int top, int right, int bottom)
	}

	~QtGLMainWindow()
	{
		delete glWidget;
		delete scrollArea;
	}

	#if 0 //ADAM CHECK SCROLL
	void resizeEvent(QResizeEvent* event) 
	{
        int glWidth = glWidget->size().width();
        int glHeight = glWidget->size().height();
        int scrollAreaWidth = scrollArea->size().width();
        int scrollAreaHeight = scrollArea->size().height();
		
        QWidget::resizeEvent(event);
      
	}
	#endif

	void SetScrollbarVisible(bool visible) {

		//bool resizable = scrollArea->widgetResizable();
		#if 1 
		if (true == visible)
			scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		else
			scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		#endif
	}

	void ConfigureScrollbar(double min, double max, double pageSize) {

		scrollArea->verticalScrollBar()->setMinimum((int)min);
		scrollArea->verticalScrollBar()->setMaximum((int)max);
		scrollArea->verticalScrollBar()->setPageStep((int)pageSize);
	}

	double GetScrollbarPosition() {

		return (double) scrollArea->verticalScrollBar()->value();
	}

	void SetScrollbarPosition(double pos)  {

		return scrollArea->verticalScrollBar()->setValue((int)pos);
	}

	void Invalidate()  {

		glWidget->update();
	}

	void StartEditing(int x, int y, int fontHeight, int minWidth, bool isMonoSpace,
		const std::string& val) {

		glWidget->startEditing(x, y, fontHeight, minWidth, isMonoSpace, val);
	}

	void StopEditing(){
		glWidget->stopEditing();
	}

	GLWidget* GetGlWidget() {
		return this->glWidget;
	}


	// Returns physical display DPI.
	double GetPixelDensity() {
		return QGuiApplication::primaryScreen()->logicalDotsPerInch();
	}

	// Returns raster graphics and coordinate scale (already applied on the platform side),
	// i.e. size of logical pixel in physical pixels, or device pixel ratio.
	double GetDevicePixelRatio() {
		return this->GetGlWidget()->devicePixelRatio();
	}

	// Returns (fractional) font scale, to be applied on top of (integral) device pixel ratio.
	double GetDeviceFontScale() {
		return GetPixelDensity() / GetDevicePixelRatio() / 96.0;
	}

	bool IsVisible()  {
		return this->isVisible();
	}

	void SetVisible(bool visible)  {
		this->setVisible(visible);
	}

	void Focus()  {
		this->setFocus();
	}

	bool IsFullScreen()  {
		return this->isFullScreen();
	}

	void SetFullScreen(bool fullScreen)  {
		if (true == fullScreen)
			this->setWindowState(Qt::WindowFullScreen);
		else
			this->setWindowState(Qt::WindowNoState); //The window has no state set (in normal state).

	}

	void SetTitle(const std::string& title)  {
		this->setWindowTitle(QString::fromStdString(title));
	}


	void GetContentSize(double* width, double* height)  {

		double pixelRatio = GetDevicePixelRatio();
		QRect rc = this->GetGlWidget()->geometry();
		*width = rc.width() / pixelRatio;
		*height = rc.height() / pixelRatio;
	}

	void SetMinContentSize(double width, double height)  {
		this->resize((int)width, (int)height);
        this->GetGlWidget()->resize(width, height);
	}

	void SetTooltip(const std::string& text, double x, double y,
		double width, double height)  {
		this->glWidget->setToolTip(QString::fromStdString(text));
	}

	bool IsEditorVisible()  {

		return this->glWidget->entry->isVisible();
	}

	void ShowEditor(double x, double y, double fontHeight, double minWidth,
		bool isMonospace, const std::string& text)  {
		
		// font size from Solvespace is very small and hard to see 
		// hard coded 20 for height and 100 for width 
		// using Arial font of size 16
		// font decalred and set to the entry QLabel in the constructor 
		// Raed Marwan
		this->glWidget->startEditing(x, y, 20, 100, isMonospace, text);
	}

	void HideEditor()  {

		this->glWidget->stopEditing();
	}

	
protected:

	void closeEvent(QCloseEvent* ev)
	{
		emit windowClosedSignal();
		ev->ignore();
	}

signals:
	void windowClosedSignal();
	
private:
	GLWidget* glWidget;
	QScrollArea* scrollArea;
	QGridLayout* gridLayout;
};


#endif
