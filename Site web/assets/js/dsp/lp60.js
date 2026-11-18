// Moteur du LP-60 pour le navigateur : portage du C++ du plugin
// (LP60/Source/LP60Engine.h, Chorus.h, Arpeggiator.h, Presets.h).
// Les valeurs mesurées sur le JU-06A sont les mêmes que dans le plugin.

import {
  clamp, fastTanh, rcCoef, polyBlep, pulseWave, lookup, interpolate,
  Smoothed, XorShift, HalfbandDecimator, DelayLine,
} from "./common.js";

const TWO_PI = 2 * Math.PI;

//------------------------------------------------------------------------------
// Réglages, identifiants et valeurs par défaut identiques au plugin.
export const PARAMS = [
  { id: "arpOn", def: 0 }, { id: "arpMode", def: 0 }, { id: "arpRange", def: 0 }, { id: "arpRate", def: 5 },
  { id: "lfoRate", def: 5 }, { id: "lfoDelay", def: 0 }, { id: "lfoMode", def: 0 },
  { id: "range", def: 1 }, { id: "dcoLfo", def: 0 }, { id: "pwm", def: 0 }, { id: "pwmMode", def: 1 },
  { id: "pulse", def: 0 }, { id: "saw", def: 1 }, { id: "subOn", def: 0 }, { id: "sub", def: 0 }, { id: "noise", def: 0 },
  { id: "hpf", def: 0 },
  { id: "cutoff", def: 7 }, { id: "resonance", def: 0 }, { id: "envPolarity", def: 0 },
  { id: "vcfEnv", def: 0 }, { id: "vcfLfo", def: 0 }, { id: "vcfKybd", def: 0 },
  { id: "vcaMode", def: 0 }, { id: "level", def: 7 },
  { id: "attack", def: 0 }, { id: "decay", def: 5 }, { id: "sustain", def: 10 }, { id: "release", def: 3 },
  { id: "chorus1", def: 0 }, { id: "chorus2", def: 0 }, { id: "chorusHiss", def: 5 },
  { id: "tune", def: 0 }, { id: "bendDco", def: 2 }, { id: "bendVcf", def: 0 }, { id: "lfoMod", def: 5 },
];

export const PRESETS = [
  ["Init", {}],
  ["Warm Pad", { saw: 1, pulse: 1, pwm: 5, pwmMode: 0, lfoRate: 3, cutoff: 5.5, resonance: 1.5, vcfEnv: 2, vcfKybd: 4,
                 attack: 6, decay: 7, sustain: 8, release: 6.5, chorus2: 1 }],
  ["Strings", { saw: 1, cutoff: 6.5, vcfKybd: 5, hpf: 1, attack: 4.5, decay: 6, sustain: 9, release: 6, chorus1: 1 }],
  ["Brass", { saw: 1, cutoff: 3.5, resonance: 2, vcfEnv: 6, vcfKybd: 5, attack: 3, decay: 5.5, sustain: 6, release: 3.5 }],
  ["Bass", { saw: 0, pulse: 1, subOn: 1, sub: 8, range: 0, cutoff: 3, resonance: 3, vcfEnv: 6, vcfKybd: 3,
             decay: 4.5, sustain: 3, release: 2 }],
  ["Sub Bass", { saw: 0, pulse: 1, subOn: 1, sub: 10, range: 0, cutoff: 2.5, release: 2 }],
  ["Organ", { saw: 0, pulse: 1, subOn: 1, sub: 6, vcaMode: 1, cutoff: 7, hpf: 1, chorus1: 1, chorus2: 1 }],
  ["Pluck Arp", { saw: 1, cutoff: 3.5, resonance: 4, vcfEnv: 6, decay: 4, sustain: 0, release: 4, arpOn: 1, arpMode: 1,
                  arpRange: 1, arpRate: 5.5, chorus2: 1 }],
  ["Wide Lead", { saw: 1, pulse: 1, pwm: 6, pwmMode: 0, lfoRate: 6, lfoDelay: 6, dcoLfo: 1.5, cutoff: 6, resonance: 2.5,
                  vcfEnv: 3, decay: 5, sustain: 7, release: 3, chorus2: 1 }],
  ["Sweep Pad", { saw: 1, cutoff: 4, resonance: 5, vcfLfo: 4, lfoRate: 1, attack: 5, release: 7, chorus1: 1 }],
  ["Bells", { saw: 0, pulse: 1, pwm: 7, cutoff: 5, resonance: 7, vcfKybd: 10, vcfEnv: 4, decay: 7, sustain: 0, release: 7,
              chorus2: 1 }],
  ["Noise FX", { saw: 0, noise: 8, cutoff: 6, resonance: 6, vcfLfo: 5, lfoRate: 2, attack: 7, release: 8, hpf: 2, chorus1: 1 }],
];

// Le jeu (accord, levier) ne change pas avec les sons d'usine, comme dans le plugin.
const KEEP_ON_PRESET = new Set (["tune", "bendDco", "bendVcf", "lfoMod", "chorusHiss"]);

//------------------------------------------------------------------------------
// Tables mesurées sur le JU-06A.
const CUTOFF_X = [0, 2, 4, 5, 6, 8, 10];
const CUTOFF_Y = [3.0, 4.17, 7.57, 10.23, 12.72, 16.3, 16.3];
const RES_X = [0, 2.5, 5, 7.5, 10];
const RES_K = [0, 0.78, 2.7, 3.95, 4.45];
const RES_GAIN = [1, 1.32, 2, 2, 2.1];
const ATTACK_TIMES = [0.001, 0.012, 0.15, 1.05, 3.0];
const ENV_TIME_CONSTANTS = [0.0015, 0.0095, 0.214, 1.15, 3.81];
const LFO_RATES = [0.042, 0.42, 4.23, 16.8, 67.0];
const LFO_DELAYS = [0, 0.01, 0.05, 0.3, 1.75];
const LFO_FADES = [0, 0.1, 0.35, 1.0, 2.75];
const HPF_HZ = [0, 244, 579, 1000];

export const cutoffHz = slider => Math.pow (2, interpolate (CUTOFF_X, CUTOFF_Y, slider));
export const attackSeconds = slider => lookup (ATTACK_TIMES, slider);
export const envSeconds = slider => 3.5 * lookup (ENV_TIME_CONSTANTS, slider);
export const lfoHz = slider => lookup (LFO_RATES, slider);

const PULSE_LEVEL = 0.86;
const CUTOFF_SPREAD = [0.02, -0.03, 0.01, -0.01, 0.03, -0.02];
const GAIN_SPREAD = [1.0, 0.97, 1.02, 0.99, 1.01, 0.98];

const IDLE = 0, ATTACK = 1, DECAY = 2, RELEASE = 3;

//------------------------------------------------------------------------------
class Voice {
  constructor (index) {
    this.phase = 0; this.noiseState = 0; this.subHigh = false;
    this.gated = false; this.note = -1;
    this.rng = new XorShift ((0x9e3779b9 ^ (index * 0x85ebca6b + 1)) >>> 0);
    this.cutoffOffset = CUTOFF_SPREAD[index];
    this.gain = GAIN_SPREAD[index];
    this.stage = IDLE; this.level = 0;            // enveloppe
    this.gate = 0;                                // rampe du mode GATE
    this.s = new Float64Array (4); this.y = new Float64Array (4); this.u = 0;   // filtre
    this.a = new Float64Array (4); this.b = new Float64Array (4);
  }

  silent () { return ! this.gated && this.stage === IDLE && this.gate <= 0; }
  gateOn () { this.gated = true; this.stage = ATTACK; }
  gateOff () { this.gated = false; if (this.stage !== IDLE) this.stage = RELEASE; }

  envelope (e) {
    switch (this.stage) {
      case ATTACK:
        this.level += (2 - this.level) * e.attackCoef;
        if (this.level >= 1) { this.level = 1; this.stage = DECAY; }
        break;
      case DECAY:
        this.level += (e.sustain - this.level) * e.decayCoef;
        break;
      case RELEASE:
        this.level -= this.level * e.releaseCoef;
        if (this.level < 1e-4) { this.level = 0; this.stage = IDLE; }
        break;
    }
    return this.level;
  }

  // Filtre 24 dB/oct à quatre étages saturés (zero-delay feedback).
  filter (x, g, k) {
    const slope = v => (Math.abs (v) < 1e-4 ? 1 : fastTanh (v) / v);
    const input = x * 0.5, s = this.s, y = this.y, a = this.a, b = this.b;
    let inputSlope = slope (this.u), A = 1, B = 0;
    for (let i = 0; i < 4; ++i) {
      const d = 1 + g * slope (y[i]);
      a[i] = g * inputSlope / d;
      b[i] = s[i] / d;
      A = a[i] * A;
      B = a[i] * B + b[i];
      inputSlope = slope (y[i]);
    }
    this.u = (input - k * B) / (1 + k * A);
    let stageInput = this.u;
    for (let i = 0; i < 4; ++i) {
      y[i] = a[i] * stageInput + b[i];
      s[i] = 2 * y[i] - s[i];
      stageInput = y[i];
    }
    return y[3] * 2;
  }

  tick (f, e) {
    const p = e.p;
    const pitch = this.note + [-12, 0, 12][p.range] + f.pitch;
    const inc = clamp (440 * Math.pow (2, (pitch - 69) / 12) / e.osRate, 0, 0.45);
    const envLevel = this.envelope (e);

    let pwmAmount = f.pwm;
    if (p.pwmMode === 0) pwmAmount *= 0.6 + 0.52 * f.lfo;
    else if (p.pwmMode === 2) pwmAmount *= envLevel;
    const a = clamp (pwmAmount, 0, 1);
    const width = 0.49 - 0.35 * a - 0.1 * a * a;

    const ph = this.phase;
    let mix = 0;
    if (p.saw) mix += 2 * ph - 1 - polyBlep (ph, inc);
    if (p.pulse) {
      let q = ph + width;
      if (q >= 1) q -= 1;
      const v = pulseWave (q, inc, width) - (2 * width - 1);
      mix += v * PULSE_LEVEL / (2 * Math.sqrt (width * (1 - width)));
    }
    if (f.sub > 0) mix += f.sub * PULSE_LEVEL * pulseWave (0.5 * (ph + (this.subHigh ? 1 : 0)), inc * 0.5, 0.5);

    this.phase += inc;
    if (this.phase >= 1) { this.phase -= 1; this.subHigh = ! this.subHigh; }

    this.noiseState += (this.rng.white () - this.noiseState) * e.noiseCoef;
    if (f.noise > 0) mix += f.noise * this.noiseState * 2;

    const octaves = f.cutoffOct + this.cutoffOffset + f.vcfKybd * (this.note - 60) / 12 + f.vcfEnv * envLevel;
    const fc = clamp (Math.pow (2, octaves), 8, 0.45 * e.osRate);
    const g = Math.tan (Math.PI * fc / e.osRate);
    const filtered = this.filter ((mix * 0.4 + this.rng.white () * 1e-5) * f.resonanceGain, g, f.resonance);

    this.gate = this.gated ? Math.min (1, this.gate + e.gateUp) : Math.max (0, this.gate - e.gateDown);
    return filtered * (p.vcaGate ? this.gate : envLevel) * this.gain;
  }
}

//------------------------------------------------------------------------------
// Chorus BBD : deux lignes à retard modulées en opposition, souffle compris.
class Chorus {
  constructor (rate) {
    this.rate = rate;
    this.lineL = new DelayLine (Math.floor (rate * 0.02));
    this.lineR = new DelayLine (Math.floor (rate * 0.02));
    this.filterCoef = rcCoef (1 / (TWO_PI * 9000), rate);
    this.state = new Float64Array (6);          // entrée, sortie gauche, sortie droite (deux pôles chacun)
    this.phase = 0; this.increment = 0; this.centre = 0.0035; this.depth = 0;
    this.mode = 0;
    this.hissGain = 0; this.hissL = 0; this.hissR = 0;
    this.hissCoef = rcCoef (1 / (TWO_PI * 1000), rate);
    this.hissRateScale = Math.sqrt (rate / 48000);
    this.seedL = new XorShift (0x1234567); this.seedR = new XorShift (0x7654321);
  }

  setMode (mode) {
    this.mode = mode;
    const set = (hz, lo, hi) => {
      this.increment = hz / this.rate; this.centre = 0.5 * (lo + hi); this.depth = 0.5 * (hi - lo);
    };
    if (mode === 1) set (0.552, 0.00145, 0.00555);
    else if (mode === 2) set (0.887, 0.00145, 0.00555);
    else if (mode === 3) set (7.55, 0.00295, 0.00405);
  }

  setHiss (amount) {
    this.hissGain = amount <= 0 ? 0 : 0.00161 * Math.pow (2, (clamp (amount, 0, 10) - 5) * 0.4);
  }

  process (left, right, n) {
    const c = this.filterCoef, st = this.state, rate = this.rate;
    for (let i = 0; i < n; ++i) {
      const x = left[i];
      const tri = 1 - 4 * Math.abs (this.phase - 0.5);
      this.phase += this.increment;
      if (this.phase >= 1) this.phase -= Math.floor (this.phase);

      st[0] += (x - st[0]) * c; st[1] += (st[0] - st[1]) * c;
      this.lineL.push (st[1]);
      this.lineR.push (st[1]);

      this.hissL += (this.seedL.white () - this.hissL) * this.hissCoef;
      this.hissR += (this.seedR.white () - this.hissR) * this.hissCoef;
      const hl = this.hissGain * this.hissRateScale * this.hissL;
      const hr = this.hissGain * this.hissRateScale * this.hissR;

      const wl = fastTanh (this.lineL.read ((this.centre + this.depth * tri) * rate)) + hl;
      const wr = fastTanh (this.lineR.read ((this.centre - this.depth * tri) * rate)) + hr;
      st[2] += (wl - st[2]) * c; st[3] += (st[2] - st[3]) * c;
      st[4] += (wr - st[4]) * c; st[5] += (st[4] - st[5]) * c;

      left[i] = (x + st[3]) * 0.67;
      right[i] = (x + st[5]) * 0.67;
    }
  }
}

//------------------------------------------------------------------------------
export class LP60 {
  constructor (sampleRate) {
    this.baseRate = sampleRate;
    this.osRate = sampleRate * 2;
    this.values = Object.fromEntries (PARAMS.map (p => [p.id, p.def]));
    this.voices = Array.from ({ length: 6 }, (_, i) => new Voice (i));
    this.nextVoice = 0; this.numHeld = 0;
    this.decimator = new HalfbandDecimator ();
    this.osBuffer = new Float32Array (4096);
    this.chorus = new Chorus (sampleRate);

    this.smoothCoef = rcCoef (0.005, this.osRate);
    this.noiseCoef = rcCoef (1 / (TWO_PI * 3800), this.osRate);
    this.gateUp = 1 / (0.003 * this.osRate);
    this.gateDown = 1 / (0.006 * this.osRate);
    this.dcCoef = 1 - TWO_PI * 10 / sampleRate;
    this.dcX = 0; this.dcY = 0; this.hpfState = 0;

    for (const name of ["tune", "dcoLfo", "pwm", "subLevel", "noiseLevel", "cutoffOct", "resonance", "resonanceGain",
                        "vcfEnv", "vcfLfo", "vcfKybd", "level", "modWheel", "lfoGain", "bend"])
      this[name] = new Smoothed ();
    this.lfoPhase = 0; this.lfoTimer = 0; this.pitchBend = 0;

    // Arpégiateur
    this.held = [];                 // touches tenues, dans l'ordre joué
    this.arpIndex = 0; this.arpNote = -1; this.arpCountdown = 0; this.arpOffCountdown = -1;
    this.tempo = 120;

    this.update ();
    for (const name of ["tune", "dcoLfo", "pwm", "subLevel", "noiseLevel", "cutoffOct", "resonance", "resonanceGain",
                        "vcfEnv", "vcfLfo", "vcfKybd", "level", "modWheel", "lfoGain", "bend"])
      this[name].snap ();
  }

  setParam (id, value) {
    if (! (id in this.values)) return;
    this.values[id] = value;
    this.update ();
  }

  loadPreset (index) {
    const [, values] = PRESETS[index] || PRESETS[0];
    for (const p of PARAMS) if (! KEEP_ON_PRESET.has (p.id)) this.values[p.id] = p.def;
    Object.assign (this.values, values);
    this.update ();
    return { ...this.values };
  }

  // Lecture des réglages, comme LP60Processor::readParams puis Engine::setParams.
  update () {
    const v = this.values, rate = this.osRate;
    const flag = id => v[id] > 0.5, choice = id => Math.round (v[id]);
    const wasArp = this.p && this.p.arpOn;
    this.p = {
      range: clamp (choice ("range"), 0, 2), pwmMode: choice ("pwmMode"), pulse: flag ("pulse"), saw: flag ("saw"),
      vcaGate: choice ("vcaMode") === 1, lfoMode: choice ("lfoMode"), bendDco: v.bendDco, bendVcf: v.bendVcf, lfoMod: v.lfoMod,
      arpOn: flag ("arpOn"), arpMode: choice ("arpMode"), arpRange: choice ("arpRange") + 1, arpRate: v.arpRate,
    };
    this.tune.target = clamp (v.tune, -50, 50) / 100;
    this.dcoLfo.target = v.dcoLfo / 10 * 3.8;
    this.pwm.target = v.pwm / 10;
    this.subLevel.target = flag ("subOn") ? v.sub / 10 : 0;
    this.noiseLevel.target = v.noise / 10;
    this.cutoffOct.target = interpolate (CUTOFF_X, CUTOFF_Y, v.cutoff);
    this.resonance.target = interpolate (RES_X, RES_K, v.resonance);
    this.resonanceGain.target = interpolate (RES_X, RES_GAIN, v.resonance);
    this.vcfEnv.target = (choice ("envPolarity") === 1 ? -1 : 1) * 10.6 * Math.pow (clamp (v.vcfEnv, 0, 10) / 10, 1.46);
    this.vcfLfo.target = v.vcfLfo / 10 * 3.3;
    this.vcfKybd.target = v.vcfKybd / 10;
    this.level.target = 0.6 * (v.level / 10) ** 2;

    this.attackCoef = rcCoef (lookup (ATTACK_TIMES, v.attack) / Math.LN2, rate);
    this.decayCoef = rcCoef (lookup (ENV_TIME_CONSTANTS, v.decay), rate);
    this.releaseCoef = rcCoef (lookup (ENV_TIME_CONSTANTS, v.release), rate);
    this.sustain = Math.pow (clamp (v.sustain, 0, 10) / 10, 0.62);

    this.lfoIncrement = lookup (LFO_RATES, v.lfoRate) / rate;
    this.lfoDelaySeconds = lookup (LFO_DELAYS, v.lfoDelay);
    this.lfoFadeSeconds = lookup (LFO_FADES, v.lfoDelay);

    const hpf = clamp (choice ("hpf"), 0, 3);
    this.hpfCoef = hpf === 0 ? 0 : rcCoef (1 / (TWO_PI * HPF_HZ[hpf]), this.baseRate);

    this.chorus.setMode ((flag ("chorus1") ? 1 : 0) + (flag ("chorus2") ? 2 : 0));
    this.chorus.setHiss (v.chorusHiss);

    if (wasArp && ! this.p.arpOn) this.stopArpNote ();
  }

  setTempo (bpm) { this.tempo = bpm; }
  setPitchBend (x) { this.pitchBend = clamp (x, -1, 1); }
  setModWheel (x) { this.modWheel.target = clamp (x, 0, 1); }

  //----------------------------------------------------------------------------
  // Clavier (passe par l'arpégiateur s'il est allumé).
  noteOn (note) {
    if (this.p.arpOn) {
      if (! this.held.includes (note)) {
        if (this.held.length === 0) { this.arpIndex = 0; this.arpCountdown = 0; }
        this.held.push (note);
      }
      return;
    }
    this.voiceOn (note);
  }

  noteOff (note) {
    if (this.p.arpOn) {
      this.held = this.held.filter (n => n !== note);
      return;
    }
    this.voiceOff (note);
  }

  allNotesOff () {
    this.held = [];
    this.stopArpNote ();
    for (const v of this.voices) v.gateOff ();
    this.numHeld = 0;
  }

  // Attribution des voix en rotation, comme le mode ROTARY du Juno-60.
  voiceOn (note) {
    if (this.numHeld === 0) this.lfoTimer = 0;
    ++this.numHeld;
    let chosen = this.voices.findIndex (v => v.gated && v.note === note);
    for (let i = 0; i < 6 && chosen < 0; ++i) {
      const c = (this.nextVoice + i) % 6;
      if (! this.voices[c].gated) chosen = c;
    }
    if (chosen < 0) chosen = this.nextVoice;
    this.nextVoice = (chosen + 1) % 6;
    const v = this.voices[chosen];
    v.note = note;
    v.gateOn ();
  }

  voiceOff (note) {
    for (const v of this.voices)
      if (v.gated && v.note === note) { v.gateOff (); this.numHeld = Math.max (0, this.numHeld - 1); }
  }

  stopArpNote () {
    if (this.arpNote >= 0) this.voiceOff (this.arpNote);
    this.arpNote = -1;
    this.arpOffCountdown = -1;
  }

  // Note suivante de l'arpège (UP, U&D, DOWN sur 1 à 3 octaves).
  nextArpNote () {
    const base = [...this.held].sort ((a, b) => a - b);
    const notes = [];
    for (let o = 0; o < this.p.arpRange; ++o)
      for (const n of base) if (n + 12 * o <= 127) notes.push (n + 12 * o);
    const i = this.arpIndex++, count = notes.length;
    if (this.p.arpMode === 1) {
      if (count === 1) return notes[0];
      const period = 2 * count - 2, k = i % period;
      return notes[k < count ? k : period - k];
    }
    if (this.p.arpMode === 2) return notes[count - 1 - (i % count)];
    return notes[i % count];
  }

  arpStepSamples () {
    return this.baseRate / (1.5 * Math.pow (50 / 1.5, clamp (this.p.arpRate, 0, 10) / 10));
  }

  //----------------------------------------------------------------------------
  tick () {
    const a = this.smoothCoef, p = this.p;
    const triangle = 1 - 4 * Math.abs (this.lfoPhase - 0.5);
    this.lfoPhase += this.lfoIncrement;
    if (this.lfoPhase >= 1) this.lfoPhase -= 1;

    let gainTarget;
    if (p.lfoMode === 0) {
      this.lfoTimer = Math.min (this.lfoTimer + 1 / this.osRate, 60);
      const t = this.lfoTimer - this.lfoDelaySeconds;
      gainTarget = t <= 0 ? 0 : (this.lfoFadeSeconds <= 0 ? 1 : Math.min (1, t / this.lfoFadeSeconds));
    } else {
      gainTarget = this.modWheel.target;
    }
    this.lfoGain.target = gainTarget;
    const lfo = triangle * this.lfoGain.next (a);
    const wheel = this.modWheel.next (a);
    this.bend.target = this.pitchBend;
    const bend = this.bend.next (a);

    const f = this.frame || (this.frame = {});
    f.lfo = lfo;
    f.pitch = this.tune.next (a) + bend * p.bendDco + lfo * this.dcoLfo.next (a);
    if (p.lfoMode === 0) f.pitch += triangle * wheel * p.lfoMod / 10 * 3;
    f.cutoffOct = this.cutoffOct.next (a) + bend * p.bendVcf / 10 * 3 + lfo * this.vcfLfo.next (a);
    f.resonance = this.resonance.next (a);
    f.resonanceGain = this.resonanceGain.next (a);
    f.vcfEnv = this.vcfEnv.next (a);
    f.vcfKybd = this.vcfKybd.next (a);
    f.pwm = this.pwm.next (a);
    f.sub = this.subLevel.next (a);
    f.noise = this.noiseLevel.next (a);

    let sum = 0;
    for (const v of this.voices) if (! v.silent ()) sum += v.tick (f, this);
    return sum * this.level.next (a);
  }

  renderMono (out, offset, n) {
    if (this.osBuffer.length < 2 * n) this.osBuffer = new Float32Array (4 * n);
    const os = this.osBuffer;
    for (let i = 0; i < 2 * n; ++i) os[i] = this.tick ();
    const dst = out.subarray (offset, offset + n);
    this.decimator.process (os, dst, n);
    for (let i = 0; i < n; ++i) {
      let x = dst[i];
      if (this.hpfCoef > 0) { this.hpfState += (x - this.hpfState) * this.hpfCoef; x -= this.hpfState; }
      this.dcY = x - this.dcX + this.dcCoef * this.dcY;
      this.dcX = x;
      dst[i] = this.dcY;
    }
  }

  // Rend n échantillons stéréo (avec l'arpège, découpé à chaque pas).
  render (left, right, n) {
    let pos = 0;
    while (pos < n) {
      let chunk = n - pos;
      if (this.p.arpOn) {
        if (this.held.length === 0 && this.arpNote >= 0 && this.arpOffCountdown < 0) this.stopArpNote ();
        const untilStep = this.held.length ? Math.max (0, Math.ceil (this.arpCountdown)) : Infinity;
        const untilOff = this.arpOffCountdown >= 0 ? this.arpOffCountdown : Infinity;
        chunk = Math.min (chunk, untilStep, untilOff);
        if (chunk === 0) {
          if (untilOff === 0) this.stopArpNote ();
          if (untilStep === 0 && this.held.length) {
            if (this.arpNote >= 0) this.voiceOff (this.arpNote);
            this.arpNote = this.nextArpNote ();
            this.voiceOn (this.arpNote);
            const step = this.arpStepSamples ();
            this.arpCountdown += step;
            this.arpOffCountdown = Math.max (1, Math.floor (step * 0.5));
          }
          continue;
        }
      }
      this.renderMono (left, pos, chunk);
      if (this.p.arpOn) {
        this.arpCountdown -= chunk;
        if (this.arpOffCountdown >= 0) this.arpOffCountdown -= chunk;
      }
      pos += chunk;
    }

    if (this.chorus.mode > 0) this.chorus.process (left, right, n);
    else right.set (left.subarray (0, n));
  }
}
