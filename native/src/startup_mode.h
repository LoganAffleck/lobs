#pragma once

#include <QStringList>

// Self-tests run on a throwaway document so they never touch the user's show. A run is a
// self-test only when one of these exact flags is among the arguments (argv[0], the exe path,
// is never looked at: a folder or user name containing "test" must not count).
inline bool isSelfTestRun(const QStringList &arguments)
{
	static const QStringList flags = {"--autotest", "--assettest", "--browsertest", "--engine-smoketest", "--enginetest", "--featuretest",
					  "--foldertest", "--importtest", "--scenariotest", "--snapshot", "--streamtest", "--texttest", "--tooltest"};
	const QStringList args = arguments.mid(1);
	for (const QString &a : args)
		if (flags.contains(a))
			return true;
	return false;
}
