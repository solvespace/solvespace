//-----------------------------------------------------------------------------
// The Android-based implementation of platform-dependent GUI functionality.
//
// Copyright 2025
//-----------------------------------------------------------------------------
#include <GLES2/gl2.h>
#include <android/asset_manager_jni.h>
#include <android/input.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/set_abort_message.h>
#include <EGL/egl.h>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <jni.h>
#include <cmath>
#include <linux/time.h>
#include <memory>
#include <unordered_map>

#include "gui.h"
#include "solvespace.h"
#include "ui.h"

namespace SolveSpace {
namespace Platform {

//-----------------------------------------------------------------------------
// Fatal errors
//-----------------------------------------------------------------------------

void FatalError(const std::string &message) {
    android_set_abort_message(message.c_str());
    abort();
}

//-----------------------------------------------------------------------------
// Android-specific JNI interface
//-----------------------------------------------------------------------------

// Global JNI references
static JavaVM* g_JavaVM = nullptr;
static jobject g_Activity = nullptr;

// JNI method IDs
// SolveSpaceActivity
static jmethodID g_ShowEditorMethod = nullptr;
static jmethodID g_HideEditorMethod = nullptr;
static jmethodID g_IsEditorVisibleMethod = nullptr;
static jmethodID g_SendDelayedMethod = nullptr;
static jmethodID g_ShowMessageDialogMethod = nullptr;
static jmethodID g_ShowFileDialogMethod = nullptr;
static jmethodID g_SetTitle = nullptr;
static jmethodID g_OnWinAdded = nullptr;
static jmethodID g_GetDensity = nullptr;
static jmethodID g_SetScrollbar = nullptr;
static jmethodID g_Popup = nullptr;
static jmethodID g_OpenContentFileMethod = nullptr;
static jmethodID g_DeleteContentFileMethod = nullptr;
static jmethodID g_GetInternalStoragePathMethod = nullptr;
static jmethodID g_Finish = nullptr;
static jmethodID g_GetSysFonts = nullptr;
static jmethodID g_InvalidateMenu = nullptr;
// Menu
static jmethodID g_Add = nullptr;
static jmethodID g_AddSubMenu = nullptr;
static jmethodID g_Clear = nullptr;
// MenuItem
static jmethodID g_GetItemId = nullptr;
static jmethodID g_SetAlphabeticShortcut = nullptr;
static jmethodID g_SetCheckable = nullptr;
static jmethodID g_SetChecked = nullptr;
static jmethodID g_SetEnabled = nullptr;
static jmethodID g_SetShowAsAction = nullptr;
// SharedPreferences
static jmethodID g_GetPreferences = nullptr;
static jmethodID g_GetInt = nullptr;
static jmethodID g_GetFloat = nullptr;
static jmethodID g_GetString = nullptr;
static jmethodID g_Edit = nullptr;
// SharedPreferences$Editor
static jmethodID g_PutInt = nullptr;
static jmethodID g_PutFloat = nullptr;
static jmethodID g_PutString = nullptr;
static jmethodID g_Commit = nullptr;

static ANativeWindow *g_NativeWindow = nullptr;
static EGLDisplay gDisplay = EGL_NO_DISPLAY;
static EGLSurface gSurface = EGL_NO_SURFACE;

JNIEnv* GetJNIEnv() {
    ssassert(g_JavaVM != NULL, "g_JavaVM is NULL");

    JNIEnv* env;
    int status = g_JavaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        ssassert(g_JavaVM->AttachCurrentThread(&env, nullptr) == JNI_OK, "Attach Failed");
    } else {
        char str[20];
        sprintf(str, "GetEnv error: %d", status);
        ssassert(status == JNI_OK, str);
    }
    return env;
}

// Opens a Storage Access Framework content URI through the Java activity's
// ContentResolver. The Java side detaches the descriptor from its
// ParcelFileDescriptor, so ownership passes to the caller (which must close
// it, e.g. via fdopen + fclose). Returns -1 on failure.
int AndroidOpenContentFile(const char *uri, const char *mode) {
    JNIEnv *env = GetJNIEnv();
    if (!(env && g_Activity && g_OpenContentFileMethod)) return -1;

    jstring juri = env->NewStringUTF(uri);
    jstring jmode = env->NewStringUTF(mode);
    jint fd = env->CallIntMethod(g_Activity, g_OpenContentFileMethod, juri, jmode);
    env->DeleteLocalRef(juri);
    env->DeleteLocalRef(jmode);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return -1;
    }
    return fd;
}

// Deletes a Storage Access Framework content URI through the Java activity.
void AndroidDeleteContentFile(const char *uri) {
    JNIEnv *env = GetJNIEnv();
    if (!(env && g_Activity && g_DeleteContentFileMethod)) return;

    jstring juri = env->NewStringUTF(uri);
    env->CallBooleanMethod(g_Activity, g_DeleteContentFileMethod, juri);
    env->DeleteLocalRef(juri);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

// Returns the absolute path of the app's private files directory, or an empty
// string if the Java activity is not available.
std::string AndroidInternalStoragePath() {
    JNIEnv *env = GetJNIEnv();
    if (!(env && g_Activity && g_GetInternalStoragePathMethod)) return "";

    jstring ret = (jstring)env->CallObjectMethod(g_Activity, g_GetInternalStoragePathMethod);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return "";
    }
    if (!ret) return "";
    const char *str = env->GetStringUTFChars(ret, NULL);
    std::string result = str ? str : "";
    if (str) env->ReleaseStringUTFChars(ret, str);
    env->DeleteLocalRef(ret);
    return result;
}

// Maps a content URI to a filesystem-safe string, so it can be used as a
// filename/key in private storage (e.g. for autosave companions).
std::string AndroidContentKey(const char *uri) {
    std::string key;
    for (const char *p = uri; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c == '.' || c == '-' || c == '_') {
            key += (char)c;
        } else {
            key += '_';
        }
    }
    return key;
}

//-----------------------------------------------------------------------------
// EGL and GL context management
//-----------------------------------------------------------------------------

class EGLContextManager {
private:
    EGLContext m_Context;
    static EGLConfig m_Config;

public:
    EGLContextManager() : 
        m_Context(EGL_NO_CONTEXT) {}

    bool Initialize() {
        if (gDisplay == EGL_NO_DISPLAY) {
            // Get display
            gDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            if (gDisplay == EGL_NO_DISPLAY) {
                ALOGE("Failed to get EGL display");
                return false;
            }

            // Initialize EGL
            if (!eglInitialize(gDisplay, 0, 0)) {
                ALOGE("Failed to initialize EGL");
                return false;
            }

            // Choose config
            const EGLint configAttribs[] = {
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_BLUE_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_RED_SIZE, 8,
                EGL_ALPHA_SIZE, 8,
                EGL_DEPTH_SIZE, 24,
                EGL_STENCIL_SIZE, 8,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_NONE
            };

            EGLint numConfigs;
            if (!eglChooseConfig(gDisplay, configAttribs, &m_Config, 1, &numConfigs)) {
                ALOGE("Failed to choose EGL config");
                return false;
            }

            EGLint format;
            eglGetConfigAttrib(gDisplay, m_Config, EGL_NATIVE_VISUAL_ID, &format);
            //NativeWindow_setBuffersGeometry(g_NativeWindow, 0, 0, format);
        }

        // Create context
        const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };

        m_Context = eglCreateContext(gDisplay, m_Config, EGL_NO_CONTEXT, contextAttribs);
        dbp("CreateContext");
        if (m_Context == EGL_NO_CONTEXT) {
            ALOGE("Failed to create EGL context");
            return false;
        }

        // Create surface
        return CreateSurface();
    }

    bool CreateSurface() {
        if (!g_NativeWindow) return false;
        gSurface = eglGetCurrentSurface(EGL_DRAW); 
        if (gSurface == EGL_NO_SURFACE) {
            gSurface = eglCreateWindowSurface(gDisplay, m_Config, (EGLNativeWindowType)g_NativeWindow, NULL);
            dbp("create surface");
        } else dbp("surf %p", gSurface);

        // Make current
        if (!eglMakeCurrent(gDisplay, gSurface, gSurface, m_Context)) {
            ALOGE("Failed to make EGL context current");
            return false;
        }

        return true;
    }

    void DestroySurface() {
        if (gDisplay != EGL_NO_DISPLAY) {
            eglMakeCurrent(gDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (gSurface != EGL_NO_SURFACE) {
                eglDestroySurface(gDisplay, gSurface);
                gSurface = EGL_NO_SURFACE;
            }
        }
    }

    void Terminate() {
        DestroySurface();
        if (gDisplay != EGL_NO_DISPLAY) {
            if (m_Context != EGL_NO_CONTEXT) {
                eglDestroyContext(gDisplay, m_Context);
                m_Context = EGL_NO_CONTEXT;
            }
            eglTerminate(gDisplay);
            gDisplay = EGL_NO_DISPLAY;
        }
    }

    void SwapBuffers() {
        if (gDisplay != EGL_NO_DISPLAY && gSurface != EGL_NO_SURFACE) {
            eglSwapBuffers(gDisplay, gSurface);
        }
    }

    bool MakeCurrent() {
        return eglMakeCurrent(gDisplay, gSurface, gSurface, m_Context);
    }

    bool isCurrent() const {
        return eglGetCurrentContext() == m_Context;
    }
};

EGLConfig EGLContextManager::m_Config = nullptr;

//-----------------------------------------------------------------------------
// Menus
//-----------------------------------------------------------------------------
class Attachable {
public:
    virtual void Attach(JNIEnv *env, jobject menu) = 0;
};

#define MITM_ENABLED 1
#define MITM_CHECKABLE 2
#define MITM_CHECKED 4
class MenuItemImplAndroid final : public MenuItem, public Attachable {
public:
    static std::unordered_map<jint,MenuItemImplAndroid*> item_map;
    static jint nextId;
    jint id, stat = MITM_ENABLED;
    jchar alt = '\0';
    jweak ref = nullptr;
    std::string title;

	~MenuItemImplAndroid() {
        if (item_map.find(id)!=item_map.end()) {
            item_map.erase(id);
        }
        JNIEnv *env = GetJNIEnv();
        if (env && ref) {
            env->DeleteWeakGlobalRef(ref);
            ref = nullptr;
        }
    }

    void SetAccelerator(KeyboardEvent accel) override {
    }

    void SetIndicator(Indicator type) override {
        switch(type) {
            case Indicator::NONE:
                stat &= ~MITM_CHECKABLE;
                break;
            case Indicator::RADIO_MARK:
            case Indicator::CHECK_MARK:
                stat |= MITM_CHECKABLE;
                break;
        }
    }

    void SetActive(bool active) override {
        if (active) {
            stat |= MITM_CHECKED;
        } else {
            stat &= ~MITM_CHECKED;
        }
        JNIEnv *env = GetJNIEnv();
        if (env && ref && !env->IsSameObject(ref, NULL)) {
            env->CallObjectMethod(ref, g_SetChecked, active);
        }
    }

    void SetEnabled(bool enabled) override {
        if(enabled) {
            stat |= MITM_ENABLED;
        } else {
            stat &= ~MITM_ENABLED;
        }
        JNIEnv *env = GetJNIEnv();
        if (env && ref && !env->IsSameObject(ref, NULL)) {
            env->CallObjectMethod(ref, g_SetEnabled, enabled);
        }
    }

    void Attach(JNIEnv *env, jobject menu) override {
        jstring str = env->NewStringUTF(title.c_str());
        jobject item = env->CallObjectMethod(menu, g_Add, 0, id, 0, str);
        env->DeleteLocalRef(str);
        if (alt) {
            // TODO: verify whether the Alt key works
            env->CallObjectMethod(item, g_SetAlphabeticShortcut, alt, AMETA_ALT_ON);
        }
        ref = env->NewWeakGlobalRef(item);
        env->CallObjectMethod(item, g_SetCheckable, 0!=(stat&MITM_CHECKABLE));
        env->CallObjectMethod(item, g_SetEnabled, 0!=(stat&MITM_ENABLED));
        env->CallObjectMethod(item, g_SetChecked, 0!=(stat&MITM_CHECKED));
        env->CallVoidMethod(item, g_SetShowAsAction, 1);
    }

    static TimerRef GetTimer() {
        static TimerRef ref = CreateTimer();
        return ref;
    }
};

std::unordered_map<jint,MenuItemImplAndroid*> MenuItemImplAndroid::item_map;
jint MenuItemImplAndroid::nextId = 1;

class MenuImplAndroid final : public Menu, public Attachable, public std::enable_shared_from_this<MenuImplAndroid> {
public:

    std::vector<std::shared_ptr<Attachable>> menus;
    std::string title;
    jchar alt = '\0';

    MenuImplAndroid() {
    }

    MenuItemRef AddItem(const std::string &label,
                        std::function<void()> onTrigger = NULL,
                        bool mnemonics = true) override {
        auto menuItem = std::make_shared<MenuItemImplAndroid>();
        menuItem->id = MenuItemImplAndroid::nextId++;
        menuItem->onTrigger = onTrigger;
        auto pAlt = label.find('&');
        menuItem->title.assign(label);
        if (pAlt != std::string::npos && pAlt+1<label.size()) {
            menuItem->alt = label[pAlt+1];
            menuItem->title.erase(pAlt, 1);
        }
        MenuItemImplAndroid::item_map[menuItem->id] = menuItem.get();
        menus.push_back(menuItem);
        return menuItem;
    }

    MenuRef AddSubMenu(const std::string &label) override {
        auto subMenu = std::make_shared<MenuImplAndroid>();
        subMenu->title.assign(label);
        auto pAlt = label.find('&');
        if (pAlt != std::string::npos && pAlt+1 < label.size()) {
            subMenu->alt = label[pAlt+1];
            subMenu->title.erase(pAlt, 1);
        }
        menus.push_back(subMenu);
        return subMenu;
    }

    void AddSeparator() override {
      //  Android do not support separator
    }

    void PopUp() override {
        JNIEnv *env = GetJNIEnv();
        if (env && g_Activity) {
            MenuRef ref = shared_from_this();
            MenuRef *pref = new MenuRef(std::move(ref));
            env->CallVoidMethod(g_Activity, g_Popup, (jlong)pref);
        }
    }

    void Clear() override {
        menus.clear();
    }

    void Attach(JNIEnv *env, jobject jmenu) override {
        jstring str = env->NewStringUTF(title.c_str());
        jobject subMenu = env->CallObjectMethod(jmenu, g_AddSubMenu, str);
        env->DeleteLocalRef(str);
        for (const auto& m : menus) {
            m->Attach(env, subMenu);
        }
    }
};

class MenuBarImplAndroid final : public MenuBar {
public:
    std::vector<std::shared_ptr<MenuImplAndroid>>       subMenus;

    MenuRef AddSubMenu(const std::string &label) override {
        auto subMenu = std::make_shared<MenuImplAndroid>();
        auto pAlt = label.find('&');
        subMenu->title.assign(label);
        if (pAlt != std::string::npos && pAlt+1<label.size()) {
            subMenu->alt = label[pAlt+1];
            subMenu->title.erase(pAlt, 1);
        }
        subMenus.push_back(subMenu);

        return subMenu;
    }

    void Clear() override {
       subMenus.clear();
    }
};

MenuRef CreateMenu() {
    return std::make_shared<MenuImplAndroid>();
}

MenuBarRef GetOrCreateMainMenu(bool *unique) {
    static std::shared_ptr<MenuBarImplAndroid> mainMenu;
    if(!mainMenu) {
        mainMenu = std::make_shared<MenuBarImplAndroid>();
    }
    *unique = true;
    return mainMenu;
}

//-----------------------------------------------------------------------------
// Android Window Implementation
//-----------------------------------------------------------------------------
// Global window map (Android typically has one main window)
static int g_NextWindowId = 0;
static int g_CurrentWindow = 0;
extern "C" JNIEXPORT void JNICALL
Java_com_solvespace_SolveSpaceActivity_nativeOnWindowChanged(JNIEnv *env, jclass jc, jint winId);
class WindowImplAndroid final : public Window {
private:
    EGLContextManager m_EGLContext;
    bool m_Visible;
    bool m_FullScreen;
    std::string m_Title;
    MenuBarRef m_MenuBar;
    double m_ScrollMin = 0.0;
    double m_ScrollMax = 1.0;
    double m_ScrollPage = 0.0;
    double m_ScrollPos = 0.0;
    bool m_ScrollVis = false;
    TimerRef m_Inval;

public:
    int id;

    WindowImplAndroid(Window::Kind kind) : 
        m_Visible(false),
        m_FullScreen(false) {
        // Android typically has only one window (the activity)
        JNIEnv *env = GetJNIEnv();
        if (env && g_Activity) {
            env->CallVoidMethod(g_Activity, g_OnWinAdded, kind == Kind::TOPLEVEL);
        }
        m_EGLContext.Initialize();
    }

    ~WindowImplAndroid() override {
        m_EGLContext.Terminate();
    }

    void CreateSurface() {
        m_EGLContext.CreateSurface();
    }

    // Window interface implementation
    double GetDevicePixelRatio() override {
        // Get density from Android display metrics
        JNIEnv* env = GetJNIEnv();
        if (!env || !g_Activity) return 1.0;

        jmethodID getDensity = g_GetDensity;
        if (getDensity) {
            jfloat density = env->CallFloatMethod(g_Activity, getDensity);
            return density;
        }
        return 1.0;
    }

    double GetPixelDensity() override {
        return GetDevicePixelRatio() * 160;
    }

    bool IsVisible() override {
        return m_Visible && g_NativeWindow != nullptr;
    }

    void SetVisible(bool visible) override {
        m_Visible = visible;
        // Visibility is controlled by Android activity lifecycle
    }

    void Focus() override {
        // Android windows are always focused when visible
        JNIEnv *env = GetJNIEnv();
        if (env)
        Java_com_solvespace_SolveSpaceActivity_nativeOnWindowChanged(env, env->GetObjectClass(g_Activity), id);
    }

    bool IsFullScreen() override {
        return m_FullScreen;
    }

    void SetFullScreen(bool fullScreen) override {
        m_FullScreen = fullScreen;
        // Request fullscreen through JNI
        JNIEnv* env = GetJNIEnv();
        if (env && g_Activity) {
            jclass activityClass = env->GetObjectClass(g_Activity);
            jmethodID setFullscreenMethod = env->GetMethodID(activityClass, "setFullScreen", "(Z)V");
            if (setFullscreenMethod) {
                env->CallVoidMethod(g_Activity, setFullscreenMethod, fullScreen);
            }
        }
    }

    void SetTitle(const std::string &title) override {
        m_Title = title;
        JNIEnv* env = GetJNIEnv();
        if (env && g_Activity) {
            jmethodID setTitleMethod = g_SetTitle;
            if (setTitleMethod) {
                jstring jTitle = env->NewStringUTF(title.c_str());
                env->CallVoidMethod(g_Activity, setTitleMethod, id, jTitle);
                env->DeleteLocalRef(jTitle);
            }
        }
    }

    void SetMenuBar(MenuBarRef newMenuBar) override {
        m_MenuBar = newMenuBar;
        if (!m_Visible || id != g_CurrentWindow)
            return;
        JNIEnv *env = GetJNIEnv();
        if (env && g_Activity) {
            env->CallVoidMethod(g_Activity, g_InvalidateMenu);
        }
    }

    void GetContentSize(double *width, double *height) override {
        float pixelRatio = GetDevicePixelRatio();
        *width = ANativeWindow_getWidth(g_NativeWindow) / pixelRatio;
        *height = ANativeWindow_getHeight(g_NativeWindow) / pixelRatio;
    }

    void SetMinContentSize(double width, double height) override {
        // Minimum size is handled by Android
        // dbp("minsize %lf %lf", width, height);
    }

    void FreezePosition(SettingsRef settings, const std::string &key) override {
        // Window position is managed by Android
    }

    void ThawPosition(SettingsRef settings, const std::string &key) override {
        // Window position is managed by Android
    }

    void SetCursor(Cursor cursor) override {
        // Cursors are not typically used on Android touch devices
    }

    void SetTooltip(const std::string &text, double x, double y,
                    double width, double height) override {
        // Tooltips are not commonly used in Android apps
    }

    bool IsEditorVisible() override {
        JNIEnv *env = GetJNIEnv();
        if (!(env && g_Activity)) return false;
        return env->CallBooleanMethod(g_Activity, g_IsEditorVisibleMethod);
    }

    void ShowEditor(double x, double y, double fontHeight, double minWidth,
                    bool isMonospace, const std::string &text) override {
        // Show software keyboard through JNI
        JNIEnv* env = GetJNIEnv();
        if (!(env && g_Activity)) return;
        jstring jText = env->NewStringUTF(text.c_str());
        env->CallVoidMethod(g_Activity, g_ShowEditorMethod, 
                            (float)x, (float)y, (float)fontHeight, (float)minWidth, isMonospace, jText);
        env->DeleteLocalRef(jText);
    }

    void HideEditor() override {
        // Hide software keyboard through JNI
        JNIEnv* env = GetJNIEnv();
        if (!(env && g_Activity)) return;
        env->CallVoidMethod(g_Activity, g_HideEditorMethod);
    }

    void SetScrollbarVisible(bool visible) override {
        m_ScrollVis = visible;
        if (id == g_CurrentWindow)
            UpdateScrollbar();
    }

    void ConfigureScrollbar(double min, double max, double pageSize) override {
        m_ScrollMin = min;
        m_ScrollMax = max;
        m_ScrollPage = pageSize;
        if (id == g_CurrentWindow)
            UpdateScrollbar();
    }

    double GetScrollbarPosition() override {
        return m_ScrollPos;
    }

    void SetScrollbarPosition(double pos) override {
        m_ScrollPos = pos;
        if (id == g_CurrentWindow)
            UpdateScrollbar();
    }

    // Pushes the current scrollbar geometry to the Java layer, which draws the
    // thumb as the SurfaceView foreground (a LayerDrawable item pinned right).
    void UpdateScrollbar() {
        JNIEnv *env = GetJNIEnv();
        if (!(env && g_Activity) || !g_SetScrollbar) return;
        env->CallVoidMethod(g_Activity, g_SetScrollbar,
                            m_ScrollMin, m_ScrollMax, m_ScrollPage,
                            m_ScrollPos, m_ScrollVis);
    }

    void Render() {
        if (m_EGLContext.isCurrent()) {
            if (onRender) {
                onRender();
                m_EGLContext.SwapBuffers();
            }
        }
    }

    void Invalidate() override {
        JNIEnv *env = GetJNIEnv();
        if (env && g_Activity) {
            if (!m_Inval) {
                m_Inval = CreateTimer();
                m_Inval->onTimeout = std::bind(&WindowImplAndroid::Render, this);
            }
            m_Inval->RunAfterNextFrame();
        }
    }

    __inline__ void InvalidateMenu(JNIEnv *env, jobject jmenu) {
        if (!m_MenuBar) return;
        auto menuBar = (MenuBarImplAndroid*)m_MenuBar.get();
        for (auto &menu : menuBar->subMenus) {
            menu->Attach(env, jmenu);
        }
    }


    __inline__ bool MakeCurrent() {
        UpdateScrollbar();
        return m_EGLContext.MakeCurrent();
    }
};
static std::unordered_map<int, std::shared_ptr<WindowImplAndroid>> g_Windows;

//-----------------------------------------------------------------------------
// Window creation
//-----------------------------------------------------------------------------

WindowRef CreateWindow(Window::Kind kind, WindowRef parentWindow) {
    auto window = std::make_shared<WindowImplAndroid>(kind);
    window->id = g_NextWindowId;
    g_Windows[g_NextWindowId++] = window;
    return window;
}

extern "C" JNIEXPORT void JNICALL
Java_com_solvespace_SolveSpaceActivity_nativeInit(JNIEnv* env, jobject thiz, jobject surface, jobject jamgr, jobjectArray strs) {
    g_Activity = env->NewGlobalRef(thiz);
    amgr = AAssetManager_fromJava(env, jamgr);
	g_NativeWindow = ANativeWindow_fromSurface(env, surface);
	ANativeWindow_acquire(g_NativeWindow);
    jsize siz = 0, i = 0;
    if (strs && (siz=env->GetArrayLength(strs))) {
        for (i=0; i<siz; i++) {
            jstring str = (jstring)env->GetObjectArrayElement(strs, i);
            const char *chars = env->GetStringUTFChars(str, nullptr);
            if (chars != nullptr) {
                if (SetLocale(chars)) {
                    i = siz; // after this loop, i = siz+1
                }
                env->ReleaseStringUTFChars(str, chars);
            }
            env->DeleteLocalRef(str);
        }
    }
    if (i <= siz) {
        SetLocale("en_US");
    }
	SS.Init();
}

extern "C" JNIEXPORT void JNICALL
Java_com_solvespace_SolveSpaceActivity_nativeClear(JNIEnv* env, jobject thiz) {
	SS.Clear();
	SK.Clear();
	g_Windows.clear();
	g_NextWindowId = 0;
	g_CurrentWindow = 0;
	MenuItemImplAndroid::nextId = 1;
	MenuItemImplAndroid::item_map.clear();
    env->DeleteGlobalRef(g_Activity);
	g_Activity = nullptr;
	if (g_NativeWindow) {
		ANativeWindow_release(g_NativeWindow);
		g_NativeWindow = nullptr;
	}
	amgr = nullptr;
}

void JNICALL
Java_com_solvespace_SolveSpaceActivity_nativeOnWindowChanged(JNIEnv *env, jclass jc, jint winId) {
    if (winId < g_NextWindowId) {
        g_CurrentWindow = winId;
        auto win = g_Windows[winId];
        win->MakeCurrent();
        win->Invalidate();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_solvespace_SolveSpaceActivity_nativeSetSurface(JNIEnv *env, jclass jc, jobject surf) {
    if (g_NativeWindow) {
        ANativeWindow_release(g_NativeWindow);
    }
    g_NativeWindow = ANativeWindow_fromSurface(env, surf);
    ANativeWindow_acquire(g_NativeWindow);
    if (gSurface != EGL_NO_SURFACE) {
        eglDestroySurface(gDisplay, gSurface);
    }
    eglMakeCurrent(gDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    gSurface = EGL_NO_SURFACE;
    for (auto& win : g_Windows) {
        win.second->CreateSurface();
    }
    Java_com_solvespace_SolveSpaceActivity_nativeOnWindowChanged(env, jc, g_CurrentWindow);
}

static time_t lastDown = 0;
static double lastDist = 0.;
#define DBL_TIMEOUT 300L

extern "C" JNIEXPORT jboolean JNICALL
Java_com_solvespace_SolveSpaceActivity_nativeOnMotionEvent(JNIEnv *env, jclass jc, jint act, jfloat x, jfloat y, jdouble dist, jint button, jint state) {
    if (g_CurrentWindow >= g_NextWindowId) {
        return false;
    }
    auto win = g_Windows[g_CurrentWindow];
    MouseEvent event = {.x = x, .y = y};
    event.shiftDown = state & AMETA_SHIFT_ON;
    event.controlDown = state & AMETA_CTRL_ON;
    switch (act) {
        case AMOTION_EVENT_ACTION_DOWN: {
            struct timespec ts;
            clock_gettime(CLOCK_BOOTTIME, &ts);
            time_t tm = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
            event.type = tm - lastDown < DBL_TIMEOUT ? MouseEvent::Type::DBL_PRESS : MouseEvent::Type::PRESS;
            lastDown = tm;
            lastDist = dist;
            break;
        }
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            lastDist = dist;
            event.type = MouseEvent::Type::PRESS;
            break;
        case AMOTION_EVENT_ACTION_MOVE:
            if (lastDist != 0. && dist != 0.) {
                event.type = MouseEvent::Type::SCROLL_VERT;
                event.scrollDelta = fmax(-2.0, fmin((dist - lastDist) / 25.0, 2.0));
                lastDist = dist;
                auto fun = win->onRender;
                win->onRender = nullptr;
                win->onMouseEvent(event);
                win->onRender = fun;
            }
            event.type = MouseEvent::Type::MOTION;
            break;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
            event.type = MouseEvent::Type::RELEASE;
            lastDist = 0.;
            break;
        case AMOTION_EVENT_ACTION_SCROLL:
            event.type = MouseEvent::Type::SCROLL_VERT;
            event.scrollDelta = dist;
            break;
        case AMOTION_EVENT_ACTION_CANCEL:
        case AMOTION_EVENT_ACTION_OUTSIDE:
            event.type = MouseEvent::Type::LEAVE;
            lastDist = 0.;
            break;
    }
    switch (button) {
        case AMOTION_EVENT_BUTTON_PRIMARY:
            event.button = MouseEvent::Button::LEFT;
            break;
        case AMOTION_EVENT_BUTTON_SECONDARY:
            event.button = MouseEvent::Button::RIGHT;
            break;
        case AMOTION_EVENT_BUTTON_TERTIARY:
            event.button = MouseEvent::Button::MIDDLE;
            break;
        default:
            event.button = MouseEvent::Button::NONE;
    }
    return win->onMouseEvent(event);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_solvespace_SolveSpaceActivity_nativeOnKeyEvent(JNIEnv *env, jclass jc, jint keyact, jint keyenc, jint metastate) {
    if (g_CurrentWindow >= g_NextWindowId) {
        return false;
    }
    if (keyact == 0)
        return false; // key char from getUnicodeChar() would failed sometimes and trigger the command::none
    auto win = g_Windows[g_CurrentWindow];
    KeyboardEvent event = {};
    if (keyact == AKEY_EVENT_ACTION_DOWN)
        event.type = KeyboardEvent::Type::PRESS;
    else if (keyact == AKEY_EVENT_ACTION_UP)
        event.type = KeyboardEvent::Type::RELEASE;
    if (keyenc < 0) {
        event.key = KeyboardEvent::Key::FUNCTION;
        event.num = -keyenc - AKEYCODE_F1 + 1;
    } else {
        event.key = KeyboardEvent::Key::CHARACTER;
        event.chr = keyenc;
    }
    event.shiftDown = metastate & AMETA_SHIFT_ON;
    event.controlDown = metastate & AMETA_CTRL_ON;
    return win->onKeyboardEvent(event);
}

extern "C" JNIEXPORT void JNICALL
Java_com_solvespace_SolveSpaceActivity_nativeOnEditorDone(JNIEnv *env, jclass clz, jstring str) {
    auto win = g_Windows[g_CurrentWindow];
    const char * cstr = env->GetStringUTFChars(str, NULL);
    win->onEditingDone(cstr);
}

extern "C" JNIEXPORT void JNICALL
Java_com_solvespace_SolveSpaceActivity_nativeOnScrollbarAdjusted(JNIEnv *env, jobject thiz, jdouble pos) {
    if (g_CurrentWindow >= g_NextWindowId) {
        return;
    }
    auto win = g_Windows[g_CurrentWindow];
    auto adjust = win->onScrollbarAdjusted;
    if (adjust) {
        adjust(pos);
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_solvespace_SolveSpaceActivity_onCreateOptionsMenu(JNIEnv *env, jobject thiz, jobject menu) {
    int currWin = g_CurrentWindow;
    if (currWin >= g_NextWindowId)
        return false;
    env->CallVoidMethod(menu, g_Clear);
    g_Windows[currWin]->InvalidateMenu(env, menu);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_solvespace_SolveSpaceActivity_onOptionsItemSelected(JNIEnv *env, jobject thiz, jobject jitem) {
    jint id = env->CallIntMethod(jitem, g_GetItemId);
    auto pfunc = MenuItemImplAndroid::item_map.find(id);
    if (pfunc == MenuItemImplAndroid::item_map.end()) {
        return false;
    }
    auto item = pfunc->second;
    if (item->onTrigger){
        auto timer = MenuItemImplAndroid::GetTimer();
        timer->onTimeout = item->onTrigger;
        timer->RunAfter(0);
        return true;
    }
    return false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_solvespace_SolveSpaceActivity_nativeOnCreateContextMenu(JNIEnv *env, jclass clz, jobject jmenu, std::shared_ptr<MenuImplAndroid> *menu) {
	if (menu && menu->get())
        for (auto m:menu->get()->menus)
			m->Attach(env, jmenu);
}

extern "C" JNIEXPORT void JNICALL
Java_com_solvespace_SolveSpaceActivity_nativeOnContextMenuClosed(JNIEnv *env, jclass, std::shared_ptr<MenuImplAndroid> *pref) {
    SS.GW.context.active = false;
    SS.ScheduleShowTW();
    delete pref;
}

//-----------------------------------------------------------------------------
// JNI initialization
//-----------------------------------------------------------------------------

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_JavaVM = vm;

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    // Find our activity class
    jclass activityClass = env->FindClass("com/solvespace/SolveSpaceActivity");
    if (!activityClass) {
        ALOGE("Failed to find activity class");
        return -1;
    }

    // Cache method IDs
    g_IsEditorVisibleMethod = env->GetMethodID(activityClass, "isEditorVisible", "()Z");
    g_ShowEditorMethod = env->GetMethodID(activityClass, "showEditor", 
                                          "(FFFFZLjava/lang/String;)V");
    g_HideEditorMethod = env->GetMethodID(activityClass, "hideEditor", "()V");
    g_ShowMessageDialogMethod = env->GetMethodID(activityClass, "showMessageDialog", 
                                                "(Ljava/lang/String;Ljava/lang/String;I)I");
    g_ShowFileDialogMethod = env->GetMethodID(activityClass, "showFileDialog", 
                                            "(ZLjava/lang/String;[Ljava/lang/String;)Ljava/lang/String;");
	g_SendDelayedMethod = env->GetMethodID(activityClass, "sendDelayed",
										"(JJ)V");
	g_SetTitle = env->GetMethodID(activityClass, "setWinTitle",
								"(ILjava/lang/String;)V");
	g_OnWinAdded = env->GetMethodID(activityClass, "onWinAdded", "(Z)V");
    g_GetPreferences = env->GetMethodID(activityClass, "getPreferences", "(I)Landroid/content/SharedPreferences;");
    g_GetDensity = env->GetMethodID(activityClass, "getDensity", "()F");
    g_SetScrollbar = env->GetMethodID(activityClass, "setScrollbar", "(DDDDZ)V");
	g_Popup = env->GetMethodID(activityClass, "popup", "(J)V");
    g_OpenContentFileMethod = env->GetMethodID(activityClass, "openContentFile",
                                               "(Ljava/lang/String;Ljava/lang/String;)I");
    g_DeleteContentFileMethod = env->GetMethodID(activityClass, "deleteContentFile",
                                                 "(Ljava/lang/String;)Z");
    g_GetInternalStoragePathMethod = env->GetMethodID(activityClass, "getInternalStoragePath",
                                                      "()Ljava/lang/String;");
    g_Finish = env->GetMethodID(activityClass, "finish", "()V");
    g_GetSysFonts = env->GetStaticMethodID(activityClass, "getSysFonts", "()[Ljava/lang/String;");
    g_InvalidateMenu = env->GetMethodID(activityClass, "invalidateOptionsMenu", "()V");

    jclass menuClz = env->FindClass("android/view/Menu");
    ssassert(menuClz != NULL, "Failed to find Menu class");
    g_Add = env->GetMethodID(menuClz, "add", "(IIILjava/lang/CharSequence;)Landroid/view/MenuItem;");
    g_AddSubMenu = env->GetMethodID(menuClz, "addSubMenu", "(Ljava/lang/CharSequence;)Landroid/view/SubMenu;");
    g_Clear = env->GetMethodID(menuClz, "clear", "()V");
    jclass itemClz = env->FindClass("android/view/MenuItem");
    ssassert(itemClz != NULL, "Failed to find MenuItem class");
    g_GetItemId = env->GetMethodID(itemClz, "getItemId", "()I");
    jmethodID setAlphaShort = env->GetMethodID(itemClz, "setAlphabeticShortcut", "(CI)Landroid/view/MenuItem;");
    if (setAlphaShort == nullptr) {
        setAlphaShort = env->GetMethodID(itemClz, "setAlphabeticShortcut", "(C)Landroid/view/MenuItem;");
    }
    g_SetAlphabeticShortcut = setAlphaShort;
    g_SetCheckable = env->GetMethodID(itemClz, "setCheckable", "(Z)Landroid/view/MenuItem;");
    g_SetChecked = env->GetMethodID(itemClz, "setChecked", "(Z)Landroid/view/MenuItem;");
	g_SetEnabled = env->GetMethodID(itemClz, "setEnabled", "(Z)Landroid/view/MenuItem;");
    g_SetShowAsAction = env->GetMethodID(itemClz, "setShowAsAction", "(I)V");
    jclass prefClass = env->FindClass("android/content/SharedPreferences");
    ssassert(prefClass != NULL , "Failed to find SharedPreferences class");
#define BL "("
#define BR ")"
#define JSTR "Ljava/lang/String;"
#define EDIT "Landroid/content/SharedPreferences$Editor;"
    g_GetInt = env->GetMethodID(prefClass, "getInt", BL JSTR "I" BR "I");
    g_GetFloat = env->GetMethodID(prefClass, "getFloat", BL JSTR "F" BR "F");
    g_GetString = env->GetMethodID(prefClass, "getString", BL JSTR JSTR BR JSTR);
    g_Edit = env->GetMethodID(prefClass, "edit", "()" EDIT);
    jclass editClass = env->FindClass("android/content/SharedPreferences$Editor");
    ssassert(editClass != NULL, "Failed to find SharedPrefereces$Editor class");

    g_PutInt = env->GetMethodID(editClass, "putInt", BL JSTR "I" BR EDIT);
    g_PutFloat = env->GetMethodID(editClass, "putFloat", BL JSTR "F" BR EDIT);
    g_PutString = env->GetMethodID(editClass, "putString", BL JSTR JSTR BR EDIT);
    g_Commit = env->GetMethodID(editClass, "commit", "()Z");
#undef BL
#undef BR
#undef JSTR
#undef EDIT
    return  JNI_VERSION_1_6;
}

//-----------------------------------------------------------------------------
// Android-specific implementations of other platform functions
//-----------------------------------------------------------------------------

// Settings, Timers, Menus, Dialogs etc. would need similar Android implementations
// For brevity, I'm showing the core window implementation

class SettingsImplAndroid final : public Settings {
public:
    jobject prefs;

    SettingsImplAndroid() {
        JNIEnv *env = GetJNIEnv();
        prefs = env->NewGlobalRef(env->CallObjectMethod(g_Activity, g_GetPreferences, 0));
    }

    ~SettingsImplAndroid() {
        JNIEnv *env = GetJNIEnv();
        if (env) {
            env->DeleteGlobalRef(prefs);
        }
    }

    void FreezeInt(const std::string &key, uint32_t value) override {
        JNIEnv *env = GetJNIEnv();
        if (!env) return;
        jobject edit = env->CallObjectMethod(prefs, g_Edit);
        jstring jkey = env->NewStringUTF(key.c_str());
        env->CallObjectMethod(edit, g_PutInt, jkey, value);
        env->CallBooleanMethod(edit, g_Commit);
        env->DeleteLocalRef(jkey);
    }

    uint32_t ThawInt(const std::string &key, uint32_t defaultValue) override {
        JNIEnv *env = GetJNIEnv();
        if (!env) return defaultValue;
        jstring jkey = env->NewStringUTF(key.c_str());
        uint32_t t = env->CallIntMethod(prefs, g_GetInt, jkey, defaultValue);
        env->DeleteLocalRef(jkey);
        return t;
    }

    void FreezeFloat(const std::string &key, double value) override {
        JNIEnv *env = GetJNIEnv();
        if (!env) return;
        jobject edit = env->CallObjectMethod(prefs, g_Edit);
        jstring jkey = env->NewStringUTF(key.c_str());
        env->CallObjectMethod(edit, g_PutFloat, jkey, (float)value);
        env->CallBooleanMethod(edit, g_Commit);
        env->DeleteLocalRef(jkey);
    }

    double ThawFloat(const std::string &key, double defaultValue) override {
        JNIEnv *env = GetJNIEnv();
        if (!env) return defaultValue;
        jstring jkey = env->NewStringUTF(key.c_str());
        double t = env->CallFloatMethod(prefs, g_GetFloat, jkey, (float)defaultValue);
        env->DeleteLocalRef(jkey);
        return t;
    }

    void FreezeString(const std::string &key, const std::string &value) override {
        JNIEnv *env = GetJNIEnv();
        if (!env) return;
        jobject edit = env->CallObjectMethod(prefs, g_Edit);
        jstring jkey = env->NewStringUTF(key.c_str());
        jstring jval = env->NewStringUTF(value.c_str());
        env->CallObjectMethod(edit, g_PutString, jkey, jval);
        env->CallBooleanMethod(edit, g_Commit);
        env->DeleteLocalRef(jkey);
        env->DeleteLocalRef(jval);
    }

    std::string ThawString(const std::string &key, const std::string &defaultValue = "") override {
        JNIEnv *env = GetJNIEnv();
        if (!env) return defaultValue;
        jstring jkey = env->NewStringUTF(key.c_str());
        jstring jdval = env->NewStringUTF(defaultValue.c_str());
        jstring jret = (jstring)env->CallObjectMethod(prefs, g_GetString, jkey, jdval);
        env->DeleteLocalRef(jkey);
        env->DeleteLocalRef(jdval);
        return env->GetStringUTFChars(jret, NULL);
    }
};

SettingsRef GetSettings() {
    static std::shared_ptr<SettingsImplAndroid> settings;
    if(!settings) {
        settings = std::make_shared<SettingsImplAndroid>();
    }
    return settings;
}

class TimerImplAndroid final : public Timer {
private:
    void post(jlong milliseconds) {
        JNIEnv *env = GetJNIEnv();
        if (!(env && g_Activity)) {
            return;
        }
        env->CallVoidMethod(g_Activity, g_SendDelayedMethod, this, milliseconds);
    }
public:
    void RunAfter(unsigned int milliseconds) override {
        post(milliseconds);
    }
    void RunAfterNextFrame() override {
        post(-1L);
    }
};

TimerRef CreateTimer() {
    return std::make_shared<TimerImplAndroid>();
}

extern "C" JNIEXPORT void JNICALL
Java_com_solvespace_MainHandler_nativeRun(JNIEnv* env, jclass clazz, Timer *timer) {
    auto func = timer->onTimeout;
    if (func) func();
}

//-----------------------------------------------------------------------------
// 3DConnexion support
//-----------------------------------------------------------------------------

void Open3DConnexion() {}
void Close3DConnexion() {}
void Request3DConnexionEventsForWindow(WindowRef window) {}

#define BTN_NO 1
#define BTN_YES 2
#define BTN_CANCEL 4
#define BTN_OK 8
//-----------------------------------------------------------------------------
// Message dialogs
//-----------------------------------------------------------------------------
class MessageDialogImplAndroid final : public MessageDialog {
public:
	std::string tit;
	std::string msg;
	int buttons = 0;

    MessageDialogImplAndroid() {
        SetTitle("Message");
    }

    void SetType(Type type) override {
    }

    void SetTitle(std::string title) override {
	   tit = title;
    }

    void SetMessage(std::string message) override {
	   msg = message;
    }

    void SetDescription(std::string description) override {
    }

    void AddButton(std::string _label, Response response, bool isDefault) override {
        int btn = 0;
        switch (response) {
            case Response::OK: btn = BTN_OK; break;
            case Response::YES: btn = BTN_YES; break;
            case Response::NO: btn = BTN_NO; break;
            case Response::CANCEL: btn = BTN_CANCEL; break;
            default: ssassert(false, "Invalid response"); break;
        }
        buttons |= btn;
    }

    Response RunModal() override {
        char buf[20];
        sprintf(buf, "Err btns %d", buttons);
        ssassert(buttons == BTN_OK
            || buttons == (BTN_OK|BTN_CANCEL)
            || buttons == (BTN_YES|BTN_NO)
            || buttons == (BTN_CANCEL|BTN_YES|BTN_NO),
            buf);
		auto env = GetJNIEnv();
		ssassert(env != NULL, "GetEnv Failed");
		auto jtit = env->NewStringUTF(tit.c_str());
		auto jmsg = env->NewStringUTF(msg.c_str());
		int ret = env->CallIntMethod(g_Activity, g_ShowMessageDialogMethod, jtit, jmsg, buttons);
		env->DeleteLocalRef(jtit);
		env->DeleteLocalRef(jmsg);
		switch (ret) {
			case BTN_OK: return Response::OK;
			case BTN_YES: return Response::YES;
			case BTN_NO: return Response::NO;
			case BTN_CANCEL: return Response::CANCEL;
			default: ssassert(false, "Invalid response");break;
		}
    }
};

MessageDialogRef CreateMessageDialog(WindowRef parentWindow) {
    return std::make_shared<MessageDialogImplAndroid>();
}

//-----------------------------------------------------------------------------
// File dialogs
//-----------------------------------------------------------------------------
class FileDialogImplAndroid final : public FileDialog {
public:

    jboolean isSaveDialog;
    std::string uri;
    std::string sugName;
    std::vector<std::string> exts;

    FileDialogImplAndroid(bool isSaveDialog) {
        this->isSaveDialog = isSaveDialog;
    }

    void SetTitle(std::string title) override {
    }

    void SetCurrentName(std::string name) override {
        sugName = name;
    }

    void SetFilename(Path path) override {
        uri = path.raw;
        sugName = path.FileName();
    }

    Path GetFilename() override {
        return Path::From(uri);
    }

    void SuggestFilename(Platform::Path path) override {
       sugName = path.FileName();
    }

    void AddFilter(std::string name, std::vector<std::string> extensions) override {
        exts.insert(exts.end(), extensions.begin(), extensions.end());
    }

    void FreezeChoices(SettingsRef settings, const std::string &key) override {
    }

    void ThawChoices(SettingsRef settings, const std::string &key) override {
    }

    bool RunModal() override {
        JNIEnv *env = GetJNIEnv();
        if (env && g_Activity) {
            jstring tit = nullptr;
            if (isSaveDialog)
                tit = env->NewStringUTF(sugName.empty() ? _("untitled") : sugName.c_str());
            jclass strClz = env->FindClass("java/lang/String");
            jsize siz = (jsize)exts.size();
            jobjectArray strArr = env->NewObjectArray(siz, strClz, nullptr);
            for (jsize i=0; i<siz; i++) {
                jstring jstr = env->NewStringUTF(exts[i].c_str());
                env->SetObjectArrayElement(strArr, i, jstr);
                env->DeleteLocalRef(jstr);
            }
            jstring ret = (jstring)env->CallObjectMethod(g_Activity, g_ShowFileDialogMethod, isSaveDialog, tit, strArr);
            if (tit) env->DeleteLocalRef(tit);
            env->DeleteLocalRef(strArr);
            if (ret) {
                uri = env->GetStringUTFChars(ret, nullptr);
                return true;
            }
        }
        return false;
    }
};

FileDialogRef CreateOpenFileDialog(WindowRef parentWindow) {
    return std::make_shared<FileDialogImplAndroid>(false);
}

FileDialogRef CreateSaveFileDialog(WindowRef parentWindow) {
    return std::make_shared<FileDialogImplAndroid>(true);
}

//-----------------------------------------------------------------------------
// Application-wide APIs
//-----------------------------------------------------------------------------

std::vector<Path> GetFontFiles() {
    std::vector<Path> fonts;
    JNIEnv *env = GetJNIEnv();
    if (!env || !g_Activity) {
        return fonts;
    }
    jobjectArray jfonts = (jobjectArray)env->CallStaticObjectMethod(env->GetObjectClass(g_Activity), g_GetSysFonts);
    jsize length = env->GetArrayLength(jfonts);
    if (length <= 0) {
        return fonts;
    }
    fonts.reserve(length);
    for (jsize i = 0; i < length; ++i) {
        jstring jStr = (jstring)env->GetObjectArrayElement(jfonts, i);

        const char* utfChars = env->GetStringUTFChars(jStr, nullptr);
        if (utfChars != nullptr) {
            fonts.emplace_back(Path::From(utfChars));
            env->ReleaseStringUTFChars(jStr, utfChars);
        }

        env->DeleteLocalRef(jStr);
    }

    return fonts;
}

void OpenInBrowser(const std::string &url) {
    JNIEnv *env = GetJNIEnv();
    if (!(env && g_Activity)) return;

    jmethodID openUrl = env->GetMethodID(env->GetObjectClass(g_Activity), "openUrl", "(Ljava/lang/String;)V");
    if (openUrl) {
        jstring jurl = env->NewStringUTF(url.c_str());
        env->CallVoidMethod(g_Activity, openUrl, jurl);
        env->DeleteLocalRef(jurl);
    }
}

void ExitGui() {
    JNIEnv *env = GetJNIEnv();
    if (env && g_Activity) {
        env->CallVoidMethod(g_Activity, g_Finish);
    }
}

} // namespace Platform
} // namespace SolveSpace
