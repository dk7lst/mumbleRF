// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MUMBLE_CUSTOMELEMENTS_H_
#define MUMBLE_MUMBLE_CUSTOMELEMENTS_H_

#include <QtCore/QObject>

#if QT_VERSION >= 0x050000
# include <QtWidgets/QLabel>
# include <QtWidgets/QTextBrowser>
# include <QtWidgets/QTextEdit>
#else
# include <QtGui/QLabel>
# include <QtGui/QTextBrowser>
# include <QtGui/QTextEdit>
#endif

class LogTextBrowser : public QTextBrowser {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(LogTextBrowser)
	protected:
		void resizeEvent(QResizeEvent *e) Q_DECL_OVERRIDE;
		bool event(QEvent *e) Q_DECL_OVERRIDE;
	public:
		LogTextBrowser(QWidget *p = NULL);

		int getLogScroll();
		int getLogScrollMaximum();
		void setLogScroll(int scroll_pos);
		void scrollLogToBottom();
};

class ChatbarTextEdit : public QTextEdit {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(ChatbarTextEdit)
		void inFocus(bool);
		QStringList qslHistory;
		QString qsHistoryTemp;
		int iHistoryIndex;
		static const int MAX_HISTORY = 50;
	protected:
		QString qsDefaultText;
		bool bDefaultVisible;
		void focusInEvent(QFocusEvent *) Q_DECL_OVERRIDE;
		void focusOutEvent(QFocusEvent *) Q_DECL_OVERRIDE;
		void contextMenuEvent(QContextMenuEvent *) Q_DECL_OVERRIDE;
		void dropEvent(QDropEvent *) Q_DECL_OVERRIDE;
		bool event(QEvent *) Q_DECL_OVERRIDE;
		QSize minimumSizeHint() const Q_DECL_OVERRIDE;
		QSize sizeHint() const Q_DECL_OVERRIDE;
		void resizeEvent(QResizeEvent *e) Q_DECL_OVERRIDE;
	public:
		void setDefaultText(const QString &, bool = false);
		unsigned int completeAtCursor();
	signals:
		void tabPressed(void);
		void backtabPressed(void);
		void ctrlSpacePressed(void);
		void entered(QString);
	public slots:
		void pasteAndSend_triggered();
		void doResize();
		void doScrollbar();
		void addToHistory(const QString &str);
		void historyUp();
		void historyDown();
	public:
		ChatbarTextEdit(QWidget *p = NULL);
};

class DockTitleBar : public QLabel {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(DockTitleBar)
	protected:
		QTimer *qtTick;
		int size;
		int newsize;
	public:
		DockTitleBar();
		QSize sizeHint() const Q_DECL_OVERRIDE;
		QSize minimumSizeHint() const Q_DECL_OVERRIDE;
	public slots:
		void tick();
	protected:
		bool eventFilter(QObject *, QEvent *) Q_DECL_OVERRIDE;
};

#endif // CUSTOMELEMENTS_H_
