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

enum eGLSEasingType {
    EASING_LINEAR = 0,
    EASING_SINE_IN = 1,
    EASING_SINE_OUT = 2,
    EASING_SINE_IN_OUT = 3,
    EASING_QUAD_IN = 4,
    EASING_QUAD_OUT = 5,
    EASING_QUAD_IN_OUT = 6,
    EASING_CUBIC_IN = 7,
    EASING_CUBIC_OUT = 8,
    EASING_CUBIC_IN_OUT = 9
};

struct eGLSAnimationParams {
    eGLSAnimationType type;
    eGLSEasingType easing;
    int startValue;
    int endValue;
    int duration;  // in milliseconds
    ePoint startPos;
    ePoint endPos;
    ePoint center;

    eGLSAnimationParams() : 
        type(TYPE_FADE),
        easing(EASING_SINE_IN_OUT),
        startValue(0),
        endValue(100),
        duration(1000)
    {}
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
    ePtr<eGLSAnimation> m_nextAnimation;  // For animation chaining

#ifdef HAVE_MALI
    EGLDisplay m_eglDisplay;
    EGLContext m_eglContext;
    EGLSurface m_eglSurface;
    GLuint m_program;
    GLuint m_texture;
#endif

    void applyFade(float progress);
    void applySlide(float progress);
    void applyZoom(float progress);
    float applyEasing(float progress);

#ifdef HAVE_MALI
    bool initEGL();
    void cleanupEGL();
    bool createShaders();
#endif

protected:
    void timerTick();
    void onAnimationFinished();

public:
    eGLSAnimation(eWidget *widget);
    virtual ~eGLSAnimation();

    void start(const eGLSAnimationParams &params);
    void stop();
    void pause();
    void resume();

    // Animation chaining
    void chain(eGLSAnimation *nextAnimation);
    void clearChain();

    // Getters
    eWidget *getWidget() const { return m_widget; }
    bool isActive() const { return m_active; }
    const eGLSAnimationParams &getParams() const { return m_params; }

    PSignal0<void> animationFinished;
};

#endif // __lib_gui_eglsanimation_h
