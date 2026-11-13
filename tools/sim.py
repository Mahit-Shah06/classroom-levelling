#!/usr/bin/env python3
"""Headless model of the in-game director (UNarrativeSubsystem). Plays thousands of synthetic students
through Content/Data and reports which ending each play style reaches. Used to tune ending conditions.
Usage: tools/sim.py [runs_per_style]   (mirrors the C++ rules; keep both in sync)"""
import glob, json, os, random, re, sys
from collections import Counter, defaultdict

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content", "Data")
STATS = ["Intellect", "Charisma", "Physique", "Sanity", "Cunning", "Reputation", "Funds", "Luck"]
AVG = {"obedience": .30, "empathy": .25, "ambition": .20, "cunning": .10, "betrayal": .05, "chaos": .05, "withdrawal": .05}
ORIGINS = [[46, 30, 26, 36, 30, 24, 14, 30], [30, 34, 26, 28, 28, 38, 50, 34], [24, 32, 50, 38, 24, 32, 22, 30],
           [32, 30, 32, 30, 48, 16, 22, 32], [28, 32, 28, 34, 26, 26, 24, 52], [32, 32, 32, 36, 32, 30, 28, 32]]
# Play styles: tag -> how much this kind of player likes choices carrying it.
STYLES = {
    "average":  {"obedience": 1.0, "empathy": .9, "ambition": .8, "loyalty": .6, "honesty": .6, "cunning": .3},
    "masher":   None,  # always the first option, main path only
    "random":   {},
    "schemer":  {"cunning": 1.2, "ambition": .7, "betrayal": .5, "withdrawal": .3},
    "loner":    {"withdrawal": 1.3, "cunning": .4, "honesty": .2},
    "rebel":    {"chaos": 1.0, "empathy": 1.0, "honesty": .6, "loyalty": .4},
    "clown":    {"humor": 1.3, "chaos": 1.0},
    "friend":   {"loyalty": 1.2, "empathy": 1.0, "honesty": .5},
    "traitor":  {"betrayal": 1.3, "cruelty": 1.0, "ambition": .5},
    "contrarian": "contrarian",  # picks whatever the System did NOT forecast
}
MIX = {"average": .55, "masher": .12, "random": .08, "schemer": .05, "loner": .03, "rebel": .04, "clown": .03, "friend": .05, "traitor": .03, "contrarian": .02}


def load():
    npcs, lets = {}, []
    for f in sorted(glob.glob(os.path.join(ROOT, "*.json"))):
        d = json.load(open(f))
        if os.path.basename(f).startswith("npc"):
            for n in d: npcs[n["Id"]] = n
        else: lets += d
    return npcs, lets


class Run:
    def __init__(s, npcs, lets, style, rng):
        s.npcs, s.lets, s.style, s.rng = npcs, lets, style, rng
        s.flags, s.tags, s.played, s.rel = set(), defaultdict(float), set(), defaultdict(float)
        s.level, s.xp, s.hits, s.total, s.slots = 1, 0.0, 0, 0, {}
        base = rng.choice(ORIGINS)
        s.sub = {f"{STATS[i]}.{k}": min(100, max(1, base[i] + rng.uniform(-6, 6))) for i in range(8) for k in "abc"}
        studs = [k for k, v in npcs.items() if v.get("Kind", "student") == "student"]; rng.shuffle(studs)
        staff = [k for k, v in npcs.items() if v.get("Kind", "student") != "student"]; rng.shuffle(staff)
        s.roles = {}
        for pool, roles in ((studs, ["rival", "ally", "gremlin", "president", "ex1", "broker"]), (staff, ["head", "homeroom", "proctor", "nurse", "coach", "counsellor"])):
            for r in roles:
                pick = next((p for p in pool if not npcs[p].get("RoleFit") or r in npcs[p]["RoleFit"]), pool[0] if pool else None)
                if pick: s.roles[r] = pick; pool.remove(pick)

    def general(s, name):
        v = [x for k, x in s.sub.items() if k.startswith(name + ".")]
        return sum(v) / len(v) if v else 0.0

    def stat(s, key): return s.sub[key] if key in s.sub else s.general(key)

    def add(s, key, d):
        if key not in s.sub: s.sub[key] = s.general(key.split(".")[0]) if "." in key else 0.0
        s.sub[key] = min(100.0, max(0.0, s.sub[key] + d))

    def resolve(s, ref):
        if not ref.startswith("@"): return ref
        return s.slots.get(ref[1:]) or s.roles.get(ref[1:])

    def tagtotal(s): return sum(max(0, v) for k, v in s.tags.items() if k != "hesitation")

    def uniq(s):
        t = s.tagtotal()
        if t <= 0: return 0.0
        keys = set(AVG) | {k for k in s.tags if k != "hesitation"}
        return sum((max(0, s.tags.get(k, 0)) / t - AVG.get(k, 0)) ** 2 for k in keys) ** .5

    def score(s): return sum(s.general(n) for n in STATS) + (s.level - 1) * 35

    def rank(s):
        mine, r = s.score(), 1
        for n in s.npcs.values():
            if n.get("Kind", "student") != "student": continue
            g = defaultdict(list)
            for k, v in n["Stats"]["Sub"].items(): g[k.split(".")[0]].append(v)
            if sum(sum(v) / len(v) for v in g.values()) > mine: r += 1
        return r

    def cond(s, c):
        c = c.strip(); neg = c.startswith("!"); c = c.lstrip("!")
        if c.startswith("flag:"): res = c[5:] in s.flags
        else:
            m = re.match(r"(.*?)\s*(>=|<=|==|>|<)\s*(-?[\d.]+)", c)
            if not m: return False
            lhs, op, rhs = m.group(1), m.group(2), float(m.group(3))
            kind, _, key = lhs.partition(":")
            L = {"stat": lambda: s.stat(key), "rel": lambda: s.rel.get(s.resolve(key), 0.0), "tag": lambda: s.tags.get(key, 0.0),
                 "uniq": s.uniq, "pred": lambda: s.hits / s.total if s.total else 1.0, "level": lambda: s.level, "share": lambda: max(0, s.tags.get(key, 0)) / s.tagtotal() if s.tagtotal() else 0.0,
                 "rush": lambda: 1.0 if s.style == "masher" and s.total else 0.0, "rank": s.rank}.get(kind, lambda: 0.0)()
            res = {">=": L >= rhs, "<=": L <= rhs, "==": abs(L - rhs) < 1e-4, ">": L > rhs, "<": L < rhs}[op]
        return res != neg

    def ok(s, conds): return all(s.cond(c) for c in conds or [])

    def find(s, kind=None, loc=None):
        best, bs = None, -1e9
        for x in s.lets:
            k = x.get("Kind")
            if kind and k != kind: continue
            if loc and x.get("Location") != loc: continue
            if not kind and k == "talk": continue
            if x.get("bOnce", True) and x["Id"] in s.played: continue
            if not s.ok(x.get("Conditions")): continue
            sc = x.get("Priority", 0) + len(x.get("Conditions") or []) * .5 + (s.rng.random() * 15 if k == "talk" else 0)
            if sc > bs: best, bs = x, sc
        return best

    def predict(s, choices):
        if len(choices) < 2: return -1
        t = s.tagtotal(); w = t / (t + 4)
        def sc(c): return sum(v * (w * (max(0, s.tags.get(k, 0)) / t if t else 0) + (1 - w) * AVG.get(k, 0)) for k, v in (c.get("Tags") or {}).items())
        return max(range(len(choices)), key=lambda i: (sc(choices[i]), -i))

    def pick(s, choices, forecast):
        if s.style == "masher": return 0
        if s.style == "contrarian":
            opts = [i for i in range(len(choices)) if i != forecast] or [0]
            return s.rng.choice(opts)
        like = STYLES[s.style]
        w = [max(.05, .25 + sum(like.get(k, 0) * v for k, v in (c.get("Tags") or {}).items())) ** 2 for c in choices]
        return s.rng.choices(range(len(choices)), w)[0]

    def play(s, x):
        while x:
            s.played.add(x["Id"])
            sel = {"best_friend": lambda: max(s.npcs, key=lambda k: s.rel[k]), "worst_enemy": lambda: min(s.npcs, key=lambda k: s.rel[k])}
            s.slots = {}
            for k, v in (x.get("Slots") or {}).items():
                s.slots[k] = sel[v]() if v in sel else s.roles.get(v[5:]) if v.startswith("role:") else getattr(s, "talk", None) if v == "talk_target" else None
                s.slots[k] = s.slots[k] or s.rng.choice(list(s.npcs))
            choices = [c for c in x.get("Choices") or [] if s.ok(c.get("Requires"))]
            if not choices: return
            fc = s.predict(choices)
            i = s.pick(choices, fc); c = choices[i]
            if fc >= 0: s.total += 1; s.hits += fc == i
            gain = 6.0
            for k, v in (c.get("StatDeltas") or {}).items(): s.add(k, v * 3); gain += abs(v) * 12
            s.xp += gain
            while s.xp >= 60 + s.level * 40: s.xp -= 60 + s.level * 40; s.level += 1; s.add("Sanity.Resilience", -2.5)
            for k, v in (c.get("Tags") or {}).items(): s.tags[k] += v
            s.flags.update(c.get("SetFlags") or [])
            wit = {s.resolve(l["Speaker"]) for l in x.get("Lines") or [] if l.get("Speaker", "").startswith("@")}
            wit |= {s.resolve(x.get("PromptSpeaker") or "")} | set(s.slots.values())
            wit |= {v for k, v in s.roles.items() if "{%s}" % k in (x.get("Text") or "")}
            luck = s.general("Luck")
            for w in wit:
                p = s.npcs.get(w)
                if not p: continue
                vals = p.get("Values") or {}; known = list(vals)
                mis = min(.6, max(0, (1 - p.get("Perception", .5)) * .5 - luck * .002)); d = 0.0
                for t, v in (c.get("Tags") or {}).items():
                    if len(known) > 1 and s.rng.random() < mis: t = s.rng.choice(known)
                    d += vals.get(t, 0) * v * 10
                r = s.rel[w]; d *= (1 + r * .005) if d >= 0 else (1 - r * .005)
                s.rel[w] = min(100, max(-100, r + d))
            nxt = c.get("Next")
            x = next((y for y in s.lets if y["Id"] == nxt), None) if nxt and nxt != "None" else None

    def ending(s): return next((f[7:] for f in s.flags if f.startswith("ending_")), None)

    def go(s):
        s.play(s.find())  # ceremony -> homeroom chain
        for _ in range(400):
            if s.ending(): break
            top = s.find()
            if not top: break
            if top.get("Priority", 0) >= 95 or s.style == "masher": s.play(top); continue  # mandatory / objective follower
            if s.rng.random() < .25:  # chat with someone nearby
                s.talk = s.rng.choice(list(s.npcs)); t = s.find("talk")
                if t: s.play(t); continue
            locs = sorted({x.get("Location") for x in s.lets if x.get("Kind") != "talk" and x["Id"] not in s.played and s.ok(x.get("Conditions"))} - {None, ""})
            s.play(top if s.rng.random() < .6 or not locs else s.find(loc=s.rng.choice(locs)))
        return s.ending() or "STUCK"


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    npcs, lets = load()
    rng = random.Random(7)
    overall, agg = Counter(), defaultdict(list)
    print(f"{'style':11} {'scenes':>6} {'lvl':>4} {'rank':>5} {'pred':>5} {'uniq':>5} {'san':>4}  endings")
    for style in STYLES:
        ends = Counter(); m = defaultdict(float)
        for _ in range(n):
            r = Run(npcs, lets, style, rng); e = r.go(); ends[e] += 1
            for k, v in (("scenes", len(r.played)), ("lvl", r.level), ("rank", r.rank()), ("pred", r.hits / max(1, r.total)), ("uniq", r.uniq()), ("san", r.general("Sanity"))): m[k] += v / n
            for k, v in r.tags.items(): agg[style, k].append(v)
        for e, c in ends.items(): overall[e] += c / n * MIX[style]
        print(f"{style:11} {m['scenes']:6.1f} {m['lvl']:4.1f} {m['rank']:5.1f} {m['pred']:5.2f} {m['uniq']:5.2f} {m['san']:4.0f}  " + "  ".join(f"{e} {100 * c // n}%" for e, c in ends.most_common()))
    print("\nPopulation mix:", "  ".join(f"{e} {100 * v:.1f}%" for e, v in sorted(overall.items(), key=lambda kv: -kv[1])))
    if "-v" in sys.argv:
        for style in STYLES:
            print(style, {k: round(sum(v) / n, 1) for (st, k), v in sorted(agg.items()) if st == style})


if __name__ == "__main__":
    main()
