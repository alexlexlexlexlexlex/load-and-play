#!/usr/bin/env python3
"""Analyse les enregistrements du kit de mesures (tests DCO pour l'instant).

Utilisation : python analyze.py <dossier Enregistrements> [A01 A02 ...]

Pour chaque fichier : format, niveau, stéréo, notes détectées, et pour chaque
note la fréquence fondamentale, la justesse et les harmoniques, d'où l'on
déduit la forme d'onde (dent de scie, carré, largeur d'impulsion...).
"""

import sys
import wave
from pathlib import Path

import numpy as np

# Notes attendues par fichier MIDI du kit (numéro MIDI, début s, durée s).
EXPECTED = {
    "notes-5-octaves": [(36 + 12 * i, 0.5 + 3.0 * i, 2.0) for i in range(5)],
    "note-tenue": [(48, 0.5, 6.0)],
}
TEST_MIDI = { "A01": "notes-5-octaves", "A02": "notes-5-octaves", "A03": "note-tenue", "A04": "note-tenue",
              "A05": "notes-5-octaves", "A06": "note-tenue", "A07": "note-tenue",
              "A08": "note-tenue", "A09": "note-tenue", "A10": "note-tenue" }

# Octave attendue par rapport à la note jouée (sous-oscillateur, ranges 16' et 4').
OCTAVE = { "A05": -1, "A07": -1, "A08": -1, "A09": 1 }


def read_wav (path):
    with wave.open (str (path), "rb") as w:
        channels, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes (frames)
    if width == 3:
        b = np.frombuffer (raw, dtype = np.uint8).reshape (-1, 3)
        ints = (b[:, 0].astype (np.int32) | (b[:, 1].astype (np.int32) << 8) | (b[:, 2].astype (np.int32) << 16))
        ints = np.where (ints & 0x800000, ints - 0x1000000, ints)
        data = ints / 8388608.0
    elif width == 2:
        data = np.frombuffer (raw, dtype = np.int16) / 32768.0
    elif width == 4:
        data = np.frombuffer (raw, dtype = np.int32) / 2147483648.0
    else:
        raise ValueError (f"{width * 8} bits non géré")
    return data.reshape (-1, channels), rate, width * 8


def db (x):
    return 20 * np.log10 (max (x, 1e-12))


def note_name (midi):
    names = ["do", "do#", "ré", "ré#", "mi", "fa", "fa#", "sol", "sol#", "la", "la#", "si"]
    return f"{names[int (round (midi)) % 12]}{int (round (midi)) // 12 - 1}"


def segments (x, rate, threshold_db = -45.0):
    """Zones où le signal dépasse un seuil (enveloppe RMS sur 10 ms)."""
    hop = int (rate * 0.01)
    rms = np.sqrt (np.convolve (x * x, np.ones (hop) / hop, mode = "same"))
    loud = rms > 10 ** (threshold_db / 20)
    edges = np.flatnonzero (np.diff (loud.astype (int)))
    starts = list (edges[::2] + 1) if loud[0] == 0 else [0] + list (edges[1::2] + 1)
    ends = list (edges[1::2] + 1) if loud[0] == 0 else list (edges[::2] + 1)
    out = []
    for s, e in zip (starts, ends + [len (x)] * (len (starts) - len (ends))):
        if (e - s) / rate > 0.2:
            out.append ((s / rate, e / rate))
    return out


def fundamental (x, rate, expected_hz):
    """Fréquence fondamentale par autocorrélation, cherchée à +/- 4 demi-tons de la note attendue."""
    x = x - x.mean()
    n = len (x)
    spec = np.fft.rfft (x * np.hanning (n), 2 * n)
    ac = np.fft.irfft (np.abs (spec) ** 2)[:n]
    lo, hi = int (rate / (expected_hz * 1.26)), int (rate / (expected_hz * 0.79)) + 1
    lag = lo + int (np.argmax (ac[lo:hi]))
    if 1 <= lag < len (ac) - 1:   # interpolation parabolique
        a, b, c = ac[lag - 1], ac[lag], ac[lag + 1]
        lag = lag + 0.5 * (a - c) / (a - 2 * b + c)
    return rate / lag


def harmonics (x, rate, f0, count = 12):
    """Amplitude des harmoniques 1..count (FFT fenêtrée, pic le plus proche)."""
    n = len (x)
    window = np.blackman (n)
    spec = np.abs (np.fft.rfft ((x - x.mean()) * window)) * 2 / window.sum()
    freqs = np.fft.rfftfreq (n, 1 / rate)
    amps = []
    for k in range (1, count + 1):
        f = k * f0
        if f > rate / 2 * 0.95:
            break
        i = np.searchsorted (freqs, f)
        width = max (2, int (0.03 * f / (rate / n)))
        amps.append (spec[max (0, i - width): i + width + 1].max())
    return np.array (amps)


def classify (h):
    """Forme d'onde la plus proche d'après les harmoniques relatives (h[0] = fondamentale)."""
    if len (h) < 6 or h[0] <= 0:
        return "indéterminée", None
    r = h / h[0]
    n = np.arange (1, len (r) + 1)
    candidates = { "dent de scie": 1 / n,
                   "triangle": np.where (n % 2 == 1, 1 / n ** 2, 0) }
    for d in np.arange (0.01, 0.501, 0.005):
        candidates[f"impulsion {d * 100:.1f} %"] = np.abs (np.sin (np.pi * n * d)) / (n * np.sin (np.pi * d))
    best, err = None, 1e9
    for name, model in candidates.items():
        e = np.mean ((db (1) + 20 * np.log10 (np.maximum (r[:8], 1e-4)) - 20 * np.log10 (np.maximum (model[:8], 1e-4))) ** 2)
        if e < err:
            best, err = name, e
    return best, np.sqrt (err)


def spectral_slope (x, rate):
    """Pente du spectre en dB/octave entre 200 Hz et 10 kHz (bruit)."""
    n = len (x)
    spec = np.abs (np.fft.rfft ((x - x.mean()) * np.hanning (n))) ** 2
    freqs = np.fft.rfftfreq (n, 1 / rate)
    bands = 2 ** np.arange (np.log2 (200), np.log2 (16000), 0.5)
    levels = []
    for lo, hi in zip (bands[:-1], bands[1:]):
        m = (freqs >= lo) & (freqs < hi)
        levels.append (10 * np.log10 (spec[m].mean() + 1e-30))
    centres = np.sqrt (bands[:-1] * bands[1:])
    fit = (centres >= 200) & (centres <= 10000)
    slope = np.polyfit (np.log2 (centres[fit]), np.array (levels)[fit], 1)[0]
    return slope, list (zip (centres.round(), np.round (np.array (levels) - max (levels), 1)))


def analyse (path):
    data, rate, bits = read_wav (path)
    test = path.stem.upper()
    print (f"\n=== {path.name} : {data.shape[1]} canaux, {rate} Hz, {bits} bits, {len (data) / rate:.2f} s")

    left, right = data[:, 0], data[:, -1]
    diff = db (np.sqrt (np.mean ((left - right) ** 2))) - db (np.sqrt (np.mean (left ** 2)))
    print (f"    crête {db (np.abs (data).max()):.1f} dBFS ; gauche/droite {'identiques' if diff < -60 else f'différents (écart {diff:.0f} dB)'}")
    x = data.mean (axis = 1)

    found = segments (x, rate)
    expected = EXPECTED.get (TEST_MIDI.get (test, ""), [])
    print (f"    {len (found)} note(s) détectée(s), {len (expected)} attendue(s)")
    if found and expected:
        print (f"    décalage de l'enregistrement : première note à {found[0][0]:.2f} s (attendue à {expected[0][1]:.2f} s)")

    noise_test = test == "A06"
    for i, (start, end) in enumerate (found):
        part = x[int ((start + 0.25) * rate): int ((end - 0.1) * rate)]
        if len (part) < rate * 0.2:
            continue
        rms = db (np.sqrt (np.mean (part ** 2)))
        if noise_test:
            slope, bands = spectral_slope (part, rate)
            print (f"    bruit {start:.2f}-{end:.2f} s : RMS {rms:.1f} dBFS, pente {slope:+.1f} dB/oct (blanc = 0, rose = -3)")
            print ("      densité par demi-octave (Hz, dB) : " + ", ".join (f"{int (f)}:{l}" for f, l in bands))
            continue

        midi = expected[i][0] if i < len (expected) else 48
        target = 440 * 2 ** ((midi - 69) / 12)
        # sous-oscillateur seul : la fondamentale est une octave plus bas
        guess = target * 2 ** OCTAVE.get (test, 0)
        f0 = fundamental (part[: int (rate * 0.5)], rate, guess)
        played = 69 + 12 * np.log2 (f0 / 440)
        cents = (played - round (played)) * 100
        h = harmonics (part[: int (rate * 1.0)], rate, f0)
        shape, err = classify (h)
        rel = ", ".join (f"{db (a / h[0]):.1f}" for a in h[1:8]) if len (h) > 1 else ""
        print (f"    note {i + 1} : {f0:8.2f} Hz = {note_name (played)} {cents:+.1f} ct (jouée {note_name (midi)}) ; "
               f"RMS {rms:.1f} dBFS ; forme ≈ {shape}" + (f" (écart {err:.1f} dB)" if err is not None else ""))
        print (f"      harmoniques 2-8 / fondamentale (dB) : {rel}")


def main():
    folder = Path (sys.argv[1])
    wanted = [a.upper() for a in sys.argv[2:]]
    files = sorted (p for p in folder.glob ("*.wav") if not wanted or p.stem.upper() in wanted)
    for f in files:
        try:
            analyse (f)
        except Exception as error:          # un fichier illisible ne bloque pas les autres
            print (f"\n=== {f.name} : ILLISIBLE ({error})")


if __name__ == "__main__":
    main()
