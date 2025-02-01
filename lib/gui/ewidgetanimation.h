#ifndef __lib_gui_ewidgetanimation_h
#define __lib_gui_ewidgetanimation_h

#include <lib/base/ebase.h>
#include <lib/gui/ewidget.h>

class eWidgetAnimation: public Object
{
    DECLARE_REF(eWidgetAnimation);

public:
    enum AnimationType {
        NONE,
        MOVE,
        ZOOM,
        FADE_IN,
        FADE_OUT,
        SLIDE
    };

    enum EasingType {
        EASE_LINEAR,
        EASE_IN,
        EASE_OUT,
        EASE_INOUT
    };

    eWidgetAnimation(eWidget* widget);

    void start(AnimationType type, EasingType easing, int duration);
    void stop();
    void setTargetPosition(const ePoint& pos);
    bool isRunning() const;

    // Getters for current animation state
    float getScale() const;
    float getOpacity() const;
    ePoint getTranslation() const;

private:
    eWidget* m_widget;
    ePtr<eTimer> m_timer;

    // Animation properties
    AnimationType m_type;
    EasingType m_easing;
    int m_duration;
    uint64_t m_startTime;
    bool m_isRunning;

    // Animation state
    float m_scale;
    float m_opacity;
    ePoint m_translation;

    // Start values
    float m_startScale;
    float m_startOpacity;
    ePoint m_startTranslation;

    // Target values
    float m_targetScale;
    float m_targetOpacity;
    ePoint m_targetTranslation;

    // Helper methods
    void tick();
    float applyEasing(float t);
    void updateWidgetState();
};

#endif
