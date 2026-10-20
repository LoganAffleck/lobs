"""Generates synthetic LOBS projects for performance tests (qa/TEST_PLAN.md, Phase 3).

  python -I qa/tools/gen_project.py heavy  <out dir>
  python -I qa/tools/gen_project.py stress <out dir>

Writes <out dir>/<profile>/<profile>.lobs.json plus an assets folder of small PNGs.
Uses the same JSON shape LOBS saves (version 1). Deterministic (seeded).
Device inputs stay realistic: one camera, one display capture, one mic, one desktop audio;
everything else is graphics (color, text, image) so the load is the editor and compositor,
not hardware.
"""
import json, os, random, struct, sys, zlib

PROFILES = {
    # scenes, extra sources, layers per scene, group depth, filter-chain length, cam-linked scenes
    "heavy":  dict(scenes=40,  sources=60,  per_scene=7,  depth=4,  chain=10, chain_layers=20, cam_links=30),
    "stress": dict(scenes=200, sources=500, per_scene=10, depth=10, chain=30, chain_layers=20, cam_links=100),
}
TAGS = ["#e5484d", "#f76b15", "#ffc53d", "#46a758", "#12a594", "#0090ff", "#8e4ec6", "#d6409f"]
FILTERS = {
    "color": {"brightness": 0.0, "contrast": 0.1, "saturation": 0.1, "gamma": 0.0, "hue": 0.0},
    "sharpness": {"amount": 0.08},
}

rng = random.Random(1234)
_n = 0
def uid(prefix):
    global _n
    _n += 1
    return f"{prefix}{_n:08x}"

def png(path, w, h, rgb):
    raw = b"".join(b"\x00" + bytes(rgb) * w for _ in range(h))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) \
        + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(data)

def props(x, y, w, h, filters=None):
    return {"transform": {"x": x, "y": y, "w": w, "h": h, "rot": 0, "flipH": False, "flipV": False},
            "crop": {"l": 0, "t": 0, "r": 0, "b": 0, "radius": 0},
            "filters": filters or [], "visibility": {"visible": True, "opacity": 1}}

def main():
    profile, out = sys.argv[1], sys.argv[2]
    p = PROFILES[profile]
    root = os.path.join(out, profile)
    assets = os.path.join(root, "assets")
    os.makedirs(assets, exist_ok=True)
    images = []
    for k in range(8):
        f = os.path.join(assets, f"img{k}.png")
        png(f, 320, 180, (40 + 25 * k, 120, 200 - 20 * k))
        images.append(f.replace("\\", "/"))

    sources, source_order = {}, []
    def add_source(kind, name, settings, has_audio=False):
        sid = uid("src_")
        sources[sid] = {"id": sid, "name": name, "kind": kind, "bin": "", "color": TAGS[len(source_order) % 8],
                        "settings": settings, "hasAudio": has_audio,
                        "audio": {"volume": 1, "muted": False, "monitor": "off", "tracks": [True] + [False] * 5,
                                  "filters": [], "mono": False, "balance": 0.5, "syncMs": 0}}
        source_order.append(sid)
        return sid

    cam = add_source("camera", "Facecam", {})
    disp = add_source("display", "Display", {})
    add_source("mic", "Mic", {}, True)
    add_source("desktop_audio", "Desktop Audio", {}, True)
    graphics = []
    for k in range(p["sources"]):
        kind = ("color", "text", "image")[k % 3]
        settings = {"color": f"#{rng.randrange(0xffffff):06x}"} if kind == "color" else \
                   {"text": f"Label {k}", "size": 96} if kind == "text" else {"file": images[k % len(images)]}
        graphics.append((add_source(kind, f"{kind.title()} {k}", settings), kind))

    scenes, scene_order, instances, links = {}, [], {}, {}
    def place(scene_id, sid, x, y, w, h, filters=None):
        iid = uid("ins_")
        instances[iid] = {"id": iid, "sceneId": scene_id, "kind": "source", "sourceId": sid,
                          "name": sources[sid]["name"], "locked": False, "props": props(x, y, w, h, filters), "links": {}}
        return iid
    def group(scene_id, children, name):
        gid = uid("ins_")
        instances[gid] = {"id": gid, "sceneId": scene_id, "kind": "group", "name": name, "locked": False,
                          "children": children, "props": props(0, 0, 1920, 1080), "links": {}}
        return gid

    cam_instances = []
    chain_left = p["chain_layers"]
    for s in range(p["scenes"]):
        sid = uid("scn_")
        layers = []
        bg = add_source("color", f"Background {s + 1}", {"color": "#101418", "background": True})
        # Graphics layers, some with long filter chains.
        for k in range(p["per_scene"]):
            gsrc, kind = graphics[(s * p["per_scene"] + k) % len(graphics)]
            w, h = (600, 120) if kind == "text" else (320 + 40 * (k % 5), 180 + 20 * (k % 5))
            filters = None
            if chain_left > 0 and k == 0:
                chain_left -= 1
                filters = [{"id": uid("flt_"), "type": t, "enabled": True, "settings": FILTERS[t]}
                           for t in (list(FILTERS) * p["chain"])[:p["chain"]]]
            layers.append(place(sid, gsrc, 60 + 37 * k, 60 + 29 * k, w, h, filters))
        # A nested group chain in every 4th scene.
        if s % 4 == 0 and len(layers) >= 2:
            inner = layers[-2:]
            layers = layers[:-2]
            node = group(sid, inner, "Group 1")
            for d in range(2, p["depth"] + 1):
                extra = place(sid, graphics[(s + d) % len(graphics)][0], 100 + 10 * d, 700, 200, 112)
                node = group(sid, [extra, node], f"Group {d}")
            layers.append(node)
        if s < p["cam_links"]:
            cam_instances.append(place(sid, cam, 1400, 700, 480, 270))
            layers.insert(0, cam_instances[-1])
        if s % 10 == 0:
            layers.append(place(sid, disp, 0, 0, 1920, 1080))
        layers.append(place(sid, bg, 0, 0, 1920, 1080))
        scenes[sid] = {"id": sid, "name": f"Scene {s + 1}", "folder": "", "favorite": False, "layers": layers}
        scene_order.append(sid)

    # The camera is linked (all groups) across its scenes.
    for g in ("transform", "crop", "filters", "visibility"):
        lid = uid("lnk_")
        first = instances[cam_instances[0]]["props"]
        links[lid] = {"id": lid, "group": g, "value": first[g]}
        for iid in cam_instances:
            instances[iid]["links"][g] = lid

    doc = {"version": 1, "canvas": {"w": 1920, "h": 1080}, "sources": sources, "sourceOrder": source_order,
           "scenes": scenes, "sceneOrder": scene_order, "instances": instances, "links": links, "bins": [],
           "swatches": ["#ffffff", "#000000"], "currentTransition": "tr_fade",
           "transitions": [{"id": "tr_fade", "name": "Fade", "type": "fade", "durationMs": 300, "settings": {}},
                           {"id": "tr_cut", "name": "Cut", "type": "cut", "durationMs": 0, "settings": {}}]}
    path = os.path.join(root, f"{profile}.lobs.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=1)
    n_filters = sum(len(i["props"]["filters"]) for i in instances.values())
    print(json.dumps({"path": path, "scenes": len(scenes), "sources": len(sources), "instances": len(instances),
                      "groups": sum(1 for i in instances.values() if i["kind"] == "group"),
                      "filters": n_filters, "camLinkedScenes": len(cam_instances),
                      "bytes": os.path.getsize(path)}))

if __name__ == "__main__":
    main()
