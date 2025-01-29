#include <lib/gui/ewindow.h>
#include <lib/gui/ewidgetdesktop.h>
#include <lib/gui/ewindowstyle.h>
#include <lib/gui/ewindowstyleskinned.h>
#include <lib/gdi/epng.h>
#include <lib/gui/eglsanimation.h>

eWindow::eWindow(eWidgetDesktop *desktop, int z): eWidget(0)
{
    m_flags = 0;
    m_desktop = desktop;
    m_animation = nullptr;
    
    ePtr<eWindowStyleManager> mgr;
    eWindowStyleManager::getInstance(mgr);
    
    ePtr<eWindowStyle> style;
    if (mgr)
        mgr->getStyle(desktop->getStyleID(), style);
    
    if (!style)
        style = new eWindowStyleSimple();
    
    setStyle(style);
    setZPosition(z);
    
    m_child = this;
    m_child = new eWidget(this);
    desktop->addRootWidget(this);
}

eWindow::~eWindow()
{
    if (m_animation)
    {
        m_animation->stop();
        //delete m_animation;
        m_animation = nullptr;
    }
    m_desktop->removeRootWidget(this);
    m_child->destruct();
}

void eWindow::setTitle(const std::string &string)
{
    if (m_title == string)
        return;
    m_title = string;
    event(evtTitleChanged);
}

std::string eWindow::getTitle() const
{
    return m_title;
}

void eWindow::setBackgroundColor(const gRGB &col)
{
    eWidget::setBackgroundColor(col);
    m_child->setBackgroundColor(col);
}

void eWindow::setFlag(int flags)
{
    m_flags |= flags;
}

void eWindow::clearFlag(int flags)
{
    m_flags &= ~flags;
}

void eWindow::setAnimation(eGLSAnimationParams::AnimationType type, int duration, int fps)
{
    if (!m_animation)
        m_animation = new eGLSAnimation(this);
    
    eGLSAnimationParams params;
    params.type = type;
    params.duration = duration;
    params.fps = fps ? fps : 60;  // Default to 60fps if not specified
    
    // Configure animation parameters based on type
    switch (type)
    {
        case eGLSAnimationParams::TYPE_FADE:
            params.startValue = 0;    // Start fully transparent
            params.endValue = 100;    // End fully opaque
            break;
            
        case eGLSAnimationParams::TYPE_SLIDE:
            {
                ePoint current = position();
                //eSize screen = m_desktop->size();
                
                // Start from left edge of screen
                params.startPos = ePoint(-size().width(), current.y());
                params.endPos = current;
                
                // Set easing for smooth slide
                params.easing = 3;  // Cubic easing
            }
            break;
            
        case eGLSAnimationParams::TYPE_ZOOM:
            {
                params.startValue = 50;     // Start at 50% size
                params.endValue = 100;      // End at 100% size
                
                // Calculate center point for zoom
                ePoint current = position();
                eSize windowSize = size();
                params.center = ePoint(
                    current.x() + (windowSize.width() / 2),
                    current.y() + (windowSize.height() / 2)
                );
                
                // Set easing for smooth zoom
                params.easing = 3;  // Cubic easing
            }
            break;
    }
    
    // Start the buffered animation
    m_animation->start(params);
}

void eWindow::clearAnimation()
{
    if (m_animation)
    {
        m_animation->stop();
        //delete m_animation;
        m_animation = nullptr;
    }
}

int eWindow::event(int event, void *data, void *data2)
{
    switch (event)
    {
        case evtPaint:
        {
            ePtr<eWindowStyle> style;
            getStyle(style);
            
            if (!style)
                return -1;
            
            eWidget::event(event, data, data2);
            
            return 0;
        }
        default:
            return eWidget::event(event, data, data2);
    }
}
