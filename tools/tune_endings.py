#!/usr/bin/env python3
"""Single source of truth for when each Year 1 ending fires. Rewrites Conditions/Priority in storylets_endings.json.
Tuned with tools/sim.py: the canon ending for most players, a hidden one for ~5-10%, the dead end for rushers."""
import copy, json, os
P = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content", "Data", "storylets_endings.json")
GATE = ["flag:market_done", "!flag:year1_complete"]
RULES = {  # id: (priority, extra conditions)
    "end_y1_mocked_rush":   (240, ["rush>0.6"]),                                   # never read anything
    "end_y1_mocked":        (235, ["share:betrayal>=0.1", "rel:@ally<-10"]),       # burned every bridge
    "end_y1_director":      (230, ["pred<0.25"]),                                  # beats the forecast on purpose
    "end_y1_clown":         (225, ["share:humor>=0.24", "share:chaos>=0.24"]),
    "end_y1_revolutionary": (220, ["share:chaos>=0.17", "share:empathy>=0.14"]),
    "end_y1_kingmaker":     (215, ["rel:@ally>45", "share:loyalty>=0.2"]),
    "end_y1_ghost":         (210, ["share:withdrawal>=0.18"]),
    "end_y1_dropout":       (205, ["stat:Sanity<=28", "share:withdrawal>=0.12"]),
    "end_y1_mirror":        (200, ["pred>=0.72", "uniq>0.42"]),                    # extreme, and perfectly modelled
    "end_y1_puppet":        (150, []),
}
d = [s for s in json.load(open(P)) if s["Id"] != "end_y1_mocked_rush"]
rush = copy.deepcopy(next(s for s in d if s["Id"] == "end_y1_mocked")); rush["Id"] = "end_y1_mocked_rush"; d.append(rush)
for s in d:
    s["Priority"], extra = RULES[s["Id"]]; s["Conditions"] = GATE[:1] + extra + GATE[1:]
json.dump(d, open(P, "w"), indent=1, ensure_ascii=False)
print("endings retuned:", len(d))
