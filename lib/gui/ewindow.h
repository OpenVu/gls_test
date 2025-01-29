#ifndef __lib_gui_ewindow_h
#define __lib_gui_ewindow_h

#include <lib/gui/ewidget.h>
#include <lib/gui/eglsanimation.h>

class eWidgetDesktop;

class eWindow: public eWidget
{
    friend class eWindowStyle;
public:
    enum {
        wfNoBorder = 1
    };
    
    eWindow(eWidgetDesktop *desktop, int z = 0);
    ~eWindow();
    
    void setTitle(const std::string &string);
    std::string getTitle() const;
    
    void setBackgroundColor(const gRGB &col);
    
    void setFlag(int flags);
    void clearFlag(int flags);
    
    // Updated GLS Animation support with buffered system
    void setAnimation(eGLSAnimationParams::AnimationType type, int duration = 500, int fps = 60);
    void clearAnimation();
    
protected:
    int event(int event, void *data = 0, void *data2 = 0);
    
private:
    enum { evtTitleChanged = evtUserWidget };
    eWidget *m_child;
    std::string m_title;
    int m_flags;
    eWidgetDesktop *m_desktop;
    ePtr<eGLSAnimation> m_animation;
};

#endif
