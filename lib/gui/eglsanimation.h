#ifndef __lib_gui_eglsanimation_h
#define __lib_gui_eglsanimation_h

#include <lib/base/object.h>
#include <lib/gdi/gpixmap.h>
#include <lib/gui/ewidget.h>
#include <lib/gui/ewindow.h>

enum eGLSAnimationType
{
    TYPE_FADE,
    TYPE_SLIDE,
    TYPE_ZOOM,
    TYPE_ROTATE,
    TYPE_BOUNCE,
    TYPE_SHAKE
};

enum eGLSEasingType
{
    EASING_LINEAR,
    EASING_SINE_IN,
    EASING_SINE_OUT,
    EASING_SINE_IN_OUT,
    EASING_QUAD_IN,
    EASING_QUAD_OUT,
    EASING_QUAD_IN_OUT,
    EASING_CUBIC_IN,
    EASING_CUBIC_OUT,
    EASING_CUBIC_IN_OUT,
    EASING_BOUNCE_IN,
    EASING_BOUNCE_OUT,
    EASING_BOUNCE_IN_OUT,
    EASING_ELASTIC_IN,
    EASING_ELASTIC_OUT,
    EASING_ELASTIC_IN_OUT,
    EASING_BACK_IN,
    EASING_BACK_OUT,
    EASING_BACK_IN_OUT
};

struct eGLSAnimationParams
{
    eGLSAnimationType type;
    eGLSEasingType easing;
    int duration;
    float startValue;
    float endValue;
    ePoint startPos;
    ePoint endPos;
    ePoint center;
    float rotationAngle;
    int bounceCount;
    float amplitude;

    eGLSAnimationParams() : 
        type(TYPE_FADE),
        easing(EASING_SINE_IN_OUT),
        duration(1000),
        startValue(0),
        endValue(100),
        rotationAngle(0),
        bounceCount(3),
        amplitude(1.0f)
    {}
};

class eGLSAnimation: public Object
{
    DECLARE_REF(eGLSAnimation);
    
public:
    eGLSAnimation(eWidget *widget);
    virtual ~eGLSAnimation();
    
    void start(const eGLSAnimationParams &params);
    void stop();
    void chain(eGLSAnimation *next);
    eWidget *getWidget() const { return m_widget; }
    
protected:
    void timerTick();
    float applyEasing(float t);
    
    // Easing functions
    float bounceEaseIn(float t);
    float bounceEaseOut(float t);
    float bounceEaseInOut(float t);
    float elasticEaseIn(float t);
    float elasticEaseOut(float t);
    float elasticEaseInOut(float t);
    float backEaseIn(float t);
    float backEaseOut(float t);
    float backEaseInOut(float t);
    
    // Animation application functions
    void applyFade(float progress);
    void applySlide(float progress);
    void applyZoom(float progress);
    void applyRotate(float progress);
    void applyBounce(float progress);
    void applyShake(float progress);
    
private:
    eWidget *m_widget;
    eTimer *m_timer;
    eGLSAnimationParams m_params;
    int m_current_tick;
    int m_total_ticks;
    bool m_active;
    ePtr<eGLSAnimation> m_next_animation;
    
    void onAnimationFinished();
    bool initEGL();
};

#endif // __lib_gui_eglsanimation_h
