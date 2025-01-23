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

float eGLSAnimation::applyEasing(float t)
{
    switch (m_params.easing)
    {
        case EASING_LINEAR:
            return t;
        
        case EASING_SINE_IN:
            return 1.0f - cos((t * M_PI) / 2);
        case EASING_SINE_OUT:
            return sin((t * M_PI) / 2);
        case EASING_SINE_IN_OUT:
            return -(cos(M_PI * t) - 1) / 2;
            
        case EASING_QUAD_IN:
            return t * t;
        case EASING_QUAD_OUT:
            return 1 - (1 - t) * (1 - t);
        case EASING_QUAD_IN_OUT:
            return t < 0.5f ? 2 * t * t : 1 - pow(-2 * t + 2, 2) / 2;
            
        case EASING_CUBIC_IN:
            return t * t * t;
        case EASING_CUBIC_OUT:
            return 1 - pow(1 - t, 3);
        case EASING_CUBIC_IN_OUT:
            return t < 0.5f ? 4 * t * t * t : 1 - pow(-2 * t + 2, 3) / 2;
            
        case EASING_BOUNCE_IN:
            return bounceEaseIn(t);
        case EASING_BOUNCE_OUT:
            return bounceEaseOut(t);
        case EASING_BOUNCE_IN_OUT:
            return bounceEaseInOut(t);
            
        case EASING_ELASTIC_IN:
            return elasticEaseIn(t);
        case EASING_ELASTIC_OUT:
            return elasticEaseOut(t);
        case EASING_ELASTIC_IN_OUT:
            return elasticEaseInOut(t);
            
        case EASING_BACK_IN:
            return backEaseIn(t);
        case EASING_BACK_OUT:
            return backEaseOut(t);
        case EASING_BACK_IN_OUT:
            return backEaseInOut(t);
            
        default:
            return t;
    }
}

float eGLSAnimation::bounceEaseOut(float t) 
{
    const float n1 = 7.5625f;
    const float d1 = 2.75f;
    
    if (t < 1 / d1) {
        return n1 * t * t;
    } else if (t < 2 / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    } else {
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }
}

float eGLSAnimation::bounceEaseIn(float t) 
{
    return 1 - bounceEaseOut(1 - t);
}

float eGLSAnimation::bounceEaseInOut(float t) 
{
    return t < 0.5f
        ? (1 - bounceEaseOut(1 - 2 * t)) / 2
        : (1 + bounceEaseOut(2 * t - 1)) / 2;
}

float eGLSAnimation::elasticEaseIn(float t) 
{
    const float c4 = (2 * M_PI) / 3;
    
    if (isNearlyEqual(t, 0.0f)) return 0;
    if (isNearlyEqual(t, 1.0f)) return 1;
    
    return -pow(2, 10 * t - 10) * sin((t * 10 - 10.75f) * c4);
}

float eGLSAnimation::elasticEaseOut(float t) 
{
    const float c4 = (2 * M_PI) / 3;
    
    if (isNearlyEqual(t, 0.0f)) return 0;
    if (isNearlyEqual(t, 1.0f)) return 1;
    
    return pow(2, -10 * t) * sin((t * 10 - 0.75f) * c4) + 1;
}

float eGLSAnimation::elasticEaseInOut(float t) 
{
    const float c5 = (2 * M_PI) / 4.5f;
    
    if (isNearlyEqual(t, 0.0f)) return 0;
    if (isNearlyEqual(t, 1.0f)) return 1;
    
    return t < 0.5f
        ? -(pow(2, 20 * t - 10) * sin((20 * t - 11.125f) * c5)) / 2
        : (pow(2, -20 * t + 10) * sin((20 * t - 11.125f) * c5)) / 2 + 1;
}

float eGLSAnimation::backEaseIn(float t)
{
    const float c1 = 1.70158f;
    const float c3 = c1 + 1;
    
    return c3 * t * t * t - c1 * t * t;
}

float eGLSAnimation::backEaseOut(float t)
{
    const float c1 = 1.70158f;
    const float c3 = c1 + 1;
    
    return 1 + c3 * pow(t - 1, 3) + c1 * pow(t - 1, 2);
}

float eGLSAnimation::backEaseInOut(float t)
{
    const float c1 = 1.70158f;
    const float c2 = c1 * 1.525f;
    
    return t < 0.5f
        ? (pow(2 * t, 2) * ((c2 + 1) * 2 * t - c2)) / 2
        : (pow(2 * t - 2, 2) * ((c2 + 1) * (t * 2 - 2) + c2) + 2) / 2;
}

eGLSAnimation::eGLSAnimation(eWidget *widget)
    : m_widget(widget)
    , m_timer(eTimer::create(eApp))
    , m_params()
    , m_current_tick(0)
    , m_total_ticks(0)
    , m_active(false)
    , m_nextAnimation(0)
#ifdef HAVE_MALI
    , m_eglDisplay(EGL_NO_DISPLAY)
    , m_eglContext(EGL_NO_CONTEXT)
    , m_eglSurface(EGL_NO_SURFACE)
    , m_program(0)
    , m_texture(0)
#endif
{
    eDebug("[eGLSAnimation] Constructor: widget=%p", widget);
    if (!m_timer) {
        eDebug("[eGLSAnimation] Failed to create timer!");
        return;
    }
    
    if (!m_widget) {
        eDebug("[eGLSAnimation] Invalid widget!");
        return;
    }
    
    m_timer->timeout.connect(sigc::mem_fun(*this, &eGLSAnimation::timerTick));
    eDebug("[eGLSAnimation] Timer connected");
    
#ifdef HAVE_MALI
    initEGL();
#endif
}

void eGLSAnimation::chain(eGLSAnimation *nextAnimation)
{
    m_nextAnimation = nextAnimation;
}

void eGLSAnimation::clearChain()
{
    m_nextAnimation = 0;
}

void eGLSAnimation::onAnimationFinished()
{
    if (m_nextAnimation)
    {
        eDebug("[eGLSAnimation] Starting chained animation");
        m_nextAnimation->start(m_nextAnimation->getParams());
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
    progress = applyEasing(progress);
    
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

void eGLSAnimation::start(const eGLSAnimationParams &params)
{
    eDebug("[eGLSAnimation] Starting animation type=%d, duration=%d, startValue=%d, endValue=%d",
           params.type, params.duration, params.startValue, params.endValue);
           
    if (!m_timer) {
        eDebug("[eGLSAnimation] No timer available!");
        return;
    }
    
    if (!m_widget) {
        eDebug("[eGLSAnimation] No widget available!");
        return;
    }
           
    if (m_active) {
        eDebug("[eGLSAnimation] Stopping previous animation");
        stop();
    }

    m_params = params;
    m_current_tick = 0;
    m_total_ticks = params.duration / 16;  // 60fps
    
    if (m_total_ticks <= 0) {
        eDebug("[eGLSAnimation] Invalid duration, must be > 16ms");
        return;
    }
    
    m_active = true;
    
    // Start timer for animation updates
    eDebug("[eGLSAnimation] Starting timer with interval=16ms, total_ticks=%d", m_total_ticks);
    m_timer->start(16);  // Start with 16ms interval
    eDebug("[eGLSAnimation] Timer started");
}

void eGLSAnimation::stop()
{
    if (m_active)
    {
        eDebug("[eGLSAnimation] Stopping animation");
        if (m_timer) {
            m_timer->stop();
            eDebug("[eGLSAnimation] Timer stopped");
        }
        m_active = false;
        m_current_tick = 0;
    }
}

void eGLSAnimation::pause()
{
    if (m_active)
    {
        eDebug("[eGLSAnimation] Pausing animation");
        m_timer->stop();
        m_active = false;
    }
}

void eGLSAnimation::resume()
{
    if (!m_active && m_current_tick < m_total_ticks)
    {
        eDebug("[eGLSAnimation] Resuming animation");
        m_timer->start(16);
        m_active = true;
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

#ifdef HAVE_MALI
bool eGLSAnimation::initEGL()
{
    m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_eglDisplay == EGL_NO_DISPLAY)
        return false;

    EGLint major, minor;
    if (!eglInitialize(m_eglDisplay, &major, &minor))
        return false;

    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLint numConfigs;
    if (!eglChooseConfig(m_eglDisplay, configAttribs, &m_eglConfig, 1, &numConfigs))
        return false;

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (m_eglContext == EGL_NO_CONTEXT)
        return false;

    return createShaders();
}

void eGLSAnimation::cleanupEGL()
{
    if (m_eglDisplay != EGL_NO_DISPLAY)
    {
        if (m_eglContext != EGL_NO_CONTEXT)
        {
            eglDestroyContext(m_eglDisplay, m_eglContext);
            m_eglContext = EGL_NO_CONTEXT;
        }
        if (m_eglSurface != EGL_NO_SURFACE)
        {
            eglDestroySurface(m_eglDisplay, m_eglSurface);
            m_eglSurface = EGL_NO_SURFACE;
        }
        eglTerminate(m_eglDisplay);
        m_eglDisplay = EGL_NO_DISPLAY;
    }

    if (m_program)
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    if (m_texture)
    {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
}

bool eGLSAnimation::createShaders()
{
    const char *vertexShader =
        "attribute vec4 position;\n"
        "attribute vec2 texcoord;\n"
        "varying vec2 v_texcoord;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "    v_texcoord = texcoord;\n"
        "}\n";

    const char *fragmentShader =
        "precision mediump float;\n"
        "varying vec2 v_texcoord;\n"
        "uniform sampler2D texture;\n"
        "uniform float alpha;\n"
        "void main() {\n"
        "    vec4 color = texture2D(texture, v_texcoord);\n"
        "    gl_FragColor = vec4(color.rgb, color.a * alpha);\n"
        "}\n";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShader, NULL);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShader, NULL);
    glCompileShader(fs);

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return true;
}
#endif

eGLSAnimation::~eGLSAnimation()
{
#ifdef HAVE_MALI
    cleanupEGL();
#endif
    stop();
}
