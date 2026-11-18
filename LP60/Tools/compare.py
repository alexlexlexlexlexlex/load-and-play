#!/usr/bin/env python3
"""Compare les enregistrements du JU-06A avec LP-60 joué dans les mêmes conditions.

Utilisation :
    python compare.py <dossier du kit> [A01 B03 ...]

Pour chaque WAV du dossier Enregistrements, LP-60 rend le même test (mêmes
notes, mêmes réglages, même fréquence d'échantillonnage) dans
Comparaison/LP-60/, puis les deux fichiers sont mesurés de la même façon.
Le rapport (Comparaison/Rapport.txt) donne chaque mesure côté JU-06A, côté
LP-60, l'écart et un verdict.

Les niveaux sont ramenés à une même référence : la dent de scie de A01.
"""

import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from scipy.ndimage import median_filter
from scipy.signal import butter, hilbert, sosfiltfilt

sys.path.insert (0, str (Path (__file__).resolve().parent))
from analyze import classify, fundamental, harmonics, read_wav   # noqa: E402
from kit_tests import LP60_BASE, MIDI, TESTS                      # noqa: E402

RENDERER = Path (__file__).resolve().parent.parent / "build" / "RenderTest_artefacts" / "Release" / "RenderTest"

# Tolérances du verdict, par type de mesure.
TOLERANCE = { "ct": 3.0, "dB": 1.0, "%": 2.0, "oct": 0.15, "ratio": 0.15 }


#------------------------------------------------------------------------------
def db (x):
    return 20 * np.log10 (np.maximum (x, 1e-12))


def hz (midi):
    return 440 * 2 ** ((midi - 69) / 12)


def render_lp60 (test_id, midi_name, settings, rate, duration, output: Path):
    values = dict (LP60_BASE)
    values.update (settings)
    lines = [f"rate {rate}", f"duration {duration}",
             f"chorus {int (values.pop ('chorus'))}", f"hiss {values.pop ('hiss')}"]
    lines += [f"param {k} {v}" for k, v in values.items()]
    lines += [f"note {n} {start} {length}" for n, start, length in MIDI[midi_name][1]]

    with tempfile.NamedTemporaryFile ("w", suffix = ".txt", delete = False) as f:
        f.write ("\n".join (lines) + "\n")
        scenario = f.name
    result = subprocess.run ([str (RENDERER), "--render", scenario, str (output)], capture_output = True, text = True)
    Path (scenario).unlink()
    if result.returncode != 0:
        raise RuntimeError (f"rendu LP-60 de {test_id} impossible : {result.stderr.strip()}")


#------------------------------------------------------------------------------
# Outils de mesure communs

def onset_offset (x, rate, notes):
    """Décalage entre la première note attendue et son début réel (s)."""
    if not notes:
        return 0.0
    hop = int (rate * 0.002)
    env = np.sqrt (np.convolve (x * x, np.ones (hop) / hop, mode = "same"))
    above = np.flatnonzero (env > env.max() * 10 ** (-40 / 20))
    return (above[0] / rate - notes[0][1]) if len (above) else 0.0


def note_part (x, rate, start, length, skip = 0.25, tail = 0.1):
    return x[int ((start + skip) * rate): int ((start + length - tail) * rate)]


def envelope_db (x, rate, frame = 0.01):
    n = int (rate * frame)
    frames = len (x) // n
    rms = np.sqrt (np.mean (x[: frames * n].reshape (frames, n) ** 2, axis = 1))
    return db (rms), frame


def harmonic_profile (x, rate, f0, max_hz = 15000):
    count = int (min (max_hz, rate * 0.45) / f0)
    return db (harmonics (x, rate, f0, count))


def response_at (freqs, response, threshold, rising = False):
    """Première fréquence où la réponse passe sous (ou au-dessus de) threshold, interpolée en log."""
    for i in range (1, len (response)):
        a, b = response[i - 1], response[i]
        crossed = (a < threshold <= b) if rising else (a > threshold >= b)
        if crossed:
            t = (threshold - a) / (b - a)
            return float (np.exp2 (np.log2 (freqs[i - 1]) + t * (np.log2 (freqs[i]) - np.log2 (freqs[i - 1]))))
    return None


def spectral_centroid_track (x, rate, frame = 0.02):
    n = int (rate * frame)
    window = np.hanning (n)
    freqs = np.fft.rfftfreq (n, 1 / rate)
    out = []
    for i in range (0, len (x) - n, n // 2):
        spec = np.abs (np.fft.rfft (x[i: i + n] * window))
        out.append (np.sum (freqs * spec) / (np.sum (spec) + 1e-12))
    return np.array (out), frame / 2


def pitch_track (x, rate, expected_hz, frame = 0.035, hop = 0.005):
    n, h = int (rate * frame), int (rate * hop)
    track = [fundamental (x[i: i + n], rate, expected_hz) for i in range (0, len (x) - n, h)]
    return 1200 * np.log2 (np.array (track) / expected_hz), hop


def modulation (track, step, low = 0.1, high = 30.0):
    """Vitesse (Hz) et profondeur crête à crête d'une courbe de modulation."""
    t = track - np.polyval (np.polyfit (np.arange (len (track)), track, 1), np.arange (len (track)))
    pad = 8 * len (t)
    spec = np.abs (np.fft.rfft (t * np.hanning (len (t)), pad))
    freqs = np.fft.rfftfreq (pad, step)
    band = (freqs >= low) & (freqs <= high)
    rate = float (freqs[band][np.argmax (spec[band])])
    depth = float (np.percentile (t, 97) - np.percentile (t, 3))
    return rate, depth


def band_densities (x, rate):
    n = len (x)
    spec = np.abs (np.fft.rfft ((x - x.mean()) * np.hanning (n))) ** 2
    freqs = np.fft.rfftfreq (n, 1 / rate)
    edges = 2 ** np.arange (np.log2 (200), np.log2 (16000) + 0.01, 1.0)
    out = []
    for lo, hi in zip (edges[:-1], edges[1:]):
        m = (freqs >= lo) & (freqs < hi)
        out.append ((int (np.sqrt (lo * hi)), 10 * np.log10 (spec[m].mean() + 1e-30)))
    top = max (v for _, v in out)
    return [(f, v - top) for f, v in out]


#------------------------------------------------------------------------------
# Mesures par groupe de tests. Chaque fonction rend une liste de
# (libellé, valeur, unité) ; les deux côtés passent par la même fonction.

class Side:
    """Un enregistrement et sa référence A01, côté JU-06A ou LP-60."""

    def __init__ (self, path, reference_path, gain_db):
        data, self.rate, _ = read_wav (path)
        self.left, self.right = data[:, 0], data[:, -1]
        self.x = data.mean (axis = 1) * 10 ** (gain_db / 20)
        self.left = self.left * 10 ** (gain_db / 20)
        self.right = self.right * 10 ** (gain_db / 20)
        self.reference = None
        if reference_path is not None and reference_path.exists():
            ref, _, _ = read_wav (reference_path)
            self.reference = ref.mean (axis = 1) * 10 ** (gain_db / 20)


def measure_dco (s, test, notes, offset):
    out = []
    if test == "A06":
        part = note_part (s.x, s.rate, notes[0][1] + offset, notes[0][2])
        out.append (("niveau du bruit", float (db (np.sqrt (np.mean (part ** 2)))), "dB"))
        out += [(f"densité vers {f} Hz", v, "dB") for f, v in band_densities (part, s.rate)]
        return out

    octave = { "A05": -1, "A07": -1, "A08": -1, "A09": 1 }.get (test, 0) + s.shift // 12

    if test == "A10":
        # Largeur d'impulsion image par image : elle suit le LFO.
        midi, start, length = notes[0]
        part = note_part (s.x, s.rate, start + offset, length, skip = 0.5)
        f0 = hz (midi) * 2 ** octave
        n, step = int (s.rate * 0.06), 0.02
        widths = []
        for i in range (0, len (part) - n, int (s.rate * step)):
            shape, _ = classify (harmonics (part[i: i + n], s.rate, f0, 8))
            widths.append (float (shape.split()[1]) if shape.startswith ("impulsion") else 50.0)
        widths = np.array (widths)
        # (la vitesse du LFO se mesure sur les tests E : ici, une largeur w et 1 - w
        # ont le même spectre, ce qui fausserait la mesure)
        return [("largeur minimale", float (np.percentile (widths, 3)), "%"),
                ("largeur maximale", float (np.percentile (widths, 97)), "%"),
                ("niveau", float (db (np.sqrt (np.mean (part ** 2)))), "dB")]

    for i, (midi, start, length) in enumerate (notes):
        part = note_part (s.x, s.rate, start + offset, length)
        f0 = fundamental (part[: int (s.rate * 0.5)], s.rate, hz (midi) * 2 ** octave)
        played = 69 + 12 * np.log2 (f0 / 440)
        tag = f"note {i + 1}" if len (notes) > 1 else "note"
        out.append ((f"{tag} justesse", float ((played - (midi + 12 * octave)) * 100), "ct"))
        out.append ((f"{tag} niveau", float (db (np.sqrt (np.mean (part ** 2)))), "dB"))
        h = harmonics (part[: int (s.rate)], s.rate, f0, 8)
        for k in range (1, min (8, len (h))):
            # sous -60 dB, une harmonique est considérée absente des deux côtés
            out.append ((f"{tag} harmonique {k + 1}", float (max (-60.0, db (h[k] / h[0]))), "dB"))
        shape, _ = classify (h)
        if shape.startswith ("impulsion"):
            out.append ((f"{tag} largeur d'impulsion", float (shape.split()[1]), "%"))
    return out


def filter_response (s, ref_notes, notes, offset, i):
    """Réponse du filtre (dB) aux harmoniques d'une note, par rapport à la dent de scie de A01."""
    midi, start, length = notes[i]
    midi += s.shift
    ref_start = next ((r for r in ref_notes if r[0] == midi), None)
    if ref_start is None:       # pas de dent de scie de référence à cette hauteur dans A01
        return None, None
    part = note_part (s.x, s.rate, start + offset, length)
    ref = note_part (s.reference, s.rate, ref_start[1] + s.ref_offset, ref_start[2])
    f0 = fundamental (ref[: int (s.rate * 0.5)], s.rate, hz (midi))
    a, b = harmonic_profile (part[: int (s.rate)], s.rate, f0), harmonic_profile (ref[: int (s.rate)], s.rate, f0)
    count = min (len (a), len (b))
    return f0 * np.arange (1, count + 1), a[:count] - b[:count]


def measure_filter (s, test, notes, offset, ref_notes):
    out = []
    if test == "B10":
        # Sans oscillateur, le filtre siffle seul : on mesure la fréquence de son pic.
        for i, (midi, start, length) in enumerate (notes):
            part = note_part (s.x, s.rate, start + offset, length)
            spec = np.abs (np.fft.rfft (part * np.hanning (len (part))))
            freqs = np.fft.rfftfreq (len (part), 1 / s.rate)
            k = int (np.argmax (spec[5:])) + 5
            out.append ((f"note {i + 1} fréquence d'auto-oscillation", float (freqs[k]), "oct"))
            out.append ((f"note {i + 1} niveau", float (db (np.sqrt (np.mean (part ** 2)))), "dB"))
        return out

    indices = range (len (notes)) if test in ("B11", "B12") else [0]
    for i in indices:
        freqs, resp = filter_response (s, ref_notes, notes, offset, i)
        if freqs is None:
            continue
        tag = f"note {i + 1} " if len (notes) > 1 else ""
        fc = response_at (freqs, resp, -12.0)
        if fc is None:
            # pas de passage à -12 dB : coupure au-dessus du dernier harmonique mesuré,
            # ou sous la fondamentale si tout est déjà atténué
            fc = float (freqs[-1]) if resp[0] > -12.0 else float (freqs[0]) / 2
        out.append ((f"{tag}coupure (-12 dB)", fc, "oct"))
        if i == 0:
            out.append ((f"{tag}niveau à la fondamentale", float (max (-60.0, resp[0])), "dB"))
        if test in ("B06", "B07", "B08", "B09"):
            k = int (np.argmax (resp))
            out.append ((f"{tag}pic de résonance", float (resp[k]), "dB"))
            out.append ((f"{tag}fréquence du pic", float (freqs[k]), "oct"))
    return out


def measure_envelope (s, test, notes, offset):
    midi, start, length = notes[0]
    on, off = start + offset, start + length + offset
    env, step = envelope_db (s.x, s.rate, 0.008)
    t = np.arange (len (env)) * step
    out = []

    def first_time (t0, condition):
        idx = np.flatnonzero ((t >= t0) & condition)
        return float (t[idx[0]] - t0) if len (idx) else None

    def slope (t0, top):
        """Pente (dB/s) entre -10 et -30 dB sous top, à partir de t0."""
        a, b = first_time (t0, env <= top - 10), first_time (t0, env <= top - 30)
        return 20.0 / (b - a) if a is not None and b is not None and b > a else None

    full = float (np.median (env[(t >= off - 1.0) & (t < off)]))     # niveau tenu juste avant le relâchement

    if test in ("C01", "C02", "C03", "C04"):
        for level in (-20, -6, -1):
            out.append ((f"attaque jusqu'à {level} dB", first_time (on, env >= full + level), "ratio"))
    if test in ("C05", "C06", "C07", "C08", "C14", "C15"):   # (en C16, le filtre qui se rouvre masque le decay)
        during = (t >= on) & (t < off)
        peak_t = float (t[np.flatnonzero (during)[np.argmax (env[during])]])
        peak = float (env[during].max())
        out.append (("decay : temps jusqu'à -6 dB", first_time (peak_t, env <= peak - 6), "ratio"))
        out.append (("decay : pente", slope (peak_t, peak), "ratio"))
    if test in ("C09", "C10", "C11", "C12", "C13", "C17", "H01"):
        if test == "C13":
            during = (t >= on) & (t < off)
            out.append (("niveau de sustain / maximum", full - float (env[during].max()), "dB"))
        # Le relâchement commence quand le niveau quitte le sustain (la note
        # MIDI peut arriver quelques dizaines de ms après l'instant prévu).
        begin = first_time (off - 0.1, env <= full - 1.0)
        if begin is not None:
            begin += off - 0.1
            out.append (("release : temps jusqu'à -6 dB", first_time (begin, env <= full - 6), "ratio"))
            out.append (("release : pente", slope (begin, full), "ratio"))
    if test in ("C14", "C15", "C16"):
        # Coupure du filtre au fil du temps : réponse aux harmoniques sur de
        # courtes fenêtres, comparée à la dent de scie de A01, ajustée par un
        # filtre 24 dB/oct.
        ref_note = next ((r for r in MIDI["notes-5-octaves"][1] if r[0] == midi + s.shift), None)
        if ref_note is not None:
            ref = note_part (s.reference, s.rate, ref_note[1] + s.ref_offset, ref_note[2])
            f0 = fundamental (ref[: int (s.rate * 0.5)], s.rate, hz (midi + s.shift))
            n = int (s.rate * 0.03)
            ref_h = harmonic_profile (ref[:n], s.rate, f0)
            for at in (0.01, 0.05, 0.1, 0.2, 0.4):
                i0 = int ((on + at) * s.rate)
                frame = s.x[i0: i0 + n]
                h = harmonic_profile (frame, s.rate, f0)
                count = min (len (h), len (ref_h))
                resp = h[:count] - ref_h[:count]
                freqs = f0 * np.arange (1, count + 1)
                if np.max (h[:count]) < np.max (ref_h) - 50:      # son déjà éteint
                    continue
                out.append ((f"coupure à {at:g} s", fit_cutoff (freqs, resp), "oct"))
    return out


def fit_cutoff (freqs, resp):
    """Coupure (Hz) d'un filtre 24 dB/oct sans résonance qui explique au mieux la réponse."""
    usable = resp > resp.max() - 45
    best, err = None, 1e9
    for lfc in np.arange (3.0, 15.3, 0.02):
        model = -20 * np.log10 (np.abs ((1 + 1j * freqs / 2 ** lfc) ** 4))
        gain = np.mean ((resp - model)[usable])
        e = np.mean (((resp - model - gain)[usable]) ** 2)
        if e < err:
            best, err = 2 ** lfc, e
    return float (best)


def measure_hpf (s, notes, offset, ref_notes):
    freqs, resp = filter_response (s, ref_notes, notes, offset, 0)
    if freqs is None:
        return []
    out = [(f"atténuation à {freqs[k]:.0f} Hz", float (resp[k]), "dB") for k in (0, 1, 3, 7) if k < len (resp)]
    fc = response_at (freqs, resp, -3.0, rising = True)
    if fc:
        out.append (("coupure (-3 dB)", fc, "oct"))
    return out


def instantaneous_cents (x, rate, f0):
    """Hauteur instantanée (cents autour de f0) : fondamentale isolée, signal analytique."""
    sos = butter (4, [f0 * 0.6, f0 * 1.6], btype = "band", fs = rate, output = "sos")
    phase = np.unwrap (np.angle (hilbert (sosfiltfilt (sos, x))))
    freq = np.diff (phase) * rate / (2 * np.pi)
    cents = 1200 * np.log2 (np.maximum (freq, 1.0) / f0)
    return median_filter (cents, int (rate * 0.004))[int (rate * 0.05): -int (rate * 0.05)]


def dominant_rate (track, rate, low = 0.02, high = 200.0):
    t = track - np.mean (track)
    spec = np.abs (np.fft.rfft (t * np.hanning (len (t))))
    freqs = np.fft.rfftfreq (len (t), 1 / rate)
    band = (freqs >= low) & (freqs <= high)
    return float (freqs[band][np.argmax (spec[band])])


def measure_lfo (s, test, notes, offset):
    midi, start, length = notes[0]
    part = s.x[int ((start + offset) * s.rate): int ((start + length + offset) * s.rate)]
    if test == "E06":
        cent, step = spectral_centroid_track (part, s.rate, 0.01)
        rate, depth = modulation (np.log2 (cent[int (0.3 / step):]), step)
        return [("vitesse", rate, "ratio"), ("profondeur sur le filtre (crête à crête)", depth, "oct")]

    cents = instantaneous_cents (part, s.rate, hz (midi + s.shift))
    steady = cents[-int (3 * s.rate):]
    if test == "E02":
        # LFO très lent : on mesure la pente de la hauteur (une montée du triangle).
        t = np.arange (len (cents)) / s.rate
        return [("variation de hauteur (vitesse lente, ct/s)", float (abs (np.polyfit (t, cents, 1)[0])), "ratio")]

    out = [("vitesse", dominant_rate (steady, s.rate), "ratio"),
           ("profondeur du vibrato (crête à crête)", float (np.percentile (steady, 99) - np.percentile (steady, 1)), "ratio")]
    if test in ("E04", "E05"):
        window = int (s.rate * 0.25)
        full = np.percentile (steady, 99) - np.percentile (steady, 1)
        depth = [np.percentile (cents[i: i + window], 99) - np.percentile (cents[i: i + window], 1)
                 for i in range (0, len (cents) - window, window // 2)]
        idx = [i for i, d in enumerate (depth) if d >= 0.5 * full]
        if idx:
            out.append (("retard (vibrato à moitié)", 0.05 + idx[0] * window / 2 / s.rate, "ratio"))
    return out


def chorus_delay_track (channel, rate, f0):
    """Retard (ms) du signal retardé au fil du temps.

    La fondamentale démodulée vaut sec + mouillé : un point qui décrit un arc de
    cercle autour du signal sec ; l'angle sur ce cercle donne le retard.
    """
    sos = butter (4, [f0 * 0.8, f0 * 1.25], btype = "band", fs = rate, output = "sos")
    analytic = hilbert (sosfiltfilt (sos, channel))
    u = analytic * np.exp (-2j * np.pi * f0 * np.arange (len (analytic)) / rate)
    step = int (rate / 2000)
    u = u[: len (u) // step * step].reshape (-1, step).mean (axis = 1)[200: -200]
    a = np.c_[2 * u.real, 2 * u.imag, np.ones (len (u))]
    cx, cy, _ = np.linalg.lstsq (a, u.real ** 2 + u.imag ** 2, rcond = None)[0]
    delay = -np.unwrap (np.angle (u - (cx + 1j * cy))) / (2 * np.pi * f0) * 1000
    i = np.arange (len (delay))
    return delay - np.polyval (np.polyfit (i, delay, 1), i), 1 / 2000


def measure_chorus (s, test, notes, offset, floor_power):
    if not notes:
        # Souffle, mesuré par canal (le bruit de gauche et celui de droite sont indépendants).
        power = max (float (np.mean (s.left ** 2)) - floor_power, 1e-20)
        out = [("niveau du souffle (par canal)", float (10 * np.log10 (power)), "dB")]
        if power > 1e-14:
            out += [(f"densité du souffle vers {f} Hz", v, "dB") for f, v in band_densities (s.left, s.rate)]
        return out

    midi, start, length = notes[0]
    a, b = int ((start + offset + 0.5) * s.rate), int ((start + length + offset - 0.1) * s.rate)
    left, right = s.left[a:b], s.right[a:b]
    f0 = fundamental ((left + right)[: int (s.rate * 0.5)], s.rate, hz (midi + s.shift))
    dl, step = chorus_delay_track (left, s.rate, f0)
    dr, _ = chorus_delay_track (right, s.rate, f0)
    pad = 8 * len (dl)
    spec = np.abs (np.fft.rfft (dl * np.hanning (len (dl)), pad))
    freqs = np.fft.rfftfreq (pad, step)
    band = (freqs >= 0.1) & (freqs <= 20)
    out = [("vitesse du chorus", float (freqs[band][np.argmax (spec[band])]), "ratio"),
           ("corrélation des retards gauche / droite", float (np.corrcoef (dl, dr)[0, 1]), "ratio")]
    if test != "F03":   # l'arc de I+II est trop court pour mesurer l'excursion de façon fiable
        out.append (("excursion du retard (ms)", float (np.percentile (dl, 99) - np.percentile (dl, 1)), "ratio"))
    return out


def detect_shift (path, test_id, group, midi_name):
    """Octave réellement jouée par le JU-06A par rapport à celle attendue (RANGE oublié...).

    Rend 0, +12 ou -12 demi-tons. Les tests où l'octave fait partie du réglage
    (ranges, sous-oscillateur) et ceux sans hauteur nette ne sont pas concernés.
    """
    notes = MIDI[midi_name][1]
    if not notes or test_id in ("A05", "A07", "A08", "A09", "A06", "B10") or group == "CHORUS" and not notes:
        return 0
    data, rate, _ = read_wav (path)
    x = data.mean (axis = 1)
    offset = onset_offset (x, rate, notes)
    midi, start, length = notes[0]
    part = note_part (x, rate, start + offset, length)[: int (rate)]
    if len (part) < rate * 0.3 or np.sqrt (np.mean (part ** 2)) < 1e-3:
        return None
    spec = np.abs (np.fft.rfft ((part - part.mean()) * np.hanning (len (part))))
    freqs = np.fft.rfftfreq (len (part), 1 / rate)
    top = spec.max()
    for shift in (-12, 0, 12, 24):
        f = hz (midi + shift)
        band = (freqs > f * 0.97) & (freqs < f * 1.03)
        if band.any() and spec[band].max() > top * 10 ** (-30 / 20):
            return shift
    return 0


#------------------------------------------------------------------------------
def verdict (unit, ju, lp, label = ""):
    if ju is None or lp is None:
        return None, "?"
    # Une harmonique très faible (quasi absente) varie beaucoup d'une note à l'autre :
    # on ne lui demande que d'être du même ordre.
    tolerance = TOLERANCE[unit]
    if "harmonique" in label and min (ju, lp) < -25.0:
        tolerance = 6.0
    if unit == "oct":
        diff = float (np.log2 (max (lp, 1e-9) / max (ju, 1e-9)))
        text = f"{diff:+.2f} oct"
    elif unit == "ratio":
        diff = (lp - ju) / ju if abs (ju) > 1e-9 else lp - ju
        text = f"{diff * 100:+.0f} %"
    else:
        diff = lp - ju
        text = f"{diff:+.1f} {unit}"
    return abs (diff) <= tolerance, text


def fmt (value, unit):
    if value is None:
        return "—"
    if unit == "oct":
        return f"{value:.0f} Hz" if value < 1000 else f"{value / 1000:.2f} kHz"
    if unit == "ratio":
        return f"{value:.3g}"
    return f"{value:.1f} {unit}"


def main():
    kit = Path (sys.argv[1])
    wanted = [a.upper() for a in sys.argv[2:]]
    recordings = kit / "Enregistrements"
    out_dir = kit / "Comparaison" / "LP-60"
    out_dir.mkdir (parents = True, exist_ok = True)
    tests = { t[0]: t for t in TESTS }

    available = sorted (p.stem.upper() for p in recordings.glob ("*.wav") if p.stem.upper() in tests)
    todo = [t for t in available if not wanted or t in wanted]

    # Octave réellement jouée côté JU-06A : LP-60 joue la même.
    shifts = {}
    for test_id in todo:
        _, _, group, midi_name, _, _, settings = tests[test_id]
        shifts[test_id] = detect_shift (recordings / f"{test_id}.wav", test_id, group, midi_name) if settings is not None else 0
    # Enregistrement trop faible pour trouver l'octave (filtre fermé), ou hauteur
    # trop modulée pour la reconnaître (LFO, chorus) : on prend celle des voisins.
    detected = dict (shifts)
    for test_id in todo:
        modulated = tests[test_id][2] in ("LFO", "CHORUS") and MIDI[tests[test_id][3]][1]
        if shifts[test_id] is None or modulated:
            group = tests[test_id][2]
            known = [v for t, v in detected.items()
                     if v is not None and tests[t][2] == group and MIDI[tests[t][3]][1]]
            shifts[test_id] = max (set (known), key = known.count) if known else 0

    # A01 sert de référence de niveau et de spectre : toujours rendu.
    rate = read_wav (recordings / "A01.wav")[1] if "A01" in available else 44100
    for test_id in sorted (set (todo) | ({ "A01" } if "A01" in available else set())):
        _, _, _, midi_name, _, _, settings = tests[test_id]
        if settings is None:
            continue
        settings = dict (settings)
        if shifts.get (test_id, 0):
            settings["range"] = int (np.clip (settings.get ("range", LP60_BASE["range"]) + shifts[test_id] // 12, 0, 2))
        duration = len (read_wav (recordings / f"{test_id}.wav")[0]) / rate
        render_lp60 (test_id, midi_name, settings, rate, round (duration, 3), out_dir / f"{test_id}.wav")

    # Gain qui aligne LP-60 sur le JU-06A (niveau de la dent de scie de A01).
    gain = 0.0
    if "A01" in available:
        ju_ref, lp_ref = read_wav (recordings / "A01.wav")[0].mean (axis = 1), read_wav (out_dir / "A01.wav")[0].mean (axis = 1)
        gain = float (db (np.sqrt (np.mean (ju_ref ** 2))) - db (np.sqrt (np.mean (lp_ref ** 2))))

    floor = { "ju": 0.0, "lp": 0.0 }
    if "F06" in available:
        floor["ju"] = float (np.mean (read_wav (recordings / "F06.wav")[0][:, 0] ** 2))

    lines = [f"Comparaison JU-06A / LP-60 — {len (todo)} test(s)",
             f"Niveaux de LP-60 corrigés de {gain:+.1f} dB pour égaler la dent de scie de A01 du JU-06A.", ""]
    totals = [0, 0]
    summary = []       # une ligne par test, et ses écarts

    for test_id in todo:
        _, _, group, midi_name, change, what, settings = tests[test_id]
        lines.append (f"== {test_id} — {what} ({change})")
        if shifts.get (test_id, 0):
            lines.append (f"   NB : le JU-06A joue {shifts[test_id] // 12:+d} octave (RANGE pas remis à 8' ?) ; LP-60 rendu pareil")
        if settings is None:
            lines += ["   pas encore simulé dans LP-60 (arpège)", ""]
            continue
        try:
            notes = MIDI[midi_name][1]
            sides = {}
            for key, folder, g in (("ju", recordings, 0.0), ("lp", out_dir, gain)):
                s = Side (folder / f"{test_id}.wav", folder / "A01.wav", g)
                s.shift = shifts.get (test_id, 0)
                s.ref_offset = onset_offset (s.reference, s.rate, MIDI["notes-5-octaves"][1]) if s.reference is not None else 0.0
                offset = onset_offset (s.x, s.rate, notes)
                ref_notes = MIDI["notes-5-octaves"][1]
                if group == "DCO":
                    m = measure_dco (s, test_id, notes, offset)
                elif group == "VCF":
                    m = measure_filter (s, test_id, notes, offset, ref_notes)
                elif group in ("ENV", "VOIX"):
                    m = measure_envelope (s, test_id, notes, offset)
                elif group == "HPF":
                    m = measure_hpf (s, notes, offset, ref_notes)
                elif group == "LFO":
                    m = measure_lfo (s, test_id, notes, offset)
                elif group == "CHORUS":
                    m = measure_chorus (s, test_id, notes, offset, floor[key] if key == "ju" else 0.0)
                else:
                    m = []
                sides[key] = m

            lp_values = { label: (value, unit) for label, value, unit in sides["lp"] }
            test_counts, test_gaps = [0, 0], []
            for label, ju_value, unit in sides["ju"]:
                lp_value = lp_values.get (label, (None, unit))[0]
                ok, diff = verdict (unit, ju_value, lp_value, label)
                mark = "ok" if ok else ("??" if ok is None else "ÉCART")
                if ok is not None:
                    totals[0 if ok else 1] += 1
                    test_counts[0 if ok else 1] += 1
                row = f"   {label:42s} JU {fmt (ju_value, unit):>11s}   LP {fmt (lp_value, unit):>11s}   {diff:>9s}  {mark}"
                lines.append (row)
                if not ok:
                    test_gaps.append (row)
            summary.append (f"{test_id}  {test_counts[0]:3d} ok  {test_counts[1]:3d} écart(s)  {what}")
            summary += test_gaps
        except Exception as error:      # un test qui échoue ne bloque pas les autres
            lines.append (f"   mesure impossible : {type (error).__name__} {error}")
            summary.append (f"{test_id}  mesure impossible ({type (error).__name__} {error})")
        lines.append ("")

    lines.append (f"Bilan : {totals[0]} mesure(s) dans la tolérance, {totals[1]} écart(s).")
    report = "\n".join (lines)
    (kit / "Comparaison" / "Rapport.txt").write_text (report + "\n", encoding = "utf-8")
    print ("\n".join (lines[:3] + summary + ["", lines[-1]]))


if __name__ == "__main__":
    main()
