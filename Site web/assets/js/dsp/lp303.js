// Moteur de LP-303 pour le navigateur : portage du C++ du plugin
// (LP303/Source/AcidEngine.h, Presets.h et Sequencer.h pour les motifs).

import { clamp, fastTanh, rcCoef, HalfbandDecimator } from "./common.js";

//------------------------------------------------------------------------------
// Réglages, identifiants et valeurs par défaut identiques au plugin.
export const PARAMS = [
  { id: "wave", def: 0 }, { id: "tune", def: 0 }, { id: "cutoff", def: 3.5 }, { id: "resonance", def: 7 },
  { id: "envMod", def: 6 }, { id: "decay", def: 4 }, { id: "accent", def: 6 }, { id: "slide", def: 0.08 },
  { id: "drive", def: 0 }, { id: "volume", def: 7 },
];

// Sons d'usine : [nom, réglages qui changent].
export const PRESETS = [
  ["Init", {}],
  ["Classic Squelch", { cutoff: 3, resonance: 8, envMod: 6, decay: 4, accent: 7 }],
  ["Rubber Square", { wave: 1, cutoff: 3.5, resonance: 6.5, envMod: 5, decay: 3, accent: 6 }],
  ["Deep Sub", { cutoff: 1.5, resonance: 3, envMod: 2.5, decay: 6, accent: 3, volume: 8 }],
  ["Screamer", { cutoff: 4.5, resonance: 9.5, envMod: 8, decay: 3.5, accent: 9, drive: 6 }],
  ["Distorted Acid", { cutoff: 3, resonance: 8.5, envMod: 7, decay: 5, accent: 8, drive: 8, volume: 6 }],
  ["Wah Tone", { wave: 1, cutoff: 2, resonance: 9, envMod: 9, decay: 7, accent: 8 }],
  ["Plucky", { cutoff: 2.5, resonance: 5, envMod: 7, decay: 1.5, accent: 5 }],
  ["Long Slide", { cutoff: 4, resonance: 7, envMod: 4, decay: 6, accent: 6, slide: 0.25 }],
  ["Acid Lead", { wave: 1, tune: 12, cutoff: 5, resonance: 7, envMod: 5, decay: 4, accent: 7, drive: 4 }],
];

// Motifs d'usine, même écriture que dans le plugin : C C# … B, C^ = do aigu,
// « . » = silence ; u = octave haut, d = bas, a = accent, s = slide.
export const PATTERNS = [
  ["Acid Roll", 16, "C Ca Cu C . D#s F Cu Ca . A#d C Gs Cu C D#a"],
  ["Squelch", 16, "Cd Cs C Cua . Cd Gd A#d Cd Cs D#a Cd . F Fs Ga"],
  ["Slide Walk", 16, "C Ds D#s F Gs G#a G Fs D# Cs Cu A#s G F D#a D"],
  ["Hypno 12", 12, "C Cu . C Cs Cu C . D#a C Cu A#d . . . ."],
  ["Offbeat Pump", 16, ". C . Ca . C . Cu . C . Ca . D# . F"],
  ["Minor Run", 16, "C D# F G A#s Cu A#s G F D# C A#d Gd A#d Ca ."],
  ["Bounce", 16, "Cd C Cd Cu Cd Ca Cd D# Cd F Cd Fs F#s G Cd A#a"],
  ["Sparse Wow", 8, "Ca . . Cs Cua . D#s Ca . . . . . . . ."],
];

export const BASE_NOTE = 36;
const NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];

export function parseStep (token) {
  const step = { note: 0, octave: 0, accent: false, slide: false, gate: true };
  if (! token || token === ".") return { ...step, gate: false };
  let length = 0;
  if (token.startsWith ("C^")) { step.note = 12; length = 2; }
  else {
    for (let n = 11; n >= 0; --n)
      if (token.startsWith (NOTE_NAMES[n])) { step.note = n; length = NOTE_NAMES[n].length; break; }
    if (! length) return { ...step, gate: false };
  }
  const mods = token.slice (length);
  if (mods.includes ("u")) step.octave = 1;
  if (mods.includes ("d")) step.octave = -1;
  step.accent = mods.includes ("a");
  step.slide = mods.includes ("s");
  return step;
}

export const factoryPattern = i => {
  const [name, length, text] = PATTERNS[i];
  return { name, length, steps: text.split (" ").map (parseStep) };
};

// Motif « acid » au hasard, comme le bouton RANDOM du plugin.
export function randomPattern () {
  const scale = [0, 0, 0, 0, 3, 5, 7, 7, 10, 12];
  const steps = [];
  for (let s = 0; s < 16; ++s) {
    const r = Math.random ();
    steps.push ({ note: scale[Math.floor (Math.random () * scale.length)], octave: r < 0.2 ? 1 : r < 0.3 ? -1 : 0,
                  accent: Math.random () < 0.25, slide: Math.random () < 0.2, gate: Math.random () > 0.15 });
  }
  return { name: "Random", length: 16, steps };
}

export const cutoffHz = v => 60 * Math.pow (2, 0.7 * v);
export const decaySeconds = v => 0.2 * Math.pow (12.5, clamp (v, 0, 10) / 10);

//------------------------------------------------------------------------------
// Tables d'onde à bande limitée, une par octave (dents de scie et carré).
const TABLE_ORDER = 12, TABLE_SIZE = 1 << TABLE_ORDER, NUM_TABLES = 12, LOWEST_HZ = 8;

// Transformée de Fourier inverse en place (radix 2), pour remplir les tables.
function inverseFft (re, im) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; ++i) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { [re[i], re[j]] = [re[j], re[i]]; [im[i], im[j]] = [im[j], im[i]]; }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const angle = 2 * Math.PI / len, wr = Math.cos (angle), wi = Math.sin (angle);
    for (let i = 0; i < n; i += len) {
      let cr = 1, ci = 0;
      for (let k = 0; k < len / 2; ++k) {
        const a = i + k, b = a + len / 2;
        const tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
        re[b] = re[a] - tr; im[b] = im[a] - ti;
        re[a] += tr; im[a] += ti;
        const ncr = cr * wr - ci * wi;
        ci = cr * wi + ci * wr; cr = ncr;
      }
    }
  }
}

function buildTables (osRate) {
  const limitHz = 0.4 * osRate;
  const tables = [[], []];
  for (let wave = 0; wave < 2; ++wave) {
    for (let t = 0; t < NUM_TABLES; ++t) {
      const topHz = LOWEST_HZ * Math.pow (2, t + 1);
      const harmonics = clamp (Math.floor (limitHz / topHz), 1, TABLE_SIZE / 2 - 1);
      const re = new Float64Array (TABLE_SIZE), im = new Float64Array (TABLE_SIZE);
      for (let n = 1; n <= harmonics; ++n) {
        // Dents de scie montante : -2/pi somme sin / n ; carré : 4/pi somme des impaires.
        const a = wave === 0 ? -2 / (Math.PI * n) : (n % 2 ? 4 / (Math.PI * n) : 0);
        im[n] = -a / 2;
        im[TABLE_SIZE - n] = a / 2;
      }
      inverseFft (re, im);
      const table = new Float32Array (TABLE_SIZE + 1);
      for (let i = 0; i < TABLE_SIZE; ++i) table[i] = re[i];
      table[TABLE_SIZE] = table[0];
      tables[wave].push (table);
    }
  }
  return tables;
}

//------------------------------------------------------------------------------
// Filtre passe-bas à diodes 4 pôles, intégration trapézoïdale à retard nul,
// saturations linéarisées autour de l'échantillon précédent (voir le C++).
const CRITICAL_K = 18.4, PEAK_RATIO = 1.1956;

function tanhSlope (v) {
  const a = Math.abs (v);
  if (a >= 3) return 1 / a;
  const v2 = v * v;
  return (27 + v2) / (27 + 9 * v2);
}

class DiodeLadder {
  constructor () { this.reset (); }
  reset () { this.s = new Float64Array (4); this.y = new Float64Array (4); this.x = 0; }

  process (input, g, k) {
    const s = this.s, y = this.y;
    const gc1 = g * tanhSlope (this.x - y[0]), gc2 = g * tanhSlope (y[0] - y[1]);
    const gc3 = g * tanhSlope (y[1] - y[2]), gc4 = g * tanhSlope (y[2] - y[3]);

    const d1 = 1 + gc1 + gc2;
    const p1 = (s[0] + gc1 * input) / d1, q1 = gc2 / d1, r1 = -gc1 * k / d1;
    const e2 = 1 + gc2 + gc3 - gc2 * q1;
    const p2 = (s[1] + gc2 * p1) / e2, q2 = gc3 / e2, r2 = gc2 * r1 / e2;
    const e3 = 1 + gc3 + gc4 - gc3 * q2;
    const p3 = (s[2] + gc3 * p2) / e3, t3 = (gc4 + gc3 * r2) / e3;

    const y4 = (s[3] + gc4 * p3) / (1 + gc4 - gc4 * t3);
    const y3 = p3 + t3 * y4;
    const y2 = p2 + q2 * y3 + r2 * y4;
    const y1 = p1 + q1 * y2 + r1 * y4;

    y[0] = y1; y[1] = y2; y[2] = y3; y[3] = y4;
    this.x = input - k * y4;
    for (let i = 0; i < 4; ++i) s[i] = 2 * y[i] - s[i];

    if (! Number.isFinite (y4) || Math.abs (y4) > 50) { this.reset (); return 0; }
    return y4;
  }
}

//------------------------------------------------------------------------------
const IDLE = 0, ATTACK = 1, DECAY = 2, RELEASE = 3;
const ACCENT_VELOCITY = 100 / 127;

export class LP303 {
  constructor (sampleRate) {
    this.baseRate = sampleRate;
    this.osRate = sampleRate * 4;
    this.tables = buildTables (this.osRate);
    this.filter = new DiodeLadder ();
    this.stage1 = new HalfbandDecimator ();     // 4x -> 2x
    this.stage2 = new HalfbandDecimator ();     // 2x -> 1x
    this.osBuffer = new Float32Array (4 * 128);
    this.midBuffer = new Float32Array (2 * 128);

    const os = this.osRate;
    this.smoothCoef = rcCoef (0.004, os);
    this.attackCoef = rcCoef (0.0012, os);
    this.releaseCoef = rcCoef (0.004, os);
    this.vcaDecayCoef = rcCoef (0.9, os);
    this.accentVcaCoef = rcCoef (0.002, os);
    this.squareHpCoef = rcCoef (1 / (2 * Math.PI * 70), os);
    this.dcCoef = 1 - 2 * Math.PI * 12 / sampleRate;

    this.values = Object.fromEntries (PARAMS.map (p => [p.id, p.def]));
    this.held = [];
    this.pitch = 36; this.targetPitch = 36; this.gliding = false; this.bend = 0;
    this.phase = 0; this.squareLow = 0;
    this.vcaStage = IDLE; this.vca = 0; this.meg = 0; this.accentSweep = 0; this.accentVca = 0;
    this.accented = false; this.megAccented = false;
    this.dcX = 0; this.dcY = 0;
    this.lastCutoff = 0;

    this.targets = {}; this.smoothed = {};
    this.update ();
    Object.assign (this.smoothed, this.targets);
  }

  setParam (id, value) {
    if (! (id in this.values)) return;
    this.values[id] = value;
    this.update ();
  }

  loadPreset (index) {
    const preset = PRESETS[clamp (index | 0, 0, PRESETS.length - 1)];
    for (const p of PARAMS) this.values[p.id] = p.def;
    Object.assign (this.values, preset[1]);
    this.update ();
    return { ...this.values };
  }

  update () {
    const v = this.values, os = this.osRate;
    const env = v.envMod / 10;
    const r = clamp (v.resonance / 10, 0, 1);
    const t = this.targets;
    t.cutoffOct = Math.log2 (60) + v.cutoff * 0.7 - 0.8 * env;
    t.envOct = 4.5 * env;
    t.k = 0.9 * CRITICAL_K * Math.pow (r, 1.3);
    t.accent = clamp (v.accent / 10, 0, 1);
    t.drive = clamp (v.drive / 10, 0, 1);
    const vol = clamp (v.volume / 10, 0, 1);
    t.gain = vol * vol;

    this.megCoef = Math.exp (-4.6 / (decaySeconds (v.decay) * os));
    this.megAccentCoef = Math.exp (-4.6 / (0.2 * os));
    this.accentSweepCoef = rcCoef (0.005 + 0.04 * r, os);
    this.glideCoef = rcCoef (Math.max (0.001, v.slide) / 3, os);
  }

  setPitchBend (v) { this.bend = v; }
  setModWheel () {}
  setTempo () {}

  // Clavier monophonique, dernière note prioritaire ; legato = slide, vélocité >= 100 = accent.
  noteOn (note, velocity = 1) {
    const accent = velocity >= ACCENT_VELOCITY - 1e-4;
    this.held = this.held.filter (n => n !== note);
    const legato = this.held.length > 0;
    this.held.push (note);
    this.targetPitch = note;
    this.accented = accent;
    if (legato) this.gliding = true;
    else {
      this.gliding = false;
      this.pitch = note;
      this.vcaStage = ATTACK;
      this.meg = 1;
      this.megAccented = accent;
    }
  }

  noteOff (note) {
    const wasTop = this.held.length && this.held[this.held.length - 1] === note;
    this.held = this.held.filter (n => n !== note);
    if (! this.held.length) { if (this.vcaStage !== IDLE) this.vcaStage = RELEASE; }
    else if (wasTop) { this.targetPitch = this.held[this.held.length - 1]; this.gliding = true; }
  }

  allNotesOff () {
    this.held = [];
    if (this.vcaStage !== IDLE) this.vcaStage = RELEASE;
  }

  tick () {
    const t = this.targets, s = this.smoothed, c = this.smoothCoef, v = this.values;
    s.cutoffOct += (t.cutoffOct - s.cutoffOct) * c;
    s.envOct += (t.envOct - s.envOct) * c;
    s.k += (t.k - s.k) * c;
    s.accent += (t.accent - s.accent) * c;
    s.drive += (t.drive - s.drive) * c;
    s.gain += (t.gain - s.gain) * c;

    if (this.gliding) this.pitch += (this.targetPitch - this.pitch) * this.glideCoef;
    const note = this.pitch + v.tune + this.bend * 2;
    const hz = 440 * Math.pow (2, (note - 69) / 12);
    const inc = clamp (hz / this.osRate, 0, 0.25);

    // Oscillateur à tables d'onde.
    const wave = v.wave > 0.5 ? 1 : 0;
    const ti = clamp (Math.floor (Math.log2 (Math.max (LOWEST_HZ, hz) / LOWEST_HZ)), 0, NUM_TABLES - 1);
    const table = this.tables[wave][ti];
    const pos = this.phase * TABLE_SIZE;
    const i = Math.min (TABLE_SIZE - 1, pos | 0);
    let osc = table[i] + (pos - i) * (table[i + 1] - table[i]);
    if (wave) {
      this.squareLow += (osc - this.squareLow) * this.squareHpCoef;
      osc -= 0.85 * this.squareLow;
    }
    this.phase += inc;
    if (this.phase >= 1) this.phase -= 1;

    // Enveloppe du filtre, accent.
    this.meg *= this.megAccented ? this.megAccentCoef : this.megCoef;
    const accentAmount = this.accented ? s.accent : 0;
    this.accentVca += (accentAmount - this.accentVca) * this.accentVcaCoef;
    this.accentSweep += ((this.accented ? this.meg * s.accent : 0) - this.accentSweep) * this.accentSweepCoef;

    // Ampli.
    switch (this.vcaStage) {
      case ATTACK:
        this.vca += (1.02 - this.vca) * this.attackCoef;
        if (this.vca >= 1) { this.vca = 1; this.vcaStage = DECAY; }
        break;
      case DECAY: this.vca -= this.vca * this.vcaDecayCoef; break;
      case RELEASE:
        this.vca -= this.vca * this.releaseCoef;
        if (this.vca < 1e-5) { this.vca = 0; this.vcaStage = IDLE; }
        break;
      default: break;
    }

    // Filtre.
    const logF = s.cutoffOct + s.envOct * this.meg + 6 * this.accentSweep;
    const fc = clamp (Math.pow (2, logF), 15, Math.min (20000, 0.2 * this.osRate));
    this.lastCutoff = fc;
    const g = Math.tan (Math.PI * (fc / PEAK_RATIO) / this.osRate);
    const k = s.k;
    const input = osc * 0.07 * (1 + 0.6 * this.accentVca) * (1 + 0.3 * k);
    const filtered = this.filter.process (input, g, k) * 12.5;

    let out = 1.25 * fastTanh (filtered * this.vca * (1 + this.accentVca) * 0.8);

    if (s.drive > 0.001) {
      const d = s.drive, pre = 1 + 30 * d * d, bias = 0.25 * d;
      const shaped = (fastTanh (pre * out + bias) - fastTanh (bias)) / (1 + 0.03 * (pre - 1));
      out += (shaped - out) * Math.min (1, d * 4);
    }
    return out * s.gain;
  }

  render (left, right, n) {
    if (this.osBuffer.length < 4 * n) { this.osBuffer = new Float32Array (4 * n); this.midBuffer = new Float32Array (2 * n); }
    const os = this.osBuffer, mid = this.midBuffer;
    for (let i = 0; i < 4 * n; ++i) os[i] = this.tick ();
    this.stage1.process (os, mid, 2 * n);
    this.stage2.process (mid, left, n);
    for (let i = 0; i < n; ++i) {
      const x = left[i];
      this.dcY = x - this.dcX + this.dcCoef * this.dcY;
      this.dcX = x;
      left[i] = this.dcY;
    }
    right.set (left.subarray (0, n));
  }
}
