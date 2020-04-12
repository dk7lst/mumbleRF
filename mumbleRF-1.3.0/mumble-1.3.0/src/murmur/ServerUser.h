// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MURMUR_SERVERUSER_H_
#define MUMBLE_MURMUR_SERVERUSER_H_

#include <QtCore/QStringList>
#include <QtCore/QElapsedTimer>

#ifdef Q_OS_UNIX
#include <sys/socket.h>
#else
#include <winsock2.h>
#endif

#include "Connection.h"
#include "Timer.h"
#include "User.h"
#include "HostAddress.h"

// Unfortunately, this needs to be "large enough" to hold
// enough frames to account for both short-term and
// long-term "maladjustments".

#define N_BANDWIDTH_SLOTS 360

struct BandwidthRecord {
	int iRecNum;
	int iSum;
	Timer tFirst;
	Timer tIdleControl;
	unsigned short a_iBW[N_BANDWIDTH_SLOTS];
	Timer a_qtWhen[N_BANDWIDTH_SLOTS];
	mutable QMutex qmMutex;

	BandwidthRecord();
	bool addFrame(int size, int maxpersec);
	int onlineSeconds() const;
	int idleSeconds() const;
	void resetIdleSeconds();
	int bandwidth() const;
};

struct WhisperTarget {
	struct Channel {
		int iId;
		bool bChildren;
		bool bLinks;
		QString qsGroup;
	};
	QList<unsigned int> qlSessions;
	QList<WhisperTarget::Channel> qlChannels;
};

class Server;

/// A simple implementation for rate-limiting.
/// See https://en.wikipedia.org/wiki/Leaky_bucket
class LeakyBucket {
	private:
		/// The amount of tokens that are drained per second.
		/// (The sze of the whole in the bucket)
		unsigned int m_tokensPerSec;
		/// The maximum amount of tokens that may be encountered.
		/// (The capacity of the bucket)
		unsigned int m_maxTokens;
		/// The amount of tokens currently stored
		/// (The amount of whater currently in the bucket)
		long m_currentTokens;
		/// A timer that is used to measure time intervals. It is essential
		/// that this timer uses a monotonic clock (which is why QElapsedTimer is
		/// used instead of QTime or QDateTime).
		QElapsedTimer m_timer;

	public:
		/// @param tokens The amount of tokens that should be added.
		/// @returns Whether adding this amount of tokens triggers rate
		/// 	limiting (true means the corresponding packet has to be
		/// 	discared and false means the packet may be processed)
		bool ratelimit(int tokens);

		LeakyBucket(unsigned int tokensPerSec, unsigned int maxTokens);
};

class ServerUser : public Connection, public User {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(ServerUser)
	protected:
		Server *s;
	public:
		enum State { Connected, Authenticated };
		State sState;
		operator QString() const;

		float dUDPPingAvg, dUDPPingVar;
		float dTCPPingAvg, dTCPPingVar;
		quint32 uiUDPPackets, uiTCPPackets;

		unsigned int uiVersion;
		QString qsRelease;
		QString qsOS;
		QString qsOSVersion;

		std::string ssContext;
		QString qsIdentity;

		bool bVerified;
		QStringList qslEmail;

		HostAddress haAddress;

		/// Holds whether the user is using TCP
		/// or UDP for voice packets.
		///
		/// If the flag is 0, the user is using
		/// TCP.
		///
		/// If the flag is 1, the user is using
		/// UDP.
		QAtomicInt aiUdpFlag;

		QList<int> qlCodecs;
		bool bOpus;

		QStringList qslAccessTokens;

		QMap<int, WhisperTarget> qmTargets;
		typedef QPair<QSet<ServerUser *>, QSet<ServerUser *> > TargetCache;
		QMap<int, TargetCache> qmTargetCache;
		QMap<QString, QString> qmWhisperRedirect;

		LeakyBucket leakyBucket;

		int iLastPermissionCheck;
		QMap<int, unsigned int> qmPermissionSent;
#ifdef Q_OS_UNIX
		int sUdpSocket;
#else
		SOCKET sUdpSocket;
#endif
		BandwidthRecord bwr;
		struct sockaddr_storage saiUdpAddress;
		struct sockaddr_storage saiTcpLocalAddress;
		ServerUser(Server *parent, QSslSocket *socket);
};

#endif
