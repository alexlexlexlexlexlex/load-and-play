#!/usr/bin/env python3
"""Fabrique les manuels PDF des plugins (français et anglais) dans « Site web/docs ».

Le texte vient du site lui-même (description, branchement, installation, caractéristiques,
« De plus près » dans assets/js/products.js) et la liste des réglages de params.json,
relevée sur les plugins compilés (voir paramdump/). Il faut Node.js et le compilateur
clang d'Apple (Command Line Tools).

    python3 "Site web/tools/manual/build.py"
"""

import html
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname (os.path.abspath (__file__))
SITE = os.path.normpath (os.path.join (HERE, "..", ".."))
BUILD = os.path.join (HERE, "build")
DOCS = os.path.join (SITE, "docs")

PRODUCTS = {
    "lp60": {
        "name": "LP-60", "file": "LP-60", "page": "lp-60.html", "card": "lp-60", "kind": "synth",
        "groups": [
            [r"Arpeggio.*", r"Hold"],
            [r"LFO .*"],
            [r"DCO .*"],
            [r"HPF .*", r"VCF .*"],
            [r"VCA .*", r"ENV .*"],
            [r"Chorus.*"],
            [r"Master Tune", r"Bender.*", r"Mod Wheel.*", r"Tempo.*"],
        ],
    },
    "modelplay": {
        "name": "Model Play", "file": "Model-Play", "page": "model-play.html", "card": "model-play", "kind": "synth",
        "groups": [
            [r"Tune", r"Glide", r"Modulation Mix", r"Modulation Amount", r"Modulation from LFO", r"Oscillator Modulation",
             r"Filter Modulation", r"Pitch Bend Range", r"Analog Drift", r"Retrigger", r"Polyphonic", r"Aftertouch Amount",
             r"Velocity to Filter", r"Tempo.*"],
            [r"Osc \d (Range|Waveform|Frequency|Fine Tune|Rectify|Offset)", r"Osc 3 Control", r"Osc Sync.*"],
            [r"Osc \d (On|Volume)", r"Noise .*", r"Pink Noise", r"Volume"],
            [r"Cutoff Frequency", r"Emphasis", r"Amount of Contour", r"Keyboard Control \d", r"Bass Compensation",
             r"Filter (Attack|Decay|Sustain|Release)", r"Loudness .*", r"Decay", r"ADSR Release", r"VCO 3 Filter Mod"],
            [r"LFO .*", r"Autopan"],
            [r"Overdrive .*", r"Chorus .*", r"Delay .*"],
            [],
            [r"Arp.*"],
        ],
    },
    "lp303": {
        "name": "LP-303", "file": "LP-303", "page": "lp-303.html", "card": "lp-303", "kind": "synth",
        "groups": [
            [],
            [r"Waveform", r"Tuning"],
            [r"Cutoff", r"Resonance", r"Env Mod", r"Decay"],
            [r"Accent", r"Slide Time"],
            [r"Drive", r"Volume"],
            [r"Sequencer On", r"Pattern", r"Swing", r"Tempo.*", r"Transpose"],
            [],
            [],
        ],
    },
    "duckhunt": {
        "name": "Duck Hunt", "file": "Duck-Hunt", "page": "duck-hunt.html", "card": "duck-hunt", "kind": "effect",
        "groups": [
            [r"Trigger", r"Mode"],
            [],
            [r"Threshold", r"Range", r"SC .*", r"Division", r"Velocity"],
            [r"Attack", r"Hold", r"Release", r"Curve"],
            [r"Depth", r"Lookahead", r"Invert"],
            [r".* Crossover", r".* Band", r"Focus .*", r"Smoothing"],
            [],
            [r"Mix", r"Output", r"Listen Sidechain"],
        ],
    },
    "chopchop": {
        "name": "Chop Chop", "file": "Chop-Chop", "page": "chop-chop.html", "card": "chop-chop", "kind": "effect",
        "groups": [
            [r"Trigger", r"Step Rate"],
            [],
            [r"Step \d+"],
            [r"Slice Size", r"Rise", r"Decay", r"Chance", r"Pitch", r"Snap To Syllable"],
            [r"Drum Band", r"Sensitivity", r"Hit Length"],
            [r"Pump", r"Pump Release", r"Mix", r"Output"],
            [r"Dirt", r"Drive", r"Drum Drive"],
            [r"Filter", r"Cutoff", r"Resonance", r"Drum Filter Mod"],
            [r"Delay Time", r"Feedback", r"Echo", r"Reverb Size", r"Reverb"],
        ],
    },
}

T = {
    "manual":    {"fr": "Manuel d'utilisation", "en": "User manual"},
    "version":   {"fr": "Version", "en": "Version"},
    "intro":     {"fr": "Présentation", "en": "Introduction"},
    "install":   {"fr": "Installation", "en": "Installation"},
    "daw":       {"fr": "Dans votre logiciel", "en": "In your DAW"},
    "interface": {"fr": "L'interface", "en": "The interface"},
    "presets":   {"fr": "Sons d'usine", "en": "Factory sounds"},
    "presetsfx": {"fr": "Presets", "en": "Presets"},
    "specs":     {"fr": "Caractéristiques", "en": "Specifications"},
    "license":   {"fr": "Licence et mentions", "en": "License and notices"},
    "others":    {"fr": "Autres réglages", "en": "Other settings"},
    "control":   {"fr": "Réglage (automation)", "en": "Control (automation)"},
    "range":     {"fr": "Plage", "en": "Range"},
    "default":   {"fr": "Défaut", "en": "Default"},
    "contents":  {"fr": "Sommaire", "en": "Contents"},
    "noparams":  {"fr": "Rien à régler ici : cette partie montre ce que fait le plugin.", "en": "Nothing to set here: this part shows what the plugin is doing."},
    "presetnote": {"fr": "Choisissez-les dans le menu en haut de la fenêtre, ou dans le menu des presets de votre logiciel.",
                   "en": "Pick them from the menu at the top of the window, or from your host's preset menu."},
    "free":      {"fr": "Gratuit et open source", "en": "Free and open source"},
    "licensetext": {
        "fr": "{name} est un logiciel libre, distribué gratuitement sous licence GNU AGPL v3 : vous pouvez l'utiliser dans vos morceaux, y compris commerciaux, sans rien devoir. Le code source complet est publié sur GitHub ; si vous le modifiez et le redistribuez, vos modifications doivent rester sous la même licence. Le logiciel est fourni tel quel, sans garantie.",
        "en": "{name} is free software, distributed at no cost under the GNU AGPL v3 license: you can use it in your tracks, commercial ones included, without owing anything. The full source code is published on GitHub; if you modify and redistribute it, your changes must stay under the same license. The software is provided as is, without warranty.",
    },
    "steps16": {"fr": "Step 1 … Step 16", "en": "Step 1 … Step 16"},
}

SITE_URL = "https://loadandplaymusic.com/"
REPO_URL = "https://github.com/alexlexlexlexlexlex/load-and-play"


def read (path):
    with open (path, encoding="utf-8") as f:
        return f.read()


def closer_data():
    js = "global.window = {}; require (process.argv[1]); console.log (JSON.stringify (window.LP_CLOSER));"
    out = subprocess.check_output (["node", "-e", js, os.path.join (SITE, "assets", "js", "products.js")])
    return json.loads (out)


def section (page, start_pattern):
    """Le HTML entre un titre (motif) et le titre h2 suivant."""
    m = re.search (start_pattern, page)
    if not m:
        return ""
    rest = page[m.end():]
    end = re.search (r"\n\s*<h2[ >]", rest)
    return rest[: end.start()] if end else rest


def lead (page):
    return "\n".join (re.findall (r'<p class="lead" lang="(?:fr|en)">.*?</p>', page, flags=re.S))


def card_text (index, card_id):
    m = re.search (r'<article class="instrument" id="%s">(.*?)</article>' % re.escape (card_id), index, flags=re.S)
    block = m.group (1)
    taglines = re.findall (r'<p class="tagline" lang="(fr|en)">(.*?)</p>', block)
    briefs = re.findall (r'<p class="brief" lang="(fr|en)">(.*?)</p>', block)
    return dict (taglines), dict (briefs)


def both (key, cls=None, tag="span", fmt=None):
    out = []
    for lang in ("fr", "en"):
        text = T[key][lang]
        if fmt:
            text = text.format (**fmt)
        attr = f' class="{cls}"' if cls else ""
        out.append (f'<{tag} lang="{lang}"{attr}>{html.escape (text)}</{tag}>')
    return "".join (out)


def pair (d, tag="span", cls=None):
    attr = f' class="{cls}"' if cls else ""
    return "".join (f'<{tag} lang="{lang}"{attr}>{d[lang]}</{tag}>' for lang in ("fr", "en"))


def range_text (p):
    choices = p.get ("choices")
    if choices and (len (choices) <= 8 or all (len (c) <= 5 for c in choices)):
        if choices == ["Off", "On"]:
            return "Off / On"
        return " / ".join (html.escape (c) for c in choices)
    return f'{html.escape (p["min"])} … {html.escape (p["max"])}'


def assign (params, groups):
    """Répartit les réglages entre les parties de l'interface ; le reste va à « Autres »."""
    buckets = [[] for _ in groups] + [[]]
    for order, p in enumerate (params):
        for i, patterns in enumerate (groups):
            hits = [k for k, pat in enumerate (patterns) if re.fullmatch (pat, p["name"])]
            if hits:
                # Rangés dans l'ordre des motifs, puis oscillateur par oscillateur.
                number = re.match (r"Osc (\d) ", p["name"])
                buckets[i].append (((hits[0], int (number.group (1)) if number else 0, order), p))
                break
        else:
            buckets[-1].append (((0, 0, order), p))
    return [[p for _, p in sorted (b, key=lambda e: e[0])] for b in buckets]


def param_table (params):
    rows = []
    collapsed = set()
    for p in params:
        m = re.fullmatch (r"(Arp Step|Step) (\d+)", p["name"])
        if m:
            if m.group (1) in collapsed:
                continue
            collapsed.add (m.group (1))
            name = f"{m.group (1)} 1 … {m.group (1)} 16"
        else:
            name = p["name"]
        rows.append (f'<tr><td>{html.escape (name)}</td><td>{range_text (p)}</td><td>{html.escape (p["def"])}</td></tr>')
    head = f'<tr><th>{both ("control")}</th><th>{both ("range")}</th><th>{both ("default")}</th></tr>'
    return f'<table class="params"><thead>{head}</thead><tbody>{"".join (rows)}</tbody></table>'


def crop (data, zone, src):
    x, y, w, h = zone
    W = data["width"]
    # Une petite zone n'est pas agrandie à toute la largeur de la page.
    style_box = f"aspect-ratio: {w} / {h}; width: {min (100.0, w / W * 175):.2f}%;"
    style_img = f"width: {W / w * 100:.4f}%; left: {-x / w * 100:.4f}%; top: {-y / h * 100:.4f}%;"
    return f'<div class="crop" style="{style_box}"><img src="{src}" style="{style_img}" alt=""></div>'


def hero_width (data):
    """Largeur de la capture sur la couverture : jamais plus haute que celle de Model Play
    (2772 x 1404), et un peu plus petite pour les effets, presque carrés."""
    ratio = data["width"] / data["height"]
    reference = 2772 / 1404
    return 100.0 if ratio >= reference else ratio / reference * 100 * 0.92


def overview (data, src):
    spots = []
    for i, part in enumerate (data["parts"]):
        x, y, w, h = part["zone"]
        cx = (x + w / 2) / data["width"] * 100
        cy = (y + h / 2) / data["height"] * 100
        spots.append (f'<span class="spot" style="left: {cx:.3f}%; top: {cy:.3f}%;">{i + 1}</span>')
    return f'<div class="overview"><img src="{src}" alt="">{"".join (spots)}</div>'


def build_page (key, info, closer, params, index_html):
    page = read (os.path.join (SITE, info["page"]))
    data = closer[key]
    src = "../../../" + data["image"]
    tagline, brief = card_text (index_html, info["card"])
    description = section (page, r'<h2 id="description">Description</h2>')
    daw = section (page, r'<h2 id="brancher">.*?</h2>')
    install = section (page, r'<h3>(?:<span lang="fr">)?Installation(?:</span><span lang="en">Installation</span>)?</h3>')
    specs = re.search (r'<table class="specs">.*?</table>', page, flags=re.S).group (0)
    notes = "\n".join (re.findall (r'<p class="note" lang="(?:fr|en)">.*?</p>', page, flags=re.S))
    p = params[key]
    buckets = assign (p["params"], info["groups"])

    chapters = [("intro", "intro"), ("install", "install")]
    if daw:
        chapters.append (("daw", "daw"))
    chapters.append (("interface", "interface"))
    if p["presets"]:
        chapters.append (("presets", "presets" if info["kind"] == "synth" else "presetsfx"))
    chapters += [("specs", "specs"), ("license", "license")]

    out = []
    out.append (f'''<section class="cover">
  <div class="brand">Load &amp; Play</div>
  <h1>{info["name"]}</h1>
  {pair (tagline, "p", "tagline")}
  <div class="hero" style="--hero-width: {hero_width (data):.1f}%"><img src="{src}" alt=""></div>
  {pair (brief, "p", "brief")}
  <div class="cover-foot">
    <span class="what">{both ("manual")}</span>
    <span>{both ("version")} 1.0.0</span>
    <span>{both ("free")}</span>
  </div>
  <div class="toc"><h2>{both ("contents")}</h2><ol>{"".join (f"<li>{both (label)}</li>" for _, label in chapters)}</ol></div>
</section>''')

    n = 0

    def chapter (label, body, cls=""):
        nonlocal n
        n += 1
        return f'<section class="chapter {cls}"><h2><span class="num">{n}</span>{both (label)}</h2>{body}</section>'

    out.append (chapter ("intro", f'<div class="lead">{lead (page)}</div><div class="desc">{description}</div>'))
    out.append (chapter ("install", f'<div class="desc">{install}</div>', "short"))
    if daw:
        out.append (chapter ("daw", f'<div class="desc">{daw}</div>', "short"))

    parts = [f'<div class="part intro-part">{overview (data, src)}{pair ({l: data["overview"][l][1] for l in ("fr", "en")}, "p")}</div>']
    for i, part in enumerate (data["parts"]):
        table = param_table (buckets[i]) if buckets[i] else f'<p class="muted">{both ("noparams")}</p>'
        parts.append (f'''<div class="part">
  <h3><span class="badge">{i + 1}</span>{pair ({l: html.escape (part[l][0]) for l in ("fr", "en")})}</h3>
  {crop (data, part["zone"], src)}
  {pair ({l: html.escape (part[l][1]) for l in ("fr", "en")}, "p")}
  {table}
</div>''')
    if buckets[-1]:
        parts.append (f'<div class="part"><h3>{both ("others")}</h3>{param_table (buckets[-1])}</div>')
    out.append (chapter ("interface", "".join (parts)))

    if p["presets"]:
        items = "".join (f"<li>{html.escape (name)}</li>" for name in p["presets"])
        out.append (chapter ("presets" if info["kind"] == "synth" else "presetsfx",
                             f'<p>{both ("presetnote")}</p><ol class="presets">{items}</ol>', "short"))

    out.append (chapter ("specs", specs, "short"))
    out.append (chapter ("license", f'''<p>{both ("licensetext", fmt={"name": info["name"]})}</p>
{notes}
<p class="links"><span lang="fr">Site :</span><span lang="en">Website:</span> <a href="{SITE_URL}">{SITE_URL}</a><br>
<span lang="fr">Code source :</span><span lang="en">Source code:</span> <a href="{REPO_URL}">{REPO_URL}</a></p>''', "short"))

    return "\n".join (out)


def wrap (body, title, lang):
    return f'''<!doctype html>
<html lang="{lang}" data-ui-lang="{lang}">
<head>
<meta charset="utf-8">
<title>{html.escape (title)}</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&family=Permanent+Marker&display=block" rel="stylesheet">
<link rel="stylesheet" href="../manual.css">
<script>
  // Prêt quand les polices et toutes les images sont chargées.
  window.addEventListener ("load", () => {{
    const images = [...document.images].map (i => i.complete ? Promise.resolve () : new Promise (r => {{ i.onload = i.onerror = r; }}));
    Promise.all ([document.fonts.ready, ...images]).then (() => setTimeout (() => {{ document.title = "READY"; }}, 150));
  }});
</script>
</head>
<body>
{body}
</body>
</html>
'''


def main():
    os.makedirs (BUILD, exist_ok=True)
    os.makedirs (DOCS, exist_ok=True)
    closer = closer_data()
    params = json.loads (read (os.path.join (HERE, "params.json")))
    index_html = read (os.path.join (SITE, "index.html"))
    only = sys.argv[1:]

    jobs = []
    for key, info in PRODUCTS.items():
        if only and key not in only:
            continue
        body = build_page (key, info, closer, params, index_html)
        for lang, suffix in (("fr", "manuel"), ("en", "manual")):
            name = f'{info["file"]}-{suffix}'
            page = os.path.join (BUILD, name + ".html")
            with open (page, "w", encoding="utf-8") as f:
                f.write (wrap (body, f'{info["name"]} — {T["manual"][lang]}', lang))
            jobs += [page, os.path.join (DOCS, name + ".pdf")]

    tool = os.path.join (BUILD, "html2pdf")
    source = os.path.join (HERE, "html2pdf.m")
    if not os.path.exists (tool) or os.path.getmtime (tool) < os.path.getmtime (source):
        subprocess.check_call (["clang", "-fobjc-arc", "-O2", "-framework", "Cocoa", "-framework", "WebKit", source, "-o", tool])
    subprocess.check_call ([tool, SITE] + jobs)


if __name__ == "__main__":
    main()
