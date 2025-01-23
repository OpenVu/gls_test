#include "eglsanimation.h"
#include <lib/base/init.h>
#include <lib/base/init_num.h>
#include <lib/gdi/grc.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Small value for floating point comparisons
#define EPSILON 1e-6f

// Helper function for float comparison
inline bool isNearlyEqual(float a, float b) {
    return std::abs(a - b) < EPSILON;
}

DEFINE_REF(eGLSAnimation);

eGLSAnimation::eGLSAnimation(eWidget *widget)
    : m_widget(widget)
    , m_timer(eTimer::create(eApp))
    , m_current_tick(0)
    , m_total_ticks(0)
    , m_active(false)
    , m_next_animation(0)
{
    CONNECT(m_timer->timeout, eGLSAnimation::timerTick);
}

eGLSAnimation::~eGLSAnimation()
{
    stop();
    m_timer = 0;  // Smart pointer will handle cleanup
}

void eGLSAnimation::start(const eGLSAnimationParams &params)
{
    if (!m_widget)
    {
        eDebug("[eGLSAnimation] No widget set!");
        return;
    }
    
    stop();
    
    m_params = params;
    m_current_tick = 0;
    m_total_ticks = std::max(1, m_params.duration / 16);  // 16ms per frame (60fps)
    m_active = true;
    
    eDebug("[eGLSAnimation] Starting animation type=%d, duration=%d, startValue=%d, endValue=%d",
           (int)params.type, params.duration, params.startValue, params.endValue);
    
    // Store initial position for position-based animations
    if (m_params.type == TYPE_SLIDE || m_params.type == TYPE_BOUNCE || m_params.type == TYPE_SHAKE)
    {
        m_params.startPos = m_widget->position();
    }
    
    // Store center point for rotate/zoom animations
    if (m_params.type == TYPE_ROTATE || m_params.type == TYPE_ZOOM)
    {
        eSize size = m_widget->size();
        m_params.center = m_widget->position() + ePoint(size.width() / 2, size.height() / 2);
    }
    
    m_timer->start(16, true);  // 16ms interval for smooth animation
}

void eGLSAnimation::stop()
{
    if (m_timer)
        m_timer->stop();
    m_active = false;
}

void eGLSAnimation::chain(eGLSAnimation *next)
{
    m_next_animation = next;
}

void eGLSAnimation::onAnimationFinished()
{
    if (m_next_animation)
    {
        m_next_animation->start(m_next_animation->m_params);
    }
    
    /*emit*/ animationFinished();
}

void eGLSAnimation::timerTick()
{
    if (!m_active || !m_widget)
    {
        eDebug("[eGLSAnimation] Tick skipped: active=%d, widget=%p", m_active, m_widget);
        stop();
        return;
    }

    m_current_tick++;
    float progress = (float)m_current_tick / m_total_ticks;
    
    eDebug("[eGLSAnimation] Tick: %d/%d, progress=%.2f", m_current_tick, m_total_ticks, progress);
    
    switch (m_params.type)
    {
        case TYPE_FADE:
            applyFade(progress);
            break;
        case TYPE_SLIDE:
            applySlide(progress);
            break;
        case TYPE_ZOOM:
            applyZoom(progress);
            break;
        case TYPE_ROTATE:
            applyRotate(progress);
            break;
        case TYPE_BOUNCE:
            applyBounce(progress);
            break;
        case TYPE_SHAKE:
            applyShake(progress);
            break;
    }
    
    if (m_current_tick >= m_total_ticks)
    {
        eDebug("[eGLSAnimation] Animation finished");
        stop();
        onAnimationFinished();
    }
    else
    {
        // Schedule next tick only if we haven't finished
        m_timer->start(16);
    }
}

void eGLSAnimation::applyFade(float progress)
{
    if (!m_widget) return;
    
    int opacity = m_params.startValue + (m_params.endValue - m_params.startValue) * progress;
    eDebug("[eGLSAnimation] Fade: progress=%.2f, opacity=%d", progress, opacity);
    m_widget->setTransparent(100 - opacity);  // Convert opacity to transparency (0-100)
}

void eGLSAnimation::applySlide(float progress)
{
    if (!m_widget) return;
    
    int x = m_params.startPos.x() + (m_params.endPos.x() - m_params.startPos.x()) * progress;
    int y = m_params.startPos.y() + (m_params.endPos.y() - m_params.startPos.y()) * progress;
    eDebug("[eGLSAnimation] Slide: progress=%.2f, pos=(%d,%d)", progress, x, y);
    m_widget->move(ePoint(x, y));
    
    // Ensure widget is visible during slide
    if (m_current_tick == 1)
    {
        eDebug("[eGLSAnimation] Making widget visible for slide");
        m_widget->setTransparent(0);
    }
}

void eGLSAnimation::applyZoom(float progress)
{
    if (!m_widget) return;
    
    float scale = (m_params.startValue + (m_params.endValue - m_params.startValue) * progress) / 100.0f;
    
    ePoint widgetPos = m_widget->position();
    eSize widgetSize = m_widget->size();
    
    // Calculate center if not specified
    ePoint center = m_params.center;
    if (center.x() == 0 && center.y() == 0)
    {
        center = ePoint(
            widgetPos.x() + widgetSize.width() / 2,
            widgetPos.y() + widgetSize.height() / 2
        );
    }
    
    // Calculate new position and size
    int newWidth = widgetSize.width() * scale;
    int newHeight = widgetSize.height() * scale;
    int newX = center.x() - (newWidth / 2);
    int newY = center.y() - (newHeight / 2);
    
    eDebug("[eGLSAnimation] Zoom: progress=%.2f, scale=%.2f, size=(%d,%d), pos=(%d,%d)", 
           progress, scale, newWidth, newHeight, newX, newY);
    
    m_widget->resize(eSize(newWidth, newHeight));
    m_widget->move(ePoint(newX, newY));
    
    // Ensure widget is visible during zoom
    if (m_current_tick == 1)
    {
        eDebug("[eGLSAnimation] Making widget visible for zoom");
        m_widget->setTransparent(0);
    }
}

void eGLSAnimation::applyRotate(float progress)
{
    if (!m_widget)
        return;
        
    // Calculate rotation angle
    float angle = m_params.startValue + (m_params.endValue - m_params.startValue) * progress;
    
    // Since we don't have matrix transforms, we'll simulate rotation by adjusting size and position
    ePoint center = m_params.center;
    eSize size = m_widget->size();
    int radius = std::min(size.width(), size.height()) / 2;
    
    // Calculate new position based on rotation
    float rad = angle * M_PI / 180.0f;
    int x = center.x() + radius * cos(rad) - size.width() / 2;
    int y = center.y() + radius * sin(rad) - size.height() / 2;
    
    // Update position
    m_widget->move(ePoint(x, y));
    
    // Update opacity to simulate perspective
    int opacity = 100 * (0.7f + 0.3f * cos(rad));
    m_widget->setTransparent(100 - opacity);
    
    m_widget->invalidate();
    
    eDebug("[eGLSAnimation] Rotate: angle=%.2f, pos=(%d,%d), opacity=%d", angle, x, y, opacity);
}

void eGLSAnimation::applyBounce(float progress)
{
    if (!m_widget) return;
    
    // Get start and end positions
    ePoint start_pos = m_params.startPos;
    ePoint end_pos = m_params.endPos;
    
    // Calculate bounce effect
    float bounce_height = m_params.amplitude * 100; // Maximum bounce height in pixels
    float bounce_phase = progress * M_PI * m_params.bounceCount; // Multiple bounces
    float vertical_offset = bounce_height * (1 - progress) * std::abs(std::sin(bounce_phase));
    
    // Calculate horizontal position with linear interpolation
    int x = start_pos.x() + (end_pos.x() - start_pos.x()) * progress;
    
    // Calculate vertical position with bounce
    int y = start_pos.y() + (end_pos.y() - start_pos.y()) * progress - vertical_offset;
    
    m_widget->move(ePoint(x, y));
    eDebug("[eGLSAnimation] Bounce: pos=(%d,%d), offset=%.2f", x, y, vertical_offset);
}

void eGLSAnimation::applyShake(float progress)
{
    if (!m_widget)
        return;
        
    ePoint base_pos = m_params.startPos;
    float amplitude = m_params.amplitude * 20.0f;  // Scale amplitude for more visible effect
    
    // Calculate shake offset using sine wave
    float shake_offset = sin(progress * M_PI * m_params.bounceCount * 2) * amplitude * (1.0f - progress);
    
    // Apply horizontal shake
    int x = base_pos.x() + shake_offset;
    int y = base_pos.y();
    
    m_widget->move(ePoint(x, y));
    m_widget->invalidate();
    
    eDebug("[eGLSAnimation] Shake: pos=(%d,%d), offset=%.2f", x, y, shake_offset);
}
