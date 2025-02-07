#include "eglsanimation.h"
#include <lib/base/eerror.h>
#include <lib/gdi/grc.h>

// Shader source code
const char* vertexShaderSource = R"(
    attribute vec2 a_position;
    uniform float u_offset;
    void main() {
        gl_Position = vec4(a_position.x + u_offset, a_position.y, 0.0, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    precision mediump float;
    void main() {
        gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Red color
    }
)";

// Quad vertices (two triangles forming a rectangle)
const GLfloat vertices[] = {
    -0.2f, -0.2f,
     0.2f, -0.2f,
    -0.2f,  0.2f,
    
    -0.2f,  0.2f,
     0.2f, -0.2f,
     0.2f,  0.2f
};

DEFINE_REF(eGLSAnimation);

eGLSAnimation::eGLSAnimation(eWidget *widget)
    : m_widget(widget)
    , m_timer(eTimer::create(eApp))
    , m_params()
    , m_current_tick(0)
    , m_total_ticks(0)
    , m_active(false)
    , m_eglDisplay(EGL_NO_DISPLAY)
    , m_eglContext(EGL_NO_CONTEXT)
    , m_eglSurface(EGL_NO_SURFACE)
    , m_program(0)
    , m_vbo(0)
    , m_positionAttrib(0)
    , m_offsetUniform(0)
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
    
    if (!initEGL()) {
        eDebug("[eGLSAnimation] Failed to initialize EGL!");
        return;
    }
}

eGLSAnimation::~eGLSAnimation()
{
    cleanupEGL();
    stop();
}

void eGLSAnimation::timerTick()
{
    if (!m_active || !m_widget) {
        eDebug("[eGLSAnimation] Tick skipped: active=%d, widget=%p", m_active, m_widget);
        stop();
        return;
    }

    m_current_tick++;
    float progress = (float)m_current_tick / m_total_ticks;

    // Apply easing (simple ease-in-out)
    progress = progress < 0.5f ? 2.0f * progress * progress : -1.0f + (4.0f - 2.0f * progress) * progress;

    // Render the frame
    renderFrame(progress);

    if (m_current_tick >= m_total_ticks) {
        eDebug("[eGLSAnimation] Animation finished");
        stop();
        /*emit*/ animationFinished();
    } else {
        // Schedule next tick
        m_timer->start(16); // 16ms for ~60fps
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
    if (m_active) {
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
    if (m_active) {
        eDebug("[eGLSAnimation] Pausing animation");
        m_timer->stop();
        m_active = false;
    }
}

void eGLSAnimation::resume()
{
    if (!m_active && m_current_tick < m_total_ticks) {
        eDebug("[eGLSAnimation] Resuming animation");
        m_timer->start(16);
        m_active = true;
    }
}

bool eGLSAnimation::initEGL()
{
    m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_eglDisplay == EGL_NO_DISPLAY) {
        eDebug("[eGLSAnimation] Failed to get EGL display");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(m_eglDisplay, &major, &minor)) {
        eDebug("[eGLSAnimation] Failed to initialize EGL");
        return false;
    }

    eDebug("[eGLSAnimation] EGL initialized: version %d.%d", major, minor);

    // Choose EGL config
    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };
    EGLint numConfigs;
    if (!eglChooseConfig(m_eglDisplay, configAttribs, &m_eglConfig, 1, &numConfigs)) {
        eDebug("[eGLSAnimation] Failed to choose EGL config");
        return false;
    }

    // Create EGL context
    const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (m_eglContext == EGL_NO_CONTEXT) {
        eDebug("[eGLSAnimation] Failed to create EGL context");
        return false;
    }

    // Create EGL surface
    m_eglSurface = eglCreateWindowSurface(m_eglDisplay, m_eglConfig, 0, NULL);
    if (m_eglSurface == EGL_NO_SURFACE) {
        eDebug("[eGLSAnimation] Failed to create EGL surface");
        return false;
    }

    // Bind context and surface
    if (!eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext)) {
        eDebug("[eGLSAnimation] Failed to make EGL context current");
        return false;
    }

    // Initialize shaders and buffers
    initShaders();

    eDebug("[eGLSAnimation] EGL and shaders initialized successfully");
    return true;
}

void eGLSAnimation::cleanupEGL()
{
    if (m_eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_eglContext != EGL_NO_CONTEXT) {
            eglDestroyContext(m_eglDisplay, m_eglContext);
        }
        if (m_eglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(m_eglDisplay, m_eglSurface);
        }
        eglTerminate(m_eglDisplay);
    }
}

GLuint eGLSAnimation::compileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        eDebug("[eGLSAnimation] Shader Compilation Error: %s", log);
    }
    return shader;
}

void eGLSAnimation::initShaders()
{
    // Compile shaders
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    // Create shader program
    m_program = glCreateProgram();
    glAttachShader(m_program, vertexShader);
    glAttachShader(m_program, fragmentShader);
    glLinkProgram(m_program);
    glUseProgram(m_program);

    // Enable Blending for Transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Get attribute and uniform locations
    m_positionAttrib = glGetAttribLocation(m_program, "a_position");
    m_offsetUniform = glGetUniformLocation(m_program, "u_offset");

    // Create vertex buffer
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Enable vertex attribute
    glEnableVertexAttribArray(m_positionAttrib);
    glVertexAttribPointer(m_positionAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

    // Set clear color for transparent background
    glClearColor(0.0, 0.0, 0.0, 0.0); // Transparent
}


void eGLSAnimation::renderFrame(float progress)
{
    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT);

    // Calculate offset based on progress
    float offset = -1.0f + 2.0f * progress; // Map progress from [0, 1] to [-1, 1]

    // Set the offset uniform
    glUniform1f(m_offsetUniform, offset);

    // Draw the rectangle
    //glDrawArrays(GL_TRIANGLES, 0, 6);

    // Swap buffers
    eglSwapBuffers(m_eglDisplay, m_eglSurface);

    // Log frame rendering
    eDebug("[eGLSAnimation] Frame rendered with progress: %.2f", progress);
}
