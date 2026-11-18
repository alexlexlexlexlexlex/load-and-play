#!/usr/bin/env python3
"""Vérifie que les moteurs web (JavaScript) sonnent comme les plugins (C++).

Chaque scénario est rendu par le moteur du plugin (RenderTest --render) et par
le moteur web (render_web.mjs), puis comparé : niveau, spectre par octave,
enveloppe de niveau au fil du temps.

Utilisation : python compare_web.py <dossier racine « Synthés Load and Play »> <dossier temporaire>
"""

import json
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np

# Réglages au niveau du plugin (identifiants de l'interface).
LP60_SCENARIOS = {
    "dent de scie, accord": ({}, [(48, 0.2, 1.5), (52, 0.2, 1.5), (55, 0.2, 1.5)]),
    "pulse PWM LFO + chorus II": ({"saw": 0, "pulse": 1, "pwm": 6, "pwmMode": 0, "lfoRate": 4, "cutoff": 6,
                                   "chorus2": 1, "attack": 3, "release": 5}, [(48, 0.2, 2.0), (55, 0.2, 2.0)]),
    "basse : sub, résonance, enveloppe": ({"saw": 0, "pulse": 1, "subOn": 1, "sub": 8, "range": 0, "cutoff": 3,
                                           "resonance": 5, "vcfEnv": 6, "decay": 4.5, "sustain": 3, "release": 2},
                                          [(36, 0.2, 0.4), (36, 0.8, 0.4), (43, 1.4, 0.6)]),
    "cloches : résonance 7, suivi 100 %": ({"saw": 0, "pulse": 1, "pwm": 7, "cutoff": 5, "resonance": 7, "vcfKybd": 10,
                                            "vcfEnv": 4, "decay": 7, "sustain": 0, "release": 7},
                                           [(72, 0.2, 0.3), (76, 0.7, 0.3), (79, 1.2, 0.3)]),
    "vibrato + HPF + chorus I": ({"dcoLfo": 6, "lfoRate": 5, "hpf": 2, "chorus1": 1, "cutoff": 6.5},
                                 [(60, 0.2, 2.0)]),
    "gate + bruit + LFO filtre": ({"vcaMode": 1, "noise": 6, "saw": 1, "vcfLfo": 5, "lfoRate": 6, "cutoff": 5,
                                   "resonance": 3}, [(57, 0.2, 1.5)]),
}

# Model Play : réglages par identifiant du plugin, lus pareil des deux côtés.
MODELPLAY_SCENARIOS = {
    "son de base, legato": ({}, [(48, 0.2, 0.8), (55, 0.7, 0.8), (52, 1.6, 0.6)]),
    "basse : 16', résonance, contour": ({"osc1Range": 2, "osc2Range": 2, "cutoff": 300, "emphasis": 5, "contour": 7,
                                        "fDecay": 0.25, "fSustain": 1}, [(36, 0.2, 0.2), (36, 0.45, 0.2), (43, 0.75, 0.45)]),
    "lead : glide, osc 3, vibrato": ({"glide": 0.12, "osc3On": 1, "osc3Level": 5, "decay": 1, "lDecay": 0.6,
                                      "oscMod": 1, "modAmount": 3, "osc3Range": 0, "osc3Ctrl": 0, "osc3Freq": 3},
                                     [(60, 0.2, 1.0), (67, 0.6, 0.6)]),
    "auto-oscillation, suivi de clavier": ({"osc1On": 0, "osc2On": 0, "emphasis": 10, "cutoff": 440, "contour": 0,
                                            "kbd1": 1, "kbd2": 1}, [(57, 0.2, 1.2), (69, 1.6, 1.0)]),
    "poly 8 voix + chorus + delay": ({"poly": 1, "cutoff": 2500, "chorusMode": 1, "delayOn": 1, "delayTime": 0.3,
                                      "delayLevel": 4, "lRelease": 0.5, "adsr": 1}, [(48, 0.2, 1.2), (52, 0.2, 1.2), (55, 0.2, 1.2), (59, 0.2, 1.2)]),
    "overdrive + LFO filtre + bruit rose": ({"driveOn": 1, "driveGain": 6, "lfoLevel": 6, "lfoDest": 2, "lfoRate": 3,
                                             "noiseOn": 1, "noiseLevel": 5, "pinkNoise": 1}, [(45, 0.2, 1.5)]),
}

LP60_DEFAULTS = {"lfoRate": 5, "lfoDelay": 0, "lfoMode": 0, "range": 1, "dcoLfo": 0, "pwm": 0, "pwmMode": 1,
                 "pulse": 0, "saw": 1, "subOn": 0, "sub": 0, "noise": 0, "hpf": 0, "cutoff": 7, "resonance": 0,
                 "envPolarity": 0, "vcfEnv": 0, "vcfLfo": 0, "vcfKybd": 0, "vcaMode": 0, "level": 7,
                 "attack": 0, "decay": 5, "sustain": 10, "release": 3, "chorus1": 0, "chorus2": 0, "chorusHiss": 5}


def lp60_engine_scenario (params, notes, duration):
    """Même scénario, avec les noms du moteur C++ (comme LP60Processor::readParams)."""
    v = dict (LP60_DEFAULTS, **params)
    lines = ["rate 44100", f"duration {duration}",
             f"chorus {(1 if v['chorus1'] else 0) + (2 if v['chorus2'] else 0)}", f"hiss {v['chorusHiss']}"]
    engine = {k: v[k] for k in ("lfoRate", "lfoDelay", "lfoMode", "range", "dcoLfo", "pwm", "pwmMode", "pulse", "saw",
                                "noise", "hpf", "cutoff", "resonance", "vcfEnv", "vcfLfo", "vcfKybd", "level",
                                "attack", "decay", "sustain", "release")}
    engine["sub"] = v["sub"] if v["subOn"] else 0
    engine["envInvert"] = v["envPolarity"]
    engine["vcaGate"] = v["vcaMode"]
    lines += [f"param {k} {x}" for k, x in engine.items()]
    lines += [f"note {n} {s} {d}" for n, s, d in notes]
    return "\n".join (lines) + "\n"


def read_wav (path):
    with wave.open (str (path), "rb") as w:
        raw = w.readframes (w.getnframes ())
        channels = w.getnchannels ()
    b = np.frombuffer (raw, dtype = np.uint8).reshape (-1, 3)
    ints = b[:, 0].astype (np.int32) | (b[:, 1].astype (np.int32) << 8) | (b[:, 2].astype (np.int32) << 16)
    ints = np.where (ints & 0x800000, ints - 0x1000000, ints)
    return (ints / 8388608.0).reshape (-1, channels)


def db (x):
    return 20 * np.log10 (np.maximum (x, 1e-9))


def bands (x, rate = 44100):
    spec = np.abs (np.fft.rfft (x * np.hanning (len (x)))) ** 2
    freqs = np.fft.rfftfreq (len (x), 1 / rate)
    edges = 100 * 2 ** np.arange (0, 8.01)
    return np.array ([10 * np.log10 (spec[(freqs >= lo) & (freqs < hi)].mean () + 1e-30) for lo, hi in zip (edges[:-1], edges[1:])])


def envelope (x, rate = 44100, frame = 0.05):
    n = int (rate * frame)
    k = len (x) // n
    return db (np.sqrt (np.mean (x[: k * n].reshape (k, n) ** 2, axis = 1)))


def align (plugin, web, max_lag = 64):
    """Compense le léger retard différent des filtres de décimation (quelques échantillons)."""
    a, b = plugin[:, 0], web[:, 0]
    n = min (len (a), len (b)) - max_lag
    lags = range (0, max_lag)
    best = max (lags, key = lambda lag: float (np.dot (a[: n], b[lag: lag + n])))
    return plugin[: len (plugin) - best], web[best:], best


def compare (plugin, web):
    plugin, web, lag = align (plugin, web)
    out = {"décalage (éch.)": float (lag)}
    for c, name in ((0, "G"), (1, "D")):
        a, b = plugin[:, c], web[:, c]
        out[f"niveau {name}"] = float (db (np.sqrt (np.mean (b ** 2))) - db (np.sqrt (np.mean (a ** 2))))
        ba, bb = bands (a), bands (b)
        loud = ba > ba.max () - 50
        out[f"spectre {name}"] = float (np.max (np.abs ((bb - ba)[loud])))
        ea, eb = envelope (a), envelope (b)
        audible = ea > ea.max () - 40
        k = min (len (ea), len (eb))
        out[f"enveloppe {name}"] = float (np.max (np.abs ((eb[:k] - ea[:k])[audible[:k]])))
    return out


def main ():
    root, tmp = Path (sys.argv[1]), Path (sys.argv[2])
    tmp.mkdir (parents = True, exist_ok = True)
    site = root / "Site web"
    render_test = root / "LP60" / "build" / "RenderTest_artefacts" / "Release" / "RenderTest"
    ok = True

    for name, (params, notes) in LP60_SCENARIOS.items ():
        duration = max (s + d for _, s, d in notes) + 1.5
        slug = "".join (ch if ch.isalnum () else "_" for ch in name)
        scenario_txt = tmp / f"{slug}.txt"
        scenario_txt.write_text (lp60_engine_scenario (params, notes, duration))
        subprocess.run ([str (render_test), "--render", str (scenario_txt), str (tmp / f"{slug}-plugin.wav")], check = True)

        scenario_json = tmp / f"{slug}.json"
        scenario_json.write_text (json.dumps ({"rate": 44100, "duration": duration, "params": params, "notes": notes}))
        subprocess.run (["node", str (site / "tools" / "render_web.mjs"), "lp60", str (scenario_json),
                         str (tmp / f"{slug}-web.wav")], check = True)

        result = compare (read_wav (tmp / f"{slug}-plugin.wav"), read_wav (tmp / f"{slug}-web.wav"))
        good = all (abs (v) < 0.5 for k, v in result.items () if k.startswith ("niveau")) \
            and all (v < 2.0 for k, v in result.items () if k.startswith ("spectre")) \
            and all (v < 2.0 for k, v in result.items () if k.startswith ("enveloppe"))
        ok &= good
        print (f"{'OK   ' if good else 'ÉCART'} LP-60 {name:38s} " + "  ".join (f"{k} {v:+.2f}" for k, v in result.items ()))

    render_test = root / "MiniMoog" / "build" / "RenderTest_artefacts" / "Release" / "RenderTest"
    for name, (params, notes) in MODELPLAY_SCENARIOS.items ():
        duration = max (s + d for _, s, d in notes) + 1.5
        slug = "mp_" + "".join (ch if ch.isalnum () else "_" for ch in name)
        lines = ["rate 44100", f"duration {duration}"] + [f"param {k} {v}" for k, v in params.items ()]
        lines += [f"note {n} {s} {d}" for n, s, d in notes]
        (tmp / f"{slug}.txt").write_text ("\n".join (lines) + "\n")
        subprocess.run ([str (render_test), "--render", str (tmp / f"{slug}.txt"), str (tmp / f"{slug}-plugin.wav")], check = True)
        (tmp / f"{slug}.json").write_text (json.dumps ({"rate": 44100, "duration": duration, "params": params, "notes": notes}))
        subprocess.run (["node", str (site / "tools" / "render_web.mjs"), "modelplay", str (tmp / f"{slug}.json"),
                         str (tmp / f"{slug}-web.wav")], check = True)
        result = compare (read_wav (tmp / f"{slug}-plugin.wav"), read_wav (tmp / f"{slug}-web.wav"))
        good = all (abs (v) < 0.5 for k, v in result.items () if k.startswith ("niveau")) \
            and all (v < 2.0 for k, v in result.items () if k.startswith ("spectre")) \
            and all (v < 2.0 for k, v in result.items () if k.startswith ("enveloppe"))
        ok &= good
        print (f"{'OK   ' if good else 'ÉCART'} Model Play {name:33s} " + "  ".join (f"{k} {v:+.2f}" for k, v in result.items ()))

    sys.exit (0 if ok else 1)


if __name__ == "__main__":
    main ()
