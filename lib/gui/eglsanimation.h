#ifndef __lib_gui_eglsanimation_h
#define __lib_gui_eglsanimation_h

#include <lib/base/object.h>
#include <lib/base/ebase.h>
#include <lib/gdi/gpixmap.h>
#include <lib/gui/ewidget.h>

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

class eGLSAnimation : public Object
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
    virtual ~eGLSAnimation();

    void start(const AnimationParams &params);
    void stop();
    void pause();
    void resume();
    void tick();

    bool isRunning() const { return m_active; }

private:
    eWidget *m_widget;
    ePtr<eTimer> m_timer;
    AnimationParams m_params;
    int m_current_tick;
    int m_total_ticks;
    bool m_active;

#ifdef HAVE_MALI
    // EGL objects for Mali
    EGLDisplay m_eglDisplay;
    EGLConfig m_eglConfig;
    EGLContext m_eglContext;
    EGLSurface m_eglSurface;
    GLuint m_program;
    GLuint m_texture;

    bool initEGL();
    void cleanupEGL();
    bool createShaders();
#endif

    void applyFade(float progress);
    void applySlide(float progress);
    void applyZoom(float progress);
};

#endif // __lib_gui_eglsanimation_h
