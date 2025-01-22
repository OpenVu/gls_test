#include "eglsanimation.h"
#include <lib/base/ebase.h>
#include <lib/gui/ewidget.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

// Add Mali specific headers
#ifdef HAVE_MALI
#include <EGL/fbdev_window.h>
#endif

DEFINE_REF(eGLSAnimation);

eGLSAnimation::eGLSAnimation(eWidget *widget)
    : m_widget(widget)
    , m_currentStep(0)
    , m_totalSteps(0)
    , m_isRunning(false)
#ifdef HAVE_MALI
    , m_display(EGL_NO_DISPLAY)
    , m_surface(EGL_NO_SURFACE)
    , m_context(EGL_NO_CONTEXT)
    , m_program(0)
    , m_texture(0)
#endif
{
    m_timer = eTimer::create(eApp);
    CONNECT(m_timer->timeout, eGLSAnimation::step);

#ifdef HAVE_MALI
    initEGL();
#endif
}

eGLSAnimation::~eGLSAnimation()
{
#ifdef HAVE_MALI
    cleanupEGL();
#endif
}

void eGLSAnimation::start(const AnimationParams &params)
{
    if (m_isRunning)
        stop();

    m_params = params;
    m_currentStep = 0;
    m_totalSteps = m_params.duration / 16; // ~60fps
    m_isRunning = true;

    // Start the animation timer
    m_timer->start(16); // 16ms for ~60fps
}

void eGLSAnimation::stop()
{
    if (m_isRunning)
    {
        m_timer->stop();
        m_isRunning = false;
        m_currentStep = 0;
    }
}

void eGLSAnimation::pause()
{
    if (m_isRunning)
        m_timer->stop();
}

void eGLSAnimation::resume()
{
    if (m_isRunning)
        m_timer->start(16);
}

void eGLSAnimation::step()
{
    if (!m_isRunning || !m_widget)
        return;

    m_currentStep++;
    float progress = static_cast<float>(m_currentStep) / m_totalSteps;

    // Apply the animation based on type
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

    // Invalidate the widget to trigger a redraw
    m_widget->invalidate();

    // Check if animation is complete
    if (m_currentStep >= m_totalSteps)
    {
        stop();
        animationFinished();
    }
}

void eGLSAnimation::applyFade(float progress)
{
    int currentAlpha = m_params.startValue + (m_params.endValue - m_params.startValue) * progress;
    m_widget->setAlpha(currentAlpha);
}

void eGLSAnimation::applySlide(float progress)
{
    ePoint currentPos(
        m_params.startPos.x() + (m_params.endPos.x() - m_params.startPos.x()) * progress,
        m_params.startPos.y() + (m_params.endPos.y() - m_params.startPos.y()) * progress
    );
    m_widget->move(currentPos);
}

void eGLSAnimation::applyZoom(float progress)
{
    float scale = m_params.startValue + (m_params.endValue - m_params.startValue) * progress;
    scale /= 100.0f; // Convert percentage to scale factor
    
    // Apply zoom transformation relative to center point
    ePoint widgetPos = m_widget->position();
    eSize widgetSize = m_widget->size();
    
    int newWidth = widgetSize.width() * scale;
    int newHeight = widgetSize.height() * scale;
    
    // Calculate new position to maintain center point
    int newX = m_params.center.x() - (newWidth / 2);
    int newY = m_params.center.y() - (newHeight / 2);
    
    m_widget->resize(eSize(newWidth, newHeight));
    m_widget->move(ePoint(newX, newY));
}

#ifdef HAVE_MALI
bool eGLSAnimation::initEGL()
{
    m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_display == EGL_NO_DISPLAY)
        return false;

    EGLint major, minor;
    if (!eglInitialize(m_display, &major, &minor))
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

    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(m_display, configAttribs, &config, 1, &numConfigs))
        return false;

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    m_context = eglCreateContext(m_display, config, EGL_NO_CONTEXT, contextAttribs);
    if (m_context == EGL_NO_CONTEXT)
        return false;

    return createShaders();
}

void eGLSAnimation::cleanupEGL()
{
    if (m_display != EGL_NO_DISPLAY)
    {
        if (m_context != EGL_NO_CONTEXT)
        {
            eglDestroyContext(m_display, m_context);
            m_context = EGL_NO_CONTEXT;
        }
        if (m_surface != EGL_NO_SURFACE)
        {
            eglDestroySurface(m_display, m_surface);
            m_surface = EGL_NO_SURFACE;
        }
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
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
