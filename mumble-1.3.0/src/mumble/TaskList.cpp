// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "mumble_pch.hpp"

#include "TaskList.h"

#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>

extern bool bIsWin7;

void TaskList::addToRecentList(QString name, QString user, QString host, int port) {
	if (! bIsWin7)
		return;

	HRESULT hr;
	IShellLink *link = NULL;
	IPropertyStore *ps = NULL;
	PROPVARIANT pt;

	hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, __uuidof(IShellLink), reinterpret_cast<void **>(&link));
	if (!link || FAILED(hr))
		return;

	QUrl url;
	url.setScheme(QLatin1String("mumble"));
	url.setUserName(user);
	url.setHost(host);
	url.setPort(port);

#if QT_VERSION >= 0x050000
	QUrlQuery query;
	query.addQueryItem(QLatin1String("title"), name);
	query.addQueryItem(QLatin1String("version"), QLatin1String("1.2.0"));
	url.setQuery(query);
#else
	url.addQueryItem(QLatin1String("title"), name);
	url.addQueryItem(QLatin1String("version"), QLatin1String("1.2.0"));
#endif

	QSettings settings(QLatin1String("HKEY_CLASSES_ROOT"), QSettings::NativeFormat);

	QString app = settings.value(QLatin1String("mumble/DefaultIcon/.")).toString();
	if (app.isEmpty() || ! QFileInfo(app).exists())
		app = QCoreApplication::applicationFilePath();

	link->SetPath(app.toStdWString().c_str());
	link->SetArguments(QString::fromLatin1(url.toEncoded()).toStdWString().c_str());

	hr = link->QueryInterface(__uuidof(IPropertyStore), reinterpret_cast<void **>(&ps));
	if (FAILED(hr)) {
		qFatal("TaskList: Failed to get property store");
		goto cleanup;
	}

	InitPropVariantFromString(name.toStdWString().c_str(), &pt);
	hr = ps->SetValue(PKEY_Title, pt);
	PropVariantClear(&pt);

	if (FAILED(hr)) {
		qFatal("TaskList: Failed to set title");
		goto cleanup;
	}

	hr = ps->Commit();
	if (FAILED(hr)) {
		qFatal("TaskList: Failed commit");
		goto cleanup;
	}

	SHAddToRecentDocs(SHARD_LINK, link);

cleanup:
	if (ps)
		ps->Release();
	if (link)
		link->Release();
}
