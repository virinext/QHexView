#include "../include/QHexView.h"
#include <QScrollBar>
#include <QPainter>
#include <QSize>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QClipboard>
#include <QApplication>

#include <QDebug>

#include <stdexcept>

const int MIN_HEXCHARS_IN_LINE = 47;
const int GAP_ADR_HEX = 10;
const int GAP_HEX_ASCII = 16;
const int MIN_BYTES_PER_LINE = 16;
const int ADR_LENGTH = 10;


QHexView::QHexView(QWidget *parent):
QAbstractScrollArea(parent),
m_pdata(NULL)
{
	setFont(QFont("Courier", 10));

#if QT_VERSION >= 0x051100
	m_charWidth = fontMetrics().horizontalAdvance(QLatin1Char('9'));
#else
	m_charWidth = fontMetrics().width(QLatin1Char('9'));
#endif
	m_charHeight = fontMetrics().height();

	m_posAddr = 0;
	m_posHex = ADR_LENGTH * m_charWidth + GAP_ADR_HEX;
	m_posAscii = m_posHex + MIN_HEXCHARS_IN_LINE * m_charWidth + GAP_HEX_ASCII;
	m_bytesPerLine = MIN_BYTES_PER_LINE;

	setMinimumWidth(m_posAscii + (MIN_BYTES_PER_LINE * m_charWidth));

	setFocusPolicy(Qt::StrongFocus);
}


QHexView::~QHexView()
{
	if(m_pdata)
		delete m_pdata;
}

void QHexView::setData(QHexView::DataStorage *pData)
{
	QMutexLocker lock(&m_dataMtx);

	verticalScrollBar()->setValue(0);
	if(m_pdata)
		delete m_pdata;
	m_pdata = pData;
	m_cursorPos = 0;
	resetSelection(0);
}


void QHexView::showFromOffset(std::size_t offset)
{
	QMutexLocker lock(&m_dataMtx);

	if(m_pdata && offset < m_pdata->size())
	{
		updatePositions();

		setCursorPos(offset * 2);

		int cursorY = m_cursorPos / (2 * m_bytesPerLine);

		verticalScrollBar() -> setValue(cursorY);
		viewport()->update();
	}
}

void QHexView::clear()
{
	QMutexLocker lock(&m_dataMtx);

	if (m_pdata)
	{
		delete m_pdata;
		m_pdata = NULL;
	}
	verticalScrollBar()->setValue(0);
	viewport()->update();
}


QSize QHexView::fullSize() const
{
	if(!m_pdata)
		return QSize(0, 0);

	std::size_t width = m_posAscii + (m_bytesPerLine * m_charWidth);
	std::size_t height = m_pdata->size() / m_bytesPerLine;
	if(m_pdata->size() % m_bytesPerLine)
		height++;

	height *= m_charHeight;

	return QSize(width, height);
}

void QHexView::updatePositions()
{
#if QT_VERSION >= 0x051100
	m_charWidth = fontMetrics().horizontalAdvance(QLatin1Char('9'));
#else
	m_charWidth = fontMetrics().width(QLatin1Char('9'));
#endif
	m_charHeight = fontMetrics().height();

	int serviceSymbolsWidth = ADR_LENGTH * m_charWidth + GAP_ADR_HEX + GAP_HEX_ASCII;

	m_bytesPerLine = (width() - serviceSymbolsWidth) / (4 * m_charWidth) - 1; // 4 symbols per byte

	m_posAddr = 0;
	m_posHex = ADR_LENGTH * m_charWidth + GAP_ADR_HEX;
	m_posAscii = m_posHex + (m_bytesPerLine * 3 - 1) * m_charWidth + GAP_HEX_ASCII;

	QSize areaSize = viewport()->size();
	QSize  widgetSize = fullSize();
	verticalScrollBar()->setPageStep(areaSize.height() / m_charHeight);
	verticalScrollBar()->setRange(0, (widgetSize.height() - areaSize.height()) / m_charHeight + 1);
}

void QHexView::paintEvent(QPaintEvent *event)
{
	QMutexLocker lock(&m_dataMtx);

	if(!m_pdata)
		return;
	QPainter painter(viewport());

	updatePositions();

	QSize areaSize = viewport()->size();
	QSize  widgetSize = fullSize();

	int firstLineIdx = verticalScrollBar() -> value();

	int lastLineIdx = firstLineIdx + areaSize.height() / m_charHeight;
    if((unsigned int)lastLineIdx > m_pdata->size() / m_bytesPerLine)
	{
		lastLineIdx = m_pdata->size() / m_bytesPerLine;
		if(m_pdata->size() % m_bytesPerLine)
			lastLineIdx++;
	}

	painter.fillRect(event->rect(), this->palette().color(QPalette::Base));

	QColor addressAreaColor = QColor(0xd4, 0xd4, 0xd4, 0xff);
	painter.fillRect(QRect(m_posAddr, event->rect().top(), m_posHex - GAP_ADR_HEX + 2 , height()), addressAreaColor);

	int linePos = m_posAscii - (GAP_HEX_ASCII / 2);
	painter.setPen(Qt::gray);

	painter.drawLine(linePos, event->rect().top(), linePos, height());

	painter.setPen(Qt::black);

	int yPosStart = m_charHeight;

	QBrush def = painter.brush();
    QBrush selected = QBrush(QColor(0x6d, 0x9e, 0xff, 0xff));
    QByteArray data = m_pdata->getData(firstLineIdx * m_bytesPerLine, (lastLineIdx - firstLineIdx) * m_bytesPerLine);

	for (int lineIdx = firstLineIdx, yPos = yPosStart;  lineIdx < lastLineIdx; lineIdx += 1, yPos += m_charHeight)
	{
		QString address = QString("%1").arg(lineIdx * m_bytesPerLine, 10, 16, QChar('0'));
		painter.drawText(m_posAddr, yPos, address);

		for(int xPos = m_posHex, i=0; i< m_bytesPerLine && ((lineIdx - firstLineIdx) * m_bytesPerLine + i) < data.size(); i++, xPos += 3 * m_charWidth)
		{
			std::size_t pos = (lineIdx * m_bytesPerLine + i) * 2;
			if(pos >= m_selectBegin && pos < m_selectEnd)
			{
				painter.setBackground(selected);
				painter.setBackgroundMode(Qt::OpaqueMode);
			}

			QString val = QString::number((data.at((lineIdx - firstLineIdx) * m_bytesPerLine + i) & 0xF0) >> 4, 16);
			painter.drawText(xPos, yPos, val);


			if((pos+1) >= m_selectBegin && (pos+1) < m_selectEnd)
			{
				painter.setBackground(selected);
				painter.setBackgroundMode(Qt::OpaqueMode);
			}
			else
			{
				painter.setBackground(def);
				painter.setBackgroundMode(Qt::OpaqueMode);
			}

			val = QString::number((data.at((lineIdx - firstLineIdx) * m_bytesPerLine + i) & 0xF), 16);
			painter.drawText(xPos+m_charWidth, yPos, val);

			painter.setBackground(def);
			painter.setBackgroundMode(Qt::OpaqueMode);

		}

		for (int xPosAscii = m_posAscii, i=0; ((lineIdx - firstLineIdx) * m_bytesPerLine + i) < data.size() && (i < m_bytesPerLine); i++, xPosAscii += m_charWidth)
		{
			char ch = data[(lineIdx - firstLineIdx) * (uint)m_bytesPerLine + i];
			if ((ch < 0x20) || (ch > 0x7e))
			ch = '.';

			painter.drawText(xPosAscii, yPos, QString(ch));
		}

	}

	if (hasFocus())
	{
		int x = (m_cursorPos % (2 * m_bytesPerLine));
		int y = m_cursorPos / (2 * m_bytesPerLine);
		y -= firstLineIdx;
		int cursorX = (((x / 2) * 3) + (x % 2)) * m_charWidth + m_posHex;
		int cursorY = y * m_charHeight + 4;
		painter.fillRect(cursorX, cursorY, 2, m_charHeight, this->palette().color(QPalette::WindowText));
	}
}


void QHexView::keyPressEvent(QKeyEvent *event)
{
	QMutexLocker lock(&m_dataMtx);

	bool setVisible = false;

/*****************************************************************************/
/* Cursor movements */
/*****************************************************************************/
	if(event->matches(QKeySequence::MoveToNextChar))
	{
		setCursorPos(m_cursorPos + 1);
		resetSelection(m_cursorPos);
		setVisible = true;
	}
	if(event->matches(QKeySequence::MoveToPreviousChar))
	{
		setCursorPos(m_cursorPos - 1);
		resetSelection(m_cursorPos);
		setVisible = true;
	}
	if(event->matches(QKeySequence::MoveToEndOfLine))
	{
		setCursorPos(m_cursorPos | ((m_bytesPerLine * 2) - 1));
		resetSelection(m_cursorPos);
		setVisible = true;
	}
	if(event->matches(QKeySequence::MoveToStartOfLine))
	{
		setCursorPos(m_cursorPos | (m_cursorPos % (m_bytesPerLine * 2)));
		resetSelection(m_cursorPos);
		setVisible = true;
	}
	if(event->matches(QKeySequence::MoveToPreviousLine))
	{
		setCursorPos(m_cursorPos - m_bytesPerLine * 2);
		resetSelection(m_cursorPos);
		setVisible = true;
	}
	if(event->matches(QKeySequence::MoveToNextLine))
	{
		setCursorPos(m_cursorPos + m_bytesPerLine * 2);
		resetSelection(m_cursorPos);
		setVisible = true;
	}

	if(event->matches(QKeySequence::MoveToNextPage))
	{
		setCursorPos(m_cursorPos + (viewport()->height() / m_charHeight - 1) * 2 * m_bytesPerLine);
		resetSelection(m_cursorPos);
		setVisible = true;
	}
	if(event->matches(QKeySequence::MoveToPreviousPage))
	{
		setCursorPos(m_cursorPos - (viewport()->height() / m_charHeight - 1) * 2 * m_bytesPerLine);
		resetSelection(m_cursorPos);
		setVisible = true;
	}
	if(event->matches(QKeySequence::MoveToEndOfDocument))
	{
		if(m_pdata)
			setCursorPos(m_pdata->size() * 2);
		resetSelection(m_cursorPos);
		setVisible = true;
	}
	if(event->matches(QKeySequence::MoveToStartOfDocument))
	{
		setCursorPos(0);
		resetSelection(m_cursorPos);
		setVisible = true;
	}

/*****************************************************************************/
/* Select commands */
/*****************************************************************************/
	if (event->matches(QKeySequence::SelectAll))
	{
		resetSelection(0);
		if(m_pdata)
			setSelection(2 * m_pdata->size() + 1);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectNextChar))
	{
		std::size_t pos = m_cursorPos + 1;
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectPreviousChar))
	{
		std::size_t pos = m_cursorPos - 1;
		setSelection(pos);
		setCursorPos(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectEndOfLine))
	{
		std::size_t pos = m_cursorPos - (m_cursorPos % (2 * m_bytesPerLine)) + (2 * m_bytesPerLine);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectStartOfLine))
	{
		std::size_t pos = m_cursorPos - (m_cursorPos % (2 * m_bytesPerLine));
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectPreviousLine))
	{
		std::size_t pos = m_cursorPos - (2 * m_bytesPerLine);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectNextLine))
	{
		std::size_t pos = m_cursorPos + (2 * m_bytesPerLine);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}

	if (event->matches(QKeySequence::SelectNextPage))
	{
		std::size_t pos = m_cursorPos + (((viewport()->height() / m_charHeight) - 1) * 2 * m_bytesPerLine);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectPreviousPage))
	{
		std::size_t pos = m_cursorPos - (((viewport()->height() / m_charHeight) - 1) * 2 * m_bytesPerLine);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectEndOfDocument))
	{
		std::size_t pos = 0;
		if(m_pdata)
			pos = m_pdata->size() * 2;
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectStartOfDocument))
	{
		std::size_t pos = 0;
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}

	if (event->matches(QKeySequence::Copy))
	{
		if(m_pdata)
		{
			QString res;
			int idx = 0;
			int copyOffset = 0;

			QByteArray data = m_pdata->getData(m_selectBegin / 2, (m_selectEnd - m_selectBegin) / 2 + 1);
			if(m_selectBegin % 2)
			{
				res += QString::number((data.at((idx+1) / 2) & 0xF), 16);
				res += " ";
				idx++;
				copyOffset = 1;
			}

			int selectedSize = m_selectEnd - m_selectBegin;
			for (;idx < selectedSize; idx+= 2)
			{
				if (data.size() > (copyOffset + idx) / 2)
				{
					QString val = QString::number((data.at((copyOffset + idx) / 2) & 0xF0) >> 4, 16);
					if (idx + 1 < selectedSize)
					{
						val += QString::number((data.at((copyOffset + idx) / 2) & 0xF), 16);
						val += " ";
					}
					res += val;

					if ((idx / 2) % m_bytesPerLine == (m_bytesPerLine - 1))
						res += "\n";
				}
			}
			QClipboard *clipboard = QApplication::clipboard();
			clipboard -> setText(res);
		}
	}

 	if(setVisible)
	    ensureVisible();
	viewport() -> update();
}

void QHexView::mouseMoveEvent(QMouseEvent * event)
{
	std::size_t actPos = cursorPos(event->pos());
	if (actPos != std::numeric_limits<std::size_t>::max())
	{
		QMutexLocker lock(&m_dataMtx);

		setCursorPos(actPos);
		setSelection(actPos);
	}

	viewport() -> update();
}

void QHexView::mousePressEvent(QMouseEvent * event)
{
	std::size_t cPos = cursorPos(event->pos());

	if((QApplication::keyboardModifiers() & Qt::ShiftModifier) && event -> button() == Qt::LeftButton)
		setSelection(cPos);
	else
		resetSelection(cPos);

	if (cPos != std::numeric_limits<std::size_t>::max())
	{
		QMutexLocker lock(&m_dataMtx);

		setCursorPos(cPos);
	}

	viewport() -> update();
}


std::size_t QHexView::cursorPos(const QPoint &position)
{
	std::size_t pos = std::numeric_limits<std::size_t>::max();

	if (((std::size_t)position.x() >= m_posHex) && ((std::size_t)position.x() < (m_posHex + (m_bytesPerLine * 3 - 1) * m_charWidth)))
	{
		int x = (position.x() - m_posHex) / m_charWidth;
		if ((x % 3) == 0)
			x = (x / 3) * 2;
        else
			x = ((x / 3) * 2) + 1;

		int firstLineIdx = verticalScrollBar() -> value();
		int y = (position.y() / m_charHeight) * 2 * m_bytesPerLine;
		pos = x + y + firstLineIdx * m_bytesPerLine * 2;
	}
	return pos;
}


void QHexView::resetSelection()
{
    m_selectBegin = m_selectInit;
    m_selectEnd = m_selectInit;
}

void QHexView::resetSelection(std::size_t pos)
{
    if (pos == std::numeric_limits<std::size_t>::max())
        pos = 0;

    m_selectInit = pos;
    m_selectBegin = pos;
    m_selectEnd = pos;
}

void QHexView::setSelection(std::size_t pos)
{
    if (pos == std::numeric_limits<std::size_t>::max())
        pos = 0;

    if ((std::size_t)pos >= m_selectInit)
    {
        m_selectEnd = pos;
        m_selectBegin = m_selectInit;
    }
    else
    {
        m_selectBegin = pos;
        m_selectEnd = m_selectInit;
    }
}

void QHexView::setSelected(std::size_t offset, std::size_t length)
{
	m_selectInit = m_selectBegin = offset * 2;
	m_selectEnd = m_selectBegin + length * 2;
	viewport() -> update();
}

void QHexView::setCursorPos(std::size_t position)
{
	if(position == std::numeric_limits<std::size_t>::max())
		position = 0;

	std::size_t maxPos = 0;
	if(m_pdata)
	{
		maxPos = m_pdata->size() * 2;
		if(m_pdata->size() % m_bytesPerLine)
			maxPos++;
	}

	if(position > maxPos)
		position = maxPos;

	m_cursorPos = position;
}

void QHexView::ensureVisible()
{
	QSize areaSize = viewport()->size();

	int firstLineIdx = verticalScrollBar() -> value();
	int lastLineIdx = firstLineIdx + areaSize.height() / m_charHeight;

	int cursorY = m_cursorPos / (2 * m_bytesPerLine);

	if(cursorY < firstLineIdx)
		verticalScrollBar() -> setValue(cursorY);
	else if(cursorY >= lastLineIdx)
		verticalScrollBar() -> setValue(cursorY - areaSize.height() / m_charHeight + 1);
}



QHexView::DataStorageArray::DataStorageArray(const QByteArray &arr)
{
	m_data = arr;
}

QByteArray QHexView::DataStorageArray::getData(std::size_t position, std::size_t length)
{
	return m_data.mid(position, length);
}


std::size_t QHexView::DataStorageArray::size()
{
	return m_data.count();
}


QHexView::DataStorageFile::DataStorageFile(const QString &fileName): m_file(fileName)
{
	m_file.open(QIODevice::ReadOnly);
	if(!m_file.isOpen())
		throw std::runtime_error(std::string("Failed to open file `") + fileName.toStdString() + "`");
}

QByteArray QHexView::DataStorageFile::getData(std::size_t position, std::size_t length)
{
	m_file.seek(position);
	return m_file.read(length);
}


std::size_t QHexView::DataStorageFile::size()
{
	return m_file.size();
}
