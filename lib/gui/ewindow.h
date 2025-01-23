#ifndef __lib_gui_ewindow_h
#define __lib_gui_ewindow_h

#include <lib/gui/ewidget.h>
#include <lib/gui/eglsanimation.h>

class eWidgetDesktop;

class eWindow: public eWidget
{
    friend class eWindowStyle;
public:
    eWindow(eWidgetDesktop *desktop, int z = 0);
    ~eWindow();
    void setTitle(const std::string &string);
    std::string getTitle() const;
    eWidget *child() { return m_child; }

    enum {
        wfNoBorder = 1,
        flagHasTrans = 2,
        flagNoAnimated = 4
    };

    void setBackgroundColor(const gRGB &col);

    void setFlag(int flags);
    void clearFlag(int flags);
    
    // GLS Animation support
    void setAnimation(eGLSAnimationType type, int duration = 500);
    void clearAnimation();

protected:
    enum eWindowEvents
    {
        evtTitleChanged = evtUserWidget,
    };
    int event(int event, void *data=0, void *data2=0);

private:
    std::string m_title;
    eWidget *m_child;
    int m_flags;
    eWidgetDesktop *m_desktop;
    ePtr<eGLSAnimation> m_animation;
    eGLSAnimationParams *m_animation_params;
};

#endif
