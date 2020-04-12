// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_GROUP_H_
#define MUMBLE_GROUP_H_

#include <QtCore/QSet>

class Channel;
class User;
class ServerUser;

class Group {
	private:
		Q_DISABLE_COPY(Group)
	public:
		Channel *c;
		QString qsName;
		bool bInherit;
		bool bInheritable;
		QSet<int> qsAdd;
		QSet<int> qsRemove;
		QSet<int> qsTemporary;
		Group(Channel *assoc, const QString &name);

#ifdef MURMUR
		QSet<int> members();
		static QSet<QString> groupNames(Channel *c);
		static Group *getGroup(Channel *c, QString name);

		static bool isMember(Channel *c, Channel *aclChan, QString name, ServerUser *);
#endif
};

#endif
