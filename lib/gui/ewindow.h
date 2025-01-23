#ifndef __lib_gui_ewindow_h
#define __lib_gui_ewindow_h

#include <lib/gui/ewidget.h>

// Forward declarations
class eGLSAnimation;
struct eGLSAnimationParams;
enum eGLSAnimationType;

class eWindow: public eWidget
{
    DECLARE_REF(eWindow);
public:
    enum {
        wfNoBorder = 1
    };
    eWindow(eWidget *parent, int flags=0);
    ~eWindow();
    void setTitle(const std::string &string);
    void setFlags(int flags);
    void clearFlags(int flags);
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
};

#endif
