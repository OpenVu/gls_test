#include "eglsanimation.h"
#include <lib/base/init.h>
#include <lib/base/init_num.h>
#include <lib/gdi/grc.h>

DEFINE_REF(eGLSAnimation);

eGLSAnimation::eGLSAnimation(eWidget *widget)
    : m_widget(widget)
    , m_timer(eTimer::create(eApp))
    , m_params()
    , m_current_tick(0)
    , m_total_ticks(0)
    , m_active(false)
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

eGLSAnimation::~eGLSAnimation()
{
#ifdef HAVE_MALI
    cleanupEGL();
#endif
    stop();
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
    
    // Apply easing (simple ease-in-out)
    progress = progress < 0.5f ? 2.0f * progress * progress : -1.0f + (4.0f - 2.0f * progress) * progress;
    
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
    }
    
    if (m_current_tick >= m_total_ticks)
    {
        eDebug("[eGLSAnimation] Animation finished");
        stop();
        /*emit*/ animationFinished();
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
    
    // Add smooth easing for fade
    float easedProgress = progress < 0.5f ? 
        2 * progress * progress :
        1 - pow(-2 * progress + 2, 2) / 2;
        
    // Calculate opacity (0-255 range for widget transparency)
    int opacity = round(m_params.startValue + (m_params.endValue - m_params.startValue) * easedProgress);
    
    // Clamp opacity between 0 and 100
    opacity = std::max(0, std::min(100, opacity));
    
    eDebug("[eGLSAnimation] Fade: progress=%.2f, eased=%.2f, opacity=%d", 
           progress, easedProgress, opacity);
           
    // Convert opacity (0-100) to transparency (0-255)
    int transparency = (100 - opacity) * 255 / 100;
    m_widget->setTransparent(transparency);
    
    // Force immediate redraw
    m_widget->invalidate();
}

void eGLSAnimation::applySlide(float progress)
{
    if (!m_widget) return;
    
    // Add cubic easing for smoother slide
    float easedProgress = progress < 0.5f ? 
        4 * progress * progress * progress :
        1 - pow(-2 * progress + 2, 3) / 2;
    
    // Use rounded integer positions to avoid sub-pixel rendering glitches
    int x = round(m_params.startPos.x() + (m_params.endPos.x() - m_params.startPos.x()) * easedProgress);
    int y = round(m_params.startPos.y() + (m_params.endPos.y() - m_params.startPos.y()) * easedProgress);
    
    eDebug("[eGLSAnimation] Slide: progress=%.2f, eased=%.2f, pos=(%d,%d)", 
           progress, easedProgress, x, y);
    
    // Apply position change
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
    
    // Add smoother easing for zoom
    float easedProgress = progress < 0.5f ? 
        4 * progress * progress * progress :
        1 - pow(-2 * progress + 2, 3) / 2;
    
    float scale = (m_params.startValue + (m_params.endValue - m_params.startValue) * easedProgress) / 100.0f;
    
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
    
    // Calculate new position and size with smoother interpolation
    int originalWidth = widgetSize.width() / (m_current_tick == 0 ? 1.0f : scale);
    int originalHeight = widgetSize.height() / (m_current_tick == 0 ? 1.0f : scale);
    int newWidth = originalWidth * scale;
    int newHeight = originalHeight * scale;
    int newX = center.x() - (newWidth / 2);
    int newY = center.y() - (newHeight / 2);
    
    eDebug("[eGLSAnimation] Zoom: progress=%.2f, eased=%.2f, scale=%.2f, size=(%d,%d), pos=(%d,%d)", 
           progress, easedProgress, scale, newWidth, newHeight, newX, newY);
    
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
