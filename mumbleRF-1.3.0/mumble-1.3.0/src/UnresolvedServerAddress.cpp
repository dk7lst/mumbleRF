// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "murmur_pch.h"

#include "UnresolvedServerAddress.h"

UnresolvedServerAddress::UnresolvedServerAddress()
	: port(0) {}

UnresolvedServerAddress::UnresolvedServerAddress(QString hostname_, unsigned short port_)
	: hostname(hostname_.toLower())
	, port(port_) {}

bool UnresolvedServerAddress::isValid() const {
	return !hostname.isEmpty() && port != 0;
}

bool operator==(const UnresolvedServerAddress &lhs, const UnresolvedServerAddress &rhs) {
	return lhs.hostname == rhs.hostname && lhs.port == rhs.port;
}

bool operator!=(const UnresolvedServerAddress &lhs, const UnresolvedServerAddress &rhs) {
	return !operator==(lhs, rhs);
}

bool operator<(const UnresolvedServerAddress &lhs, const UnresolvedServerAddress &rhs) {
	if (lhs.hostname < rhs.hostname) {
		return true;
	}
	if (lhs.hostname == rhs.hostname) {
		if (lhs.port < rhs.port) {
			return true;
		}
	}
	return false;
}

uint qHash(const UnresolvedServerAddress &key) {
	return qHash(key.hostname) ^ uint(key.port);
}
