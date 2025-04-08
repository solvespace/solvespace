//-----------------------------------------------------------------------------
// The Emscripten-based implementation of platform-dependent GUI functionality.
//
// Copyright 2018 whitequark
//-----------------------------------------------------------------------------
#include <emscripten.h>
#include <emscripten/val.h>
#include <emscripten/html5.h>
#include <emscripten/bind.h>
#include "config.h"
#include "solvespace.h"

using namespace emscripten;

EMSCRIPTEN_BINDINGS(solvespace) {
    emscripten::class_<std::function<void()>>("VoidFunctor")
        .constructor<>()
        .function("opcall", &std::function<void()>::operator());

    emscripten::class_<std::function<void(val)>>("Void1Functor")
        .constructor<>()
        .function("opcall", &std::function<void(val)>::operator());
}

namespace SolveSpace {
namespace Platform {

//-----------------------------------------------------------------------------
// Emscripten API bridging
//-----------------------------------------------------------------------------

#define sscheck(expr) do {                                                    \
        EMSCRIPTEN_RESULT emResult = (EMSCRIPTEN_RESULT)(expr);               \
        if(emResult < 0)                                                      \
            HandleError(__FILE__, __LINE__, __func__, #expr, emResult);       \
    } while(0)

static void HandleError(const char *file, int line, const char *function, const char *expr,
                 EMSCRIPTEN_RESULT emResult) {
    const char *error = "Unknown error";
    switch(emResult) {
        case EMSCRIPTEN_RESULT_DEFERRED:            error = "Deferred";               break;
        case EMSCRIPTEN_RESULT_NOT_SUPPORTED:       error = "Not supported";          break;
        case EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED: error = "Failed (not deferred)";  break;
        case EMSCRIPTEN_RESULT_INVALID_TARGET:      error = "Invalid target";         break;
        case EMSCRIPTEN_RESULT_UNKNOWN_TARGET:      error = "Unknown target";         break;
        case EMSCRIPTEN_RESULT_INVALID_PARAM:       error = "Invalid parameter";      break;
        case EMSCRIPTEN_RESULT_FAILED:              error = "Failed";                 break;
        case EMSCRIPTEN_RESULT_NO_DATA:             error = "No data";                break;
    }

    std::string message;
    message += ssprintf("File %s, line %u, function %s:\n", file, line, function);
    message += ssprintf("Emscripten API call failed: %s.\n", expr);
    message += ssprintf("Error: %s\n", error);
    FatalError(message);
}

static val Wrap(const std::function<void()> &functor) {
    return val(functor)["opcall"].call<val>("bind", val(functor));
}

static void RegisterEventListener(const val &target, std::string event, std::function<void()> functor) {
    std::function<void(val)> wrapper = [functor](val) { if(functor) functor(); };
    val wrapped = val(wrapper)["opcall"].call<val>("bind", val(wrapper));
    target.call<void>("addEventListener", event, wrapped);
}

//-----------------------------------------------------------------------------
// Fatal errors
//-----------------------------------------------------------------------------

void FatalError(const std::string &message) {
    dbp("%s", message.c_str());
#ifndef NDEBUG
    emscripten_debugger();
#endif
    abort();
}

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------

class SettingsImplHtml : public Settings {
public:
    void FreezeInt(const std::string &key, uint32_t value) {
        val::global("localStorage").call<void>("setItem", key, value);
    }

    uint32_t ThawInt(const std::string &key, uint32_t defaultValue = 0) {
        val value = val::global("localStorage").call<val>("getItem", key);
        if(value == val::null())
        return defaultValue;
        return val::global("parseInt")(value, 0).as<int>();
    }

    void FreezeFloat(const std::string &key, double value) {
        val::global("localStorage").call<void>("setItem", key, value);
    }

    double ThawFloat(const std::string &key, double defaultValue = 0.0) {
        val value = val::global("localStorage").call<val>("getItem", key);
        if(value == val::null())
        return defaultValue;
        return val::global("parseFloat")(value).as<double>();
    }

    void FreezeString(const std::string &key, const std::string &value) {
        val::global("localStorage").call<void>("setItem", key, value);
    }

    std::string ThawString(const std::string &key,
                           const std::string &defaultValue = "") {
        val value = val::global("localStorage").call<val>("getItem", key);
        if(value == val::null()) {
            return defaultValue;
        }
        return value.as<std::string>();
    }
};

SettingsRef GetSettings() {
    return std::make_shared<SettingsImplHtml>();
}

//-----------------------------------------------------------------------------
// Timers
//-----------------------------------------------------------------------------

class TimerImplHtml : public Timer {
public:
    static void Callback(void *arg) {
        TimerImplHtml *timer = (TimerImplHtml *)arg;
        if(timer->onTimeout) {
            timer->onTimeout();
        }
    }

    void RunAfter(unsigned milliseconds) override {
        emscripten_async_call(TimerImplHtml::Callback, this, milliseconds + 1);
    }

    void RunAfterNextFrame() override {
        emscripten_async_call(TimerImplHtml::Callback, this, 0);
    }

    void RunAfterProcessingEvents() override {
        emscripten_push_uncounted_main_loop_blocker(TimerImplHtml::Callback, this);
    }
};

TimerRef CreateTimer() {
    return std::unique_ptr<TimerImplHtml>(new TimerImplHtml);
}

//-----------------------------------------------------------------------------
// Menus
//-----------------------------------------------------------------------------

class MenuItemImplHtml : public MenuItem {
public:
    val htmlMenuItem;

    MenuItemImplHtml() :
        htmlMenuItem(val::global("document").call<val>("createElement", val("li")))
    {}

    void SetAccelerator(KeyboardEvent accel) override {
        val htmlAccel = htmlMenuItem.call<val>("querySelector", val(".accel"));
        if(htmlAccel.as<bool>()) {
            htmlAccel.call<void>("remove");
        }
        htmlAccel = val::global("document").call<val>("createElement", val("span"));
        htmlAccel.call<void>("setAttribute", val("class"), val("accel"));
        htmlAccel.set("innerText", AcceleratorDescription(accel));
        htmlMenuItem.call<void>("appendChild", htmlAccel);
    }

    void SetIndicator(Indicator type) override {
        val htmlClasses = htmlMenuItem["classList"];
        htmlClasses.call<void>("remove", val("check"));
        htmlClasses.call<void>("remove", val("radio"));
        switch(type) {
            case Indicator::NONE:
                break;

            case Indicator::CHECK_MARK:
                htmlClasses.call<void>("add", val("check"));
                break;

            case Indicator::RADIO_MARK:
                htmlClasses.call<void>("add", val("radio"));
                break;
        }
    }

    void SetEnabled(bool enabled) override {
        if(enabled) {
            htmlMenuItem["classList"].call<void>("remove", val("disabled"));
        } else {
            htmlMenuItem["classList"].call<void>("add", val("disabled"));
        }
    }

    void SetActive(bool active) override {
        if(active) {
            htmlMenuItem["classList"].call<void>("add", val("active"));
        } else {
            htmlMenuItem["classList"].call<void>("remove", val("active"));
        }
    }
};

class MenuImplHtml;

static std::shared_ptr<MenuImplHtml> popupMenuOnScreen;

class MenuImplHtml : public Menu,
                     public std::enable_shared_from_this<MenuImplHtml> {
public:
    val htmlMenu;

    std::vector<std::shared_ptr<MenuItemImplHtml>> menuItems;
    std::vector<std::shared_ptr<MenuImplHtml>>     subMenus;

    std::function<void()> popupDismissFunc;

    MenuImplHtml() :
        htmlMenu(val::global("document").call<val>("createElement", val("ul")))
    {
        htmlMenu["classList"].call<void>("add", val("menu"));
    }

    MenuItemRef AddItem(const std::string &label, std::function<void()> onTrigger,
                        bool mnemonics = true) override {
        std::shared_ptr<MenuItemImplHtml> menuItem = std::make_shared<MenuItemImplHtml>();
        menuItems.push_back(menuItem);
        menuItem->onTrigger = onTrigger;

        if(mnemonics) {
            val::global("window").call<void>("setLabelWithMnemonic", menuItem->htmlMenuItem,
                                             label);
        } else {
            val htmlLabel = val::global("document").call<val>("createElement", val("span"));
            htmlLabel["classList"].call<void>("add", val("label"));
            htmlLabel.set("innerText", label);
            menuItem->htmlMenuItem.call<void>("appendChild", htmlLabel);
        }
        RegisterEventListener(menuItem->htmlMenuItem, "trigger", [menuItem]() {
            if(menuItem->onTrigger) {
                menuItem->onTrigger();
            }
        });
        htmlMenu.call<void>("appendChild", menuItem->htmlMenuItem);
        return menuItem;
    }

    std::shared_ptr<Menu> AddSubMenu(const std::string &label) override {
        val htmlMenuItem = val::global("document").call<val>("createElement", val("li"));
        val::global("window").call<void>("setLabelWithMnemonic", htmlMenuItem, label);
        htmlMenuItem["classList"].call<void>("add", val("has-submenu"));
        htmlMenu.call<void>("appendChild", htmlMenuItem);

        std::shared_ptr<MenuImplHtml> subMenu = std::make_shared<MenuImplHtml>();
        subMenus.push_back(subMenu);
        htmlMenuItem.call<void>("appendChild", subMenu->htmlMenu);
        return subMenu;
    }

    void AddSeparator() override {
        val htmlSeparator = val::global("document").call<val>("createElement", val("li"));
        htmlSeparator["classList"].call<void>("add", val("separator"));
        htmlMenu.call<void>("appendChild", htmlSeparator);
    }

    void PopUp() override {
        if(popupMenuOnScreen) {
            popupMenuOnScreen->htmlMenu.call<void>("remove");
            popupMenuOnScreen = NULL;
        }

        EmscriptenMouseEvent emStatus = {};
        sscheck(emscripten_get_mouse_status(&emStatus));
        htmlMenu["classList"].call<void>("add", val("popup"));
        htmlMenu["style"].set("left", std::to_string(emStatus.clientX) + "px");
        htmlMenu["style"].set("top",  std::to_string(emStatus.clientY) + "px");

        val::global("document")["body"].call<void>("appendChild", htmlMenu);
        popupMenuOnScreen = shared_from_this();
    }

    void Clear() override {
        while(htmlMenu["childElementCount"].as<int>() > 0) {
            htmlMenu["firstChild"].call<void>("remove");
        }
    }
};

MenuRef CreateMenu() {
    return std::make_shared<MenuImplHtml>();
}

class MenuBarImplHtml final : public MenuBar {
public:
    val htmlMenuBar;

    std::vector<std::shared_ptr<MenuImplHtml>> subMenus;

    MenuBarImplHtml() :
        htmlMenuBar(val::global("document").call<val>("createElement", val("ul")))
    {
        htmlMenuBar["classList"].call<void>("add", val("menu"));
        htmlMenuBar["classList"].call<void>("add", val("menubar"));
    }

    std::shared_ptr<Menu> AddSubMenu(const std::string &label) override {
        val htmlMenuItem = val::global("document").call<val>("createElement", val("li"));
        val::global("window").call<void>("setLabelWithMnemonic", htmlMenuItem, label);
        htmlMenuBar.call<void>("appendChild", htmlMenuItem);

        std::shared_ptr<MenuImplHtml> subMenu = std::make_shared<MenuImplHtml>();
        subMenus.push_back(subMenu);
        htmlMenuItem.call<void>("appendChild", subMenu->htmlMenu);
        return subMenu;
    }

    void Clear() override {
        while(htmlMenuBar["childElementCount"].as<int>() > 0) {
            htmlMenuBar["firstChild"].call<void>("remove");
        }
    }
};

MenuBarRef GetOrCreateMainMenu(bool *unique) {
    *unique = false;
    return std::make_shared<MenuBarImplHtml>();
}

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------

class TouchEventHelper {
public:
    // FIXME(emscripten): Workaround. touchstart and touchend repeats two times.
    bool touchActionStarted = false;

    int previousNumTouches = 0;

    double centerX = 0;
    double centerY = 0;

    // double startPinchDistance = 0;
    double previousPinchDistance = 0;

    std::function<void(MouseEvent*, void*)> onPointerDown;
    std::function<void(MouseEvent*, void*)> onPointerMove;
    std::function<void(MouseEvent*, void*)> onPointerUp;
    std::function<void(MouseEvent*, void*)> onScroll;

    void clear(void) {
        touchActionStarted = false;
        previousNumTouches = 0;
        centerX = 0;
        centerY = 0;
        // startPinchDistance = 0;
        previousPinchDistance = 0;
    }

    void calculateCenterPosition(const EmscriptenTouchEvent& emEvent, double& dst_x, double& dst_y) {
        double x = 0;
        double y = 0;
        for (int i = 0; i < emEvent.numTouches; i++) {
            x += emEvent.touches[i].targetX;
            y += emEvent.touches[i].targetY;
        }
        dst_x = x / emEvent.numTouches;
        dst_y = y / emEvent.numTouches;
    }

    void calculatePinchDistance(const EmscriptenTouchEvent& emEvent, double& dst_distance) {
        if (emEvent.numTouches < 2) {
            return;
        }
        double x1 = emEvent.touches[0].targetX;
        double y1 = emEvent.touches[0].targetY;
        double x2 = emEvent.touches[1].targetX;
        double y2 = emEvent.touches[1].targetY;
        dst_distance = std::sqrt(std::pow(x1 - x2, 2) + std::pow(y1 - y2, 2));
    }

    void createMouseEventPRESS(const EmscriptenTouchEvent& emEvent, MouseEvent& dst_mouseevent) {
        double x = 0, y = 0;
        this->calculateCenterPosition(emEvent, x, y);
        this->centerX = x;
        this->centerY = y;
        this->touchActionStarted = true;
        this->previousNumTouches = emEvent.numTouches;
        dst_mouseevent.type = MouseEvent::Type::PRESS;
        dst_mouseevent.x = x;
        dst_mouseevent.y = y;
        dst_mouseevent.shiftDown = emEvent.shiftKey;
        dst_mouseevent.controlDown = emEvent.ctrlKey;
        switch(emEvent.numTouches) {
        case 1:
            dst_mouseevent.button = MouseEvent::Button::LEFT;
            break;
        case 2: {
            dst_mouseevent.button = MouseEvent::Button::RIGHT;
            // double distance = 0;
            this->calculatePinchDistance(emEvent, this->previousPinchDistance);
            // this->startPinchDistance = distance;
            // this->previousPinchDistance = distance;
            break;
        }
        default:
            dst_mouseevent.button = MouseEvent::Button::MIDDLE;
            break;
        }
    }

    void createMouseEventRELEASE(const EmscriptenTouchEvent& emEvent, MouseEvent& dst_mouseevent) {
        this->calculateCenterPosition(emEvent, this->centerX, this->centerY);
        this->previousNumTouches = 0;
        dst_mouseevent.type = MouseEvent::Type::RELEASE;
        dst_mouseevent.x = this->centerX;
        dst_mouseevent.y = this->centerY;
        dst_mouseevent.shiftDown = emEvent.shiftKey;
        dst_mouseevent.controlDown = emEvent.ctrlKey;
        switch(this->previousNumTouches) {
        case 1:
            dst_mouseevent.button = MouseEvent::Button::LEFT;
            break;
        case 2:
            dst_mouseevent.button = MouseEvent::Button::RIGHT;
            break;
        default:
            dst_mouseevent.button = MouseEvent::Button::MIDDLE;
            break;
        }
    }

    void createMouseEventMOTION(const EmscriptenTouchEvent& emEvent, MouseEvent& dst_mouseevent) {
        dst_mouseevent.type = MouseEvent::Type::MOTION;
        this->calculateCenterPosition(emEvent, this->centerX, this->centerY);
        dst_mouseevent.x = this->centerX;
        dst_mouseevent.y = this->centerY;
        dst_mouseevent.shiftDown = emEvent.shiftKey;
        dst_mouseevent.controlDown = emEvent.ctrlKey;
        switch(emEvent.numTouches) {
        case 1:
            dst_mouseevent.button = MouseEvent::Button::LEFT;
            break;
        case 2:
            dst_mouseevent.button = MouseEvent::Button::RIGHT;
            break;
        default:
            dst_mouseevent.button = MouseEvent::Button::MIDDLE;
            break;
        }
    }

    void createMouseEventSCROLL(const EmscriptenTouchEvent& emEvent, MouseEvent& event) {
        event.type = MouseEvent::Type::SCROLL_VERT;
        double newDistance = 0;
        this->calculatePinchDistance(emEvent, newDistance);
        this->calculateCenterPosition(emEvent, this->centerX, this->centerY);
        event.x = this->centerX;
        event.y = this->centerY;
        event.shiftDown = emEvent.shiftKey;
        event.controlDown = emEvent.ctrlKey;
        // FIXME(emscripten): best value range for scrollDelta ?
        event.scrollDelta = (newDistance - this->previousPinchDistance) / 25.0;
        if (std::abs(event.scrollDelta) > 2) {
            event.scrollDelta = 2;
            if (std::signbit(event.scrollDelta)) {
                event.scrollDelta *= -1.0;
            }
        }
        this->previousPinchDistance = newDistance;
    }

    void onTouchStart(const EmscriptenTouchEvent& emEvent, void* callbackParameter) {
        if (this->touchActionStarted) {
            // dbp("onTouchStart(): Break due to already started.");
            return;
        }

        MouseEvent event;
        this->createMouseEventPRESS(emEvent, event);
        this->previousNumTouches = emEvent.numTouches;
        if (this->onPointerDown) {
            // dbp("onPointerDown(): numTouches=%d, timestamp=%f", emEvent.numTouches, emEvent.timestamp);
            this->onPointerDown(&event, callbackParameter);
        }
    }

    void onTouchMove(const EmscriptenTouchEvent& emEvent, void* callbackParameter) {
        this->calculateCenterPosition(emEvent, this->centerX, this->centerY);
        int newNumTouches = emEvent.numTouches;
        if (newNumTouches != this->previousNumTouches) {
            MouseEvent releaseEvent;

            this->createMouseEventRELEASE(emEvent, releaseEvent);
            if (this->onPointerUp) {
                // dbp("onPointerUp(): numTouches=%d, timestamp=%f", emEvent.numTouches, emEvent.timestamp);
                this->onPointerUp(&releaseEvent, callbackParameter);
            }

            MouseEvent pressEvent;

            this->createMouseEventPRESS(emEvent, pressEvent);
            if (this->onPointerDown) {
                // dbp("onPointerDown(): numTouches=%d, timestamp=%f", emEvent.numTouches, emEvent.timestamp);
                this->onPointerDown(&pressEvent, callbackParameter);
            }
        }

        MouseEvent motionEvent = { };
        this->createMouseEventMOTION(emEvent, motionEvent);

        if (this->onPointerMove) {
            // dbp("onPointerMove(): numTouches=%d, timestamp=%f", emEvent.numTouches, emEvent.timestamp);
            this->onPointerMove(&motionEvent, callbackParameter);
        }

        if (emEvent.numTouches == 2) {
            MouseEvent scrollEvent;
            this->createMouseEventSCROLL(emEvent, scrollEvent);
            if (scrollEvent.scrollDelta != 0) {
                if (this->onScroll) {
                    // dbp("Scroll %f", scrollEvent.scrollDelta);
                    this->onScroll(&scrollEvent, callbackParameter);
                }
            }
        }

        this->previousNumTouches = emEvent.numTouches;
    }

    void onTouchEnd(const EmscriptenTouchEvent& emEvent, void* callbackParameter) {
        if (!this->touchActionStarted) {
            return;
        }

        MouseEvent releaseEvent = { };
        this->createMouseEventRELEASE(emEvent, releaseEvent);
        
        if (this->onPointerUp) {
            // dbp("onPointerUp(): numTouches=%d, timestamp=%d", emEvent.numTouches, emEvent.timestamp);
            this->onPointerUp(&releaseEvent, callbackParameter);
        }

        this->clear();
    }

    void onTouchCancel(const EmscriptenTouchEvent& emEvent, void* callbackParameter) {
        this->onTouchEnd(emEvent, callbackParameter);
    }
};

static TouchEventHelper touchEventHelper;

static KeyboardEvent handledKeyboardEvent;

class WindowImplHtml final : public Window {
public:
    std::string emCanvasSel;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emContext = 0;

    val htmlContainer;
    val htmlEditor;
    val scrollbarHelper;

    std::shared_ptr<MenuBarImplHtml> menuBar;

    WindowImplHtml(val htmlContainer, std::string emCanvasSel) :
        emCanvasSel(emCanvasSel),
        htmlContainer(htmlContainer),
        htmlEditor(val::global("document").call<val>("createElement", val("input")))
    {
        htmlEditor["classList"].call<void>("add", val("editor"));
        htmlEditor["style"].set("display", "none");
        auto editingDoneFunc = [this] {
            if(onEditingDone) {
                onEditingDone(htmlEditor["value"].as<std::string>());
            }
        };
        RegisterEventListener(htmlEditor, "trigger", editingDoneFunc);
        htmlContainer["parentElement"].call<void>("appendChild", htmlEditor);

        std::string scrollbarElementQuery = emCanvasSel + "scrollbar";
        dbp("scrollbar element query: \"%s\"", scrollbarElementQuery.c_str());
        val scrollbarElement = val::global("document").call<val>("querySelector", val(scrollbarElementQuery));
        if (scrollbarElement == val::null()) {
            // dbp("scrollbar element is null.");
            this->scrollbarHelper = val::null();
        } else {
            dbp("scrollbar element OK.");
            this->scrollbarHelper = val::global("window")["ScrollbarHelper"].new_(val(scrollbarElementQuery));
            static std::function<void()> onScrollCallback = [this] {
                // dbp("onScrollCallback std::function this=%p", (void*)this);
                if (this->onScrollbarAdjusted) {
                    double newpos = this->scrollbarHelper.call<double>("getScrollbarPosition");
                    // dbp("  call onScrollbarAdjusted(%f)", newpos);
                    this->onScrollbarAdjusted(newpos);
                }
                this->Invalidate();
            };
            this->scrollbarHelper.set("onScrollCallback", Wrap(onScrollCallback));
        }

        sscheck(emscripten_set_resize_callback(
            EMSCRIPTEN_EVENT_TARGET_WINDOW, this, /*useCapture=*/false,
                    WindowImplHtml::ResizeCallback));
        sscheck(emscripten_set_resize_callback(
                    emCanvasSel.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::ResizeCallback));
        sscheck(emscripten_set_mousemove_callback(
                    emCanvasSel.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::MouseCallback));
        sscheck(emscripten_set_mousedown_callback(
                    emCanvasSel.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::MouseCallback));
        sscheck(emscripten_set_click_callback(
                    emCanvasSel.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::MouseCallback));
        sscheck(emscripten_set_dblclick_callback(
                    emCanvasSel.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::MouseCallback));
        sscheck(emscripten_set_mouseup_callback(
                    emCanvasSel.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::MouseCallback));
        sscheck(emscripten_set_mouseleave_callback(
                    emCanvasSel.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::MouseCallback));

        sscheck(emscripten_set_touchstart_callback(
                    emCanvasSel.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::TouchCallback));
        sscheck(emscripten_set_touchmove_callback(
                    emCanvasSel.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::TouchCallback));
        sscheck(emscripten_set_touchend_callback(
                    emCanvasSel.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::TouchCallback));
        sscheck(emscripten_set_touchcancel_callback(
                    emCanvasSel.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::TouchCallback));

        sscheck(emscripten_set_wheel_callback(
                    emCanvasSel.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::WheelCallback));
        sscheck(emscripten_set_keydown_callback(
            EMSCRIPTEN_EVENT_TARGET_WINDOW, this, /*useCapture=*/false,
                    WindowImplHtml::KeyboardCallback));
        sscheck(emscripten_set_keyup_callback(
            EMSCRIPTEN_EVENT_TARGET_WINDOW, this, /*useCapture=*/false,
                    WindowImplHtml::KeyboardCallback));
        sscheck(emscripten_set_webglcontextlost_callback(
                    emCanvasSel.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::ContextLostCallback));
        sscheck(emscripten_set_webglcontextrestored_callback(
                    emCanvasSel.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::ContextRestoredCallback));

        ResizeCanvasElement();
        SetupWebGLContext();
    }

    ~WindowImplHtml() {
        if(emContext != 0) {
            sscheck(emscripten_webgl_destroy_context(emContext));
        }
    }

    static EM_BOOL ResizeCallback(int emEventType, const EmscriptenUiEvent *emEvent, void *data) {
        WindowImplHtml *window = (WindowImplHtml *)data;
        window->Invalidate();
        return EM_TRUE;
    }

    static EM_BOOL MouseCallback(int emEventType, const EmscriptenMouseEvent *emEvent,
                                 void *data) {
        if(val::global("window").call<bool>("isModal")) return EM_FALSE;

        WindowImplHtml *window = (WindowImplHtml *)data;
        MouseEvent event = {};
        switch(emEventType) {
            case EMSCRIPTEN_EVENT_MOUSEMOVE:
                event.type = MouseEvent::Type::MOTION;
                break;
            case EMSCRIPTEN_EVENT_MOUSEDOWN:
                event.type = MouseEvent::Type::PRESS;
                break;
            case EMSCRIPTEN_EVENT_DBLCLICK:
                event.type = MouseEvent::Type::DBL_PRESS;
                break;
            case EMSCRIPTEN_EVENT_MOUSEUP:
                event.type = MouseEvent::Type::RELEASE;
                break;
            case EMSCRIPTEN_EVENT_MOUSELEAVE:
                event.type = MouseEvent::Type::LEAVE;
                break;
            default:
                return EM_FALSE;
        }
        switch(emEventType) {
            case EMSCRIPTEN_EVENT_MOUSEMOVE:
                if(emEvent->buttons & 1) {
                    event.button = MouseEvent::Button::LEFT;
                } else if(emEvent->buttons & 2) {
                    event.button = MouseEvent::Button::RIGHT;
                } else if(emEvent->buttons & 4) {
                    event.button = MouseEvent::Button::MIDDLE;
                }
                break;
            case EMSCRIPTEN_EVENT_MOUSEDOWN:
            case EMSCRIPTEN_EVENT_DBLCLICK:
            case EMSCRIPTEN_EVENT_MOUSEUP:
                switch(emEvent->button) {
                    case 0: event.button = MouseEvent::Button::LEFT;   break;
                    case 1: event.button = MouseEvent::Button::MIDDLE; break;
                    case 2: event.button = MouseEvent::Button::RIGHT;  break;
                }
                break;
            default:
                return EM_FALSE;
        }
        event.x           = emEvent->targetX;
        event.y           = emEvent->targetY;
        event.shiftDown   = emEvent->shiftKey || emEvent->altKey;
        event.controlDown = emEvent->ctrlKey;

        if(window->onMouseEvent) {
            return window->onMouseEvent(event);
        }
        return EM_FALSE;
    }

    static EM_BOOL TouchCallback(int emEventType, const EmscriptenTouchEvent *emEvent,
                                 void *data) {

        if(val::global("window").call<bool>("isModal")) return EM_FALSE;

        static bool initialized = false;

        WindowImplHtml *window = (WindowImplHtml *)data;

        if (!initialized) {
            touchEventHelper.onPointerDown = [](MouseEvent* event, void* param) -> void {
                WindowImplHtml* window = (WindowImplHtml*)param;
                if (window->onMouseEvent) {
                    window->onMouseEvent(*event);
                }
            };
            touchEventHelper.onPointerMove = [](MouseEvent* event, void* param) -> void {
                WindowImplHtml* window = (WindowImplHtml*)param;
                if (window->onMouseEvent) {
                    window->onMouseEvent(*event);
                }
            };
            touchEventHelper.onPointerUp = [](MouseEvent* event, void* param) -> void {
                WindowImplHtml* window = (WindowImplHtml*)param;
                if (window->onMouseEvent) {
                    window->onMouseEvent(*event);
                }
            };
            touchEventHelper.onScroll = [](MouseEvent* event, void* param) -> void {
                WindowImplHtml* window = (WindowImplHtml*)param;
                if (window->onMouseEvent) {
                    window->onMouseEvent(*event);
                }
            };
            initialized = true;
        }

        switch(emEventType) {
            case EMSCRIPTEN_EVENT_TOUCHSTART:
                touchEventHelper.onTouchStart(*emEvent, window);
                break;
            case EMSCRIPTEN_EVENT_TOUCHMOVE:
                touchEventHelper.onTouchMove(*emEvent, window);
                break;
            case EMSCRIPTEN_EVENT_TOUCHEND:
                touchEventHelper.onTouchEnd(*emEvent, window);
                break;
            case EMSCRIPTEN_EVENT_TOUCHCANCEL:
                touchEventHelper.onTouchCancel(*emEvent, window);
                break;
            default:
                return EM_FALSE;
        }

        return true;
    }

    static EM_BOOL WheelCallback(int emEventType, const EmscriptenWheelEvent *emEvent,
                                 void *data) {
        if(val::global("window").call<bool>("isModal")) return EM_FALSE;

        WindowImplHtml *window = (WindowImplHtml *)data;
        MouseEvent event = {};
        if(emEvent->deltaY != 0) {
            event.type = MouseEvent::Type::SCROLL_VERT;
            // FIXME(emscripten):
            // Pay attention to:
            //   dbp("Mouse wheel delta mode: %lu", emEvent->deltaMode);
            //     https://emscripten.org/docs/api_reference/html5.h.html#id11
            //     https://www.w3.org/TR/DOM-Level-3-Events/#dom-wheelevent-deltamode
            // and adjust the 0.01 below. deltaMode == 0 on a Firefox on a Windows.
            event.scrollDelta = -emEvent->deltaY * 0.01;
        } else {
            return EM_FALSE;
        }

        const EmscriptenMouseEvent &emStatus = emEvent->mouse;
        event.x           = emStatus.targetX;
        event.y           = emStatus.targetY;
        event.shiftDown   = emStatus.shiftKey;
        event.controlDown = emStatus.ctrlKey;

        if(window->onMouseEvent) {
            return window->onMouseEvent(event);
        }
        return EM_FALSE;
    }

    static EM_BOOL KeyboardCallback(int emEventType, const EmscriptenKeyboardEvent *emEvent,
                                    void *data) {
        if(emEvent->altKey) return EM_FALSE;
        if(emEvent->repeat) return EM_FALSE;

        WindowImplHtml *window = (WindowImplHtml *)data;
        KeyboardEvent event = {};
        switch(emEventType) {
            case EMSCRIPTEN_EVENT_KEYDOWN:
                event.type = KeyboardEvent::Type::PRESS;
                break;

            case EMSCRIPTEN_EVENT_KEYUP:
                event.type = KeyboardEvent::Type::RELEASE;
                break;

            default:
                return EM_FALSE;
        }
        event.shiftDown   = emEvent->shiftKey;
        event.controlDown = emEvent->ctrlKey;

        std::string key = emEvent->key;
        if(key[0] == 'F' && isdigit(key[1])) {
            event.key = KeyboardEvent::Key::FUNCTION;
            event.num = std::stol(key.substr(1));
        } else {
            event.key = KeyboardEvent::Key::CHARACTER;

            auto utf8 = ReadUTF8(key);
            if(++utf8.begin() == utf8.end()) {
                event.chr = tolower(*utf8.begin());
            } else if(key == "Escape") {
                event.chr = '\e';
            } else if(key == "Tab") {
                event.chr = '\t';
            } else if(key == "Backspace") {
                event.chr = '\b';
            } else if(key == "Delete") {
                event.chr = '\x7f';
            } else {
                return EM_FALSE;
            }

            if(event.chr == '>' && event.shiftDown) {
                event.shiftDown = false;
            }
        }

        if(event.Equals(handledKeyboardEvent)) return EM_FALSE;
        if(val::global("window").call<bool>("isModal")) {
            handledKeyboardEvent = {};
            return EM_FALSE;
        }

        if(window->onKeyboardEvent) {
            if(window->onKeyboardEvent(event)) {
                handledKeyboardEvent = event;
                return EM_TRUE;
            }
        }
        return EM_FALSE;
    }

    void SetupWebGLContext() {
        EmscriptenWebGLContextAttributes emAttribs = {};
        emscripten_webgl_init_context_attributes(&emAttribs);
        emAttribs.alpha = false;
        emAttribs.failIfMajorPerformanceCaveat = true;

        sscheck(emContext = emscripten_webgl_create_context(emCanvasSel.c_str(), &emAttribs));
        dbp("Canvas %s: got context %d", emCanvasSel.c_str(), emContext);
    }

    static bool ContextLostCallback(int eventType, const void *reserved, void *data) {
        WindowImplHtml *window = (WindowImplHtml *)data;
        dbp("Canvas %s: context lost", window->emCanvasSel.c_str());
        window->emContext = 0;

        if(window->onContextLost) {
            window->onContextLost();
        }
        return EM_TRUE;
    }

    static bool ContextRestoredCallback(int eventType, const void *reserved, void *data) {
        WindowImplHtml *window = (WindowImplHtml *)data;
        dbp("Canvas %s: context restored", window->emCanvasSel.c_str());
        window->SetupWebGLContext();
        return EM_TRUE;
    }

    void ResizeCanvasElement() {
        double width, height;
        std::string htmlContainerSel = "#" + htmlContainer["id"].as<std::string>();
        sscheck(emscripten_get_element_css_size(htmlContainerSel.c_str(), &width, &height));
        // sscheck(emscripten_get_element_css_size(emCanvasSel.c_str(), &width, &height));

        double devicePixelRatio = GetDevicePixelRatio();
        width *= devicePixelRatio;
        height *= devicePixelRatio;

        int currentWidth = 0, currentHeight = 0;
        sscheck(emscripten_get_canvas_element_size(emCanvasSel.c_str(), &currentWidth, &currentHeight));
        
        if ((int)width != currentWidth || (int)height != currentHeight) {
            // dbp("Canvas %s container current size: (%d, %d)", emCanvasSel.c_str(), (int)currentWidth, (int)currentHeight);
            // dbp("Canvas %s: resizing to (%d, %d)", emCanvasSel.c_str(), (int)width, (int)height);
            sscheck(emscripten_set_canvas_element_size(emCanvasSel.c_str(), (int)width, (int)height));
        }
    }

    static void RenderCallback(void *data) {
        WindowImplHtml *window = (WindowImplHtml *)data;
        if(window->emContext == 0) {
            dbp("Canvas %s: cannot render: no context", window->emCanvasSel.c_str());
            return;
        }

        window->ResizeCanvasElement();
        sscheck(emscripten_webgl_make_context_current(window->emContext));
        if(window->onRender) {
            window->onRender();
        }
    }

    double GetPixelDensity() override {
        return 96.0 * GetDevicePixelRatio();
    }

    double GetDevicePixelRatio() override {
        return emscripten_get_device_pixel_ratio();
    }

    bool IsVisible() override {
        // FIXME(emscripten): implement
        return true;
    }

    void SetVisible(bool visible) override {
        // FIXME(emscripten): implement
    }

    void Focus() override {
        // Do nothing, we can't affect focus of browser windows.
    }

    bool IsFullScreen() override {
        EmscriptenFullscreenChangeEvent emEvent = {};
        sscheck(emscripten_get_fullscreen_status(&emEvent));
        return emEvent.isFullscreen;
    }

    void SetFullScreen(bool fullScreen) override {
        if(fullScreen) {
            EmscriptenFullscreenStrategy emStrategy = {};
            emStrategy.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH;
            emStrategy.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_HIDEF;
            emStrategy.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
            sscheck(emscripten_request_fullscreen_strategy(
                        emCanvasSel.c_str(), /*deferUntilInEventHandler=*/true, &emStrategy));
        } else {
            sscheck(emscripten_exit_fullscreen());
        }
    }

    void SetTitle(const std::string &title) override {
        // FIXME(emscripten): implement
    }

    void SetMenuBar(MenuBarRef menuBar) override {
        std::shared_ptr<MenuBarImplHtml> menuBarImpl =
            std::static_pointer_cast<MenuBarImplHtml>(menuBar);
        this->menuBar = menuBarImpl;

        val htmlMain = val::global("document").call<val>("querySelector", val("main"));
        val htmlCurrentMenuBar = htmlMain.call<val>("querySelector", val(".menubar"));
        if(htmlCurrentMenuBar.as<bool>()) {
            htmlCurrentMenuBar.call<void>("remove");
        }
        htmlMain.call<void>("insertBefore", menuBarImpl->htmlMenuBar,
                                 htmlMain["firstChild"]);
        ResizeCanvasElement();
    }

    void GetContentSize(double *width, double *height) override {
        sscheck(emscripten_get_element_css_size(emCanvasSel.c_str(), width, height));
    }

    void SetMinContentSize(double width, double height) override {
        // Do nothing, we can't affect sizing of browser windows.
    }

    void FreezePosition(SettingsRef settings, const std::string &key) override {
        // Do nothing, we can't position browser windows.
    }

    void ThawPosition(SettingsRef settings, const std::string &key) override {
        // Do nothing, we can't position browser windows.
    }

    void SetCursor(Cursor cursor) override {
        std::string htmlCursor;
        switch(cursor) {
            case Cursor::POINTER: htmlCursor = "default"; break;
            case Cursor::HAND:    htmlCursor = "pointer"; break;
        }
        htmlContainer["style"].set("cursor", htmlCursor);
    }

    void SetTooltip(const std::string &text, double x, double y,
                    double width, double height) override {
        val htmlCanvas =
            val::global("document").call<val>("querySelector", emCanvasSel);
        htmlCanvas.set("title", text);
    }

    bool IsEditorVisible() override {
        return htmlEditor["style"]["display"].as<std::string>() != "none";
    }

    void ShowEditor(double x, double y, double fontHeight, double minWidth,
                    bool isMonospace, const std::string &text) override {
        htmlEditor["style"].set("display", val(""));
        val canvasClientRect = val::global("document").call<val>("querySelector", val(this->emCanvasSel)).call<val>("getBoundingClientRect");
        double canvasLeft = canvasClientRect["left"].as<double>();
        double canvasTop = canvasClientRect["top"].as<double>();
        htmlEditor["style"].set("left", std::to_string(canvasLeft + x - 4) + "px");
        htmlEditor["style"].set("top",  std::to_string(canvasTop + y - fontHeight - 2) + "px");
        htmlEditor["style"].set("fontSize", std::to_string(fontHeight) + "px");
        htmlEditor["style"].set("minWidth", std::to_string(minWidth) + "px");
        htmlEditor["style"].set("fontFamily", isMonospace ? "monospace" : "sans");
        htmlEditor.set("value", text);
        htmlEditor.call<void>("focus");
    }

    void HideEditor() override {
        htmlEditor["style"].set("display", val("none"));
    }

    void SetScrollbarVisible(bool visible) override {
        // dbp("SetScrollbarVisible(): visible=%d", visible ? 1 : 0);
        if (this->scrollbarHelper == val::null()) {
            // dbp("scrollbarHelper is null.");
            return;
        }
        if (!visible) {
            this->scrollbarHelper.call<void>("setScrollbarEnabled", val(false));
        }
    }

    double scrollbarPos = 0.0;
    double scrollbarMin = 0.0;
    double scrollbarMax = 0.0;
    double scrollbarPageSize = 0.0;

    void ConfigureScrollbar(double min, double max, double pageSize) override {
        // dbp("ConfigureScrollbar(): min=%f, max=%f, pageSize=%f", min, max, pageSize);
        if (this->scrollbarHelper == val::null()) {
            // dbp("scrollbarHelper is null.");
            return;
        }
        // FIXME(emscripten): implement
        this->scrollbarMin = min;
        this->scrollbarMax = max;
        this->scrollbarPageSize = pageSize;

        this->scrollbarHelper.call<void>("setRange", this->scrollbarMin, this->scrollbarMax);
        this->scrollbarHelper.call<void>("setPageSize", pageSize);
    }

    double GetScrollbarPosition() override {
        // dbp("GetScrollbarPosition()");
        if (this->scrollbarHelper == val::null()) {
            // dbp("scrollbarHelper is null.");
            return 0;
        }
        this->scrollbarPos = this->scrollbarHelper.call<double>("getScrollbarPosition");
        // dbp("  GetScrollbarPosition() returns %f", this->scrollbarPos);
        return scrollbarPos;
    }

    void SetScrollbarPosition(double pos) override {
        // dbp("SetScrollbarPosition(): pos=%f", pos);
        if (this->scrollbarHelper == val::null()) {
            // dbp("scrollbarHelper is null.");
            return;
        }
        this->scrollbarHelper.call<void>("setScrollbarPosition", pos);
        scrollbarPos = pos;
    }

    void Invalidate() override {
        emscripten_async_call(WindowImplHtml::RenderCallback, this, -1);
    }
};

WindowRef CreateWindow(Window::Kind kind, WindowRef parentWindow) {
    static int windowNum;

    std::string htmlContainerId = std::string("container") + std::to_string(windowNum);
    val htmlContainer =
        val::global("document").call<val>("getElementById", htmlContainerId);
    std::string emCanvasSel = std::string("#canvas") + std::to_string(windowNum);

    windowNum++;
    return std::make_shared<WindowImplHtml>(htmlContainer, emCanvasSel);
}

//-----------------------------------------------------------------------------
// 3DConnexion support
//-----------------------------------------------------------------------------

void Open3DConnexion() {}
void Close3DConnexion() {}
void Request3DConnexionEventsForWindow(WindowRef window) {}

//-----------------------------------------------------------------------------
// Message dialogs
//-----------------------------------------------------------------------------

class MessageDialogImplHtml;

static std::vector<std::shared_ptr<MessageDialogImplHtml>> dialogsOnScreen;

class MessageDialogImplHtml final : public MessageDialog,
                                    public std::enable_shared_from_this<MessageDialogImplHtml> {
public:
    val htmlModal;
    val htmlDialog;
    val htmlMessage;
    val htmlDescription;
    val htmlButtons;

    bool is_shown = false;

    Response latestResponse = Response::NONE;

    MessageDialogImplHtml() :
        htmlModal(val::global("document").call<val>("createElement", val("div"))),
        htmlDialog(val::global("document").call<val>("createElement", val("div"))),
        htmlMessage(val::global("document").call<val>("createElement", val("strong"))),
        htmlDescription(val::global("document").call<val>("createElement", val("p"))),
        htmlButtons(val::global("document").call<val>("createElement", val("div")))
    {
        htmlModal["classList"].call<void>("add", val("modal"));
        htmlModal.call<void>("appendChild", htmlDialog);
        htmlDialog["classList"].call<void>("add", val("dialog"));
        htmlDialog.call<void>("appendChild", htmlMessage);
        htmlDialog.call<void>("appendChild", htmlDescription);
        htmlButtons["classList"].call<void>("add", val("buttons"));
        htmlDialog.call<void>("appendChild", htmlButtons);
    }

    void SetType(Type type) {
        // FIXME(emscripten): implement
    }

    void SetTitle(std::string title) {
        // FIXME(emscripten): implement
    }

    void SetMessage(std::string message) {
        htmlMessage.set("innerText", message);
    }

    void SetDescription(std::string description) {
        htmlDescription.set("innerText", description);
    }

    void AddButton(std::string label, Response response, bool isDefault = false) {
        val htmlButton = val::global("document").call<val>("createElement", val("div"));
        htmlButton["classList"].call<void>("add", val("button"));
        val::global("window").call<void>("setLabelWithMnemonic", htmlButton, label);
        if(isDefault) {
            htmlButton["classList"].call<void>("add", val("default"), val("selected"));
        }

        std::function<void()> responseFunc = [this, response] {
            htmlModal.call<void>("remove");
            this->latestResponse = response;
            if(onResponse) {
                onResponse(response);
            }
            auto it = std::remove(dialogsOnScreen.begin(), dialogsOnScreen.end(),
                                  shared_from_this());
            dialogsOnScreen.erase(it);
            
            this->is_shown = false;
        };
        RegisterEventListener(htmlButton, "trigger", responseFunc);

        htmlButtons.call<void>("appendChild", htmlButton);
    }

    Response RunModal() {
        this->ShowModal();
        //FIXME(emscripten): use val::await() with JavaScript's Promise
        while (true) {
            if (this->is_shown) {
                emscripten_sleep(50);
            } else {
                break;
            }
        }
        
        if (this->latestResponse != Response::NONE) {
            return this->latestResponse;
        } else {
            // FIXME(emscripten):
            return this->latestResponse;
        }
    }

    void ShowModal() {
        dialogsOnScreen.push_back(shared_from_this());
        val::global("document")["body"].call<void>("appendChild", htmlModal);

        this->is_shown = true;
    }
};

MessageDialogRef CreateMessageDialog(WindowRef parentWindow) {
    return std::make_shared<MessageDialogImplHtml>();
}

//-----------------------------------------------------------------------------
// File dialogs
//-----------------------------------------------------------------------------

// In emscripten pseudo filesystem, all userdata will be stored in this directory.
static std::string basePathInFilesystem = "/data/";


/* FileDialog that can open, save and browse. Also refer `src/platform/html/filemanagerui.js`.
 */
class FileDialogImplHtml : public FileDialog {
public:

    enum class Modes {
        OPEN = 0,
        SAVE,
        BROWSER
    };

    Modes mode;

    std::string title;
    std::string filename;
    std::string filters;

    val jsFileManagerUI;

    FileDialogImplHtml(Modes mode) {
        dbp("FileDialogImplHtml::FileDialogImplHtml()");
        val fileManagerUIClass = val::global("window")["FileManagerUI"];
        val dialogModeValue;
        this->mode = mode;
        if (mode == Modes::OPEN) {
            dialogModeValue = val(0);
        } else if (mode == Modes::SAVE) {
            dialogModeValue = val(1);
        } else {
            dialogModeValue = val(2);
        }
        this->jsFileManagerUI = fileManagerUIClass.new_(dialogModeValue);
        dbp("FileDialogImplHtml::FileDialogImplHtml() Done.");
    }

    ~FileDialogImplHtml() override {
        dbp("FileDialogImplHtml::~FileDialogImplHtml()");
        this->jsFileManagerUI.call<void>("dispose");
    }

    void SetTitle(std::string title) override {
        dbp("FileDialogImplHtml::SetTitle(): title=\"%s\"", title.c_str());
        this->title = title;
        this->jsFileManagerUI.call<void>("setTitle", val(title));
    }

    void SetCurrentName(std::string name) override {
        dbp("FileDialogImplHtml::SetCurrentName(): name=\"%s\", parent=\"%s\"", name.c_str(), this->GetFilename().Parent().raw.c_str());
      
        Path filepath = Path::From(name);
        if (filepath.IsAbsolute()) {
            // dbp("FileDialogImplHtml::SetCurrentName(): path is absolute.");
            SetFilename(filepath);
        } else {
            // dbp("FileDialogImplHtml::SetCurrentName(): path is relative.");
            SetFilename(GetFilename().Parent().Join(name));
        }
    }

    Platform::Path GetFilename() override {
        return Platform::Path::From(this->filename.c_str());
    }

    void SetFilename(Platform::Path path) override {
        dbp("FileDialogImplHtml::GetFilename(): path=\"%s\"", path.raw.c_str());
        this->filename = std::string(path.raw);
        std::string filename_ = Path::From(this->filename).FileName();
        this->jsFileManagerUI.call<void>("setDefaultFilename", val(filename_));
    }
    
    void SuggestFilename(Platform::Path path) override {
        dbp("FileDialogImplHtml::SuggestFilename(): path=\"%s\"", path.raw.c_str());
        SetFilename(Platform::Path::From(path.FileStem()));
    }

    void AddFilter(std::string name, std::vector<std::string> extensions) override {
        if (this->filters.length() > 0) {
            this->filters += ",";
        }
        for (size_t i = 0; i < extensions.size(); i++) {
            if (i != 0) {
                this->filters += ",";
            }
            this->filters += "." + extensions[i];
        }
        dbp("FileDialogImplHtml::AddFilter(): filter=%s", this->filters.c_str());
        this->jsFileManagerUI.call<void>("setFilter", val(this->filters));
    }

    void FreezeChoices(SettingsRef settings, const std::string &key) override {
        //FIXME(emscripten): implement
    }

    void ThawChoices(SettingsRef settings, const std::string &key) override {
        //FIXME(emscripten): implement
    }

    bool RunModal() override {
        dbp("FileDialogImplHtml::RunModal()");

        this->jsFileManagerUI.call<void>("setBasePath", val(basePathInFilesystem));
        this->jsFileManagerUI.call<void>("show");
        while (true) {
            bool isShown = this->jsFileManagerUI.call<bool>("isShown");
            if (!isShown) {
                break;
            } else {
                emscripten_sleep(50);
            }
        }

        dbp("FileSaveDialogImplHtml::RunModal() : dialog closed.");

        std::string selectedFilename = this->jsFileManagerUI.call<std::string>("getSelectedFilename");
        if (selectedFilename.length() > 0) {
            // Dummy call to set parent directory
            this->SetFilename(Path::From(basePathInFilesystem + "/dummy"));
            this->SetCurrentName(selectedFilename);
        }


        if (selectedFilename.length() > 0) {
            return true;
        } else {
            return false;
        }
    }
};

FileDialogRef CreateOpenFileDialog(WindowRef parentWindow) {
    dbp("CreateOpenFileDialog()");
    return std::shared_ptr<FileDialogImplHtml>(new FileDialogImplHtml(FileDialogImplHtml::Modes::OPEN));
}

FileDialogRef CreateSaveFileDialog(WindowRef parentWindow) {
    dbp("CreateSaveFileDialog()");
    return std::shared_ptr<FileDialogImplHtml>(new FileDialogImplHtml(FileDialogImplHtml::Modes::SAVE));
}

//-----------------------------------------------------------------------------
// Application-wide APIs
//-----------------------------------------------------------------------------

std::vector<Platform::Path> GetFontFiles() {
    return {};
}

void OpenInBrowser(const std::string &url) {
    val::global("window").call<void>("open", url);
}


void OnSaveFinishedCallback(const Platform::Path& filename, bool is_saveAs, bool is_autosave) {
    dbp("OnSaveFinished(): %s, is_saveAs=%d, is_autosave=%d\n", filename.FileName().c_str(), is_saveAs, is_autosave);
    val::global("window").call<void>("saveFileDone", filename.raw, is_saveAs, is_autosave);
}

std::vector<std::string> InitGui(int argc, char **argv) {
    std::function<void()> onBeforeUnload = std::bind(&SolveSpaceUI::Exit, &SS);
    RegisterEventListener(val::global("window"), "beforeunload", onBeforeUnload);

    // dbp("Set onSaveFinished");
    SS.OnSaveFinished = OnSaveFinishedCallback;

    // FIXME(emscripten): get locale from user preferences
    SetLocale("en_US");

    return {};
}

static void MainLoopIteration() {
    // We don't do anything here, as all our work is registered via timers.
}

void RunGui() {
    emscripten_set_main_loop(MainLoopIteration, 0, /*simulate_infinite_loop=*/true);
}

void ExitGui() {
    exit(0);
}

void ClearGui() {}

}
}
