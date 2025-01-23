#ifndef __lib_gui_eglsanimation_h
#define __lib_gui_eglsanimation_h

#include <lib/base/object.h>
#include <lib/base/ebase.h>
#include <lib/gui/ewidget.h>
#include <lib/gdi/gpixmap.h>

#ifdef HAVE_MALI
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#else
// Dummy types when no EGL/GLES2 is available
typedef void* EGLDisplay;
typedef void* EGLSurface;
typedef void* EGLContext;
typedef void* EGLConfig;
typedef unsigned int GLuint;
#endif

enum eGLSAnimationType {
    TYPE_FADE = 0,
    TYPE_SLIDE = 1,
    TYPE_ZOOM = 2
};

struct eGLSAnimationParams {
    eGLSAnimationType type;
    int startValue;
    int endValue;
    int duration;  // in milliseconds
    ePoint startPos;
    ePoint endPos;
    ePoint center;

    eGLSAnimationParams() : 
        type(TYPE_FADE),
        startValue(0),
        endValue(100),
        duration(1000) {}
};

class eGLSAnimation : public iObject
{
    DECLARE_REF(eGLSAnimation);

private:
    eWidget *m_widget;
    ePtr<eTimer> m_timer;
    eGLSAnimationParams m_params;
    int m_current_tick;
    int m_total_ticks;
    bool m_active;

#ifdef HAVE_MALI
    EGLDisplay m_eglDisplay;
    EGLConfig m_eglConfig;
    EGLContext m_eglContext;
    EGLSurface m_eglSurface;
    GLuint m_program;
    GLuint m_texture;
#endif

    void applyFade(float progress);
    void applySlide(float progress);
    void applyZoom(float progress);

#ifdef HAVE_MALI
    bool initEGL();
    void cleanupEGL();
    bool createShaders();
#endif

protected:
    void timerTick();

public:
    eGLSAnimation(eWidget *widget);
    virtual ~eGLSAnimation();

    void start(const eGLSAnimationParams &params);
    void stop();
    void pause();
    void resume();

    // Getter for widget
    eWidget *getWidget() const { return m_widget; }
    PSignal0<void> animationFinished;

    bool isRunning() const { return m_active; }
};

#endif // __lib_gui_eglsanimation_h
