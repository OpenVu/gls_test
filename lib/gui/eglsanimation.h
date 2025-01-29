#ifndef __lib_gui_eglsanimation_h
#define __lib_gui_eglsanimation_h

#include <lib/gui/ewidget.h>
#include <lib/base/ebase.h>
#include <lib/base/object.h>
#include <lib/gdi/gpixmap.h>

#ifdef HAVE_MALI
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#endif

// Animation frame structure for buffering
struct AnimationFrame {
    ePoint position;
    eSize size;
    float opacity;
    float scale;
    bool valid;
    
    AnimationFrame() : opacity(1.0f), scale(1.0f), valid(false) {}
};

class eGLSAnimationParams {
public:
    enum {
        TYPE_FADE,
        TYPE_SLIDE,
        TYPE_ZOOM
    };
    
    int type;
    int duration;  // milliseconds
    int fps;       // frames per second
    int easing;    // easing function type
    
    // Slide parameters
    ePoint startPos;
    ePoint endPos;
    
    // Zoom parameters
    int startValue;  // start size in percentage
    int endValue;    // end size in percentage
    ePoint center;   // zoom center point
    
    eGLSAnimationParams() :
        type(TYPE_SLIDE),
        duration(250),
        fps(60),
        easing(3),
        startValue(100),
        endValue(100) {}
};

class eGLSAnimation: public Object {
    DECLARE_REF(eGLSAnimation);
    
public:
    eGLSAnimation(eWidget *widget);
    virtual ~eGLSAnimation();
    
    void start(const eGLSAnimationParams &params);
    void stop();
    void pause();
    void resume();
    
    Signal0<void> animationFinished;
    
private:
    eWidget *m_widget;
    ePtr<eTimer> m_timer;
    eGLSAnimationParams m_params;
    int m_current_tick;
    int m_total_ticks;
    bool m_active;
    
    // Buffer implementation
    static const int BUFFER_SIZE = 60;  // 1 second at 60fps
    std::vector<AnimationFrame> m_frame_buffer;
    int m_buffer_index;
    bool m_buffer_ready;
    
    // Buffer management methods
    void prepareFrameBuffer();
    void renderBufferedFrame();
    float calculateEasing(float progress);
    
    // Frame calculation methods
    AnimationFrame calculateSlideFrame(float progress);
    AnimationFrame calculateZoomFrame(float progress);
    AnimationFrame calculateFadeFrame(float progress);
    
    // Timer callback
    void timerTick();
    
#ifdef HAVE_MALI
    // GLES/EGL members
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
};

#endif // __lib_gui_eglsanimation_h
