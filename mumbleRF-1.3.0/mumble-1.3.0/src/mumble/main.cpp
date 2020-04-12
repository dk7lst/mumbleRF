// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "mumble_pch.hpp"

#include "Overlay.h"
#include "MainWindow.h"
#include "ServerHandler.h"
#include "AudioInput.h"
#include "AudioOutput.h"
#include "AudioWizard.h"
#include "Cert.h"
#include "Database.h"
#include "Log.h"
#include "Plugins.h"
#include "LogEmitter.h"
#include "DeveloperConsole.h"
#include "Global.h"
#include "LCD.h"
#ifdef USE_BONJOUR
#include "BonjourClient.h"
#endif
#ifdef USE_DBUS
#include "DBus.h"
#endif
#ifdef USE_VLD
#include "vld.h"
#endif
#include "VersionCheck.h"
#include "NetworkConfig.h"
#include "CrashReporter.h"
#include "SocketRPC.h"
#include "SSL.h"
#include "MumbleApplication.h"
#include "ApplicationPalette.h"
#include "Themes.h"
#include "UserLockFile.h"
#include "License.h"
#include "EnvUtils.h"

#if defined(Q_OS_WIN) && defined(QT_NO_DEBUG)
#include <shellapi.h> // For CommandLineToArgvW()
#endif

#if defined(USE_STATIC_QT_PLUGINS) && QT_VERSION < 0x050000
Q_IMPORT_PLUGIN(qtaccessiblewidgets)
# ifdef Q_OS_WIN
   Q_IMPORT_PLUGIN(qico)
# endif
Q_IMPORT_PLUGIN(qsvg)
Q_IMPORT_PLUGIN(qsvgicon)
# ifdef Q_OS_MAC
   Q_IMPORT_PLUGIN(qicnsicon)
# endif
#endif

#ifdef BOOST_NO_EXCEPTIONS
namespace boost {
	void throw_exception(std::exception const &) {
		qFatal("Boost exception caught!");
	}
}
#endif

extern void os_init();
extern char *os_lang;

#ifdef Q_OS_WIN
// from os_early_win.cpp
extern int os_early_init();
// from os_win.cpp
extern HWND mumble_mw_hwnd;
#endif // Q_OS_WIN

#if defined(Q_OS_WIN) && !defined(QT_NO_DEBUG)
extern "C" __declspec(dllexport) int main(int argc, char **argv) {
#else
int main(int argc, char **argv) {
#endif
	int res = 0;

#if defined(Q_OS_WIN)
	int ret = os_early_init();
	if (ret != 0) {
		return ret;
	}
#endif

	QT_REQUIRE_VERSION(argc, argv, "4.4.0");

#if defined(Q_OS_WIN)
	SetDllDirectory(L"");
#else
#ifndef Q_OS_MAC
	EnvUtils::setenv(QLatin1String("AVAHI_COMPAT_NOWARN"), QLatin1String("1"));
#endif
#endif

	// Initialize application object.
	MumbleApplication a(argc, argv);
	a.setApplicationName(QLatin1String("Mumble"));
	a.setOrganizationName(QLatin1String("Mumble"));
	a.setOrganizationDomain(QLatin1String("mumble.sourceforge.net"));
	a.setQuitOnLastWindowClosed(false);

#if QT_VERSION >= 0x050100
	a.setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

#if QT_VERSION >= 0x050000 && defined(Q_OS_WIN)
	a.installNativeEventFilter(&a);
#endif

	MumbleSSL::initialize();

	#ifdef USE_SBCELT
	{
		QDir d(a.applicationVersionRootPath());
		QString helper = d.absoluteFilePath(QString::fromLatin1("sbcelt-helper"));
		EnvUtils::setenv(QLatin1String("SBCELT_HELPER_BINARY"), helper.toUtf8().constData());
	}
#endif

	Global::g_global_struct = new Global();

	qsrand(QDateTime::currentDateTime().toTime_t());

	g.le = QSharedPointer<LogEmitter>(new LogEmitter());
	g.c = new DeveloperConsole();

	os_init();

	bool bAllowMultiple = false;
	bool suppressIdentity = false;
	bool customJackClientName = false;
	bool bRpcMode = false;
	QString rpcCommand;
	QUrl url;
	if (a.arguments().count() > 1) {
		QStringList args = a.arguments();
		for (int i = 1; i < args.count(); ++i) {
			if (args.at(i) == QLatin1String("-h") || args.at(i) == QLatin1String("--help")
#if defined(Q_OS_WIN)
				|| args.at(i) == QLatin1String("/?")
#endif
			) {
				QString helpMessage = MainWindow::tr(
					"Usage: mumble [options] [<url>]\n"
					"\n"
					"<url> specifies a URL to connect to after startup instead of showing\n"
					"the connection window, and has the following form:\n"
					"mumble://[<username>[:<password>]@]<host>[:<port>][/<channel>[/<subchannel>...]][?version=<x.y.z>]\n"
					"\n"
					"The version query parameter has to be set in order to invoke the\n"
					"correct client version. It currently defaults to 1.2.0.\n"
					"\n"
					"Valid options are:\n"
					"  -h, --help    Show this help text and exit.\n"
					"  -m, --multiple\n"
					"                Allow multiple instances of the client to be started.\n"
					"  -n, --noidentity\n"
					"                Suppress loading of identity files (i.e., certificates.)\n"
					"  -jn, --jackname\n"
					"                Set custom Jack client name.\n"
					"  --license\n"
					"                Show the Mumble license.\n"
					"  --authors\n"
					"                Show the Mumble authors.\n"
					"  --third-party-licenses\n"
					"                Show licenses for third-party software used by Mumble.\n"
					"\n"
				);
				QString rpcHelpBanner = MainWindow::tr(
					"Remote controlling Mumble:\n"
					"\n"
				);
				QString rpcHelpMessage = MainWindow::tr(
					"Usage: mumble rpc <action> [options]\n"
					"\n"
					"It is possible to remote control a running instance of Mumble by using\n"
					"the 'mumble rpc' command.\n"
					"\n"
					"Valid actions are:\n"
					"  mute\n"
					"                Mute self\n"
					"  unmute\n"
					"                Unmute self\n"
					"  togglemute\n"
					"                Toggle self-mute status\n"
					"  deaf\n"
					"                Deafen self\n"
					"  undeaf\n"
					"                Undeafen self\n"
					"  toggledeaf\n"
					"                Toggle self-deafen status\n"
					"\n"
				);

				QString helpOutput = helpMessage + rpcHelpBanner + rpcHelpMessage;
				if (bRpcMode) {
					helpOutput = rpcHelpMessage;
				}

#if defined(Q_OS_WIN)
				QMessageBox::information(NULL, MainWindow::tr("Invocation"), helpOutput);
#else
				printf("%s", qPrintable(helpOutput));
#endif
				return 1;
			} else if (args.at(i) == QLatin1String("-m") || args.at(i) == QLatin1String("--multiple")) {
				bAllowMultiple = true;
			} else if (args.at(i) == QLatin1String("-n") || args.at(i) == QLatin1String("--noidentity")) {
				suppressIdentity = true;
				g.s.bSuppressIdentity = true;
			} else if (args.at(i) == QLatin1String("-jn") || args.at(i) == QLatin1String("--jackname")) {
				g.s.qsJackClientName = QString(args.at(i+1));
				customJackClientName = true;
			} else if (args.at(i) == QLatin1String("-license") || args.at(i) == QLatin1String("--license")) {
				printf("%s\n", qPrintable(License::license()));
				return 0;
			} else if (args.at(i) == QLatin1String("-authors") || args.at(i) == QLatin1String("--authors")) {
				printf("%s\n", qPrintable(License::authors()));
				return 0;
			} else if (args.at(i) == QLatin1String("-third-party-licenses") || args.at(i) == QLatin1String("--third-party-licenses")) {
				printf("%s", qPrintable(License::printableThirdPartyLicenseInfo()));
				return 0;
			} else if (args.at(i) == QLatin1String("rpc")) {
				bRpcMode = true;
				if (args.count() - 1 > i) {
					rpcCommand = QString(args.at(i + 1));
				}
				else {
					QString rpcError = MainWindow::tr("Error: No RPC command specified");
#if defined(Q_OS_WIN)
					QMessageBox::information(NULL, MainWindow::tr("RPC"), rpcError);
#else
					printf("%s\n", qPrintable(rpcError));
#endif
					return 1;
				}
			} else {
				if (!bRpcMode) {
					QUrl u = QUrl::fromEncoded(args.at(i).toUtf8());
					if (u.isValid() && (u.scheme() == QLatin1String("mumble"))) {
						url = u;
					} else {
						QFile f(args.at(i));
						if (f.exists()) {
							url = QUrl::fromLocalFile(f.fileName());
						}
					}
				}
			}
		}
	}

#ifdef USE_DBUS
#ifdef Q_OS_WIN
	// By default, windbus expects the path to dbus-daemon to be in PATH, and the path
	// should contain bin\\, and the path to the config is hardcoded as ..\etc

	{
		size_t reqSize;
		if (_wgetenv_s(&reqSize, NULL, 0, L"PATH") != 0)) {
			qWarning() << "Failed to get PATH. Not adding application directory to PATH. DBus bindings may not work.";
		} else if (reqSize > 0) {
			STACKVAR(wchar_t, buff, reqSize+1);
			if (_wgetenv_s(&reqSize, buff, reqSize, L"PATH") != 0) {
				qWarning() << "Failed to get PATH. Not adding application directory to PATH. DBus bindings may not work.";
			} else {
				QString path = QString::fromLatin1("%1;%2").arg(QDir::toNativeSeparators(MumbleApplication::instance()->applicationVersionRootPath())).arg(QString::fromWCharArray(buff));
				STACKVAR(wchar_t, buffout, path.length() + 1);
				path.toWCharArray(buffout);
				if (_wputenv_s(L"PATH", buffout) != 0) {
					qWarning() << "Failed to set PATH. DBus bindings may not work.";
				}
			}
		}
	}
#endif
#endif

	if (bRpcMode) {
		bool sent = false;
		QMap<QString, QVariant> param;
		param.insert(rpcCommand, rpcCommand);
		sent = SocketRPC::send(QLatin1String("Mumble"), QLatin1String("self"), param);
		if (sent) {
			return 0;
		} else {
			return 1;
		}
	}

	if (! bAllowMultiple) {
		if (url.isValid()) {
#ifndef USE_DBUS
			QMap<QString, QVariant> param;
			param.insert(QLatin1String("href"), url);
#endif
			bool sent = false;
#ifdef USE_DBUS
			QDBusInterface qdbi(QLatin1String("net.sourceforge.mumble.mumble"), QLatin1String("/"), QLatin1String("net.sourceforge.mumble.Mumble"));

			QDBusMessage reply=qdbi.call(QLatin1String("openUrl"), QLatin1String(url.toEncoded()));
			sent = (reply.type() == QDBusMessage::ReplyMessage);
#else
			sent = SocketRPC::send(QLatin1String("Mumble"), QLatin1String("url"), param);
#endif
			if (sent)
				return 0;
		} else {
			bool sent = false;
#ifdef USE_DBUS
			QDBusInterface qdbi(QLatin1String("net.sourceforge.mumble.mumble"), QLatin1String("/"), QLatin1String("net.sourceforge.mumble.Mumble"));

			QDBusMessage reply=qdbi.call(QLatin1String("focus"));
			sent = (reply.type() == QDBusMessage::ReplyMessage);
#else
			sent = SocketRPC::send(QLatin1String("Mumble"), QLatin1String("focus"));
#endif
			if (sent)
				return 0;

		}
	}

#ifdef Q_OS_WIN
	// The code above this block is somewhat racy, in that it might not
	// be possible to do RPC/DBus if two processes start at almost the
	// same time.
	//
	// In order to be completely sure we don't open multiple copies of
	// Mumble, we open a lock file. The file is opened without any sharing
	// modes enabled. This gives us exclusive access to the file.
	// If another Mumble instance attempts to open the file, it will fail,
	// and that instance will know to terminate itself.
	UserLockFile userLockFile(g.qdBasePath.filePath(QLatin1String("mumble.lock")));
	if (! bAllowMultiple) {
		if (!userLockFile.acquire()) {
			qWarning("Another process has already acquired the lock file at '%s'. Terminating...", qPrintable(userLockFile.path()));
			return 1;
		}
	}
#endif

	// Load preferences
	g.s.load();

	// Check whether we need to enable accessibility features
#ifdef Q_OS_WIN
	// Only windows for now. Could not find any information on how to query this for osx or linux
	{
		HIGHCONTRAST hc;
		hc.cbSize = sizeof(HIGHCONTRAST);
		SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(HIGHCONTRAST), &hc, 0);

		if (hc.dwFlags & HCF_HIGHCONTRASTON)
			g.s.bHighContrast = true;

	}
#endif

	DeferInit::run_initializers();

	ApplicationPalette applicationPalette;
	
	Themes::apply();

	QString qsSystemLocale = QLocale::system().name();

#ifdef Q_OS_MAC
	if (os_lang) {
		qWarning("Using Mac OS X system language as locale name");
		qsSystemLocale = QLatin1String(os_lang);
	}
#endif

	const QString locale = g.s.qsLanguage.isEmpty() ? qsSystemLocale : g.s.qsLanguage;
	qWarning("Locale is \"%s\" (System: \"%s\")", qPrintable(locale), qPrintable(qsSystemLocale));

	QTranslator translator;
	if (translator.load(QLatin1String(":mumble_") + locale))
		a.installTranslator(&translator);

	QTranslator loctranslator;
	if (loctranslator.load(QLatin1String("mumble_") + locale, a.applicationDirPath()))
		a.installTranslator(&loctranslator); // Can overwrite strings from bundled mumble translation

	// With modularization of Qt 5 some - but not all - of the qt_<locale>.ts files have become
	// so-called meta catalogues which no longer contain actual translations but refer to other
	// more specific ts files like qtbase_<locale>.ts . To successfully load a meta catalogue all
	// of its referenced translations must be available. As we do not want to bundle them all
	// we now try to load the old qt_<locale>.ts file first and then fall back to loading
	// qtbase_<locale>.ts if that failed.
	//
	// See http://doc.qt.io/qt-5/linguist-programmers.html#deploying-translations for more information
	QTranslator qttranslator;
	if (qttranslator.load(QLatin1String("qt_") + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath))) { // Try system qt translations
		a.installTranslator(&qttranslator);
	} else if (qttranslator.load(QLatin1String("qtbase_") + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
		a.installTranslator(&qttranslator);
	} else if (qttranslator.load(QLatin1String(":qt_") + locale)) { // Try bundled translations
		a.installTranslator(&qttranslator);
	} else if (qttranslator.load(QLatin1String(":qtbase_") + locale)) {
		a.installTranslator(&qttranslator);
	}
	
	// Initialize proxy settings
	NetworkConfig::SetupProxy();

	g.nam = new QNetworkAccessManager();

#ifndef NO_CRASH_REPORT
	CrashReporter *cr = new CrashReporter();
	cr->run();
	delete cr;
#endif

	// Initialize logger
	g.l = new Log();

	// Initialize database
	g.db = new Database(QLatin1String("main"));

#ifdef USE_BONJOUR
	// Initialize bonjour
	g.bc = new BonjourClient();
#endif

	g.o = new Overlay();
	g.o->setActive(g.s.os.bEnable);

	g.lcd = new LCD();

	// Process any waiting events before initializing our MainWindow.
	// The mumble:// URL support for Mac OS X happens through AppleEvents,
	// so we need to loop a little before we begin.
	a.processEvents();

	// Main Window
	g.mw=new MainWindow(NULL);
	g.mw->show();

#ifdef Q_OS_WIN
	// Set mumble_mw_hwnd in os_win.cpp.
	// Used by APIs in ASIOInput, DirectSound and GlobalShortcut_win that require a HWND.
	mumble_mw_hwnd = GetForegroundWindow();
#endif

#ifdef USE_DBUS
	new MumbleDBus(g.mw);
	QDBusConnection::sessionBus().registerObject(QLatin1String("/"), g.mw);
	QDBusConnection::sessionBus().registerService(QLatin1String("net.sourceforge.mumble.mumble"));
#endif

	SocketRPC *srpc = new SocketRPC(QLatin1String("Mumble"));

	g.l->log(Log::Information, MainWindow::tr("Welcome to Mumble."));

	// Plugins
	g.p = new Plugins(NULL);
	g.p->rescanPlugins();

	Audio::start();

	a.setQuitOnLastWindowClosed(false);

	// Configuration updates
	bool runaudiowizard = false;
	if (g.s.uiUpdateCounter == 0) {
		// Previous version was an pre 1.2.3 release or this is the first run
		runaudiowizard = true;

	} else if (g.s.uiUpdateCounter == 1) {
		// Previous versions used old idle action style, convert it

		if (g.s.iIdleTime == 5 * 60) { // New default
			g.s.iaeIdleAction = Settings::Nothing;
		} else {
			g.s.iIdleTime = 60 * qRound(g.s.iIdleTime / 60.); // Round to minutes
			g.s.iaeIdleAction = Settings::Deafen; // Old behavior
		}
	}

	if (runaudiowizard) {
		AudioWizard *aw = new AudioWizard(g.mw);
		aw->exec();
		delete aw;
	}

	g.s.uiUpdateCounter = 2;

	if (! CertWizard::validateCert(g.s.kpCertificate)) {
#if QT_VERSION >= 0x050000
		QDir qd(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
#else
		QDir qd(QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation));
#endif

		QFile qf(qd.absoluteFilePath(QLatin1String("MumbleAutomaticCertificateBackup.p12")));
		if (qf.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
			Settings::KeyPair kp = CertWizard::importCert(qf.readAll());
			qf.close();
			if (CertWizard::validateCert(kp))
				g.s.kpCertificate = kp;
		}
		if (! CertWizard::validateCert(g.s.kpCertificate)) {
			CertWizard *cw = new CertWizard(g.mw);
			cw->exec();
			delete cw;

			if (! CertWizard::validateCert(g.s.kpCertificate)) {
				g.s.kpCertificate = CertWizard::generateNewCert();
				if (qf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Unbuffered)) {
					qf.write(CertWizard::exportCert(g.s.kpCertificate));
					qf.close();
				}
			}
		}
	}

	if (QDateTime::currentDateTime().daysTo(g.s.kpCertificate.first.first().expiryDate()) < 14)
		g.l->log(Log::Warning, CertWizard::tr("<b>Certificate Expiry:</b> Your certificate is about to expire. You need to renew it, or you will no longer be able to connect to servers you are registered on."));

#ifdef QT_NO_DEBUG
#ifndef SNAPSHOT_BUILD
	if (g.s.bUpdateCheck)
#endif
		new VersionCheck(true, g.mw);
#ifdef SNAPSHOT_BUILD
	new VersionCheck(false, g.mw, true);
#endif
#else
	g.mw->msgBox(MainWindow::tr("Skipping version check in debug mode."));
#endif
	if (g.s.bPluginCheck) {
		g.p->checkUpdates();
	}

	if (url.isValid()) {
		OpenURLEvent *oue = new OpenURLEvent(url);
		qApp->postEvent(g.mw, oue);
#ifdef Q_OS_MAC
	} else if (! a.quLaunchURL.isEmpty()) {
		OpenURLEvent *oue = new OpenURLEvent(a.quLaunchURL);
		qApp->postEvent(g.mw, oue);
#endif
	} else {
		g.mw->on_qaServerConnect_triggered(true);
	}

	if (! g.bQuit)
		res=a.exec();

	g.s.save();

	url.clear();
	
	ServerHandlerPtr sh = g.sh;
	if (sh && sh->isRunning()) {
		url = sh->getServerURL();
		g.db->setShortcuts(g.sh->qbaDigest, g.s.qlShortcuts);
	}

	Audio::stop();

	if (sh)
		sh->disconnect();

	delete srpc;

	g.sh.reset();
	while (sh && ! sh.unique())
		QThread::yieldCurrentThread();
	sh.reset();

	delete g.mw;

	delete g.nam;
	delete g.lcd;

	delete g.db;
	delete g.p;
	delete g.l;

#ifdef USE_BONJOUR
	delete g.bc;
#endif

	delete g.o;

	delete g.c;
	g.le.clear();

	DeferInit::run_destroyers();

	delete Global::g_global_struct;
	Global::g_global_struct = NULL;

#ifndef QT_NO_DEBUG
	// Hide Qt memory leak.
	QSslSocket::setDefaultCaCertificates(QList<QSslCertificate>());
	// Release global protobuf memory allocations.
#if (GOOGLE_PROTOBUF_VERSION >= 2001000)
	google::protobuf::ShutdownProtobufLibrary();
#endif
#endif

#ifdef Q_OS_WIN
	// Release the userLockFile.
	//
	// It is important that we release it before we attempt to
	// restart Mumble (if requested). If we do not release it
	// before that, the new instance might not be able to start
	// correctly.
	userLockFile.release();
#endif

	// Tear down OpenSSL state.
	MumbleSSL::destroy();
	
	// At this point termination of our process is immenent. We can safely
	// launch another version of Mumble. The reason we do an actual
	// restart instead of re-creating our data structures is that making
	// sure we won't leave state is quite tricky. Mumble has quite a
	// few spots which might not consider seeing to basic initializations.
	// Until we invest the time to verify this, rather be safe (and a bit slower)
	// than sorry (and crash/bug out). Also take care to reconnect if possible.
	if (res == MUMBLE_EXIT_CODE_RESTART) {
		QStringList arguments;
		
		if (bAllowMultiple)   arguments << QLatin1String("--multiple");
		if (suppressIdentity) arguments << QLatin1String("--noidentity");
		if (customJackClientName) arguments << QLatin1String("--jackname ") + g.s.qsJackClientName;
		if (!url.isEmpty())   arguments << url.toString();
		
		qWarning() << "Triggering restart of Mumble with arguments: " << arguments;
		
#ifdef Q_OS_WIN
		// Work around bug related to QTBUG-7645. Mumble has uiaccess=true set
		// on windows which makes normal CreateProcess calls (like Qt uses in
		// startDetached) fail unless they specifically enable additional priviledges.
		// Note that we do not actually require user interaction by UAC nor full admin
		// rights but only the right token on launch. Here we use ShellExecuteEx
		// which handles this transparently for us.
		const std::wstring applicationFilePath = qApp->applicationFilePath().toStdWString();
		const std::wstring argumentsString = arguments.join(QLatin1String(" ")).toStdWString();
		
		SHELLEXECUTEINFO si;
		ZeroMemory(&si, sizeof(SHELLEXECUTEINFO));
		si.cbSize = sizeof(SHELLEXECUTEINFO);
		si.lpFile = applicationFilePath.data();
		si.lpParameters = argumentsString.data();
		
		bool ok = (ShellExecuteEx(&si) == TRUE);
#else
		bool ok = QProcess::startDetached(qApp->applicationFilePath(), arguments);
#endif
		if(!ok) {
			QMessageBox::warning(NULL,
			                     QApplication::tr("Failed to restart mumble"),
			                     QApplication::tr("Mumble failed to restart itself. Please restart it manually.")
			);
			return 1;
		}
		return 0;
	}
	return res;
}

#if defined(Q_OS_WIN) && defined(QT_NO_DEBUG)
extern "C" __declspec(dllexport) int MumbleMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdArg, int cmdShow) {
	Q_UNUSED(instance)
	Q_UNUSED(prevInstance)
	Q_UNUSED(cmdArg)
	Q_UNUSED(cmdShow)

	int argc;
	wchar_t **argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argvW == Q_NULLPTR) {
		return -1;
	}

	QVector<QByteArray> argvS;
	argvS.reserve(argc);

	QVector<char *> argvV(argc, Q_NULLPTR);
	for (int i = 0; i < argc; ++i) {
		argvS.append(QString::fromWCharArray(argvW[i]).toLocal8Bit());
		argvV[i] = argvS.back().data();
	}

	LocalFree(argvW);

	return main(argc, argvV.data());
}
#endif
