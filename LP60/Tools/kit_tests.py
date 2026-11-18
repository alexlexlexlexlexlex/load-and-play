"""Définition des tests du kit de mesures LP-60 / JU-06A : seule source pour
la fiche, les fichiers MIDI et l'outil de comparaison.

Les réglages LP-60 utilisent les noms de champs de lp60::Params (LP60Engine.h),
plus « chorus » (0 à 3) et « hiss » (souffle du chorus, 0 à 10).
"""

# Son de base de LP-60, équivalent du son de base du JU-06A.
LP60_BASE = { "lfoRate": 5, "lfoDelay": 0, "lfoMode": 0, "range": 1, "dcoLfo": 0, "pwm": 0, "pwmMode": 1,
              "pulse": 0, "saw": 1, "sub": 0, "noise": 0, "hpf": 0, "cutoff": 10, "resonance": 0,
              "envInvert": 0, "vcfEnv": 0, "vcfLfo": 0, "vcfKybd": 0, "vcaGate": 0, "level": 5,
              "attack": 0, "decay": 0, "sustain": 10, "release": 0, "chorus": 0, "hiss": 5 }

#------------------------------------------------------------------------------
# Son de base : tous les tests partent de là et ne listent que ce qui change.

BASE = [
    ("LFO", [("RATE", "5"), ("DELAY", "0"), ("TRIG", "AUTO")]),
    ("DCO", [("RANGE", "8'"), ("LFO", "0"), ("PWM", "0"), ("PWM MODE", "MAN"),
             ("PULSE", "éteint"), ("SAW", "allumé"), ("SUB", "0"), ("NOISE", "0")]),
    ("HPF", [("FREQ", "0 (tout en bas)")]),
    ("VCF", [("FREQ", "10 (tout en haut)"), ("RES", "0"), ("ENV polarité", "+ (normale)"),
             ("ENV", "0"), ("LFO", "0"), ("KYBD", "0")]),
    ("VCA", [("MODE", "ENV"), ("LEVEL", "5")]),
    ("ENV", [("A", "0"), ("D", "0"), ("S", "10"), ("R", "0")]),
    ("Effets et jeu", [("CHORUS", "éteint"), ("DELAY", "LEVEL 0 (coupé)"), ("PORTAMENTO", "0"),
                       ("ARPEGGIO", "éteint"), ("Levier / molette", "au repos")]),
]

#------------------------------------------------------------------------------
# Fichiers MIDI : (nom, description, notes [(note, début s, durée s)], durée totale s)

MIDI = {
    "notes-5-octaves": ("Do 36, 48, 60, 72, 84 : 2 s chacun, 1 s de silence entre",
                        [(36 + 12 * i, 0.5 + 3.0 * i, 2.0) for i in range(5)], 15.5),
    "note-tenue": ("Do 48 tenu 6 s, puis 3 s de silence", [(48, 0.5, 6.0)], 9.5),
    "note-grave": ("Do 36 tenu 3 s, puis 1 s de silence", [(36, 0.5, 3.0)], 4.5),
    "note-enveloppe-longue": ("Do 48 tenu 22 s, puis 22 s de silence", [(48, 0.5, 22.0)], 44.5),
    "accord-4-notes": ("Do-mi-sol-si (48, 52, 55, 59) tenus 6 s, puis 4 s",
                       [(n, 0.5, 6.0) for n in (48, 52, 55, 59)], 10.5),
    "silence": ("Aucune note : 10 s de silence (pour le souffle)", [], 10.0),
}

#------------------------------------------------------------------------------
# Tests : (identifiant, priorité, groupe, fichier MIDI, réglages qui changent,
#          ce que je mesure, réglages LP-60 équivalents ou None si non simulé)

TESTS = [
    # DCO
    ("A01", 1, "DCO", "notes-5-octaves", "Aucun (son de base : dent de scie seule)", "forme et niveau de la dent de scie", {}),
    ("A02", 1, "DCO", "notes-5-octaves", "SAW éteint, PULSE allumé (PWM 0)", "onde carrée", {'saw': 0, 'pulse': 1}),
    ("A03", 1, "DCO", "note-tenue", "SAW éteint, PULSE allumé, PWM 5", "largeur d'impulsion à mi-course", {'saw': 0, 'pulse': 1, 'pwm': 5}),
    ("A04", 1, "DCO", "note-tenue", "SAW éteint, PULSE allumé, PWM 10", "largeur d'impulsion maximale", {'saw': 0, 'pulse': 1, 'pwm': 10}),
    ("A05", 1, "DCO", "notes-5-octaves", "SAW éteint, SUB 10", "sous-oscillateur seul", {'saw': 0, 'sub': 10}),
    ("A06", 1, "DCO", "note-tenue", "SAW éteint, NOISE 10", "bruit seul", {'saw': 0, 'noise': 10}),
    ("A07", 1, "DCO", "note-tenue", "SAW + PULSE (PWM 0) + SUB 10", "équilibre des niveaux", {'pulse': 1, 'sub': 10}),
    ("A08", 2, "DCO", "note-tenue", "RANGE 16'", "range 16'", {'range': 0}),
    ("A09", 2, "DCO", "note-tenue", "RANGE 4'", "range 4'", {'range': 2}),
    ("A10", 1, "DCO", "note-tenue", "SAW éteint, PULSE allumé, PWM 7, PWM MODE LFO, LFO RATE 3", "PWM par le LFO", {'saw': 0, 'pulse': 1, 'pwm': 7, 'pwmMode': 0, 'lfoRate': 3}),
    # VCF
    ("B01", 1, "VCF", "note-grave", "VCF FREQ 0", "coupure au minimum", {'cutoff': 0}),
    ("B02", 1, "VCF", "note-grave", "VCF FREQ 2", "coupure à 2", {'cutoff': 2}),
    ("B03", 1, "VCF", "note-grave", "VCF FREQ 4", "coupure à 4", {'cutoff': 4}),
    ("B04", 1, "VCF", "note-grave", "VCF FREQ 6", "coupure à 6", {'cutoff': 6}),
    ("B05", 1, "VCF", "note-grave", "VCF FREQ 8", "coupure à 8", {'cutoff': 8}),
    ("B06", 1, "VCF", "note-grave", "VCF FREQ 5, RES 2,5", "résonance 2,5", {'cutoff': 5, 'resonance': 2.5}),
    ("B07", 1, "VCF", "note-grave", "VCF FREQ 5, RES 5", "résonance 5", {'cutoff': 5, 'resonance': 5}),
    ("B08", 1, "VCF", "note-grave", "VCF FREQ 5, RES 7,5", "résonance 7,5", {'cutoff': 5, 'resonance': 7.5}),
    ("B09", 1, "VCF", "note-grave", "VCF FREQ 5, RES 10", "résonance maximale", {'cutoff': 5, 'resonance': 10}),
    ("B10", 1, "VCF", "notes-5-octaves", "SAW éteint, VCF FREQ 5, RES 10, KYBD 10", "auto-oscillation et sa justesse", {'saw': 0, 'cutoff': 5, 'resonance': 10, 'vcfKybd': 10}),
    ("B11", 2, "VCF", "notes-5-octaves", "VCF FREQ 4, KYBD 10", "suivi de clavier", {'cutoff': 4, 'vcfKybd': 10}),
    ("B12", 2, "VCF", "notes-5-octaves", "VCF FREQ 4, KYBD 5", "suivi de clavier à mi-course", {'cutoff': 4, 'vcfKybd': 5}),
    # ENV
    ("C01", 1, "ENV", "note-enveloppe-longue", "ENV A 2,5", "attaque 2,5", {'attack': 2.5}),
    ("C02", 1, "ENV", "note-enveloppe-longue", "ENV A 5", "attaque 5", {'attack': 5}),
    ("C03", 1, "ENV", "note-enveloppe-longue", "ENV A 7,5", "attaque 7,5", {'attack': 7.5}),
    ("C04", 1, "ENV", "note-enveloppe-longue", "ENV A 10", "attaque maximale", {'attack': 10}),
    ("C05", 1, "ENV", "note-enveloppe-longue", "ENV D 2,5, S 0", "decay 2,5", {'decay': 2.5, 'sustain': 0}),
    ("C06", 1, "ENV", "note-enveloppe-longue", "ENV D 5, S 0", "decay 5", {'decay': 5, 'sustain': 0}),
    ("C07", 1, "ENV", "note-enveloppe-longue", "ENV D 7,5, S 0", "decay 7,5", {'decay': 7.5, 'sustain': 0}),
    ("C08", 1, "ENV", "note-enveloppe-longue", "ENV D 10, S 0", "decay maximal", {'decay': 10, 'sustain': 0}),
    ("C09", 1, "ENV", "note-enveloppe-longue", "ENV R 2,5", "release 2,5", {'release': 2.5}),
    ("C10", 1, "ENV", "note-enveloppe-longue", "ENV R 5", "release 5", {'release': 5}),
    ("C11", 1, "ENV", "note-enveloppe-longue", "ENV R 7,5", "release 7,5", {'release': 7.5}),
    ("C12", 1, "ENV", "note-enveloppe-longue", "ENV R 10", "release maximal", {'release': 10}),
    ("C13", 2, "ENV", "note-enveloppe-longue", "ENV D 5, S 5, R 5", "niveau de sustain", {'decay': 5, 'sustain': 5, 'release': 5}),
    ("C14", 1, "ENV", "note-enveloppe-longue", "VCF FREQ 2, VCF ENV 10, ENV D 5, S 0, R 5", "enveloppe vers le filtre, au maximum", {'cutoff': 2, 'vcfEnv': 10, 'decay': 5, 'sustain': 0, 'release': 5}),
    ("C15", 1, "ENV", "note-enveloppe-longue", "VCF FREQ 2, VCF ENV 5, ENV D 5, S 0, R 5", "enveloppe vers le filtre, à mi-course", {'cutoff': 2, 'vcfEnv': 5, 'decay': 5, 'sustain': 0, 'release': 5}),
    ("C16", 2, "ENV", "note-enveloppe-longue", "VCF FREQ 8, VCF ENV 10 polarité -, ENV D 5, S 0, R 5", "enveloppe inversée", {'cutoff': 8, 'vcfEnv': 10, 'envInvert': 1, 'decay': 5, 'sustain': 0, 'release': 5}),
    ("C17", 2, "ENV", "note-tenue", "VCA MODE GATE, ENV R 10", "mode GATE", {'vcaGate': 1, 'release': 10}),
    # HPF
    ("D01", 1, "HPF", "notes-5-octaves", "HPF FREQ position 1", "passe-haut position 1", {'hpf': 1}),
    ("D02", 1, "HPF", "notes-5-octaves", "HPF FREQ position 2", "passe-haut position 2", {'hpf': 2}),
    ("D03", 1, "HPF", "notes-5-octaves", "HPF FREQ position 3", "passe-haut position 3", {'hpf': 3}),
    # LFO
    ("E01", 1, "LFO", "note-tenue", "DCO LFO 10", "profondeur et vitesse du vibrato", {'dcoLfo': 10}),
    ("E02", 2, "LFO", "note-tenue", "DCO LFO 10, LFO RATE 0", "vitesse minimale", {'dcoLfo': 10, 'lfoRate': 0}),
    ("E03", 2, "LFO", "note-tenue", "DCO LFO 10, LFO RATE 10", "vitesse maximale", {'dcoLfo': 10, 'lfoRate': 10}),
    ("E04", 2, "LFO", "note-tenue", "DCO LFO 10, LFO DELAY 5", "retard du LFO à mi-course", {'dcoLfo': 10, 'lfoDelay': 5}),
    ("E05", 2, "LFO", "note-tenue", "DCO LFO 10, LFO DELAY 10", "retard maximal", {'dcoLfo': 10, 'lfoDelay': 10}),
    ("E06", 1, "LFO", "note-tenue", "VCF FREQ 5, VCF LFO 10", "LFO vers le filtre", {'cutoff': 5, 'vcfLfo': 10}),
    # Chorus
    ("F01", 1, "CHORUS", "note-tenue", "CHORUS I", "chorus I", {'chorus': 1}),
    ("F02", 1, "CHORUS", "note-tenue", "CHORUS II", "chorus II", {'chorus': 2}),
    ("F03", 1, "CHORUS", "note-tenue", "CHORUS I+II (si ton JU-06A le propose)", "chorus I+II", {'chorus': 3}),
    ("F04", 1, "CHORUS", "silence", "CHORUS I, sans jouer", "souffle du chorus I", {'chorus': 1}),
    ("F05", 1, "CHORUS", "silence", "CHORUS II, sans jouer", "souffle du chorus II", {'chorus': 2}),
    ("F06", 1, "CHORUS", "silence", "Chorus éteint, sans jouer", "bruit de fond de ta chaîne d'enregistrement", {}),
    # Arpège et polyphonie
    ("G01", 2, "ARPÈGE", "accord-4-notes", "ARPEGGIO UP, RANGE 1, RATE 5", "vitesse de l'arpège à mi-course", None),
    ("G02", 2, "ARPÈGE", "accord-4-notes", "ARPEGGIO UP, RANGE 1, RATE 10", "vitesse maximale", None),
    ("G03", 2, "ARPÈGE", "accord-4-notes", "ARPEGGIO UP & DOWN, RANGE 2, RATE 5", "ordre des notes et octaves", None),
    ("H01", 2, "VOIX", "accord-4-notes", "ENV R 5", "accord, relâchement des voix", {'release': 5}),
]
