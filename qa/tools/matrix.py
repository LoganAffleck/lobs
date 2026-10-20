"""Story matrix: the latest result of every story across qa/runs/*/<scenario>/results.json.

  python -I qa/tools/matrix.py            prints a Markdown table
"""
import glob, json, os, sys
sys.stdout.reconfigure(encoding="utf-8")

qa = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
latest = {}
for path in sorted(glob.glob(os.path.join(qa, "runs", "*", "*", "results.json"))):
    run = os.path.basename(os.path.dirname(os.path.dirname(path)))
    scenario = os.path.basename(os.path.dirname(path))
    try:
        r = json.load(open(path, encoding="utf-8"))
    except Exception:
        continue
    for s in r.get("stories", []):
        if s["id"].startswith(("_", "probe", "H-")):
            continue
        status = s.get("status")
        if status == "running":
            status = "died"
        fails = [c["name"] for c in s.get("checks", []) if not c.get("ok")]
        latest[s["id"]] = dict(id=s["id"], title=s.get("title", ""), status=status, run=run, scenario=scenario,
                               checks=len(s.get("checks", [])), fails=fails)

def key(i):
    return i.replace("PF-0", "PF-").lower()

print("| Story | Title | Result | Checks | Failed checks | Run |")
print("|---|---|---|---|---|---|")
for k in sorted(latest, key=key):
    s = latest[k]
    mark = {"pass": "✅ pass", "fail": "❌ fail", "blocked": "⛔ blocked", "died": "💥 died", "nochecks": "—"}.get(s["status"], s["status"])
    f = "; ".join(s["fails"])[:160].replace("|", "/")
    print(f"| {s['id']} | {s['title']} | {mark} | {s['checks']} | {f} | `{s['run']}/{s['scenario']}` |")
counts = {}
for s in latest.values():
    counts[s["status"]] = counts.get(s["status"], 0) + 1
print()
print("Totals: " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())), file=sys.stderr)
