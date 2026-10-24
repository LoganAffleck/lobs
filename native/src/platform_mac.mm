#include "platform.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

#include <QProcess>
#include <QSettings>

#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Security/Security.h>

namespace platform {

void reveal(const QString &path, bool select)
{
	NSURL *url = [NSURL fileURLWithPath:QDir::cleanPath(path).toNSString()];
	if (select)
		[[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[url]];
	else
		[[NSWorkspace sharedWorkspace] openURL:url];
}

void setHidden(const QString &, bool) {}

// One generic-password item in the login Keychain; settings.json only records that it's there.
static const char *kStoredInKeychain = "keychain";

static NSDictionary *keyQuery()
{
	return @{(__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
		 (__bridge id)kSecAttrService: @"LOBS",
		 (__bridge id)kSecAttrAccount: @"stream key"};
}

QString protectSecret(const QString &plain)
{
	SecItemDelete((__bridge CFDictionaryRef)keyQuery());
	if (plain.isEmpty())
		return {};
	NSMutableDictionary *add = [keyQuery() mutableCopy];
	add[(__bridge id)kSecValueData] = [plain.toNSString() dataUsingEncoding:NSUTF8StringEncoding];
	add[(__bridge id)kSecAttrLabel] = @"LOBS stream key";
	return SecItemAdd((__bridge CFDictionaryRef)add, nullptr) == errSecSuccess ? QString(kStoredInKeychain) : QString();
}

QString unprotectSecret(const QString &stored)
{
	if (stored != kStoredInKeychain)
		return {};
	NSMutableDictionary *q = [keyQuery() mutableCopy];
	q[(__bridge id)kSecReturnData] = @YES;
	q[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
	CFTypeRef data = nullptr;
	if (SecItemCopyMatching((__bridge CFDictionaryRef)q, &data) != errSecSuccess || !data)
		return {};
	const QString plain = QString::fromUtf8(static_cast<const char *>([(__bridge NSData *)data bytes]), int([(__bridge NSData *)data length]));
	CFRelease(data);
	return plain;
}

static AVMediaType mediaType(const QString &permission)
{
	return permission == "camera" ? AVMediaTypeVideo : AVMediaTypeAudio;
}

Access access(const QString &permission)
{
	if (permission == "screen" || permission == "hotkeys") {
		// macOS only says whether it's allowed; LOBS remembers whether it already asked.
		if (permission == "screen" ? CGPreflightScreenCaptureAccess() : CGPreflightListenEventAccess())
			return Access::Granted;
		return QSettings().value("permissions/" + permission + "Asked").toBool() ? Access::Denied : Access::NotAsked;
	}
	switch ([AVCaptureDevice authorizationStatusForMediaType:mediaType(permission)]) {
	case AVAuthorizationStatusAuthorized: return Access::Granted;
	case AVAuthorizationStatusNotDetermined: return Access::NotAsked;
	default: return Access::Denied; // denied, or restricted by a profile
	}
}

void requestAccess(const QString &permission, std::function<void()> answered)
{
	if (permission == "screen" || permission == "hotkeys") {
		QSettings().setValue("permissions/" + permission + "Asked", true);
		// The system's alert, which leads to System Settings.
		permission == "screen" ? CGRequestScreenCaptureAccess() : CGRequestListenEventAccess();
		answered();
		return;
	}
	[AVCaptureDevice requestAccessForMediaType:mediaType(permission)
				 completionHandler:^(BOOL) {
					 QMetaObject::invokeMethod(qApp, answered, Qt::QueuedConnection);
				 }];
}

void openPrivacySettings(const QString &permission)
{
	const char *pane = permission == "screen"    ? "Privacy_ScreenCapture"
			   : permission == "hotkeys" ? "Privacy_ListenEvent"
			   : permission == "camera"  ? "Privacy_Camera"
						     : "Privacy_Microphone";
	NSString *url = [NSString stringWithFormat:@"x-apple.systempreferences:com.apple.preference.security?%s", pane];
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:url]];
}

QVariantList captureDevices(const QString &kind)
{
	QVariantList out;
	if (kind == "display") {
		for (NSScreen *screen in NSScreen.screens) { // as mac-capture names them: "Name: WxH @ x,y"
			const CGDirectDisplayID id = [screen.deviceDescription[@"NSScreenNumber"] unsignedIntValue];
			CFUUIDRef uuid = CGDisplayCreateUUIDFromDisplayID(id);
			if (!uuid)
				continue;
			CFStringRef text = CFUUIDCreateString(kCFAllocatorDefault, uuid);
			const NSRect f = screen.frame;
			out << QVariantMap{{"name", QString("%1: %2x%3 @ %4,%5").arg(QString::fromNSString(screen.localizedName)).arg(int(f.size.width))
							   .arg(int(f.size.height)).arg(int(f.origin.x)).arg(int(f.origin.y))},
					   {"value", QString::fromCFString(text)}};
			CFRelease(text);
			CFRelease(uuid);
		}
	} else if (kind == "window") {
		// On-screen app windows with a title, "[App] Title", sorted like mac-capture's list.
		QList<QVariantMap> windows;
		if (CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID)) {
			for (NSDictionary *w in (__bridge NSArray *)list) {
				NSString *app = w[(__bridge NSString *)kCGWindowOwnerName], *title = w[(__bridge NSString *)kCGWindowName];
				if ([w[(__bridge NSString *)kCGWindowLayer] intValue] != 0 || !app.length || !title.length)
					continue;
				windows << QVariantMap{{"name", QString("[%1] %2").arg(QString::fromNSString(app), QString::fromNSString(title))},
						       {"value", QString::number([w[(__bridge NSString *)kCGWindowNumber] unsignedIntValue])}};
			}
			CFRelease(list);
		}
		std::sort(windows.begin(), windows.end(), [](const QVariantMap &a, const QVariantMap &b) {
			return a["name"].toString().compare(b["name"].toString(), Qt::CaseInsensitive) < 0;
		});
		for (const QVariantMap &w : windows)
			out << w;
	} else if (kind == "camera") {
		NSArray *types;
		if (@available(macOS 14, *))
			types = @[AVCaptureDeviceTypeBuiltInWideAngleCamera, AVCaptureDeviceTypeExternal, AVCaptureDeviceTypeDeskViewCamera];
		else
			types = @[AVCaptureDeviceTypeBuiltInWideAngleCamera, AVCaptureDeviceTypeExternalUnknown, AVCaptureDeviceTypeDeskViewCamera];
		for (AVMediaType media : @[AVMediaTypeVideo, AVMediaTypeMuxed]) // muxed: capture cards and the like
			for (AVCaptureDevice *d in [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:types mediaType:media
											    position:AVCaptureDevicePositionUnspecified].devices)
				out << QVariantMap{{"name", QString::fromNSString(d.localizedName)}, {"value", QString::fromNSString(d.uniqueID)}};
	}
	return out;
}

void relaunchAfterExit()
{
	const QString bundle = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../..");
	const QString script = QString("while kill -0 %1 2>/dev/null; do sleep 0.2; done; open -n \"$0\"").arg(QCoreApplication::applicationPid());
	QProcess::startDetached("/bin/sh", {"-c", script, bundle});
}

void fatalError(const QString &message)
{
	NSAlert *alert = [[NSAlert alloc] init];
	alert.alertStyle = NSAlertStyleCritical;
	alert.messageText = @"LOBS";
	alert.informativeText = message.toNSString();
	[alert runModal];
}

QString resourceDir() { return QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../Resources"); }

QString outputDir()
{
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	QDir().mkpath(dir);
	return dir;
}

} // namespace platform
