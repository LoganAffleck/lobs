// The few things LOBS asks of the operating system directly (platform_win.cpp, platform_mac.mm).
#pragma once

#include <QString>

#include <functional>

namespace platform {

// Shows a file selected in File Explorer / Finder, or opens a folder (select = false).
void reveal(const QString &path, bool select);
// Hides a file from File Explorer. Dotfiles are already hidden in Finder, so this does nothing there.
void setHidden(const QString &path, bool hidden);
// The stream key at rest: DPAPI-encrypted on Windows, a login Keychain item on macOS. What
// protectSecret returns is what settings.json stores; unprotectSecret reads it back.
QString protectSecret(const QString &plain);
QString unprotectSecret(const QString &stored);
// Privacy permissions: "microphone", "camera" and "screen" (Screen Recording, which covers
// display, window and desktop audio capture). macOS asks the user for each once; Windows
// always answers Granted. None of these block (see permissions.h).
enum class Access { Granted, Denied, NotAsked };
Access access(const QString &permission);
// Shows the system's prompt (only possible while NotAsked); answered runs on the GUI thread.
void requestAccess(const QString &permission, std::function<void()> answered);
// System Settings > Privacy & Security, at that permission's list.
void openPrivacySettings(const QString &permission);
// A message box for errors that stop LOBS before its UI exists.
void fatalError(const QString &message);
// Read-only app files (fonts, icons): next to LOBS.exe, or LOBS.app/Contents/Resources.
QString resourceDir();
// Files LOBS writes about itself (lobs.log, test reports): next to LOBS.exe, or
// ~/Library/Application Support/LOBS/LOBS, since a signed app bundle must not change.
QString outputDir();

} // namespace platform
