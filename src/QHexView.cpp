#include "../include/QHexView.h"
#include <QScrollBar>
#include <QPainter>
#include <QSize>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QClipboard>
#include <QApplication>

#include <stdexcept>

#define UPDATE viewport()->update();
#define CHAR_VALID(caracter) ((caracter < 0x20) || (caracter > 0x7e)) ? caracter = '.' : caracter;
#define SET_BACKGROUND_MARK(pos)                    \
    if (pos >= m_selectBegin && pos < m_selectEnd){ \
		painter.setBackground(QBrush(QColor(0x6d, 0x9e, 0xff, 0xff)));			\
		painter.setBackgroundMode(Qt::OpaqueMode);  \
	}


#define MIN_HEXCHARS_IN_LINE 47
#define GAP_ADR_HEX  10
#define GAP_HEX_ASCII  16
#define MIN_BYTES_PER_LINE  16
#define ADR_LENGTH  10


QHexView::QHexView(QWidget *parent):
QAbstractScrollArea(parent),
m_pdata(NULL)
{
	setFont(QFont("Courier", 13));

#if QT_VERSION >= 0x051100
    m_charWidth = fontMetrics().width(QLatin1Char('9'));
#else
    m_charWidth = fontMetrics().horizontalAdvance(QLatin1Char('9'));
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


void QHexView::showFromOffset(int offset)
{
	QMutexLocker lock(&m_dataMtx);

	if(m_pdata && offset < m_pdata->size())
	{
		updatePositions();

		setCursorPos(offset * 2);

		int cursorY = m_cursorPos / (2 * m_bytesPerLine);

		verticalScrollBar() -> setValue(cursorY);
		UPDATE
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

	UPDATE;
}


QSize QHexView::fullSize() const
{
	if(!m_pdata)
		return QSize(0, 0);

	int width = m_posAscii + (m_bytesPerLine * m_charWidth);
	int height = m_pdata->size() / m_bytesPerLine;
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
	verticalScrollBar()->setPageStep(areaSize.height() / m_charHeight);
	verticalScrollBar()->setRange(0, (widgetSize.height() - areaSize.height()) / m_charHeight + 1);

	int firstLineIdx = verticalScrollBar() -> value();

	int lastLineIdx = firstLineIdx + areaSize.height() / m_charHeight;
    if(lastLineIdx > m_pdata->size() / m_bytesPerLine)
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
    QByteArray data = m_pdata->getData(firstLineIdx * m_bytesPerLine, (lastLineIdx - firstLineIdx) * m_bytesPerLine);

	for (int lineIdx = firstLineIdx, yPos = yPosStart;  lineIdx < lastLineIdx; lineIdx += 1, yPos += m_charHeight)
	{
		// ascii position
		for (int xPosAscii = m_posAscii, i=0; ((lineIdx - firstLineIdx) * m_bytesPerLine + i) < data.size() && (i < m_bytesPerLine); i++, xPosAscii += m_charWidth)
		{
			char caracter = data[(lineIdx - firstLineIdx) * (uint)m_bytesPerLine + i];
            CHAR_VALID(caracter);

            int pos = ((lineIdx * m_bytesPerLine + i) * 2);
            SET_BACKGROUND_MARK(pos);

            painter.drawText(xPosAscii, yPos, QString(caracter));

            painter.setBackground(painter.brush());
            painter.setBackgroundMode(Qt::OpaqueMode);
		}

		// binary position
		for(int xPos = m_posHex, i=0; i< m_bytesPerLine && ((lineIdx - firstLineIdx) * m_bytesPerLine + i) < data.size(); i++, xPos += 3 * m_charWidth)
		{
			int pos = ((lineIdx * m_bytesPerLine + i) * 2);
			SET_BACKGROUND_MARK(pos);

			QString val = QString::number((data.at((lineIdx - firstLineIdx) * m_bytesPerLine + i) & 0xF0) >> 4, 16);
			painter.drawText(xPos, yPos, val);

			if((pos+1) >= m_selectBegin && (pos+1) < m_selectEnd)
			{
				painter.setBackground(QBrush(QColor(0x6d, 0x9e, 0xff, 0xff)));
				painter.setBackgroundMode(Qt::OpaqueMode);
			}
			else
			{
				painter.setBackground(def);
				painter.setBackgroundMode(Qt::OpaqueMode);
			}

			val = QString::number((data.at((lineIdx - firstLineIdx) * m_bytesPerLine + i) & 0xF), 16);
			painter.drawText(xPos+m_charWidth, yPos, val);

			painter.setBackground(painter.brush());
			painter.setBackgroundMode(Qt::OpaqueMode);
		}

		// offsets
		QString address = QString("%1").arg(lineIdx * m_bytesPerLine, 10, 16, QChar('0'));
		painter.drawText(m_posAddr, yPos, address);

		UPDATE
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
		int pos = m_cursorPos + 1;
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectPreviousChar))
	{
		int pos = m_cursorPos - 1;
		setSelection(pos);
		setCursorPos(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectEndOfLine))
	{
		int pos = m_cursorPos - (m_cursorPos % (2 * m_bytesPerLine)) + (2 * m_bytesPerLine);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectStartOfLine))
	{
		int pos = m_cursorPos - (m_cursorPos % (2 * m_bytesPerLine));
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectPreviousLine))
	{
		int pos = m_cursorPos - (2 * m_bytesPerLine);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectNextLine))
	{
		int pos = m_cursorPos + (2 * m_bytesPerLine);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}

	if (event->matches(QKeySequence::SelectNextPage))
	{
		int pos = m_cursorPos + (((viewport()->height() / m_charHeight) - 1) * 2 * m_bytesPerLine);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectPreviousPage))
	{
		int pos = m_cursorPos - (((viewport()->height() / m_charHeight) - 1) * 2 * m_bytesPerLine);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectEndOfDocument))
	{
		int pos = 0;
		if(m_pdata)
			pos = m_pdata->size() * 2;
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectStartOfDocument))
	{
		int pos = 0;
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
	int actPos = cursorPos(event->pos());
	if (actPos != std::numeric_limits<int >::max())
	{
		QMutexLocker lock(&m_dataMtx);

		setCursorPos(actPos);
		setSelection(actPos);
	}

	viewport() -> update();
}

void QHexView::mousePressEvent(QMouseEvent * event)
{
	int  cPos = cursorPos(event->pos());

	if((QApplication::keyboardModifiers() & Qt::ShiftModifier) && event -> button() == Qt::LeftButton)
		setSelection(cPos);
	else
		resetSelection(cPos);

	if (cPos != std::numeric_limits<int>::max())
	{
		QMutexLocker lock(&m_dataMtx);

		setCursorPos(cPos);
	}

	viewport() -> update();
}


int QHexView::cursorPos(const QPoint &position)
{
	int pos = std::numeric_limits<int>::max();

	if (((int)position.x() >= m_posHex) && ((int)position.x() < (m_posHex + (m_bytesPerLine * 3 - 1) * m_charWidth)))
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

void QHexView::resetSelection(int pos)
{
    if (pos == std::numeric_limits<int>::max())
        pos = 0;

    m_selectInit = pos;
    m_selectBegin = pos;
    m_selectEnd = pos;
}

void QHexView::setSelection(int pos)
{
    if (pos == std::numeric_limits<int>::max())
        pos = 0;

    if ((int)pos >= m_selectInit)
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

void QHexView::setSelected(int offset, int length)
{
	m_selectInit = m_selectBegin = offset * 2;
	m_selectEnd = m_selectBegin + length * 2;
	viewport() -> update();
}

void QHexView::setCursorPos(int position)
{
	if(position == std::numeric_limits<int>::max())
		position = 0;

	int maxPos = 0;
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

QByteArray QHexView::DataStorageArray::getData(int position, int length)
{
	return m_data.mid(position, length);
}


int QHexView::DataStorageArray::size()
{
	return m_data.count();
}


QHexView::DataStorageFile::DataStorageFile(const QString &fileName): m_file(fileName)
{
	m_file.open(QIODevice::ReadOnly);
	if(!m_file.isOpen())
		throw std::runtime_error(std::string("Failed to open file `") + fileName.toStdString() + "`");
}

QByteArray QHexView::DataStorageFile::getData(int position, int length)
{
	m_file.seek(position);
	return m_file.read(length);
}


int QHexView::DataStorageFile::size()
{
	return m_file.size();
}
