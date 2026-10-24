// Engine version helpers shared by every platform (the engine store itself is Windows-only).
#include "engine_manager.h"

#include <obs-config.h>

#include <QList>

namespace engine_store {

int compareVersions(const QString &a, const QString &b)
{
	// "32.2.2" vs "33.0.0-rc2": numeric parts first, then a release beats any pre-release.
	const auto split = [](const QString &v) {
		const QString core = v.section('-', 0, 0), pre = v.section('-', 1);
		QList<int> n;
		for (const QString &p : core.split('.'))
			n << p.toInt();
		while (n.size() < 3)
			n << 0;
		return std::pair{n, pre};
	};
	const auto [na, pa] = split(a);
	const auto [nb, pb] = split(b);
	for (int i = 0; i < 3; ++i)
		if (na[i] != nb[i])
			return na[i] < nb[i] ? -1 : 1;
	if (pa == pb) return 0;
	if (pa.isEmpty()) return 1;
	if (pb.isEmpty()) return -1;
	return QString::compare(pa, pb) < 0 ? -1 : 1; // beta < rc, rc1 < rc2
}

int supportedMajor() { return LIBOBS_API_MAJOR_VER; }

} // namespace engine_store
