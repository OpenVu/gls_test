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
		/* ask style manager for current style */
	ePtr<eWindowStyleManager> mgr;
	eWindowStyleManager::getInstance(mgr);

	ePtr<eWindowStyle> style;
	if (mgr)
		mgr->getStyle(desktop->getStyleID(), style);

		/* when there is either no style manager or no style, revert to simple style. */
	if (!style)
		style = new eWindowStyleSimple();

	setStyle(style);

	setZPosition(z); /* must be done before addRootWidget */

		/* we are the parent for the child window. */
		/* as we are in the constructor, this is thread safe. */
	m_child = this;
	m_child = new eWidget(this);
	desktop->addRootWidget(this);
}

eWindow::~eWindow()
{
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
		/* set background color for child, too */
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

void eWindow::setAnimation(eGLSAnimationType type, int duration)
{
    eDebug("[eWindow] Setting animation type=%d, duration=%d", type, duration);
    
    if (!m_animation_params)
    {
        m_animation_params = new eGLSAnimationParams();
    }
    
    // Default values for animation parameters
    m_animation_params->type = type;
    m_animation_params->duration = duration;
    m_animation_params->easing = EASING_SINE_IN_OUT;
    m_animation_params->bounceCount = 3;
    m_animation_params->amplitude = 1.0f;
    
    switch (type)
    {
        case TYPE_FADE:
            m_animation_params->startValue = 0;
            m_animation_params->endValue = 100;
            break;
            
        case TYPE_SLIDE:
            {
                ePoint pos = position();
                m_animation_params->startPos = ePoint(pos.x() - 100, pos.y());  // Slide from left
                m_animation_params->endPos = pos;
            }
            break;
            
        case TYPE_ZOOM:
            m_animation_params->startValue = 50;  // Start at 50% size
            m_animation_params->endValue = 100;   // End at 100% size
            m_animation_params->center = position() + ePoint(size().width() / 2, size().height() / 2);
            break;
            
        case TYPE_ROTATE:
            m_animation_params->startValue = 0;     // Start at 0 degrees
            m_animation_params->endValue = 360;     // Full rotation
            m_animation_params->rotationAngle = 360;
            m_animation_params->center = position() + ePoint(size().width() / 2, size().height() / 2);
            break;
            
        case TYPE_BOUNCE:
            {
                ePoint pos = position();
                m_animation_params->startPos = ePoint(pos.x(), pos.y() - 100);  // Start above
                m_animation_params->endPos = pos;
                m_animation_params->bounceCount = 3;
                m_animation_params->amplitude = 1.2f;
                m_animation_params->easing = EASING_BOUNCE_OUT;
            }
            break;
            
        case TYPE_SHAKE:
            {
                ePoint pos = position();
                m_animation_params->startPos = pos;
                m_animation_params->endPos = pos;
                m_animation_params->amplitude = 1.5f;
                m_animation_params->bounceCount = 5;
                m_animation_params->easing = EASING_ELASTIC_OUT;
            }
            break;
    }
    
    if (!m_animation)
        m_animation = new eGLSAnimation(this);
    
    m_animation->start(*m_animation_params);
}

void eWindow::clearAnimation()
{
    if (m_animation)
    {
        m_animation->stop();
        m_animation = nullptr;
    }
}

int eWindow::event(int event, void *data, void *data2)
{
	switch (event)
	{
	case evtWillChangeSize:
	{
		eSize &new_size = *static_cast<eSize*>(data);
		eSize &offset = *static_cast<eSize*>(data2);
		if (!(m_flags & wfNoBorder))
		{
			ePtr<eWindowStyle> style;
			if (!getStyle(style))
			{
//			eDebug("[eWindow] evtWillChangeSize to %d %d", new_size.width(), new_size.height());
				style->handleNewSize(this, new_size, offset);
			}
		} else
			m_child->resize(new_size);
		break;
	}
	case evtPaint:
	{
		if (!(m_flags & wfNoBorder))
		{
			ePtr<eWindowStyle> style;
			if (!getStyle(style))
			{
				gPainter &painter = *static_cast<gPainter*>(data2);
				style->paintWindowDecoration(this, painter, m_title);
			}
		}
		return 0;
	}
	case evtTitleChanged:
			/* m_visible_region contains, in contrast to m_visible_with_childs,
			   only the decoration. though repainting the whole decoration is bad,
			   repainting the whole window is even worse. */
		invalidate(m_visible_region);
		break;
	default:
		break;
	}
	return eWidget::event(event, data, data2);
}
