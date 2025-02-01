#ifndef __grc_h
#define __grc_h

/*
	gPainter ist die high-level version. die highlevel daten werden zu low level opcodes ueber
	die gRC-queue geschickt und landen beim gDC der hardwarespezifisch ist, meist aber auf einen
	gPixmap aufsetzt (und damit unbeschleunigt ist).
*/

// for debugging use:
// #define SYNC_PAINT
#undef SYNC_PAINT

#include <pthread.h>
#include <stack>
#include <list>
#include <vector>

#include <string>
#include <lib/base/elock.h>
#include <lib/base/message.h>
#include <lib/gdi/erect.h>
#include <lib/gdi/gpixmap.h>
#include <lib/gdi/region.h>
#include <lib/gdi/gfont.h>
#include <lib/gdi/compositing.h>

#if defined(USE_LIBVUGLES2) || defined(HAVE_MALI)
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#endif

class eTextPara;

class gDC;
struct gOpcode
{
    enum Opcode
    {
        renderText,
        renderPara,
        setFont,

        fill,
        fillRegion,
        clear,
        blit,
        gradient,
        rectangle,

        setPalette,
        mergePalette,

        line,

        setBackgroundColor,
        setForegroundColor,

        // Animation opcodes
        setScale,
        setOpacity,
        setTransform,
        startAnimation,
        stopAnimation,
        updateAnimation
    };

    union para
    {
        struct ptext
        {
            eRect area;
            char *text;
            int flags;
            gRGB bordercolor;
            int border;
            int markedpos;
            int *offset;
        } *text;

        struct ppara
        {
            ePoint offset;
            eTextPara *textpara;
        } *para;

        struct pfill
        {
            eRect area;
        } *fill;

        struct pfillRegion
        {
            gRegion region;
        } *fillRegion;

        struct pset
        {
            gColor color;
        } *set;

        struct prect
        {
            eRect area;
        } *rect;

        struct pline
        {
            ePoint start, end;
        } *line;

        struct pblit
        {
            gPixmap *pixmap;
            ePoint position;
            eRect clip;
            int flags;
        } *blit;

        struct pgradient
        {
            std::vector<gRGB> colors;
            uint8_t orientation;
            bool alphablend;
            int fullSize;
        } *gradient;

        struct panimation
        {
            float scale;
            float opacity;
            ePoint translation;
            bool enabled;
        } *animation;
    } parm;

    int opcode;

    gOpcode(int type)
        :opcode(type)
    {
    }
    
    ~gOpcode()
    {
    }
};

#define MAXSIZE 2048

class gRC : public iObject
{
    DECLARE_REF(gRC);
    friend class gPainter;
    static gRC *instance;

#ifndef SYNC_PAINT
    static void *thread_wrapper(void *ptr);
    pthread_t the_thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
#endif

    void *thread();
    
    gOpcode queue[MAXSIZE];
    int rp, wp;

    eFixedMessagePump<int> m_notify_pump;
    void recv_notify(const int &i);
    
    ePtr<gDC> m_spinner_dc;
    int m_spinner_enabled;
    int m_spinneronoff;
    
    void enableSpinner();
    void disableSpinner();

    ePtr<gCompositingData> m_compositing;
    int m_prev_idle_count;

public:
    gRC();
    virtual ~gRC();
    
    void submit(const gOpcode &o);
    sigc::signal<void()> notify;
    
    void setSpinnerDC(gDC *dc) { m_spinner_dc = dc; }
    void setSpinnerOnOff(int onoff) { m_spinneronoff = onoff; }
    
    static gRC *getInstance();
};

class gPainter
{
    ePtr<gDC> m_dc;
    ePtr<gRC> m_rc;
    
    friend class gRC;
    
    gOpcode *beginptr;
    void begin(const eRect &rect);
    void end();

public:
    gPainter(gDC *dc, eRect rect = eRect());
    virtual ~gPainter();
    
    void setBackgroundColor(const gColor &color);
    void setForegroundColor(const gColor &color);
    
    void setBackgroundColor(const gRGB &color);
    void setForegroundColor(const gRGB &color);
    
    void setBorder(const gRGB &borderColor, int width);
    void setGradient(const std::vector<gRGB> &colors, uint8_t orientation, bool alphablend, int fullSize = 0);
    void setRadius(int radius, uint8_t edges);
    
    void setFont(gFont *font);
    
    void renderText(const eRect &position, const std::string &string, int flags = 0, gRGB bordercolor = gRGB(), int border = 0, int markedpos = -1, int *offset = 0);
    void renderPara(eTextPara *para, ePoint offset = ePoint(0, 0));
    
    void fill(const eRect &area);
    void fill(const gRegion &area);
    void clear();
    
    void blitScale(gPixmap *pixmap, const eRect &pos, const eRect &clip=eRect(), int flags=0, int aflags = BT_SCALE);
    void blit(gPixmap *pixmap, ePoint pos, const eRect &clip=eRect(), int flags=0);
    void blit(gPixmap *pixmap, const eRect &pos, const eRect &clip=eRect(), int flags=0);
    
    void drawRectangle(const eRect &area);
    
    void setPalette(gRGB *colors, int start = 0, int len = 256);
    void setPalette(gPixmap *source);
    void mergePalette(gPixmap *target);
    
    void line(ePoint start, ePoint end);
    
    void setOffset(ePoint val);
    void moveOffset(ePoint rel);
    void resetOffset();
    
    void resetClip(const gRegion &clip);
    void clip(const gRegion &clip);
    void clippop();
    
    void waitVSync();
    void flip();
    void notify();
    
    void setCompositing(gCompositingData *comp);
    void flush();
    
    void sendShow(ePoint point, eSize size);
    void sendHide(ePoint point, eSize size);

#if defined(USE_LIBVUGLES2) || defined(HAVE_MALI)
    void sendShowItem(long dir, ePoint point, eSize size);
    void setFlush(bool val);
    void setView(eSize size);
    void updateTransformation(float scale, float opacity, ePoint translation);
#endif
};

class gDC : public iObject
{
    DECLARE_REF(gDC);

protected:
    ePtr<gPixmap> m_pixmap;

#if defined(USE_LIBVUGLES2) || defined(HAVE_MALI)
    // OpenGL ES members
    EGLDisplay m_eglDisplay;
    EGLContext m_eglContext;
    EGLSurface m_eglSurface;
    GLuint m_shaderProgram;
    bool m_glesInitialized;

    // OpenGL ES methods
    void initOpenGLES();
    void cleanupOpenGLES();
    void updateTransformation(float scale, float opacity, ePoint translation);
#endif

    gColor m_foreground_color, m_background_color;
    gRGB m_foreground_color_rgb, m_background_color_rgb;
    
    ePtr<gFont> m_current_font;
    ePoint m_current_offset;
    
    std::vector<gRGB> m_gradient_colors;
    uint8_t m_gradient_orientation;
    bool m_gradient_alphablend;
    int m_gradient_fullSize;
    
    int m_radius;
    uint8_t m_radius_edges;
    
    gRGB m_border_color;
    int m_border_width;
    
    std::stack<gRegion> m_clip_stack;
    gRegion m_current_clip;
    
    ePtr<gPixmap> m_spinner_saved, m_spinner_temp;
    ePtr<gPixmap> *m_spinner_pic;
    eRect m_spinner_pos;
    int m_spinner_num, m_spinner_i;

public:
    virtual void exec(const gOpcode *opcode);
    gDC(gPixmap *pixmap);
    gDC();
    virtual ~gDC();
    
    gRegion &getClip() { return m_current_clip; }
    int getPixmap(ePtr<gPixmap> &pm)
    {
        pm = m_pixmap;
        return 0;
    }
    
    gRGB getRGB(gColor col);
    virtual eSize size() { return m_pixmap->size(); }
    virtual int islocked() const { return 0; }
    
    virtual void enableSpinner();
    virtual void disableSpinner();
    virtual void incrementSpinner();
    virtual void setSpinner(eRect pos, ePtr<gPixmap> *pic, int len);

#if defined(USE_LIBVUGLES2) || defined(HAVE_MALI)
    void updateTransformation(float scale, float opacity, ePoint translation);
    void initOpenGLES();
    void cleanupOpenGLES();
#endif
};

#endif
