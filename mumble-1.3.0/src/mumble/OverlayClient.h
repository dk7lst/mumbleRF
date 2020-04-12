// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MUMBLE_OVERLAYCLIENT_H_
#define MUMBLE_MUMBLE_OVERLAYCLIENT_H_

#include <QtCore/QUrl>
#include <QtNetwork/QLocalSocket>

#include "Timer.h"
#include "../../overlay/overlay.h"
#include "SharedMemory.h"
#include "OverlayUserGroup.h"

class ClientUser;
class Overlay;
class QLibrary;
class QLocalServer;
class OverlayPositionableItem;

class OverlayClient : public QObject {
		friend class Overlay;
	private:
		Q_OBJECT
		Q_DISABLE_COPY(OverlayClient)
	protected:
		OverlayMsg omMsg;
		QLocalSocket *qlsSocket;
		SharedMemory2 *smMem;
		QRect qrLast;
		Timer t;

		float framesPerSecond;
		int iOffsetX, iOffsetY;
		QGraphicsPixmapItem *qgpiCursor;
		QGraphicsPixmapItem *qgpiLogo;
		OverlayPositionableItem *qgpiFPS;
		OverlayPositionableItem *qgpiTime;

		/// The process ID of the process this OverlayClient is connected to.
		quint64 uiPid;
		/// The path to the executable of the process that this OverlayClient is connected to.
		QString qsExecutablePath;

		QGraphicsScene qgs;
		OverlayUserGroup ougUsers;

#ifdef Q_OS_MAC
		QMap<Qt::CursorShape, QPixmap> qmCursors;
#endif

		bool bWasVisible;
		bool bDelete;

		void setupRender();
		void setupScene(bool show);

		bool eventFilter(QObject *, QEvent *) Q_DECL_OVERRIDE;

		void readyReadMsgInit(unsigned int length);

		QList<QRectF> qlDirty;
	protected slots:
		void readyRead();
		void changed(const QList<QRectF> &);
		void render();
	public:
		QGraphicsView qgv;
		unsigned int uiWidth, uiHeight;
		int iMouseX, iMouseY;

		OverlayClient(QLocalSocket *, QObject *);
		~OverlayClient() Q_DECL_OVERRIDE;
		void reset();
	public slots:
		void showGui();
		void hideGui();
		void scheduleDelete();
		void updateMouse();
		void updateFPS();
		void updateTime();
		bool update();
		void openEditor();
};

#endif
