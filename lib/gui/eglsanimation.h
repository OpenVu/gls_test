#ifndef __lib_gui_eglsanimation_h
#define __lib_gui_eglsanimation_h

#include <lib/base/object.h>
#include <lib/gdi/gpixmap.h>
#include <lib/base/etimer.h>
#include <lib/gui/ewidget.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#ifdef HAVE_MALI
#include <EGL/fbdev_window.h>
#endif

class eGLSAnimation: public Object
{
    DECLARE_REF(eGLSAnimation);
public:
    enum AnimationType {
        TYPE_FADE,
        TYPE_SLIDE,
        TYPE_ZOOM
    };

    struct AnimationParams {
        AnimationType type;
        int startValue;
        int endValue;
        int duration;  // in milliseconds
        ePoint startPos;
        ePoint endPos;
        ePoint center;
    };

    eGLSAnimation(eWidget *widget);
    ~eGLSAnimation();

    void start(const AnimationParams &params);
    void stop();
    void pause();
    void resume();
    
    Signal1<void> animationFinished;

private:
    ePtr<eTimer> m_timer;
    eWidget *m_widget;
    AnimationParams m_params;
    int m_currentStep;
    int m_totalSteps;
    bool m_isRunning;

    // EGL objects for Mali
    EGLDisplay m_eglDisplay;
    EGLConfig m_eglConfig;
    EGLContext m_eglContext;
    EGLSurface m_eglSurface;

    void step();
    void applyFade(float progress);
    void applySlide(float progress);
    void applyZoom(float progress);
};
