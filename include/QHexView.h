#ifndef Q_HEX_VIEWER_H_
#define Q_HEX_VIEWER_H_

#include <QAbstractScrollArea>
#include <QByteArray>

class QHexView: public QAbstractScrollArea

{
	public:
		QHexView(QWidget *parent = 0);

		QByteArray data() const;

	public slots:
		void setData(const QByteArray &arr);
		void clear();
		void showFromOffset(std::size_t offset);

	protected:
		void paintEvent(QPaintEvent *event);
		void keyPressEvent(QKeyEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
		void mousePressEvent(QMouseEvent *event);
	private:
		QByteArray            m_data;
		std::size_t           m_posAddr; 
		std::size_t           m_posHex;
		std::size_t           m_posAscii;
		std::size_t           m_charWidth;
		std::size_t           m_charHeight;


		std::size_t           m_selectBegin;
		std::size_t           m_selectEnd;
		std::size_t           m_selectInit;
		std::size_t           m_cursorPos;


		QSize fullSize() const;
		void resetSelection();
		void resetSelection(int pos);
		void setSelection(int pos);
		void ensureVisible();
		void setCursorPos(int pos);
		std::size_t cursorPos(const QPoint &position);
};

#endif
