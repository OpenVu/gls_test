#include <lib/gui/elistbox.h>
#include <lib/gui/elistboxcontent.h>
#include <lib/gui/eslider.h>
#include <lib/actions/action.h>

int eListbox::defaultItemRadius[2] = {0,0};
int eListbox::defaultItemRadiusEdges[2] = {0,0};

eListbox::eListbox(eWidget *parent):
	eWidget(parent), m_scrollbar_mode(showNever), m_prev_scrollbar_page(-1),
	m_content_changed(false), m_enabled_wrap_around(false), m_scrollbar_width(10), m_scrollbar_height(10),
	m_top(0), m_left(0), m_selected(0), m_itemheight(25), m_itemwidth(25), m_orientation(orVertical),
	m_items_per_page(0), m_selection_enabled(1), m_native_keys_bound(false), m_scrollbar(nullptr),
	m_columns(1), m_item_spacing(10), m_flex_mode(flexVertical)
{
	memset(static_cast<void*>(&m_style), 0, sizeof(m_style));
	m_style.m_text_offset = ePoint(1,1);
	allowNativeKeys(true);
}

void eListbox::setFlexMode(FlexMode mode)
{
	if (mode <= flexHorizontal)
	{
		int old_mode = m_flex_mode;
		m_flex_mode = mode;
		if (old_mode != mode)
		{
			recalcSize();
			entryReset(false);
		}
	}
}

void eListbox::moveSelection(long dir)
{
	/* refuse to do anything without a valid list. */
	if (!m_content)
		return;
	/* if our list does not have one entry, don't do anything. */
	if (!m_items_per_page || !m_content->size())
		return;
	/* we need the old top/sel to see what we have to redraw */
	int oldtop = m_top;
	int oldsel = m_selected;
	int prevsel = oldsel;
	int newsel;

	switch (dir)
	{
	case moveEnd:
		m_content->cursorEnd();
		[[fallthrough]];
	case moveUp:
		do
		{
			m_content->cursorMove((m_flex_mode == flexGrid) ? -m_columns : -1);
			newsel = m_content->cursorGet();
			if (newsel == prevsel)
			{ // cursorMove reached top and left cursor position the same. Must wrap around ?
				if (m_enabled_wrap_around)
				{
					m_content->cursorEnd();
					m_content->cursorMove(-1);
					newsel = m_content->cursorGet();
				}
				else
				{
					m_content->cursorSet(oldsel);
					break;
				}
			}
			prevsel = newsel;
		}
		while (newsel != oldsel && !m_content->currentCursorSelectable());
		break;
	case moveTop:
		m_content->cursorHome();
		[[fallthrough]];
	case justCheck:
		if (m_content->cursorValid() && m_content->currentCursorSelectable())
			break;
		[[fallthrough]];
	case moveDown:
		do
		{
			m_content->cursorMove((m_flex_mode == flexGrid) ? m_columns : 1);
			if (!m_content->cursorValid())
			{ // cursorMove reached end and left cursor position past the list. Must wrap around ?
				if (m_enabled_wrap_around)
					m_content->cursorHome();
				else
				{
					m_content->cursorSet(oldsel);
					break;
				}
			}
			newsel = m_content->cursorGet();
			if (newsel == prevsel)
			{ // cursorMove reached end and left cursor position the same. Must wrap around ?
				if (m_enabled_wrap_around)
				{
					m_content->cursorHome();
					newsel = m_content->cursorGet();
				}
				else
				{
					m_content->cursorSet(oldsel);
					break;
				}
			}
			prevsel = newsel;
		}
		while (newsel != oldsel && !m_content->currentCursorSelectable());
		break;
	}

	/* now, look wether the current selection is out of screen */
	m_selected = m_content->cursorGet();
	m_top = m_selected - (m_selected % m_items_per_page);

	if (oldsel != m_selected)
		/* emit */ selectionChanged();

	updateScrollBar();

	if (m_top != oldtop)
		invalidate();
	else if (m_selected != oldsel)
	{
		/* redraw the old and newly selected */
		gRegion inv = eRect(0, m_itemheight * (oldsel-m_top), size().width(), m_itemheight);
		inv |= eRect(0, m_itemheight * (m_selected-m_top), size().width(), m_itemheight);
		invalidate(inv);
	}
}

void eListbox::recalcSize()
{
	m_content_changed = true;
	m_prev_scrollbar_page = -1;
	
	if (m_content)
	{
		if (m_flex_mode == flexGrid)
		{
			int available_width = size().width();
			if (m_scrollbar && m_scrollbar_mode == showLeft)
				available_width -= m_scrollbar_width;
			else if (m_scrollbar && m_scrollbar_mode == showRight)
				available_width -= m_scrollbar_width;
				
			m_itemwidth = (available_width - (m_columns - 1) * xoffset) / m_columns;
			m_content->setSize(eSize(m_itemwidth, m_itemheight));
			
			int items_per_row = m_columns;
			int rows = (size().height() + yoffset - 1) / (m_itemheight + yoffset);
			m_items_per_page = items_per_row * rows;
		}
		else if (m_flex_mode == flexHorizontal)
		{
			int available_width = size().width();
			if (m_scrollbar && m_scrollbar_mode == showLeft)
				available_width -= m_scrollbar_width;
			else if (m_scrollbar && m_scrollbar_mode == showRight)
				available_width -= m_scrollbar_width;
				
			m_items_per_page = (available_width + xoffset - 1) / (m_itemwidth + xoffset);
			m_content->setSize(eSize(m_itemwidth, m_itemheight));
		}
		else // flexVertical
		{
			m_content->setSize(eSize(size().width(), m_itemheight));
			m_items_per_page = (size().height() + yoffset - 1) / (m_itemheight + yoffset);
		}
		
		if (m_items_per_page < 0)
			m_items_per_page = 0;
	}
}

void eListbox::setOrientation(int orientation)
{
	m_orientation = orientation;
}

void eListbox::setScrollbarMode(int mode)
{
	m_scrollbar_mode = mode;
	if (m_scrollbar)
	{
		if (m_scrollbar_mode == showNever)
		{
			delete m_scrollbar;
			m_scrollbar=0;
		}
	}
	else
	{
		m_scrollbar = new eSlider(this);
		m_scrollbar->hide();
		m_scrollbar->setBorderWidth(1);
		if (m_orientation == orVertical) {
			m_scrollbar->setOrientation(eSlider::orVertical);
		} else {
			m_scrollbar->setOrientation(eSlider::orHorizontal);
		}
		m_scrollbar->setRange(0,100);
		if (m_scrollbarbackgroundpixmap) m_scrollbar->setBackgroundPixmap(m_scrollbarbackgroundpixmap);
		if (m_scrollbarpixmap) m_scrollbar->setPixmap(m_scrollbarpixmap);
		if (m_style.m_sliderborder_color_set) m_scrollbar->setSliderBorderColor(m_style.m_sliderborder_color);
	}
}

void eListbox::setWrapAround(bool state)
{
	m_enabled_wrap_around = state;
}

void eListbox::setContent(iListboxContent *content)
{
	m_content = content;
	if (content)
		m_content->setListbox(this);
	entryReset();
}

void eListbox::allowNativeKeys(bool allow)
{
	if (m_native_keys_bound != allow)
	{
		ePtr<eActionMap> ptr;
		eActionMap::getInstance(ptr);
		if (allow)
			ptr->bindAction("ListboxActions", 0, 0, this);
		else
			ptr->unbindAction(this, 0);
		m_native_keys_bound = allow;
	}
}

bool eListbox::atBegin()
{
	if (m_content && !m_selected)
		return true;
	return false;
}

bool eListbox::atEnd()
{
	if (m_content && m_content->size() == m_selected+1)
		return true;
	return false;
}

void eListbox::moveToEnd()
{
	if (!m_content)
		return;
	/* move to last existing one ("end" is already invalid) */
	m_content->cursorEnd(); m_content->cursorMove(-1);
	/* current selection invisible? */
	if (m_orientation == orVertical)
	{
		if (m_top + m_items_per_page <= m_content->cursorGet())
		{
			int rest = m_content->size() % m_items_per_page;
			if (rest)
				m_top = m_content->cursorGet() - rest + 1;
			else
				m_top = m_content->cursorGet() - m_items_per_page + 1;
			if (m_top < 0)
				m_top = 0;
		}
	}
	else
	{
		if (m_left + m_items_per_page <= m_content->cursorGet())
		{
			int rest = m_content->size() % m_items_per_page;
			if (rest)
				m_left = m_content->cursorGet() - rest + 1;
			else
				m_left = m_content->cursorGet() - m_items_per_page + 1;
			if (m_left < 0)
				m_left = 0;
		}
	}
}

void eListbox::moveSelectionTo(int index)
{
	if (m_content)
	{
		m_content->cursorSet(index);
		moveSelection(justCheck);
	}
}

int eListbox::getCurrentIndex()
{
	if (m_content && m_content->cursorValid())
		return m_content->cursorGet();
	return 0;
}

int eListbox::getOrientation()
{
	return m_orientation;
}

void eListbox::updateScrollBar()
{
	if (!m_content)
		return;

	int width = size().width();
	int height = size().height();

	if (m_scrollbar_mode == showNever) {
		if (m_orientation == orVertical){
			m_content->setSize(eSize(width, m_itemheight));
		} else {
			m_content->setSize(eSize(m_itemwidth, height));
		}
		return;
	}
	
	int entries = m_content->size();
	if (m_content_changed)
	{
		m_content_changed = false;
		if (m_scrollbar_mode == showLeft || m_scrollbar_mode == showTop)
		{
			if (m_orientation == orVertical) {
				m_content->setSize(eSize(width-m_scrollbar_width-5, m_itemheight));
				m_scrollbar->move(ePoint(0, 0));
				m_scrollbar->resize(eSize(m_scrollbar_width, height));
			} else {
				m_content->setSize(eSize(m_itemwidth, height-m_scrollbar_height-5));
				m_scrollbar->move(ePoint(0, 0));
				m_scrollbar->resize(eSize(width, m_scrollbar_height));
			}
			if (entries > m_items_per_page)
			{
				m_scrollbar->show();
			}
			else
			{
				m_scrollbar->hide();
			}
		}
		else if (entries > m_items_per_page || m_scrollbar_mode == showAlways)
		{
			if (m_orientation == orVertical) {
				if (m_scrollbar_mode != showNever) {
					m_scrollbar->move(ePoint(width-m_scrollbar_width, 0));
					m_scrollbar->resize(eSize(m_scrollbar_width, height));
					m_content->setSize(eSize(width-m_scrollbar_width-5, m_itemheight));
				} else {
					m_content->setSize(eSize(width, m_itemheight));
				}
			} else {
				if (m_scrollbar_mode != showNever) {
					m_scrollbar->move(ePoint(0, height-m_scrollbar_height));
					m_scrollbar->resize(eSize(width, m_scrollbar_height));
					m_content->setSize(eSize(m_itemwidth, height-m_scrollbar_height-5));
				} else {
					m_content->setSize(eSize(m_itemwidth, height));
				}
			}
			if (m_scrollbar_mode != showNever) {
				m_scrollbar->show();
			} else {
				m_scrollbar->hide();
			}
		}
		else
		{
			if (m_orientation == orVertical)
				m_content->setSize(eSize(width, m_itemheight));
			else
				m_content->setSize(eSize(m_itemwidth, height));

			m_scrollbar->hide();
		}
	}
	if (m_items_per_page && entries)
	{
		int topleft = m_orientation == orVertical ? m_top : m_left;

		int curVisiblePage = topleft / m_items_per_page;
		if (m_prev_scrollbar_page != curVisiblePage)
		{
			m_prev_scrollbar_page = curVisiblePage;
			int pages = entries / m_items_per_page;
			if ((pages*m_items_per_page) < entries)
				++pages;
			int start=(topleft*100)/(pages*m_items_per_page);
			int vis=(m_items_per_page*100+pages*m_items_per_page-1)/(pages*m_items_per_page);
			if (vis < 3)
				vis=3;
			m_scrollbar->setStartEnd(start,start+vis);
		}
	}
}

int eListbox::getEntryTop()
{
	if (m_orientation == orVertical)
	{
		return (m_selected - m_top) * m_itemheight;
	}
	else
	{
		return (m_selected - m_left) * m_itemwidth;
	}
}

void eListbox::event(int event, void *data, void *data2)
{
	switch (event)
	{
		case evtPaint:
		{
			ePtr<eWindowStyle> style;

			if (!m_content)
				return eWidget::event(event, data, data2);
			ASSERT(m_content);

			getStyle(style);

			if (!m_content)
				return 0;

			gPainter &painter = *(gPainter*)data2;

			m_content->cursorSave();
			if (m_orientation == orVertical)
			{
				m_content->cursorMove(m_top - m_selected);
			}
			else
			{
				m_content->cursorMove(m_left - m_selected);
			}

			gRegion entryrect = m_orientation == orVertical ? eRect(0, 0, size().width(), m_itemheight) : eRect(0, 0, m_itemwidth, size().height());
			const gRegion &paint_region = *(gRegion*)data;

			if (!isTransparent())
			{
				int cornerRadius = getCornerRadius();
				int cornerRadiusEdges = getCornerRadiusEdges();
				painter.clip(paint_region);
				style->setStyle(painter, eWindowStyle::styleListboxNormal);
				if (m_style.m_background_color_set)
					painter.setBackgroundColor(m_style.m_background_color);
				
				if (cornerRadius && cornerRadiusEdges)
				{
					painter.setRadius(cornerRadius, cornerRadiusEdges);
					painter.drawRectangle(eRect(ePoint(0, 0), size()));
				}
				else
					painter.clear();
				painter.clippop();
			}

			int xoffset = 0;
			int yoffset = 0;
			if (m_scrollbar && m_scrollbar_mode == showLeft)
			{
				xoffset = m_scrollbar->size().width() + 5;
			}

			if (m_scrollbar && m_scrollbar_mode == showTop)
			{
				yoffset = m_scrollbar->size().height() + 5;
			}
			
			if (m_flex_mode == flexGrid)
			{
				for (int i = 0; i < m_items_per_page; ++i)
				{
					int item_index = m_top + i;
					if (item_index >= m_content->size())
						break;
						
					int row = i / m_columns;
					int col = i % m_columns;
					
					int xpos = offset.x() + col * (m_itemwidth + xoffset); // 10 pixels spacing
					int ypos = offset.y() + row * (m_itemheight + yoffset);
					
					ePoint item_offset(xpos, ypos);
					gRegion item_clip(eRect(item_offset, m_content->getSize()));
					
					painter.clip(item_clip);
					m_content->paint(painter, *style, item_offset, item_index == m_selected);
					painter.clippop();
				}
			}
			else
			{
				int items_shown = (m_orientation == orVertical) ? 
					((size().height() + m_itemheight - 1) / m_itemheight) :
					((size().width() + m_itemwidth - 1) / m_itemwidth);
					
				for (int i = 0; i < items_shown; ++i)
				{
					int item_index = (m_orientation == orVertical) ? m_top + i : m_left + i;
					if (item_index >= m_content->size())
						break;
						
					ePoint item_offset = (m_orientation == orVertical) ?
						ePoint(offset.x(), offset.y() + i * m_itemheight) :
						ePoint(offset.x() + i * m_itemwidth, offset.y());
						
					gRegion item_clip(eRect(item_offset, m_content->getSize()));
					painter.clip(item_clip);
					m_content->paint(painter, *style, item_offset, item_index == m_selected);
					painter.clippop();
				}
			}

			// clear/repaint empty/unused space between scrollbar and listboxentrys
			if (m_scrollbar_mode == showLeft)
			{
				if (m_scrollbar)
				{
					style->setStyle(painter, eWindowStyle::styleListboxNormal);
					if (m_scrollbar->isVisible())
					{
						painter.clip(eRect(m_scrollbar->position() + ePoint(m_scrollbar->size().width(), 0), eSize(5,m_scrollbar->size().height())));
					}
					else
					{
						painter.clip(eRect(m_scrollbar->position(), eSize(m_scrollbar->size().width() + 5, m_scrollbar->size().height())));
					}
					painter.clear();
					painter.clippop();
				}
			} else if (m_scrollbar_mode == showTop) {
				if (m_scrollbar)
				{
					style->setStyle(painter, eWindowStyle::styleListboxNormal);
					if (m_scrollbar->isVisible())
					{
						painter.clip(eRect(m_scrollbar->position() + ePoint(0, m_scrollbar->size().height()), eSize(m_scrollbar->size().width(), 5)));
					}
					else
					{
						painter.clip(eRect(m_scrollbar->position(), eSize(m_scrollbar->size().width(), m_scrollbar->size().height() + 5)));
					}
					painter.clear();
					painter.clippop();
				}
			}
			else
			{
				if (m_scrollbar && m_scrollbar->isVisible())
				{
					style->setStyle(painter, eWindowStyle::styleListboxNormal);
					if (m_orientation == orVertical) {
						painter.clip(eRect(m_scrollbar->position() - ePoint(5,0), eSize(5,m_scrollbar->size().height())));
					} else {
						painter.clip(eRect(m_scrollbar->position() - ePoint(0,5), eSize(m_scrollbar->size().width(), 5)));
					}
					painter.clear();
					painter.clippop();
				}
			}

			m_content->cursorRestore();

			return 0;
		}

		case evtChangedSize:
			recalcSize();
			return eWidget::event(event, data, data2);

		case evtAction:
			if (isVisible() && !isLowered())
			{
				moveSelection((long)data2);
				return 1;
			}
			return 0;
		default:
			return eWidget::event(event, data, data2);
	}
}

void eListbox::entryAdded(int index)
{
	if (m_content && (m_content->size() % m_items_per_page) == 1)
		m_content_changed=true;
	/* manage our local pointers. when the entry was added before the current position, we have to advance. */

		/* we need to check <= - when the new entry has the (old) index of the cursor, the cursor was just moved down. */
	if (index <= m_selected)
		++m_selected;
	if (m_orientation == orVertical)
	{
		if (index <= m_top)
			++m_top;
	}
	else
	{
		if (index <= m_left)
			++m_left;
	}

		/* we have to check wether our current cursor is gone out of the screen. */
		/* moveSelection will check for this case */
	moveSelection(justCheck);

		/* now, check if the new index is visible. */
	if (m_orientation == orVertical)
	{
		if ((m_top <= index) && (index < (m_top + m_items_per_page)))
		{
				/* todo, calc exact invalidation... */
			invalidate();
		}
	}
	else
	{
		if ((m_left <= index) && (index < (m_left + m_items_per_page)))
		{
				/* todo, calc exact invalidation... */
			invalidate();
		}
	}
}

void eListbox::entryRemoved(int index)
{
	if (m_content && !(m_content->size() % m_items_per_page))
		m_content_changed=true;

	if (index == m_selected && m_content)
		m_selected = m_content->cursorGet();

	if (m_content && m_content->cursorGet() >= m_content->size())
		moveSelection(moveUp);
	else
		moveSelection(justCheck);
	
	if (m_orientation == orVertical) 
	{
		if ((m_top <= index) && (index < (m_top + m_items_per_page)))
		{
				/* todo, calc exact invalidation... */
			invalidate();
		}
	}
	else
	{
		if ((m_left <= index) && (index < (m_left + m_items_per_page)))
		{
				/* todo, calc exact invalidation... */
			invalidate();
		}
	}
}

void eListbox::entryChanged(int index)
{
	if (m_orientation == orVertical) 
	{
		if ((m_top <= index) && (index < (m_top + m_items_per_page)))
		{
			gRegion inv = eRect(0, m_itemheight * (index-m_top), size().width(), m_itemheight);
			invalidate(inv);
		}
	}
	else
	{
		if ((m_left <= index) && (index < (m_left + m_items_per_page)))
		{
			gRegion inv = eRect(m_itemwidth * (index-m_left), 0, m_itemwidth, size().height());
			invalidate(inv);
		}
	}
}

void eListbox::entryReset(bool selectionHome)
{
	m_content_changed = true;
	m_prev_scrollbar_page = -1;
	int oldsel;

	if (selectionHome)
	{
		if (m_content)
			m_content->cursorHome();
		m_top = 0;
		m_left = 0;
		m_selected = 0;
	}

	if (m_content && (m_selected >= m_content->size()))
	{
		if (m_content->size())
			m_selected = m_content->size() - 1;
		else
			m_selected = 0;
		m_content->cursorSet(m_selected);
	}

	oldsel = m_selected;
	moveSelection(justCheck);
		/* if oldsel != m_selected, selectionChanged was already
		   emitted in moveSelection. we want it in any case, so otherwise,
		   emit it now. */
	if (oldsel == m_selected)
		/* emit */ selectionChanged();
	invalidate();
}

void eListbox::setFont(gFont *font)
{
	m_style.m_font = font;
}

void eListbox::setSecondFont(gFont *font)
{
	m_style.m_secondfont = font;
}

void eListbox::setVAlign(int align)
{
	m_style.m_valign = align;
}

void eListbox::setHAlign(int align)
{
	m_style.m_halign = align;
}

void eListbox::setTextOffset(const ePoint &textoffset)
{
	m_style.m_text_offset = textoffset;
}

void eListbox::setBackgroundColor(gRGB &col)
{
	m_style.m_background_color = col;
	m_style.m_background_color_set = 1;
}

void eListbox::setBackgroundColorSelected(gRGB &col)
{
	m_style.m_background_color_selected = col;
	m_style.m_background_color_selected_set = 1;
}

void eListbox::setForegroundColor(gRGB &col)
{
	m_style.m_foreground_color = col;
	m_style.m_foreground_color_set = 1;
}

void eListbox::setForegroundColorSelected(gRGB &col)
{
	m_style.m_foreground_color_selected = col;
	m_style.m_foreground_color_selected_set = 1;
}

void eListbox::setBorderColor(const gRGB &col)
{
	m_style.m_border_color = col;
}

void eListbox::setBorderWidth(int size)
{
	m_style.m_border_size = size;
	if (m_scrollbar) m_scrollbar->setBorderWidth(size);
}

void eListbox::setScrollbarSliderBorderWidth(int size)
{
	m_style.m_scrollbarsliderborder_size = size;
	m_style.m_scrollbarsliderborder_size_set = 1;
	if (m_scrollbar) m_scrollbar->setSliderBorderWidth(size);
}

void eListbox::setScrollbarWidth(int size)
{
	m_scrollbar_width = size;
}

void eListbox::setScrollbarHeight(int size)
{
	m_scrollbar_height = size;
}

void eListbox::setBackgroundPicture(ePtr<gPixmap> &pm)
{
	m_style.m_background = pm;
}

void eListbox::setSelectionPicture(ePtr<gPixmap> &pm)
{
	m_style.m_selection = pm;
}

void eListbox::setSelectionPictureLarge(ePtr<gPixmap> &pm)
{
	m_style.m_selection_large = pm;
}

void eListbox::setSelectionBorderHidden()
{
	m_style.m_border_set = 1;
}

void eListbox::setSliderPicture(ePtr<gPixmap> &pm)
{
	m_scrollbarpixmap = pm;
	if (m_scrollbar && m_scrollbarpixmap) m_scrollbar->setPixmap(pm);
}

void eListbox::setSliderForegroundColor(gRGB &col)
{
	m_style.m_sliderforeground_color = col;
	m_style.m_sliderforeground_color_set = 1;
	if (m_scrollbar) m_scrollbar->setSliderForegroundColor(col);
}

void eListbox::setSliderBorderColor(const gRGB &col)
{
	m_style.m_sliderborder_color = col;
	m_style.m_sliderborder_color_set = 1;
	if (m_scrollbar) m_scrollbar->setSliderBorderColor(col);
}

void eListbox::setSliderBorderWidth(int size)
{
	m_style.m_sliderborder_size = size;
	if (m_scrollbar) m_scrollbar->setSliderBorderWidth(size);
}

void eListbox::setScrollbarBackgroundPicture(ePtr<gPixmap> &pm)
{
	m_scrollbarbackgroundpixmap = pm;
	if (m_scrollbar && m_scrollbarbackgroundpixmap) m_scrollbar->setBackgroundPixmap(pm);
}

void eListbox::invalidate(const gRegion &region)
{
	gRegion tmp(region);
	if (m_content)
		m_content->updateClip(tmp);
	eWidget::invalidate(tmp);
}

struct eListboxStyle *eListbox::getLocalStyle(void)
{
		/* transparency is set directly in the widget */
	m_style.m_transparent_background = isTransparent();
	return &m_style;
}

void eListbox::setItemCornerRadiusInternal(int radius, int edges, int index)
{
	m_style.m_itemCornerRadius[index] = radius;
	m_style.m_itemCornerRadiusEdges[index] = edges;
}

void eListbox::setItemCornerRadius(int radius, int edges)
{
	for (int x = 0; x < 2; x++)
	{
		setItemCornerRadiusInternal(radius, edges, x);
	}
}

void eListbox::setItemCornerRadiusSelected(int radius, int edges)
{
	setItemCornerRadiusInternal(radius, edges, 1);
}
