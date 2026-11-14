#!/usr/bin/env python3
"""Generate the cast as 3D characters: Rodin (text -> A-pose model) then Blender (skin to the UE mannequin skeleton).

  tools/chars.py            generate + rig everything missing
  tools/chars.py <id> ...   only these ids (delete Assets/Characters/src/<id>.glb first to re-roll)
Then: tools/import_assets.sh chars
"""
from __future__ import annotations

import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import assets

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "Assets" / "Characters" / "src"
RIG = ROOT / "Assets" / "Characters" / "rig"
OUT = ROOT / "Assets" / "Characters"

STYLE = ("full body character, stylised semi-realistic anime-inspired game character, clean simple shapes, neutral calm expression, "
         "standing straight in A-pose with both arms held away from the body and legs slightly apart, empty open hands, no text, no logos")
UF = "elite academy uniform: navy blazer, white shirt, striped tie, navy pleated skirt, white socks, black shoes"
UM = "elite academy uniform: navy blazer, white shirt, striped tie, grey trousers, black shoes"

# id -> (gender, description). Original characters; looks follow each character's values in Content/Data/npcs_*.json.
CHARS: dict[str, tuple[str, str]] = {
    # staff
    "staff_okonkwo": ("f", "Nigerian woman in her fifties, school headmistress, short natural grey hair, tailored dark burgundy skirt suit, pearl necklace, stern posture"),
    "staff_haugen": ("f", "Norwegian woman in her thirties, homeroom teacher, blonde hair in a low bun, glasses, cream cardigan over a blouse, long grey skirt, flat shoes"),
    "staff_venkatesh": ("m", "Indian man in his forties, strict exam proctor, neat side-parted black hair, moustache, grey waistcoat, white shirt with rolled sleeves, dark trousers"),
    "staff_delgado": ("m", "Spanish man in his thirties, school nurse, curly dark hair, light stubble, teal medical scrubs, white sneakers"),
    "staff_raukawa": ("m", "Maori man in his forties, sports coach, broad muscular build, short black hair, red tracksuit jacket, black track trousers, whistle on a cord"),
    "staff_rashid": ("m", "Arab man in his fifties, school counsellor, grey beard, round glasses, brown tweed jacket over a mustard sweater, dark trousers"),
    "staff_seo": ("f", "Korean woman in her late twenties, ambitious young teacher, sleek black shoulder-length hair, sharp charcoal blazer, white blouse, pencil skirt, heels"),
    # students A
    "stu_okafor": ("f", f"Nigerian teenage girl, long black box braids tied high, confident sharp look, {UF}, blazer immaculate, gold prefect pin"),
    "stu_ferreira": ("m", f"Brazilian teenage boy, warm smile, wavy brown hair, tan skin, {UM}, sleeves pushed up"),
    "stu_solberg": ("f", f"Norwegian teenage girl, messy platinum blonde pixie cut, mischievous grin, {UF}, tie loose, mismatched striped socks, sneakers"),
    "stu_mehta": ("m", f"Indian teenage boy, neat short black hair, rectangular glasses, {UM}, perfectly buttoned, class president armband"),
    "stu_tanabe": ("f", f"Japanese teenage girl, short black bob haircut, reserved, {UF}"),
    "stu_alvarez": ("m", f"Mexican teenage boy, slicked back dark hair, sly half smile, {UM}, blazer worn open, silver wristwatch"),
    "stu_haddad": ("f", f"Lebanese teenage girl, long curly dark brown hair, playful smirk, {UF}, blazer sleeves rolled, colourful bracelets"),
    "stu_volkov": ("m", f"Russian teenage boy, tall, pale, short ash blond crew cut, cold stare, {UM}, immaculate"),
    "stu_kelly": ("f", f"Irish teenage girl, wavy ginger hair in a ponytail, freckles, friendly, {UF}, green scarf"),
    "stu_mensah": ("m", f"Ghanaian teenage boy, short fade haircut, quiet, headphones around the neck, {UM}, dark hoodie under the blazer"),
    "stu_rojas": ("f", f"Colombian teenage girl, long straight dark brown hair with a hairband, kind eyes, {UF}"),
    "stu_brandt": ("m", f"German teenage boy, spiky light brown hair, smug grin, {UM}, tie undone, blazer slung open"),
    # students B
    "stu_raman": ("f", f"Indian teenage girl, long black single plait, poised, small glasses, {UF}"),
    "stu_alamin": ("m", f"Sudanese teenage boy, tall and gentle, short curly black hair, {UM}, knitted grey vest under the blazer"),
    "stu_zhou": ("f", f"Chinese teenage girl, black hair in two buns, blunt fringe, {UF}, oversized blazer, paint-stained sleeves"),
    "stu_grant": ("m", f"English teenage boy, tidy chestnut hair, round face, earnest, {UM}, buttoned up"),
    "stu_bello": ("f", f"Nigerian teenage girl, short afro with a bright yellow headband, wide grin, {UF}, sneakers"),
    "stu_lund": ("m", f"Danish teenage boy, athletic, swept blond hair, competitive glare, {UM}"),
    "stu_dubois": ("f", f"French teenage girl, chin-length brown wavy hair, beret, gentle smile, {UF}"),
    "stu_park": ("m", f"Korean teenage boy, black hair with a long fringe over one eye, unreadable, {UM}, hands relaxed"),
    "stu_petrova": ("f", f"Bulgarian teenage girl, long straight light brown hair, blunt serious look, {UF}"),
    "stu_brooks": ("m", f"African American teenage boy, short twists hairstyle, big laugh, {UM}, no tie, high-top sneakers"),
    "stu_siddiqui": ("f", f"Pakistani teenage girl, navy hijab, neat and composed, {UF} with long skirt"),
    "stu_novak": ("m", f"Czech teenage boy, thin, dark blond hair combed flat, calculating eyes, {UM}, leather satchel strap across the chest"),
    # the player (one body per gender) and background students
    "player_m": ("m", f"teenage boy, ordinary new transfer student, medium brown tousled hair, {UM}, slightly too-big blazer, plain backpack straps"),
    "player_f": ("f", f"teenage girl, ordinary new transfer student, medium brown shoulder-length hair, {UF}, slightly too-big blazer, plain backpack straps"),
    "extra_m1": ("m", f"teenage boy, short black hair, {UM}"),
    "extra_m2": ("m", f"teenage boy, curly red hair, glasses, {UM}"),
    "extra_m3": ("m", f"teenage boy, dark skin, buzz cut, {UM}"),
    "extra_f1": ("f", f"teenage girl, long black hair, {UF}"),
    "extra_f2": ("f", f"teenage girl, blonde ponytail, {UF}"),
    "extra_f3": ("f", f"teenage girl, dark skin, shoulder-length curly hair, {UF}"),
}


def one(cid: str, key: str) -> str:
    gender, desc = CHARS[cid]
    glb, fbx = SRC / f"{cid}.glb", OUT / f"{cid}.fbx"
    try:
        if not glb.is_file():
            assets.generate_rodin(key, cid, f"{desc}, {STYLE}", glb, {"tier": "Gen-2.5-Medium", "TAPose": "true"})
        if not fbx.is_file():
            man = RIG / ("Quinn.fbx" if gender == "f" else "Manny.fbx")
            r = assets.run_blender(str(ROOT / "tools" / "rig_char.py"), "--", str(man), str(glb), str(fbx), str(SRC / f"{cid}.png"))
            if not fbx.is_file():
                return f"{cid}: RIG FAILED {r.stdout[-300:]}"
            assets.log(f"[{cid}] rigged")
        return f"{cid}: ok"
    except Exception as e:  # noqa: BLE001
        return f"{cid}: FAILED {e}"


def main() -> int:
    key = assets.load_env(assets.ENV_PATH)["RODIN_API_KEY"]
    SRC.mkdir(parents=True, exist_ok=True)
    ids = sys.argv[1:] or list(CHARS)
    with ThreadPoolExecutor(max_workers=6) as pool:
        res = list(pool.map(lambda c: one(c, key), ids))
    print("\n".join(res))
    return 1 if any("FAILED" in r for r in res) else 0


if __name__ == "__main__":
    sys.exit(main())
