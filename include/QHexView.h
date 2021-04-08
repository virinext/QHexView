#ifndef Q_HEX_VIEWER_H_
#define Q_HEX_VIEWER_H_

#include <QAbstractScrollArea>
#include <QByteArray>
#include <QFile>
#include <QLineEdit>
#include <stdint.h>

class QHexView: public QAbstractScrollArea

{
    enum Mode {READ_ONLY, EDIT};
	public:
		class DataStorage
		{
			public:
				virtual ~DataStorage() {};
				virtual QByteArray getData(std::size_t position, std::size_t length) = 0;
                virtual void setByte(std::size_t position, uint8_t value) = 0;
				virtual std::size_t size() = 0;
		};


		class DataStorageArray: public DataStorage
		{
			public:
				DataStorageArray(const QByteArray &arr);
				virtual QByteArray getData(std::size_t position, std::size_t length);
                virtual void setByte(std::size_t position, uint8_t value);
				virtual std::size_t size();
			private:
				QByteArray    m_data;
		};

		class DataStorageFile: public DataStorage
		{
			public:
				DataStorageFile(const QString &fileName);
				virtual QByteArray getData(std::size_t position, std::size_t length);
                virtual void setByte(std::size_t position, uint8_t value){}
				virtual std::size_t size();
			private:
				QFile      m_file;
		};


        class QHexCellEdit : public  QLineEdit
        {
        public:
            QHexCellEdit(QWidget *parent, uint8_t value);

            uint8_t getValue() const {return m_value;};

        protected:

            void keyPressEvent(QKeyEvent *event);

        private:
            uint8_t m_value;
        };

		QHexView(QWidget *parent = 0);
		~QHexView();

	public slots:
		void setData(DataStorage *pData);
		void clear();
		void showFromOffset(std::size_t offset);
        void releaseEditCell(bool newValue);
	protected:
		void paintEvent(QPaintEvent *event);
		void keyPressEvent(QKeyEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
		void mousePressEvent(QMouseEvent *event);
        void mouseDoubleClickEvent(QMouseEvent *e);
        void wheelEvent(QWheelEvent* event);
        void scrollContentsBy(int dx, int dy);
	private:
		DataStorage          *m_pdata;
		std::size_t           m_posAddr; 
		std::size_t           m_posHex;
		std::size_t           m_posAscii;
		std::size_t           m_charWidth;
		std::size_t           m_charHeight;


		std::size_t           m_selectBegin;
		std::size_t           m_selectEnd;
		std::size_t           m_selectInit;
		std::size_t           m_cursorPos;

        Mode                  m_mode;
        QHexCellEdit          *m_edit;


		QSize fullSize() const;
		void resetSelection();
		void resetSelection(int pos);
		void setSelection(int pos);
		void ensureVisible();
		void setCursorPos(int pos);
		std::size_t cursorPos(const QPoint &position);
        void setMode(bool readOnly);
};

#endif
