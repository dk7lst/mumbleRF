// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "mumble_pch.hpp"

#include "Plugins.h"

#include "Log.h"
#include "MainWindow.h"
#include "Message.h"
#include "ServerHandler.h"
#include "../../plugins/mumble_plugin.h"
#include "WebFetch.h"
#include "MumbleApplication.h"
#include "ManualPlugin.h"

// We define a global macro called 'g'. This can lead to issues when included code uses 'g' as a type or parameter name (like protobuf 3.7 does). As such, for now, we have to make this our last include.
#include "Global.h"

static ConfigWidget *PluginConfigDialogNew(Settings &st) {
	return new PluginConfig(st);
}

static ConfigRegistrar registrar(5000, PluginConfigDialogNew);

struct PluginInfo {
	bool locked;
	bool enabled;
	QLibrary lib;
	QString filename;
	QString description;
	QString shortname;
	MumblePlugin *p;
	MumblePlugin2 *p2;
	MumblePluginQt *pqt;
	PluginInfo();
};

PluginInfo::PluginInfo() {
	locked = false;
	enabled = false;
	p = NULL;
	p2 = NULL;
	pqt = NULL;
}

struct PluginFetchMeta {
	QString hash;
	QString path;
	
	PluginFetchMeta(const QString &hash_ = QString(), const QString &path_ = QString())
		: hash(hash_)
		, path(path_) { /* Empty */ }
};


PluginConfig::PluginConfig(Settings &st) : ConfigWidget(st) {
	setupUi(this);

#if QT_VERSION >= 0x050000
	qtwPlugins->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	qtwPlugins->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
#else
	qtwPlugins->header()->setResizeMode(0, QHeaderView::Stretch);
	qtwPlugins->header()->setResizeMode(1, QHeaderView::ResizeToContents);
#endif

	refillPluginList();
}

QString PluginConfig::title() const {
	return tr("Plugins");
}

QIcon PluginConfig::icon() const {
	return QIcon(QLatin1String("skin:config_plugin.png"));
}

void PluginConfig::load(const Settings &r) {
	loadCheckBox(qcbTransmit, r.bTransmitPosition);
}

void PluginConfig::save() const {
	QReadLocker lock(&g.p->qrwlPlugins);

	s.bTransmitPosition = qcbTransmit->isChecked();
	s.qmPositionalAudioPlugins.clear();

	QList<QTreeWidgetItem *> list = qtwPlugins->findItems(QString(), Qt::MatchContains);
	foreach(QTreeWidgetItem *i, list) {
		bool enabled = (i->checkState(1) == Qt::Checked);

		PluginInfo *pi = pluginForItem(i);
		if (pi) {
			s.qmPositionalAudioPlugins.insert(pi->filename, enabled);
			pi->enabled = enabled;
		}
	}
}

PluginInfo *PluginConfig::pluginForItem(QTreeWidgetItem *i) const {
	if (i) {
		foreach(PluginInfo *pi, g.p->qlPlugins) {
			if (pi->filename == i->data(0, Qt::UserRole).toString())
				return pi;
		}
	}
	return NULL;
}

void PluginConfig::on_qpbConfig_clicked() {
	PluginInfo *pi;
	{
		QReadLocker lock(&g.p->qrwlPlugins);
		pi = pluginForItem(qtwPlugins->currentItem());
	}

	if (! pi)
		return;

	if (pi->pqt && pi->pqt->config) {
		pi->pqt->config(this);
	} else if (pi->p->config) {
		pi->p->config(0);
	} else {
		QMessageBox::information(this, QLatin1String("Mumble"), tr("Plugin has no configure function."), QMessageBox::Ok, QMessageBox::NoButton);
	}
}

void PluginConfig::on_qpbAbout_clicked() {
	PluginInfo *pi;
	{
		QReadLocker lock(&g.p->qrwlPlugins);
		pi = pluginForItem(qtwPlugins->currentItem());
	}

	if (! pi)
		return;

	if (pi->pqt && pi->pqt->about) {
		pi->pqt->about(this);
	} else if (pi->p->about) {
		pi->p->about(0);
	} else {
		QMessageBox::information(this, QLatin1String("Mumble"), tr("Plugin has no about function."), QMessageBox::Ok, QMessageBox::NoButton);
	}
}

void PluginConfig::on_qpbReload_clicked() {
	g.p->rescanPlugins();
	refillPluginList();
}

void PluginConfig::refillPluginList() {
	QReadLocker lock(&g.p->qrwlPlugins);
	qtwPlugins->clear();

	foreach(PluginInfo *pi, g.p->qlPlugins) {
		QTreeWidgetItem *i = new QTreeWidgetItem(qtwPlugins);
		i->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
		i->setCheckState(1, pi->enabled ? Qt::Checked : Qt::Unchecked);
		i->setText(0, pi->description);
		if (pi->p->longdesc)
			i->setToolTip(0, Qt::escape(QString::fromStdWString(pi->p->longdesc())));
		i->setData(0, Qt::UserRole, pi->filename);
	}
	qtwPlugins->setCurrentItem(qtwPlugins->topLevelItem(0));
	on_qtwPlugins_currentItemChanged(qtwPlugins->topLevelItem(0), NULL);
}

void PluginConfig::on_qtwPlugins_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *) {
	QReadLocker lock(&g.p->qrwlPlugins);

	PluginInfo *pi=pluginForItem(current);
	if (pi) {
		bool showAbout = false;
		if (pi->p->about) {
			showAbout = true;
		}
		if (pi->pqt && pi->pqt->about) {
			showAbout = true;
		}
		qpbAbout->setEnabled(showAbout);

		bool showConfig = false;
		if (pi->p->config) {
			showConfig = true;
		}
		if (pi->pqt && pi->pqt->config) {
			showConfig = true;
		}
		qpbConfig->setEnabled(showConfig);
	} else {
		qpbAbout->setEnabled(false);
		qpbConfig->setEnabled(false);
	}
}

Plugins::Plugins(QObject *p) : QObject(p) {
	QTimer *timer=new QTimer(this);
	timer->setObjectName(QLatin1String("Timer"));
	timer->start(500);
	locked = prevlocked = NULL;
	bValid = false;
	iPluginTry = 0;
	for (int i=0;i<3;i++)
		fPosition[i]=fFront[i]=fTop[i]= 0.0;
	QMetaObject::connectSlotsByName(this);

#ifdef QT_NO_DEBUG
#ifndef PLUGIN_PATH
	qsSystemPlugins=QString::fromLatin1("%1/plugins").arg(MumbleApplication::instance()->applicationVersionRootPath());
#ifdef Q_OS_MAC
	qsSystemPlugins=QString::fromLatin1("%1/../Plugins").arg(qApp->applicationDirPath());
#endif
#else
	qsSystemPlugins=QLatin1String(MUMTEXT(PLUGIN_PATH));
#endif

	qsUserPlugins = g.qdBasePath.absolutePath() + QLatin1String("/Plugins");
#else
	qsSystemPlugins = QString::fromLatin1("%1/plugins").arg(MumbleApplication::instance()->applicationVersionRootPath());
	qsUserPlugins = QString();
#endif

#ifdef Q_OS_WIN
	// According to MS KB Q131065, we need this to OpenProcess()

	hToken = NULL;

	if (!OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &hToken)) {
		if (GetLastError() == ERROR_NO_TOKEN) {
			ImpersonateSelf(SecurityImpersonation);
			OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &hToken);
		}
	}

	TOKEN_PRIVILEGES tp;
	LUID luid;
	cbPrevious=sizeof(TOKEN_PRIVILEGES);

	LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid);

	tp.PrivilegeCount           = 1;
	tp.Privileges[0].Luid       = luid;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), &tpPrevious, &cbPrevious);
#endif
}

Plugins::~Plugins() {
	clearPlugins();

#ifdef Q_OS_WIN
	AdjustTokenPrivileges(hToken, FALSE, &tpPrevious, cbPrevious, NULL, NULL);
	CloseHandle(hToken);
#endif
}

void Plugins::clearPlugins() {
	QWriteLocker lock(&g.p->qrwlPlugins);
	foreach(PluginInfo *pi, qlPlugins) {
		if (pi->locked)
			pi->p->unlock();
		pi->lib.unload();
		delete pi;
	}
	qlPlugins.clear();
}

void Plugins::rescanPlugins() {
	clearPlugins();

	QWriteLocker lock(&g.p->qrwlPlugins);
	prevlocked = locked = NULL;
	bValid = false;

	QDir qd(qsSystemPlugins, QString(), QDir::Name, QDir::Files | QDir::Readable);
#ifdef QT_NO_DEBUG
	QDir qud(qsUserPlugins, QString(), QDir::Name, QDir::Files | QDir::Readable);
	QFileInfoList libs = qud.entryInfoList() + qd.entryInfoList();
#else
	QFileInfoList libs = qd.entryInfoList();
#endif

	QSet<QString> evaluated;
	foreach(const QFileInfo &libinfo, libs) {
		QString fname = libinfo.fileName();
		QString libname = libinfo.absoluteFilePath();
		if (!evaluated.contains(fname) && QLibrary::isLibrary(libname)) {
			PluginInfo *pi = new PluginInfo();
			pi->lib.setFileName(libname);
			pi->filename = fname;
			if (pi->lib.load()) {
				mumblePluginFunc mpf = reinterpret_cast<mumblePluginFunc>(pi->lib.resolve("getMumblePlugin"));
				if (mpf) {
					evaluated.insert(fname);
					pi->p = mpf();

					// Check whether the plugin has a valid plugin magic and that it's not retracted.
					// A retracted plugin is a plugin that clients should disregard, typically because
					// the game the plugin was written for now provides positional audio via the
					// link plugin (see null_plugin.cpp).
					if (pi->p && pi->p->magic == MUMBLE_PLUGIN_MAGIC && pi->p->shortname != L"Retracted") {

						pi->description = QString::fromStdWString(pi->p->description);
						pi->shortname = QString::fromStdWString(pi->p->shortname);
						pi->enabled = g.s.qmPositionalAudioPlugins.value(pi->filename, true);

						mumblePlugin2Func mpf2 = reinterpret_cast<mumblePlugin2Func>(pi->lib.resolve("getMumblePlugin2"));
						if (mpf2) {
							pi->p2 = mpf2();
							if (pi->p2->magic != MUMBLE_PLUGIN_MAGIC_2) {
								pi->p2 = NULL;
							}
						}

						mumblePluginQtFunc mpfqt = reinterpret_cast<mumblePluginQtFunc>(pi->lib.resolve("getMumblePluginQt"));
						if (mpfqt) {
							pi->pqt = mpfqt();
							if (pi->pqt->magic != MUMBLE_PLUGIN_MAGIC_QT) {
								pi->pqt = NULL;
							}
						}

						qlPlugins << pi;
						continue;
					}
				}
				pi->lib.unload();
			} else {
				qWarning("Plugins: Failed to load %s: %s", qPrintable(pi->filename), qPrintable(pi->lib.errorString()));
			}
			delete pi;
		}
	}

	// Handle built-in plugins
	{
#if defined(USE_MANUAL_PLUGIN)
		// Manual plugin
		PluginInfo *pi = new PluginInfo();
		pi->filename = QLatin1String("manual.builtin");
		pi->p = ManualPlugin_getMumblePlugin();
		pi->pqt = ManualPlugin_getMumblePluginQt();
		pi->description = QString::fromStdWString(pi->p->description);
		pi->shortname = QString::fromStdWString(pi->p->shortname);
		pi->enabled = g.s.qmPositionalAudioPlugins.value(pi->filename, true);
		qlPlugins << pi;
#endif
	}
}

bool Plugins::fetch() {
	if (g.bPosTest) {
		fPosition[0] = fPosition[1] = fPosition[2] = 0.0f;
		fFront[0] = 0.0f;
		fFront[1] = 0.0f;
		fFront[2] = 1.0f;
		fTop[0] = 0.0f;
		fTop[1] = 1.0f;
		fTop[2] = 0.0f;

		for (int i=0;i<3;++i) {
			fCameraPosition[i] = fPosition[i];
			fCameraFront[i] = fFront[i];
			fCameraTop[i] = fTop[i];
		}

		bValid = true;
		return true;
	}

	if (! locked) {
		bValid = false;
		return bValid;
	}

	QReadLocker lock(&qrwlPlugins);
	if (! locked) {
		bValid = false;
		return bValid;
	}

	if (!locked->enabled)
		bUnlink = true;

	bool ok;
	{
		QMutexLocker mlock(&qmPluginStrings);
		ok = locked->p->fetch(fPosition, fFront, fTop, fCameraPosition, fCameraFront, fCameraTop, ssContext, swsIdentity);
	}
	if (! ok || bUnlink) {
		lock.unlock();
		QWriteLocker wlock(&qrwlPlugins);

		if (locked) {
			locked->p->unlock();
			locked->locked = false;
			prevlocked = locked;
			locked = NULL;
			for (int i=0;i<3;i++)
				fPosition[i]=fFront[i]=fTop[i]=fCameraPosition[i]=fCameraFront[i]=fCameraTop[i] = 0.0f;
		}
	}
	bValid = ok;
	return bValid;
}

void Plugins::on_Timer_timeout() {
	fetch();

	QReadLocker lock(&qrwlPlugins);

	if (prevlocked) {
		g.l->log(Log::Information, tr("%1 lost link.").arg(Qt::escape(prevlocked->shortname)));
		prevlocked = NULL;
	}


	{
		QMutexLocker mlock(&qmPluginStrings);

		if (! locked) {
			ssContext.clear();
			swsIdentity.clear();
		}

		std::string context;
		if (locked)
			context.assign(u8(QString::fromStdWString(locked->p->shortname)) + static_cast<char>(0) + ssContext);

		if (! g.uiSession) {
			ssContextSent.clear();
			swsIdentitySent.clear();
		} else if ((context != ssContextSent) || (swsIdentity != swsIdentitySent)) {
			MumbleProto::UserState mpus;
			mpus.set_session(g.uiSession);
			if (context != ssContextSent) {
				ssContextSent.assign(context);
				mpus.set_plugin_context(context);
			}
			if (swsIdentity != swsIdentitySent) {
				swsIdentitySent.assign(swsIdentity);
				mpus.set_plugin_identity(u8(QString::fromStdWString(swsIdentitySent)));
			}
			if (g.sh)
				g.sh->sendMessage(mpus);
		}
	}

	if (locked) {
		return;
	}

	if (! g.s.bTransmitPosition)
		return;

	lock.unlock();
	QWriteLocker wlock(&qrwlPlugins);

	if (qlPlugins.isEmpty())
		return;

	++iPluginTry;
	if (iPluginTry >= qlPlugins.count())
		iPluginTry = 0;

	std::multimap<std::wstring, unsigned long long int> pids;
#if defined(Q_OS_WIN)
	PROCESSENTRY32 pe;

	pe.dwSize = sizeof(pe);
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap != INVALID_HANDLE_VALUE) {
		BOOL ok = Process32First(hSnap, &pe);

		while (ok) {
			pids.insert(std::pair<std::wstring, unsigned long long int>(std::wstring(pe.szExeFile), pe.th32ProcessID));
			ok = Process32Next(hSnap, &pe);
		}
		CloseHandle(hSnap);
	}
#elif defined(Q_OS_LINUX)
	QDir d(QLatin1String("/proc"));
	QStringList entries = d.entryList();
	bool ok;
	foreach (const QString &entry, entries) {
		// Check if the entry is a PID
		// by checking whether it's a number.
		// If it is not, skip it.
		unsigned long long int pid = static_cast<unsigned long long int>(entry.toLongLong(&ok, 10));
		if (!ok) {
			continue;
		}

		QString exe = QFile::symLinkTarget(QString(QLatin1String("/proc/%1/exe")).arg(entry));
		QFileInfo fi(exe);
		QString firstPart = fi.baseName();
		QString completeSuffix = fi.completeSuffix();
		QString baseName;
		if (completeSuffix.isEmpty()) {
			baseName = firstPart;
		} else {
			baseName = firstPart + QLatin1String(".") + completeSuffix;
		}

		if (baseName == QLatin1String("wine-preloader") || baseName == QLatin1String("wine64-preloader")) {
			QFile f(QString(QLatin1String("/proc/%1/cmdline")).arg(entry));
			if (f.open(QIODevice::ReadOnly)) {
				QByteArray cmdline = f.readAll();
				f.close();

				int nul = cmdline.indexOf('\0');
				if (nul != -1) {
					cmdline.truncate(nul);
				}

				QString exe = QString::fromUtf8(cmdline);
				if (exe.contains(QLatin1String("\\"))) {
					int lastBackslash = exe.lastIndexOf(QLatin1String("\\"));
					if (exe.count() > lastBackslash + 1) {
						baseName = exe.mid(lastBackslash + 1);
					}
				}
			}
		}

		if (!baseName.isEmpty()) {
			pids.insert(std::pair<std::wstring, unsigned long long int>(baseName.toStdWString(), pid));
		}
	}
#endif

	PluginInfo *pi = qlPlugins.at(iPluginTry);
	if (pi->enabled) {
		if (pi->p2 ? pi->p2->trylock(pids) : pi->p->trylock()) {
			pi->shortname = QString::fromStdWString(pi->p->shortname);
			g.l->log(Log::Information, tr("%1 linked.").arg(Qt::escape(pi->shortname)));
			pi->locked = true;
			bUnlink = false;
			locked = pi;
		}
	}
}

void Plugins::checkUpdates() {
	QUrl url;
	url.setPath(QLatin1String("/v1/pa-plugins"));

	QList<QPair<QString, QString> > queryItems;
	queryItems << qMakePair(QString::fromUtf8("ver"), QString::fromUtf8(QUrl::toPercentEncoding(QString::fromUtf8(MUMBLE_RELEASE))));
#if defined(Q_OS_WIN)
# if defined(Q_OS_WIN64)
	queryItems << qMakePair(QString::fromUtf8("os"), QString::fromUtf8("WinX64"));
# else
	queryItems << qMakePair(QString::fromUtf8("os"), QString::fromUtf8("Win32"));
# endif
	queryItems << qMakePair(QString::fromUtf8("abi"), QString::fromUtf8(MUMTEXT(_MSC_VER)));
#elif defined(Q_OS_MAC)
# if defined(USE_MAC_UNIVERSAL)
	queryItems << qMakePair(QString::fromUtf8("os"), QString::fromUtf8("MacOSX-Universal"));
# else
	queryItems << qMakePair(QString::fromUtf8("os"), QString::fromUtf8("MacOSX"));
# endif
#else
	queryItems << qMakePair(QString::fromUtf8("os"), QString::fromUtf8("Unix"));
#endif


#ifdef QT_NO_DEBUG
#if QT_VERSION >= 0x050000
	QUrlQuery query;
	query.setQueryItems(queryItems);
	url.setQuery(query);
#else
	for (int i = 0; i < queryItems.size(); i++) {
		const QPair<QString, QString> &queryPair = queryItems.at(i);
		url.addQueryItem(queryPair.first, queryPair.second);
	}
#endif
	WebFetch::fetch(QLatin1String("update"), url, this, SLOT(fetchedUpdatePAPlugins(QByteArray,QUrl)));
#else
	g.mw->msgBox(tr("Skipping plugin update in debug mode."));
#endif
}

void Plugins::fetchedUpdatePAPlugins(QByteArray data, QUrl) {
	if (data.isNull())
		return;

	bool rescan = false;
	qmPluginFetchMeta.clear();
	QDomDocument doc;
	doc.setContent(data);

	QDomElement root=doc.documentElement();
	QDomNode n = root.firstChild();
	while (!n.isNull()) {
		QDomElement e = n.toElement();
		if (!e.isNull()) {
			if (e.tagName() == QLatin1String("plugin")) {
				QString name = QFileInfo(e.attribute(QLatin1String("name"))).fileName();
				QString hash = e.attribute(QLatin1String("hash"));
				QString path = e.attribute(QLatin1String("path"));
				qmPluginFetchMeta.insert(name, PluginFetchMeta(hash, path));
			}
		}
		n = n.nextSibling();
	}

	QDir qd(qsSystemPlugins, QString(), QDir::Name, QDir::Files | QDir::Readable);
	QDir qdu(qsUserPlugins, QString(), QDir::Name, QDir::Files | QDir::Readable);

	QFileInfoList libs = qd.entryInfoList();
	foreach(const QFileInfo &libinfo, libs) {
		QString libname = libinfo.absoluteFilePath();
		QString filename = libinfo.fileName();
		PluginFetchMeta pfm = qmPluginFetchMeta.value(filename);
		QString wanthash = pfm.hash;
		if (! wanthash.isNull() && QLibrary::isLibrary(libname)) {
			QFile f(libname);
			if (wanthash.isEmpty()) {
				// Outdated plugin
				if (f.exists()) {
					clearPlugins();
					f.remove();
					rescan=true;
				}
			} else if (f.open(QIODevice::ReadOnly)) {
				QString h = QLatin1String(sha1(f.readAll()).toHex());
				f.close();
				if (h == wanthash) {
					if (qd != qdu) {
						QFile qfuser(qsUserPlugins + QString::fromLatin1("/") + filename);
						if (qfuser.exists()) {
							clearPlugins();
							qfuser.remove();
							rescan=true;
						}
					}
					// Mark for removal from userplugins
					qmPluginFetchMeta.insert(filename, PluginFetchMeta());
				}
			}
		}
	}

	if (qd != qdu) {
		libs = qdu.entryInfoList();
		foreach(const QFileInfo &libinfo, libs) {
			QString libname = libinfo.absoluteFilePath();
			QString filename = libinfo.fileName();
			PluginFetchMeta pfm = qmPluginFetchMeta.value(filename);
			QString wanthash = pfm.hash;
			if (! wanthash.isNull() && QLibrary::isLibrary(libname)) {
				QFile f(libname);
				if (wanthash.isEmpty()) {
					// Outdated plugin
					if (f.exists()) {
						clearPlugins();
						f.remove();
						rescan=true;
					}
				} else if (f.open(QIODevice::ReadOnly)) {
					QString h = QLatin1String(sha1(f.readAll()).toHex());
					f.close();
					if (h == wanthash) {
						qmPluginFetchMeta.remove(filename);
					}
				}
			}
		}
	}
	QMap<QString, PluginFetchMeta>::const_iterator i;
	for (i = qmPluginFetchMeta.constBegin(); i != qmPluginFetchMeta.constEnd(); ++i) {
		PluginFetchMeta pfm = i.value();
		if (! pfm.hash.isEmpty()) {
			QUrl pluginDownloadUrl;
			if (pfm.path.isEmpty()) {
				pluginDownloadUrl.setPath(QString::fromLatin1("%1").arg(i.key()));
			} else {
				pluginDownloadUrl.setPath(pfm.path);
			}

			WebFetch::fetch(QLatin1String("pa-plugin-dl"), pluginDownloadUrl, this, SLOT(fetchedPAPluginDL(QByteArray,QUrl)));
		}
	}

	if (rescan)
		rescanPlugins();
}

void Plugins::fetchedPAPluginDL(QByteArray data, QUrl url) {
	if (data.isNull())
		return;

	bool rescan = false;

	const QString &urlPath = url.path();
	QString fname = QFileInfo(urlPath).fileName();
	if (qmPluginFetchMeta.contains(fname)) {
		PluginFetchMeta pfm = qmPluginFetchMeta.value(fname);
		if (pfm.hash == QLatin1String(sha1(data).toHex())) {
			bool verified = true;
#ifdef Q_OS_WIN
			verified = false;
			QString tempname;
			std::wstring tempnative;
			{
				QTemporaryFile temp(QDir::tempPath() + QLatin1String("/plugin_XXXXXX.dll"));
				if (temp.open()) {
					tempname = temp.fileName();
					tempnative = QDir::toNativeSeparators(tempname).toStdWString();
					temp.write(data);
					temp.setAutoRemove(false);
				}
			}
			if (! tempname.isNull()) {
				WINTRUST_FILE_INFO file;
				ZeroMemory(&file, sizeof(file));
				file.cbStruct = sizeof(file);
				file.pcwszFilePath = tempnative.c_str();

				WINTRUST_DATA data;
				ZeroMemory(&data, sizeof(data));
				data.cbStruct = sizeof(data);
				data.dwUIChoice = WTD_UI_NONE;
				data.fdwRevocationChecks = WTD_REVOKE_NONE;
				data.dwUnionChoice = WTD_CHOICE_FILE;
				data.pFile = &file;
				data.dwProvFlags = WTD_SAFER_FLAG | WTD_USE_DEFAULT_OSVER_CHECK;
				data.dwUIContext = WTD_UICONTEXT_INSTALL;

				static GUID guid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

				LONG ts = WinVerifyTrust(0, &guid , &data);

				QFile deltemp(tempname);
				deltemp.remove();
				verified = (ts == 0);
			}
#endif
			if (verified) {
				clearPlugins();

				QFile f;
				f.setFileName(qsSystemPlugins + QLatin1String("/") + fname);
				if (f.open(QIODevice::WriteOnly)) {
					f.write(data);
					f.close();
					g.mw->msgBox(tr("Downloaded new or updated plugin to %1.").arg(Qt::escape(f.fileName())));
				} else {
					f.setFileName(qsUserPlugins + QLatin1String("/") + fname);
					if (f.open(QIODevice::WriteOnly)) {
						f.write(data);
						f.close();
						g.mw->msgBox(tr("Downloaded new or updated plugin to %1.").arg(Qt::escape(f.fileName())));
					} else {
						g.mw->msgBox(tr("Failed to install new plugin to %1.").arg(Qt::escape(f.fileName())));
					}
				}

				rescan=true;
			}
		}
	}

	if (rescan)
		rescanPlugins();
}
