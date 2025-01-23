#include "ewindow.h"
#include <lib/gui/ewidgetdesktop.h>

DEFINE_REF(eWindow);

eWindow::eWindow(eWidget *parent, int flags)
    : eWidget(parent)
    , m_flags(flags)
    , m_child(0)
    , m_animation_params(0)
{
    m_animation = 0;
}

eWindow::~eWindow()
{
    if (m_child)
        delete m_child;
    if (m_animation_params)
        delete m_animation_params;
}

void eWindow::setTitle(const std::string &string)
{
    m_title = string;
    event(evtTitleChanged);
}

void eWindow::setBackgroundColor(const gRGB &col)
{
    eWidget::setBackgroundColor(col);
}

void eWindow::setAnimation(eGLSAnimationType type, int duration)
{
    if (m_flags & flagNoAnimated)
        return;
        
    if (!m_animation_params)
        m_animation_params = new eGLSAnimationParams();
        
    m_animation_params->type = type;
    m_animation_params->duration = duration;
    
    if (!m_animation)
        m_animation = new eGLSAnimation(this);
        
    m_animation->start(*m_animation_params);
}

void eWindow::clearAnimation()
{
    if (m_animation)
    {
        m_animation->stop();
        m_animation = 0;
    }
}

int eWindow::event(int event, void *data, void *data2)
{
    switch (event)
    {
        case evtPaint:
        {
            ePtr<eWindowStyle> style;
            if (!getStyle(style))
            {
                gPainter &painter = *(gPainter*)data2;
                
                // Draw window background
                painter.clear();
                
                // Draw window frame if needed
                if (!(m_flags & wfNoBorder))
                {
                    style->drawFrame(painter, eRect(ePoint(0, 0), size()), eWindowStyle::frameGeneric);
                }
            }
            return 0;
        }
        case evtReceivedFocus:
        case evtLostFocus:
            break;
        case evtSizeChanged:
        {
            eSize size = *static_cast<eSize*>(data);
            if (m_child)
                m_child->resize(size);
            break;
        }
        default:
            break;
    }
    return eWidget::event(event, data, data2);
}
