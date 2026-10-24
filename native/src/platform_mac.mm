#include "platform.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

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
	if (permission == "screen") {
		// macOS only says whether it's allowed; LOBS remembers whether it already asked.
		if (CGPreflightScreenCaptureAccess())
			return Access::Granted;
		return QSettings().value("permissions/screenAsked").toBool() ? Access::Denied : Access::NotAsked;
	}
	switch ([AVCaptureDevice authorizationStatusForMediaType:mediaType(permission)]) {
	case AVAuthorizationStatusAuthorized: return Access::Granted;
	case AVAuthorizationStatusNotDetermined: return Access::NotAsked;
	default: return Access::Denied; // denied, or restricted by a profile
	}
}

void requestAccess(const QString &permission, std::function<void()> answered)
{
	if (permission == "screen") {
		QSettings().setValue("permissions/screenAsked", true);
		CGRequestScreenCaptureAccess(); // the system's alert, which leads to System Settings
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
	const char *pane = permission == "screen" ? "Privacy_ScreenCapture" : permission == "camera" ? "Privacy_Camera" : "Privacy_Microphone";
	NSString *url = [NSString stringWithFormat:@"x-apple.systempreferences:com.apple.preference.security?%s", pane];
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:url]];
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
