#include "eglsanimation.h"
#include <lib/base/init.h>
#include <lib/base/init_num.h>
#include <lib/gdi/grc.h>
#include <cmath>
#include <GLES2/gl2.h> // For OpenGL ES 2.0

DEFINE_REF(eGLSAnimation);

eGLSAnimation::eGLSAnimation(eWidget *widget)
    : m_widget(widget)
    , m_timer(eTimer::create(eApp))
    , m_params()
    , m_current_tick(0)
    , m_total_ticks(0)
    , m_active(false)
    , m_buffer_index(0)
    , m_buffer_ready(false)
    , m_fbo(0) // Initialize m_fbo to 0
    , m_colorTexture(0) // Initialize m_colorTexture to 0
#ifdef HAVE_MALI
    , m_eglDisplay(EGL_NO_DISPLAY)
    , m_eglConfig()
    , m_eglContext(EGL_NO_CONTEXT)
    , m_eglSurface(EGL_NO_SURFACE)
    , m_program(0)
    , m_texture(0)
#endif
{
    if (!m_timer) {
        eDebug("[eGLSAnimation] Failed to create timer!");
        return;
    }
    
    if (!m_widget) {
        eDebug("[eGLSAnimation] Invalid widget!");
        return;
    }
    
    m_timer->timeout.connect(sigc::mem_fun(*this, &eGLSAnimation::timerTick));
    
#ifdef HAVE_MALI
    if (initEGL()) {
        eDebug("[eGLSAnimation] GLES hardware acceleration initialized");
    } else {
        eDebug("[eGLSAnimation] Falling back to software rendering");
        cleanupEGL();
    }
#endif
}

eGLSAnimation::~eGLSAnimation()
{
#ifdef HAVE_MALI
    // Cleanup framebuffer and texture
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_colorTexture) {
        glDeleteTextures(1, &m_colorTexture);
        m_colorTexture = 0;
    }
    cleanupEGL();
#endif
    stop();
}

float eGLSAnimation::calculateEasing(float progress)
{
    switch (m_params.easing)
    {
        case 1: // Linear
            return progress;
        case 2: // Quadratic
            return progress < 0.5f ? 
                2.0f * progress * progress : 
                1.0f - pow(-2.0f * progress + 2.0f, 2) / 2.0f;
        case 3: // Cubic
            return progress < 0.5f ? 
                4.0f * progress * progress * progress : 
                1.0f - pow(-2.0f * progress + 2.0f, 3) / 2.0f;
        case 4: // Quartic
            return progress < 0.5f ? 
                8.0f * progress * progress * progress * progress : 
                1.0f - pow(-2.0f * progress + 2.0f, 4) / 2.0f;
        default:
            return progress;
    }
}

AnimationFrame eGLSAnimation::calculateSlideFrame(float progress)
{
    AnimationFrame frame;
    float easedProgress = calculateEasing(progress);
    
    // Calculate position with sub-pixel precision
    float x = m_params.startPos.x() + (m_params.endPos.x() - m_params.startPos.x()) * easedProgress;
    float y = m_params.startPos.y() + (m_params.endPos.y() - m_params.startPos.y()) * easedProgress;
    
    frame.position = ePoint(round(x), round(y));
    frame.size = m_widget->size();
    frame.valid = true;
    
    return frame;
}

AnimationFrame eGLSAnimation::calculateZoomFrame(float progress)
{
    AnimationFrame frame;
    float easedProgress = calculateEasing(progress);
    
    // Calculate scale
    frame.scale = m_params.startValue + (m_params.endValue - m_params.startValue) * easedProgress;
    frame.scale /= 100.0f;
    
    ePoint widgetPos = m_widget->position();
    eSize widgetSize = m_widget->size();
    
    // Calculate center point
    ePoint center = m_params.center;
    if (center.x() == 0 && center.y() == 0)
    {
        center = ePoint(
            widgetPos.x() + widgetSize.width() / 2,
            widgetPos.y() + widgetSize.height() / 2
        );
    }
    
    // Calculate new dimensions
    int newWidth = round(widgetSize.width() * frame.scale);
    int newHeight = round(widgetSize.height() * frame.scale);
    
    // Calculate position relative to center
    frame.position = ePoint(
        center.x() - (newWidth / 2),
        center.y() - (newHeight / 2)
    );
    
    frame.size = eSize(newWidth, newHeight);
    frame.valid = true;
    
    return frame;
}

AnimationFrame eGLSAnimation::calculateFadeFrame(float progress)
{
    AnimationFrame frame;
    float easedProgress = calculateEasing(progress);
    
    frame.position = m_widget->position();
    frame.size = m_widget->size();
    frame.opacity = m_params.startValue + (m_params.endValue - m_params.startValue) * easedProgress;
    frame.opacity /= 100.0f;
    frame.valid = true;
    
    return frame;
}

void eGLSAnimation::setupDoubleBuffer()
{
    // Get the width and height of the widget
    int width = m_widget->size().width();
    int height = m_widget->size().height();

    // Generate the framebuffer
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Create a texture to render to
    glGenTextures(1, &m_colorTexture);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Attach the texture to the framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);

    // Check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        eDebug("[eGLSAnimation] Framebuffer not complete!");
    }

    // Unbind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void eGLSAnimation::prepareFrameBuffer()
{
    // Clear existing buffer
    m_frame_buffer.clear();
    m_frame_buffer.resize(BUFFER_SIZE);
    m_buffer_index = 0;
    
    // Calculate total frames based on duration and FPS
    size_t totalFrames = static_cast<size_t>((m_params.duration * m_params.fps) / 1000);
    float frameStep = 1.0f / totalFrames;
    
    // Pre-calculate frames
    for (size_t i = 0; i < totalFrames && i < BUFFER_SIZE; i++)
    {
        float progress = i * frameStep;
        AnimationFrame frame;
        
        switch (m_params.type)
        {
            case eGLSAnimationParams::TYPE_SLIDE:
                frame = calculateSlideFrame(progress);
                break;
            case eGLSAnimationParams::TYPE_ZOOM:
                frame = calculateZoomFrame(progress);
                break;
            case eGLSAnimationParams::TYPE_FADE:
                frame = calculateFadeFrame(progress);
                break;
        }
        
        m_frame_buffer[i] = frame;
    }
    
    m_buffer_ready = true;
}

void eGLSAnimation::renderBufferedFrame()
{
    if (!m_buffer_ready || m_buffer_index >= m_frame_buffer.size())
        return;

    const AnimationFrame& frame = m_frame_buffer[m_buffer_index];
    if (!frame.valid)
        return;

    // Bind the framebuffer for off-screen rendering
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glClear(GL_COLOR_BUFFER_BIT);

    // Apply frame properties
    m_widget->move(frame.position);

    if (frame.size.width() > 0 && frame.size.height() > 0)
        m_widget->resize(frame.size);

    // Apply opacity for fade animations
    if (m_params.type == eGLSAnimationParams::TYPE_FADE)
        m_widget->setTransparent(255 * (1.0f - frame.opacity));

    // Unbind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Now render the off-screen texture to the widget's area
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    
    // Use the shader program
    glUseProgram(m_program);

    // Set up vertex data for a quad
    GLfloat vertices[] = {
        // Positions        // Texture Coords
        -1.0f,  1.0f, 0.0f,  // Top Left
         1.0f,  1.0f, 1.0f,  // Top Right
         1.0f, -1.0f, 1.0f,  // Bottom Right
        -1.0f, -1.0f, 0.0f   // Bottom Left
    };

    GLfloat texCoords[] = {
        0.0f, 0.0f,  // Top Left
        1.0f, 0.0f,  // Top Right
        1.0f, 1.0f,  // Bottom Right
        0.0f, 1.0f   // Bottom Left
    };

    // Create and bind a Vertex Buffer Object (VBO) for vertices
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Create and bind a VBO for texture coordinates
    GLuint texVbo;
    glGenBuffers(1, &texVbo);
    glBindBuffer(GL_ARRAY_BUFFER, texVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texCoords), texCoords, GL_STATIC_DRAW);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(1);

    // Draw the quad
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Clean up
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &texVbo);

    m_buffer_index++;
}

void eGLSAnimation::timerTick()
{
    if (!m_active || !m_widget)
    {
        stop();
        return;
    }
    
    // Render current frame
    renderBufferedFrame();
    
    // Check if animation is complete
    if (m_buffer_index >= m_frame_buffer.size())
    {
        stop();
        /*emit*/ animationFinished();
    }
}

void eGLSAnimation::start(const eGLSAnimationParams &params)
{
    if (!m_widget)
        return;
    
    // Stop any existing animation
    stop();
    
    m_params = params;
    m_active = true;
    
    // Prepare animation buffer
    prepareFrameBuffer();
    
    // Calculate timer interval based on FPS
    int interval = 1000 / m_params.fps;
    m_timer->start(interval, false);  // Regular interval for smooth animation
    
    eDebug("[eGLSAnimation] Started animation: type=%d, duration=%d, fps=%d", 
           m_params.type, m_params.duration, m_params.fps);
}

void eGLSAnimation::stop()
{
    m_active = false;
    m_timer->stop();
    m_buffer_ready = false;
    m_frame_buffer.clear();
    m_buffer_index = 0;
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
    if (!m_active && m_buffer_ready)
    {
        m_active = true;
        int interval = 1000 / m_params.fps;
        m_timer->start(interval, false);
    }
}

#ifdef HAVE_MALI
bool eGLSAnimation::initEGL()
{
    eDebug("[eGLSAnimation] initEGL: Getting display...");
    m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_eglDisplay == EGL_NO_DISPLAY) {
        eDebug("[eGLSAnimation] initEGL: Failed to get display");
        return false;
    }

    eDebug("[eGLSAnimation] initEGL: Initializing EGL...");
    EGLint major, minor;
    if (!eglInitialize(m_eglDisplay, &major, &minor)) {
        eDebug("[eGLSAnimation] initEGL: Failed to initialize EGL");
        return false;
    }
    eDebug("[eGLSAnimation] initEGL: EGL version %d.%d", major, minor);

    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    eDebug("[eGLSAnimation] initEGL: Choosing config...");
    EGLint numConfigs;
    if (!eglChooseConfig(m_eglDisplay, configAttribs, &m_eglConfig, 1, &numConfigs)) {
        eDebug("[eGLSAnimation] initEGL: Failed to choose config");
        return false;
    }

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    eDebug("[eGLSAnimation] initEGL: Creating context...");
    m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (m_eglContext == EGL_NO_CONTEXT) {
        eDebug("[eGLSAnimation] initEGL: Failed to create context");
        return false;
    }

    eDebug("[eGLSAnimation] initEGL: Creating shaders...");
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
