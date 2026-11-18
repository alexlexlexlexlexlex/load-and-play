#!/usr/bin/env python3
"""Fabrique le kit de mesures LP-60 / JU-06A : fichiers MIDI et fiche HTML.

Utilisation : python3 make_measure_kit.py <dossier de sortie>

Les tests sont définis dans kit_tests.py, partagé avec l'outil de comparaison.
"""

import html
import struct
import sys
from pathlib import Path

sys.path.insert (0, str (Path (__file__).resolve().parent))
from kit_tests import BASE, MIDI, TESTS   # noqa: E402

#------------------------------------------------------------------------------
def write_midi (path: Path, notes, total_seconds: float):
    """Fichier MIDI de type 0, 120 BPM, canal 1, vélocité 100."""
    ticks_per_quarter = 480
    ticks_per_second = ticks_per_quarter * 2          # 120 BPM

    events = []
    for note, start, length in notes:
        events.append ((round (start * ticks_per_second), 1, bytes ([0x90, note, 100])))
        events.append ((round ((start + length) * ticks_per_second), 0, bytes ([0x80, note, 0])))
    events.sort (key = lambda e: (e[0], e[1]))

    def varlen (value):
        out = [value & 0x7F]
        value >>= 7
        while value:
            out.insert (0, (value & 0x7F) | 0x80)
            value >>= 7
        return bytes (out)

    track = bytearray()
    track += varlen (0) + bytes ([0xFF, 0x51, 0x03]) + (500000).to_bytes (3, "big")   # tempo 120
    now = 0
    for tick, _, data in events:
        track += varlen (tick - now) + data
        now = tick
    end = round (total_seconds * ticks_per_second)
    track += varlen (max (0, end - now)) + bytes ([0xFF, 0x2F, 0x00])

    header = b"MThd" + struct.pack (">IHHH", 6, 0, 1, ticks_per_quarter)
    path.write_bytes (header + b"MTrk" + struct.pack (">I", len (track)) + bytes (track))


#------------------------------------------------------------------------------
def fiche_html() -> str:
    e = html.escape
    base_rows = "".join (
        f"<tr><th>{e (section)}</th><td>{' · '.join (f'<b>{e (k)}</b> {e (v)}' for k, v in values)}</td></tr>"
        for section, values in BASE)

    midi_rows = "".join (
        f"<tr><td><code>MIDI/{e (name)}.mid</code></td><td>{e (desc)}</td><td>{total:g} s</td></tr>"
        for name, (desc, _, total) in MIDI.items())

    groups = []
    for group in dict.fromkeys (t[2] for t in TESTS):
        rows = "".join (
            f"<tr class='p{prio}'><td><input type='checkbox'></td><td><code>{tid}.wav</code></td>"
            f"<td>{'★' if prio == 1 else ''}</td><td><code>{e (midi)}</code></td><td>{e (change)}</td><td>{e (what)}</td></tr>"
            for tid, prio, g, midi, change, what, _ in TESTS if g == group)
        groups.append (f"<h3>{e (group)}</h3><table class='tests'><tr><th></th><th>Fichier à enregistrer</th>"
                       f"<th>Prio.</th><th>MIDI à jouer</th><th>Réglages (différences avec le son de base)</th>"
                       f"<th>Ce que je mesure</th></tr>{rows}</table>")

    count1 = sum (1 for t in TESTS if t[1] == 1)

    return f"""<!doctype html>
<html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Kit de mesures LP-60 / JU-06A</title>
<style>
  :root {{ --ink:#1b1b1b; --muted:#666; --line:#ddd; --accent:#c9352d; --blue:#3e5fab; --bg:#fff; }}
  body {{ font: 15px/1.5 -apple-system, system-ui, sans-serif; color: var(--ink); background: var(--bg);
         max-width: 1100px; margin: 0 auto; padding: 24px 16px 60px; }}
  h1 {{ margin: 0 0 4px; }} h2 {{ margin-top: 36px; border-bottom: 3px solid var(--blue); padding-bottom: 4px; }}
  h3 {{ margin: 24px 0 6px; color: var(--accent); }}
  table {{ border-collapse: collapse; width: 100%; }}
  th, td {{ border-bottom: 1px solid var(--line); padding: 6px 8px; text-align: left; vertical-align: top; }}
  table.tests th {{ font-size: 13px; color: var(--muted); }}
  tr.p2 td {{ color: #555; }}
  code {{ font-size: 13px; }}
  ol li {{ margin-bottom: 6px; }}
  .note {{ background: #f6f3ea; border-left: 4px solid #e9c440; padding: 10px 14px; }}
  @media print {{ input {{ display: none; }} h2 {{ break-before: auto; }} }}
</style></head><body>
<h1>Kit de mesures LP-60 / JU-06A</h1>
<p>{len (TESTS)} enregistrements courts, dont {count1} prioritaires (★). Commence par les ★ : ils couvrent l'essentiel du son.</p>

<h2>1. Une fois pour toutes</h2>
<ol>
  <li><b>JU-06A en mode Juno-60</b> pour tout ce qui distingue 60 et 106 (type de HPF notamment). Note sa version de firmware.</li>
  <li><b>Le plus simple : en USB.</b> Le JU-06A est sa propre carte son. Dans Cubase, choisis « Boutique JU-06A » comme périphérique audio et enregistre son entrée stéréo (1-2) en <b>WAV 24 bits, 44,1 kHz</b>, sans aucun effet ni traitement. Écoute au casque branché sur le JU-06A.
      Sinon : sortie jack du JU-06A vers ta carte son, en 24 bits.</li>
  <li>Une piste MIDI qui joue vers le JU-06A, canal 1. Glisse le fichier MIDI indiqué pour chaque test.</li>
  <li>Règle le <b>volume du JU-06A</b> et le <b>gain de la carte son</b> une seule fois, sur le test A07 (le plus fort, crêtes vers -12 dB), puis <b>ne les touche plus</b>.</li>
  <li>Lance l'enregistrement un peu avant la première note : l'outil d'analyse retrouve tout seul le début.</li>
  <li>Exporte chaque prise sous le nom indiqué (<code>A01.wav</code>, <code>A02.wav</code>...) dans le dossier <code>Enregistrements</code>.</li>
</ol>
<p class="note">Avant chaque test, remets le son de base, puis change seulement ce qui est indiqué. Si un réglage n'existe pas ou ne se règle pas pareil sur le JU-06A, saute le test et note-le : c'est une information utile.</p>

<h2>2. Le son de base</h2>
<table>{base_rows}</table>

<h2>3. Les fichiers MIDI</h2>
<table>{midi_rows}</table>

<h2>4. Les tests</h2>
{''.join (groups)}

<h2>5. En plus, si tu veux</h2>
<ul>
  <li>2 à 5 sons du JU-06A que tu aimes, joués avec <code>note-tenue</code> ou <code>accord-4-notes</code>, avec une photo des curseurs : <code>S01.wav</code> + <code>S01.jpg</code>, etc.</li>
  <li>Ta liste de ce qui ne va pas dans LP-60, même en vrac, dans <code>Remarques.txt</code>.</li>
</ul>
</body></html>
"""


def main():
    if len (sys.argv) < 2:
        print ("usage: make_measure_kit.py <dossier de sortie>")
        return 2

    out = Path (sys.argv[1])
    (out / "MIDI").mkdir (parents = True, exist_ok = True)
    (out / "Enregistrements").mkdir (exist_ok = True)

    for name, (_, notes, total) in MIDI.items():
        write_midi (out / "MIDI" / f"{name}.mid", notes, total)

    (out / "Fiche de mesures.html").write_text (fiche_html(), encoding = "utf-8")
    remarks = out / "Enregistrements" / "Remarques.txt"
    if not remarks.exists():
        remarks.write_text ("Ce qui ne va pas dans LP-60 par rapport au JU-06A (en vrac) :\n\n- \n", encoding = "utf-8")

    print (f"{len (TESTS)} tests, {len (MIDI)} fichiers MIDI -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit (main())
