#include "eglsanimation.h"
#include <lib/base/init.h>
#include <lib/base/init_num.h>
#include <lib/gdi/grc.h>

DEFINE_REF(eGLSAnimation);

eGLSAnimation::eGLSAnimation(eWidget *widget)
    : m_widget(widget)
    , m_timer(eTimer::create(eApp))
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
    CONNECT(m_timer->timeout, eGLSAnimation::tick);
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

void eGLSAnimation::start(const eGLSAnimationParams &params)
{
    if (m_active)
        stop();

    m_params = params;
    m_current_tick = 0;
    m_total_ticks = params.duration / 16;  // 60fps
    m_active = true;
    
    // Start timer for animation updates
    m_timer->start(16);  // ~60fps
}

void eGLSAnimation::stop()
{
    if (m_active)
    {
        m_timer->stop();
        m_active = false;
        m_current_tick = 0;
    }
}

void eGLSAnimation::pause()
{
    if (m_active)
    {
        m_timer->stop();
        m_active = false;
    }
}

void eGLSAnimation::resume()
{
    if (!m_active && m_current_tick < m_total_ticks)
    {
        m_timer->start(16);
        m_active = true;
    }
}

void eGLSAnimation::tick()
{
    if (!m_active || !m_widget)
        return;

    m_current_tick++;
    float progress = (float)m_current_tick / m_total_ticks;
    
    // Apply easing (simple ease-in-out)
    progress = progress < 0.5f ? 2.0f * progress * progress : -1.0f + (4.0f - 2.0f * progress) * progress;
    
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
        stop();
        animationFinished();
        return;
    }

    m_timer->start(16);
}

void eGLSAnimation::applyFade(float progress)
{
    int opacity = m_params.startValue + (m_params.endValue - m_params.startValue) * progress;
    m_widget->setTransparent(100 - opacity);  // Convert opacity to transparency (0-100)
}

void eGLSAnimation::applySlide(float progress)
{
    int x = m_params.startPos.x() + (m_params.endPos.x() - m_params.startPos.x()) * progress;
    int y = m_params.startPos.y() + (m_params.endPos.y() - m_params.startPos.y()) * progress;
    m_widget->move(ePoint(x, y));
    
    // Ensure widget is visible during slide
    if (m_current_tick == 1)
        m_widget->setTransparent(0);
}

void eGLSAnimation::applyZoom(float progress)
{
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
    
    m_widget->resize(eSize(newWidth, newHeight));
    m_widget->move(ePoint(newX, newY));
    
    // Ensure widget is visible during zoom
    if (m_current_tick == 1)
        m_widget->setTransparent(0);
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
