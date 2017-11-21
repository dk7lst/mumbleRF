/* Copyright (C) 2005-2011, Thorvald Natvig <thorvald@natvig.com>
   Copyright (C) 2010-2011, Mikkel Krautz <mikkel@krautz.dk>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "mumble_pch.hpp"
#import <ScriptingBridge/ScriptingBridge.h>
#include <Carbon/Carbon.h>
#include "Overlay.h"
#include "Global.h"
#include "MainWindow.h"

extern "C" {
#include <xar/xar.h>
}

static NSString *MumbleOverlayLoaderBundle = @"/Library/ScriptingAdditions/MumbleOverlay.osax";
static NSString *MumbleOverlayLoaderBundleIdentifier = @"net.sourceforge.mumble.OverlayScriptingAddition";

@interface OverlayInjectorMac : NSObject {
	BOOL active;
}
- (id) init;
- (void) dealloc;
- (void) appLaunched:(NSNotification *)notification;
- (void) setActive:(BOOL)flag;
- (void) eventDidFail:(const AppleEvent *)event withError:(NSError *)error;
@end

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
@interface OverlayInjectorMac () <SBApplicationDelegate>
@end
#endif

@implementation OverlayInjectorMac

- (id) init {
	self = [super init];

	if (self) {
		active = NO;
		NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
		[[workspace notificationCenter] addObserver:self
		                                selector:@selector(appLaunched:)
		                                name:NSWorkspaceDidLaunchApplicationNotification
		                                object:workspace];
		return self;
	}

	return nil;
}

- (void) dealloc {
	NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
	[[workspace notificationCenter] removeObserver:self
		                                name:NSWorkspaceDidLaunchApplicationNotification
		                                object:workspace];

	[super dealloc];
}

- (void) appLaunched:(NSNotification *)notification {
	if (active) {
		BOOL overlayEnabled = NO;
		NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
		NSDictionary *userInfo = [notification userInfo];

		NSString *bundleId = [userInfo objectForKey:@"NSApplicationBundleIdentifier"];
		if ([bundleId isEqualToString:[[NSBundle mainBundle] bundleIdentifier]])
			return;

		QString qsBundleIdentifier = QString::fromUtf8([bundleId UTF8String]);

		if (g.s.os.bUseWhitelist) {
			if (g.s.os.qslWhitelist.contains(qsBundleIdentifier))
				overlayEnabled = YES;
		} else {
			if (! g.s.os.qslBlacklist.contains(qsBundleIdentifier))
				overlayEnabled = YES;
		}

		if (overlayEnabled) {
			pid_t pid = [[userInfo objectForKey:@"NSApplicationProcessIdentifier"] intValue];
			SBApplication *app = [SBApplication applicationWithProcessIdentifier:pid];
			[app setDelegate:self];

			// This timeout is specified in 'ticks'.
			// A tick defined as: "[...] (a tick is approximately 1/60 of a second) [...]" in the
			// Apple Event Manager Refernce documentation:
			// http://developer.apple.com/legacy/mac/library/documentation/Carbon/reference/Event_Manager/Event_Manager.pdf
			[app setTimeout:10*60];

			[app setSendMode:kAEWaitReply];
			[app sendEvent:kASAppleScriptSuite id:kGetAEUT parameters:0];

			[app setSendMode:kAENoReply];
			if (QSysInfo::MacintoshVersion == QSysInfo::MV_LEOPARD) {
				[app sendEvent:'MUOL' id:'daol' parameters:0];
			} else if (QSysInfo::MacintoshVersion >= QSysInfo::MV_SNOWLEOPARD) {
				[app sendEvent:'MUOL' id:'load' parameters:0];
			}
		}

		[pool release];
	}
}

- (void) setActive:(BOOL)flag {
	active = flag;
}

// SBApplication delegate method
- (void)eventDidFail:(const AppleEvent *)event withError:(NSError *)error {
	Q_UNUSED(event);
	Q_UNUSED(error);

	// Do nothing. This method is only here to avoid an exception.
}

@end

class OverlayPrivateMac : public OverlayPrivate {
	protected:
		OverlayInjectorMac *olm;
	public:
		void setActive(bool);
		OverlayPrivateMac(QObject *);
		~OverlayPrivateMac();
};

OverlayPrivateMac::OverlayPrivateMac(QObject *p) : OverlayPrivate(p) {
	olm = [[OverlayInjectorMac alloc] init];
}

OverlayPrivateMac::~OverlayPrivateMac() {
	[olm release];
}

void OverlayPrivateMac::setActive(bool act) {
	[olm setActive:act];
}

void Overlay::platformInit() {
	d = new OverlayPrivateMac(this);
}

void Overlay::setActive(bool act) {
	static_cast<OverlayPrivateMac *>(d)->setActive(act);
}

bool OverlayConfig::supportsInstallableOverlay() {
	return true;
}

void OverlayClient::updateMouse() {
	QCursor c = qgv.viewport()->cursor();
	NSCursor *cursor = nil;
	Qt::CursorShape csShape = c.shape();

	switch (csShape) {
		case Qt::IBeamCursor:        cursor = [NSCursor IBeamCursor]; break;
		case Qt::CrossCursor:        cursor = [NSCursor crosshairCursor]; break;
		case Qt::ClosedHandCursor:   cursor = [NSCursor closedHandCursor]; break;
		case Qt::OpenHandCursor:     cursor = [NSCursor openHandCursor]; break;
		case Qt::PointingHandCursor: cursor = [NSCursor pointingHandCursor]; break;
		case Qt::SizeVerCursor:      cursor = [NSCursor resizeUpDownCursor]; break;
		case Qt::SplitVCursor:       cursor = [NSCursor resizeUpDownCursor]; break;
		case Qt::SizeHorCursor:      cursor = [NSCursor resizeLeftRightCursor]; break;
		case Qt::SplitHCursor:       cursor = [NSCursor resizeLeftRightCursor]; break;
		default:                     cursor = [NSCursor arrowCursor]; break;
	}

	QPixmap pm = qmCursors.value(csShape);
	if (pm.isNull()) {
		NSImage *img = [cursor image];
		CGImageRef cgimg = NULL;
		NSArray *reps = [img representations];
		for (NSUInteger i = 0; i < [reps count]; i++) {
			NSImageRep *rep = [reps objectAtIndex:i];
			if ([rep class] == [NSBitmapImageRep class]) {
				cgimg = [(NSBitmapImageRep *)rep CGImage];
			}
		}

		if (cgimg) {
			pm = QPixmap::fromMacCGImageRef(cgimg);
			qmCursors.insert(csShape, pm);
		}
	}

	NSPoint p = [cursor hotSpot];
	iOffsetX = (int) p.x;
	iOffsetY = (int) p.y;

	qgpiCursor->setPixmap(pm);
	qgpiCursor->setPos(iMouseX - iOffsetX, iMouseY - iOffsetY);
}

QString installerPath() {
	NSString *installerPath = [[NSBundle mainBundle] pathForResource:@"MumbleOverlay" ofType:@"pkg"];
	if (installerPath) {
		return QString::fromUtf8([installerPath UTF8String]);
	}
	return QString();
}

bool OverlayConfig::isInstalled() {
	bool ret = false;

	// Determine if the installed bundle is correctly installed (i.e. it's loadable)
	NSBundle *bundle = [NSBundle bundleWithPath:MumbleOverlayLoaderBundle];
	ret = [bundle preflightAndReturnError:NULL];

	// Do the bundle identifiers match?
	if (ret) {
		ret = [[bundle bundleIdentifier] isEqualToString:MumbleOverlayLoaderBundleIdentifier];
	}

	return ret;
}

// Check whether this installer installs something 'newer' than what we already have.
// Also checks whether the new installer is compatiable with the current version of
// Mumble.
static bool isInstallerNewer(QString path, NSUInteger curVer) {
	xar_t pkg = NULL;
	xar_iter_t iter = NULL;
	xar_file_t file = NULL;
	char *data = NULL;
	size_t size = 0;
	bool ret = false;
	QString qsMinVer, qsOverlayVer;

	pkg = xar_open(path.toUtf8().constData(), READ);
	if (pkg == NULL) {
		qWarning("isInstallerNewer: Unable to open pkg.");
		goto out;
	}

	iter = xar_iter_new();
	if (iter == NULL) {
		qWarning("isInstallerNewer: Unable to allocate iter");
		goto out;
	}

	file = xar_file_first(pkg, iter);
	while (file != NULL) {
		if (!strcmp(xar_get_path(file), "upgrade.xml"))
			break;
		file = xar_file_next(iter);
	}

	if (file != NULL) {
		if (xar_extract_tobuffersz(pkg, file, &data, &size) == -1) {
			goto out;
		}

		QXmlStreamReader reader(QByteArray::fromRawData(data, size));
		while (! reader.atEnd()) {
			QXmlStreamReader::TokenType tok = reader.readNext();
			if (tok == QXmlStreamReader::StartElement) {
				if (reader.name() == QLatin1String("upgrade")) {
					qsOverlayVer = reader.attributes().value(QLatin1String("version")).toString();
					qsMinVer = reader.attributes().value(QLatin1String("minclient")).toString();
				}
			}
		}

		if (reader.hasError() || qsMinVer.isNull() || qsOverlayVer.isNull()) {
			qWarning("isInstallerNewer: Error while parsing XML version info.");
			goto out;
		}

		NSUInteger newVer = qsOverlayVer.toUInt();

		QRegExp rx(QLatin1String("(\\d+)\\.(\\d+)\\.(\\d+)"));
		int major, minor, patch;
		int minmajor, minminor, minpatch;
		if (! rx.exactMatch(QLatin1String(MUMTEXT(MUMBLE_VERSION_STRING))))
			goto out;
		major = rx.cap(1).toInt();
		minor = rx.cap(2).toInt();
		patch = rx.cap(3).toInt();
		if (! rx.exactMatch(qsMinVer))
			goto out;
		minmajor = rx.cap(1).toInt();
		minminor = rx.cap(2).toInt();
		minpatch = rx.cap(3).toInt();

		ret = (major >= minmajor) && (minor >= minminor) && (patch >= minpatch) && (newVer > curVer);
	}

out:
	xar_close(pkg);
	xar_iter_free(iter);
	free(data);
	return ret;
}

bool OverlayConfig::needsUpgrade() {
	NSDictionary *infoPlist = [NSDictionary dictionaryWithContentsOfFile:[NSString stringWithFormat:@"%@/Contents/Info.plist", MumbleOverlayLoaderBundle]];
	if (infoPlist) {
		NSUInteger curVersion = [[infoPlist objectForKey:@"MumbleOverlayVersion"] unsignedIntegerValue];

		QString path = installerPath();
		if (path.isEmpty())
			return false;

		return isInstallerNewer(path, curVersion);
	}

	return false;
}

static bool authExec(AuthorizationRef ref, const char **argv) {
	OSStatus err;
	int pid, status;

	err = AuthorizationExecuteWithPrivileges(ref, argv[0], kAuthorizationFlagDefaults, const_cast<char * const *>(&argv[1]), NULL);
	if (err == errAuthorizationSuccess) {
		do {
			pid = wait(&status);
		} while (pid == -1 && errno == EINTR);
		return (pid != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0);
	}

	qWarning("Overlay_macx: Failed to AuthorizeExecuteWithPrivileges. (err=%i)", err);
	qWarning("Overlay_macx: Status: (pid=%i, exited=%u, exitStatus=%u)", pid, WIFEXITED(status), WEXITSTATUS(status));

	return false;
}

bool OverlayConfig::installFiles() {
	bool ret = false;
	OSStatus err;

	QString path = installerPath();
	if (path.isEmpty()) {
		qWarning("OverlayConfig: No installers found in search paths.");
		return false;
	}

	QProcess installer(this);
	QStringList args;
	args << QString::fromLatin1("-W");
	args << path;
	installer.start(QLatin1String("/usr/bin/open"), args, QIODevice::ReadOnly);

	while (!installer.waitForFinished(1000)) {
		qApp->processEvents();
	}

	return ret;
}

bool OverlayConfig::uninstallFiles() {
	AuthorizationRef auth;
	NSBundle *loaderBundle;
	bool ret = false, bundleOk = false;
	OSStatus err;

	// Load the installed loader bundle and check if it's something we're willing to uninstall.
	loaderBundle = [NSBundle bundleWithPath:MumbleOverlayLoaderBundle];
	bundleOk = [[loaderBundle bundleIdentifier] isEqualToString:MumbleOverlayLoaderBundleIdentifier];

	// Perform uninstallation using Authorization Services. (Pops up a dialog asking for admin privileges)
	if (bundleOk) {
		err = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &auth);
		if (err == errAuthorizationSuccess) {
			QByteArray tmp = QString::fromLatin1("/tmp/%1_Uninstalled_MumbleOverlay.osax").arg(QDateTime::currentMSecsSinceEpoch()).toLocal8Bit();
			const char *remove[] = { "/bin/mv", [MumbleOverlayLoaderBundle UTF8String], tmp.constData(), NULL };
			ret = authExec(auth, remove);
		}
		AuthorizationFree(auth, kAuthorizationFlagDefaults);
	}

	return ret;
}
