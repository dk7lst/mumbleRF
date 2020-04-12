// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MUMBLE_DATABASE_H_
#define MUMBLE_MUMBLE_DATABASE_H_

#include <QSqlDatabase>
#include "Settings.h"
#include "UnresolvedServerAddress.h"

struct FavoriteServer {
	QString qsName;
	QString qsUsername;
	QString qsPassword;
	QString qsHostname;
	QString qsUrl;
	unsigned short usPort;
};

class Database : public QObject {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(Database)

		QSqlDatabase db;
	public:
		Database(const QString &dbname);
		~Database() Q_DECL_OVERRIDE;

		QList<FavoriteServer> getFavorites();
		void setFavorites(const QList<FavoriteServer> &servers);
		void setPassword(const QString &host, unsigned short port, const QString &user, const QString &pw);
		bool fuzzyMatch(QString &name, QString &user, QString &pw, QString &host, unsigned short port);

		bool isLocalIgnored(const QString &hash);
		void setLocalIgnored(const QString &hash, bool ignored);

		bool isLocalMuted(const QString &hash);
		void setLocalMuted(const QString &hash, bool muted);

		float getUserLocalVolume(const QString &hash);
		void setUserLocalVolume(const QString &hash, float volume);

		bool isChannelFiltered(const QByteArray &server_cert_digest, const int channel_id);
		void setChannelFiltered(const QByteArray &server_cert_digest, const int channel_id, bool hidden);

		QMap<UnresolvedServerAddress, unsigned int> getPingCache();
		void setPingCache(const QMap<UnresolvedServerAddress, unsigned int> &cache);

		bool seenComment(const QString &hash, const QByteArray &commenthash);
		void setSeenComment(const QString &hash, const QByteArray &commenthash);

		QByteArray blob(const QByteArray &hash);
		void setBlob(const QByteArray &hash, const QByteArray &blob);

		QStringList getTokens(const QByteArray &digest);
		void setTokens(const QByteArray &digest, QStringList &tokens);

		QList<Shortcut> getShortcuts(const QByteArray &digest);
		bool setShortcuts(const QByteArray &digest, QList<Shortcut> &shortcuts);

		void addFriend(const QString &name, const QString &hash);
		void removeFriend(const QString &hash);
		const QString getFriend(const QString &hash);
		const QMap<QString, QString> getFriends();

		const QString getDigest(const QString &hostname, unsigned short port);
		void setDigest(const QString &hostname, unsigned short port, const QString &digest);

		bool getUdp(const QByteArray &digest);
		void setUdp(const QByteArray &digest, bool udp);
};

#endif
