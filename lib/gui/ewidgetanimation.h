#include <lib/gui/ewidgetanimation.h>
#include <lib/gdi/grc.h>

eWidgetAnimation::eWidgetAnimation(eWidget* widget)
    : m_widget(widget)
    , m_type(NONE)
    , m_easing(EASE_LINEAR)
    , m_duration(0)
    , m_startTime(0)
    , m_isRunning(false)
    , m_scale(1.0f)
    , m_opacity(1.0f)
    , m_translation(ePoint(0, 0))
{
}

void eWidgetAnimation::start(AnimationType type, EasingType easing, int duration)
{
    if (m_isRunning) {
        stop();
    }

    m_type = type;
    m_easing = easing;
    m_duration = duration;
    m_startTime = eTimer::getTimestamp();
    m_isRunning = true;

    // Store initial values
    m_startScale = m_scale;
    m_startOpacity = m_opacity;
    m_startTranslation = m_translation;

    // Set target values based on animation type
    switch (m_type) {
        case MOVE:
            // Target translation will be set by setTargetPosition
            break;
        case ZOOM:
            m_targetScale = 2.0f;  // Double the size
            break;
        case FADE_IN:
            m_startOpacity = 0.0f;
            m_targetOpacity = 1.0f;
            break;
        case FADE_OUT:
            m_startOpacity = 1.0f;
            m_targetOpacity = 0.0f;
            break;
        case SLIDE:
            // Target translation will be set by setTargetPosition
            break;
        default:
            break;
    }

    // Start the animation timer
    m_timer = eTimer::create(eApp);
    CONNECT(m_timer->timeout, eWidgetAnimation::tick);
    m_timer->start(16);  // ~60 FPS
}

void eWidgetAnimation::stop()
{
    if (!m_isRunning)
        return;

    m_isRunning = false;
    if (m_timer) {
        m_timer->stop();
        m_timer = nullptr;
    }

    // Reset to final values
    switch (m_type) {
        case FADE_OUT:
            m_opacity = 0.0f;
            break;
        case FADE_IN:
            m_opacity = 1.0f;
            break;
        default:
            break;
    }

    // Update the widget's final state
    updateWidgetState();
}

void eWidgetAnimation::tick()
{
    if (!m_isRunning)
        return;

    uint64_t currentTime = eTimer::getTimestamp();
    float progress = (float)(currentTime - m_startTime) / m_duration;

    if (progress >= 1.0f) {
        stop();
        return;
    }

    // Apply easing function
    float easedProgress = applyEasing(progress);

    // Update current values based on animation type
    switch (m_type) {
        case MOVE:
        case SLIDE:
            m_translation.setX(m_startTranslation.x() + (m_targetTranslation.x() - m_startTranslation.x()) * easedProgress);
            m_translation.setY(m_startTranslation.y() + (m_targetTranslation.y() - m_startTranslation.y()) * easedProgress);
            break;
        case ZOOM:
            m_scale = m_startScale + (m_targetScale - m_startScale) * easedProgress;
            break;
        case FADE_IN:
        case FADE_OUT:
            m_opacity = m_startOpacity + (m_targetOpacity - m_startOpacity) * easedProgress;
            break;
        default:
            break;
    }

    // Update the widget's state
    updateWidgetState();
}

float eWidgetAnimation::applyEasing(float t)
{
    switch (m_easing) {
        case EASE_IN:
            return t * t;
        case EASE_OUT:
            return t * (2 - t);
        case EASE_INOUT:
            return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
        case EASE_LINEAR:
        default:
            return t;
    }
}

void eWidgetAnimation::setTargetPosition(const ePoint& pos)
{
    if (m_type == MOVE || m_type == SLIDE) {
        m_targetTranslation = pos;
    }
}

void eWidgetAnimation::updateWidgetState()
{
    if (!m_widget)
        return;

    // Create an animation update opcode
    gOpcode op;
    op.opcode = gOpcode::updateAnimation;
    op.parm.animation = this;

    // Get the widget's DC and execute the opcode
    gDC *dc = m_widget->getDC();
    if (dc) {
        dc->exec(&op);
    }

    // Invalidate the widget to trigger a redraw
    m_widget->invalidate();
}

bool eWidgetAnimation::isRunning() const
{
    return m_isRunning;
}

float eWidgetAnimation::getScale() const
{
    return m_scale;
}

float eWidgetAnimation::getOpacity() const
{
    return m_opacity;
}

ePoint eWidgetAnimation::getTranslation() const
{
    return m_translation;
}
