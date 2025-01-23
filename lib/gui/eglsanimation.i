%module eglsanimation

%{
#include "eglsanimation.h"
%}

// Import required SWIG interfaces
%include "std_string.i"
%include "typemaps.i"

// Expose eGLSAnimationType enum
enum eGLSAnimationType {
    TYPE_FADE = 0,
    TYPE_SLIDE = 1,
    TYPE_ZOOM = 2,
    TYPE_ROTATE = 3,
    TYPE_BOUNCE = 4,
    TYPE_SHAKE = 5
};

// Expose eGLSEasingType enum
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
    EASING_CUBIC_IN_OUT = 9,
    EASING_BOUNCE_IN = 10,
    EASING_BOUNCE_OUT = 11,
    EASING_BOUNCE_IN_OUT = 12,
    EASING_ELASTIC_IN = 13,
    EASING_ELASTIC_OUT = 14,
    EASING_ELASTIC_IN_OUT = 15,
    EASING_BACK_IN = 16,
    EASING_BACK_OUT = 17,
    EASING_BACK_IN_OUT = 18
};

// Expose eGLSAnimationParams struct
struct eGLSAnimationParams {
    eGLSAnimationType type;
    eGLSEasingType easing;
    int startValue;
    int endValue;
    int duration;
    ePoint startPos;
    ePoint endPos;
    ePoint center;
    float rotationAngle;
    int bounceCount;
    float amplitude;
};

// Expose eGLSAnimation class
class eGLSAnimation : public iObject
{
public:
    eGLSAnimation(eWidget *widget);
    virtual ~eGLSAnimation();

    void start(const eGLSAnimationParams &params);
    void stop();
    void pause();
    void resume();
    void chain(eGLSAnimation *nextAnimation);
    void clearChain();
    eWidget *getWidget() const;
    bool isActive() const;
    const eGLSAnimationParams &getParams() const;

    Signal0<void> animationFinished;
};
