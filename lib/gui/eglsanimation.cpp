#include "eglsanimation.h"
#include <lib/base/init.h>
#include <lib/base/eerror.h>
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
    if (!initEGL()) {
        eDebug("[eGLSAnimation] Failed to initialize EGL!");
        return;
    }
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
    
    // Render the animation using OpenGL ES 2.0
    renderFrame(progress);
    
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

#ifdef HAVE_MALI
bool eGLSAnimation::initEGL()
{
    m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_eglDisplay == EGL_NO_DISPLAY)
    {
        eDebug("[eGLSAnimation] Failed to get EGL display");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(m_eglDisplay, &major, &minor))
    {
        eDebug("[eGLSAnimation] Failed to initialize EGL");
        return false;
    }

    eDebug("[eGLSAnimation] EGL initialized: version %d.%d", major, minor);
    return true;

    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLint numConfigs;
    if (!eglChooseConfig(m_eglDisplay, configAttribs, &m_eglConfig, 1, &numConfigs))
    {
        eDebug("[eGLSAnimation] Failed to choose EGL config");
        return false;
    }
    eDebug("[eGLSAnimation] EGL config chosen: numConfigs=%d", numConfigs);

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (m_eglContext == EGL_NO_CONTEXT)
    {
        eDebug("[eGLSAnimation] Failed to create EGL context");
        return false;
    }

    // Create a pbuffer surface
    const EGLint pbufferAttribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    m_eglSurface = eglCreatePbufferSurface(m_eglDisplay, m_eglConfig, pbufferAttribs);
    if (m_eglSurface == EGL_NO_SURFACE)
    {
        eDebug("[eGLSAnimation] Failed to create pbuffer surface");
        return false;
    }

    if (!eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext))
    {
        eDebug("[eGLSAnimation] eglMakeCurrent failed");
        return false;
    }

    eDebug("[eGLSAnimation] EGL initialized successfully");
    return true;
}

void eGLSAnimation::cleanupEGL()
{
    if (m_eglDisplay != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_eglContext != EGL_NO_CONTEXT)
        {
            eglDestroyContext(m_eglDisplay, m_eglContext);
        }
        if (m_eglSurface != EGL_NO_SURFACE)
        {
            eglDestroySurface(m_eglDisplay, m_eglSurface);
        }
        eglTerminate(m_eglDisplay);
    }
}

bool eGLSAnimation::createShaders()
{
    // Vertex Shader Source
    const char *vertexShaderSource = R"(
        attribute vec4 a_position;
        uniform float u_scale;
        uniform vec2 u_offset;
        void main() {
            vec4 pos = a_position;
            pos.xy *= u_scale;
            pos.xy += u_offset;
            gl_Position = pos;
        }
    )";

    // Fragment Shader Source
    const char *fragmentShaderSource = R"(
        precision mediump float;
        uniform float u_alpha;
        void main() {
            gl_FragColor = vec4(1.0, 0.0, 0.0, u_alpha);
        }
    )";

    // Compile Vertex Shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // Check Vertex Shader Compilation Status
    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(vertexShader, 512, NULL, log);
        eDebug("[eGLSAnimation] Vertex shader compilation failed: %s", log);
        return false;
    }

    // Compile Fragment Shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // Check Fragment Shader Compilation Status
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(fragmentShader, 512, NULL, log);
        eDebug("[eGLSAnimation] Fragment shader compilation failed: %s", log);
        return false;
    }

    // Create Shader Program
    m_program = glCreateProgram();
    glAttachShader(m_program, vertexShader);
    glAttachShader(m_program, fragmentShader);
    glLinkProgram(m_program);

    // Check Shader Program Linking Status
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetProgramInfoLog(m_program, 512, NULL, log);
        eDebug("[eGLSAnimation] Shader program linking failed: %s", log);
        return false;
    }

    // Clean Up Shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    eDebug("[eGLSAnimation] Shaders created successfully");
    return true;
}

bool eGLSAnimation::createGeometry()
{
    const GLfloat vertices[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
        -0.5f,  0.5f,
         0.5f,  0.5f
    };

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    eDebug("[eGLSAnimation] Geometry created successfully");
    return true;
}

void eGLSAnimation::renderFrame(float progress)
{
    // Log the widget's position
    ePoint pos = m_widget->position();
    eDebug("[eGLSAnimation] Widget position: x=%d, y=%d", pos.x(), pos.y());

    // Force redraw
    m_widget->invalidate();
    
    // Clear the screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Use the shader program
    glUseProgram(m_program);

    // Bind the vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Enable and set up the vertex attribute
    GLint positionAttrib = glGetAttribLocation(m_program, "a_position");
    glEnableVertexAttribArray(positionAttrib);
    glVertexAttribPointer(positionAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

    // Apply animation based on type
    switch (m_params.type)
    {
        case TYPE_FADE:
        {
            float alpha = m_params.startValue + (m_params.endValue - m_params.startValue) * progress;
            glUniform1f(glGetUniformLocation(m_program, "u_alpha"), alpha / 100.0f);
            break;
        }
        case TYPE_SLIDE:
        {
            float offsetX = m_params.startPos.x() + (m_params.endPos.x() - m_params.startPos.x()) * progress;
            float offsetY = m_params.startPos.y() + (m_params.endPos.y() - m_params.startPos.y()) * progress;
            glUniform2f(glGetUniformLocation(m_program, "u_offset"), offsetX, offsetY);
            break;
        }
        case TYPE_ZOOM:
        {
            float scale = m_params.startValue + (m_params.endValue - m_params.startValue) * progress;
            glUniform1f(glGetUniformLocation(m_program, "u_scale"), scale / 100.0f);
            break;
        }
    }

    // Draw the geometry
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Swap buffers to display the rendered frame
    eglSwapBuffers(m_eglDisplay, m_eglSurface);

    eDebug("[eGLSAnimation] Frame rendered with progress: %.2f", progress);
}
#endif
