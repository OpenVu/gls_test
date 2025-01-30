#ifndef __lib_gui_ewindow_h
#define __lib_gui_ewindow_h

#include <lib/gui/ewidget.h>
#include <lib/gui/eglsanimation.h>

class eWindow: public eWidget
{
    DECLARE_REF(eWindow);
public:
    enum {
        wfNoBorder = 1,
        flagHasTrans = 2,
        flagNoAnimated = 4
    };
    
    eWindow(eWidget *parent, int flags=0);
    ~eWindow();
    
    void setTitle(const std::string &string);
    std::string getTitle() const { return m_title; }
    void setBackgroundColor(const gRGB &col);
    
    void setFlags(int flags) { m_flags |= flags; }
    void clearFlags(int flags) { m_flags &= ~flags; }
    
    void setAnimation(eGLSAnimationType type, int duration = 500);
    void clearAnimation();
    
protected:
    int event(int event, void *data=0, void *data2=0);
    
private:
    enum eWindowEvents
    {
        evtTitleChanged = evtUserWidget,
    };
    
    ePtr<eGLSAnimation> m_animation;
    eGLSAnimationParams *m_animation_params;
    std::string m_title;
    int m_flags;
    eWidget *m_child;
};

#endif
