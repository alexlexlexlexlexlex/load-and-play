// Moteur de Model Play pour le navigateur : portage du C++ du plugin
// (MiniMoog/Source/SynthEngine.h et Effects.h).

import { clamp, fastTanh, polyBlep, Smoothed, HalfbandDecimator, DelayLine } from "./common.js";

const TWO_PI = 2 * Math.PI;

// Comme dans le plugin : ratio fixe à quel point de l'exponentielle la durée est écoulée.
const rcCoef = (seconds, rate, ratio) => 1 - Math.exp (-ratio / (Math.max (seconds, 1e-4) * rate));

//------------------------------------------------------------------------------
// Réglages, identifiants et valeurs par défaut identiques au plugin.
export const PARAMS = [
  { id: "tune", def: 0 }, { id: "glide", def: 0 }, { id: "modMix", def: 0 }, { id: "oscMod", def: 0 }, { id: "osc3Ctrl", def: 1 },
  { id: "osc1Range", def: 3 }, { id: "osc1Wave", def: 2 }, { id: "osc2Range", def: 3 }, { id: "osc2Freq", def: 0.1 },
  { id: "osc2Wave", def: 2 }, { id: "osc3Range", def: 3 }, { id: "osc3Freq", def: -0.1 }, { id: "osc3Wave", def: 0 },
  { id: "osc1On", def: 1 }, { id: "osc1Level", def: 8 }, { id: "osc2On", def: 1 }, { id: "osc2Level", def: 8 },
  { id: "osc3On", def: 0 }, { id: "osc3Level", def: 5 }, { id: "noiseOn", def: 0 }, { id: "noiseLevel", def: 0 },
  { id: "pinkNoise", def: 0 },
  { id: "cutoff", def: 1200 }, { id: "emphasis", def: 2 }, { id: "contour", def: 5 }, { id: "filterMod", def: 0 },
  { id: "kbd1", def: 0 }, { id: "kbd2", def: 0 }, { id: "bassComp", def: 0 },
  { id: "fAttack", def: 0.005 }, { id: "fDecay", def: 0.4 }, { id: "fSustain", def: 3 },
  { id: "lAttack", def: 0.002 }, { id: "lDecay", def: 0.3 }, { id: "lSustain", def: 8 }, { id: "decay", def: 0 },
  { id: "volume", def: 7 }, { id: "bendRange", def: 2 }, { id: "drift", def: 3 },
  { id: "retrigger", def: 0 }, { id: "poly", def: 0 }, { id: "aftertouch", def: 0 }, { id: "velocity", def: 0 },
  { id: "modAmount", def: 0 }, { id: "modFromLfo", def: 0 },
  { id: "osc1Freq", def: 0 }, { id: "osc2Fine", def: 0 }, { id: "osc3Fine", def: 0 },
  { id: "osc1Rect", def: 0 }, { id: "osc2Rect", def: 0 }, { id: "osc3Rect", def: 0 },
  { id: "osc1Offset", def: 0 }, { id: "osc2Offset", def: 0 }, { id: "osc3Offset", def: 0 }, { id: "oscSync", def: 0 },
  { id: "lfoLevel", def: 0 }, { id: "lfoWave", def: 0 }, { id: "lfoRate", def: 2 }, { id: "lfoSync", def: 0 },
  { id: "lfoDivision", def: 4 }, { id: "lfoDest", def: 0 },
  { id: "vco3Filter", def: 0 }, { id: "adsr", def: 0 }, { id: "fRelease", def: 0.3 }, { id: "lRelease", def: 0.3 },
  { id: "driveOn", def: 0 }, { id: "driveGain", def: 5 }, { id: "driveLowCut", def: 20 }, { id: "driveHighCut", def: 20000 },
  { id: "chorusMode", def: 0 }, { id: "chorusLevel", def: 10 }, { id: "autopan", def: 0 },
  { id: "delayOn", def: 0 }, { id: "delayLevel", def: 3 }, { id: "delayTime", def: 0.35 }, { id: "delaySync", def: 0 },
  { id: "delayDivision", def: 5 }, { id: "delayRight2x", def: 0 }, { id: "delayFeedback", def: 4 },
];

// Durées des valeurs de note, en noires (LFO et delay synchronisés).
const DIVISION_BEATS = [16, 8, 4, 2, 1, 0.5, 0.25, 0.125, 2 / 3, 1 / 3, 1 / 6];
export const DIVISION_NAMES = ["4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T"];

const RANGE_OCTAVES = [-7, -2, -1, 0, 1, 2];
const IDLE = 0, ATTACK = 1, DECAY = 2, RELEASE = 3;

const levelCurve = v => Math.pow (clamp (v, 0, 10) / 10, 1.7);

//------------------------------------------------------------------------------
class Noise {
  constructor (state) { this.state = state >>> 0; this.b = new Float64Array (7); }
  white () {
    let s = this.state;
    s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0;
    this.state = s;
    return s * (2 / 4294967296) - 1;
  }
  // Filtre rose de Paul Kellet.
  pink (w) {
    const b = this.b;
    b[0] = 0.99886 * b[0] + w * 0.0555179;
    b[1] = 0.99332 * b[1] + w * 0.0750759;
    b[2] = 0.969 * b[2] + w * 0.153852;
    b[3] = 0.8665 * b[3] + w * 0.3104856;
    b[4] = 0.55 * b[4] + w * 0.5329522;
    b[5] = -0.7616 * b[5] - w * 0.016898;
    const out = b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362;
    b[6] = w * 0.115926;
    return out * 0.11;
  }
}

const triangle = p => 1 - 4 * Math.abs (p - 0.5);
const saw = (p, dt) => 2 * p - 1 - polyBlep (p, dt);
function pulse (p, dt, width) {
  let v = p < width ? 1 : -1;
  v += polyBlep (p, dt);
  let p2 = p - width;
  if (p2 < 0) p2 += 1;
  v -= polyBlep (p2, dt);
  return v - (2 * width - 1);
}

class Oscillator {
  constructor () { this.phase = 0; this.wrapped = false; }
  process (inc, wave, isOsc3) {
    const p = this.phase;
    let out;
    switch (wave) {
      case 0: out = triangle (p); break;
      case 1: out = isOsc3 ? -saw (p, inc) : 0.5 * (triangle (p) + saw (p, inc)); break;
      case 2: out = saw (p, inc); break;
      case 3: out = pulse (p, inc, 0.5); break;
      case 4: out = pulse (p, inc, 0.35); break;
      default: out = pulse (p, inc, 0.15); break;
    }
    this.phase += inc;
    this.wrapped = this.phase >= 1;
    if (this.wrapped) this.phase -= 1;
    return out;
  }
}

class Drift {
  constructor () { this.value = 0; this.target = 0; this.countdown = 0; }
  process (noise, rate) {
    if (--this.countdown <= 0) {
      this.target = noise.white ();
      this.countdown = Math.trunc (rate * (0.3 + 0.35 * (noise.white () + 1)));
    }
    this.value += (this.target - this.value) * (1 / (rate * 0.8));
    return this.value;
  }
}

class Contour {
  constructor () { this.stage = IDLE; this.level = 0; }
  gateOn () { this.stage = ATTACK; }
  gateOff () { if (this.stage !== IDLE) this.stage = RELEASE; }
  process (c) {
    switch (this.stage) {
      case ATTACK:
        this.level += (1.3 - this.level) * c.attack;
        if (this.level >= 1) { this.level = 1; this.stage = DECAY; }
        break;
      case DECAY: this.level += (c.sustain - this.level) * c.decay; break;
      case RELEASE:
        this.level -= this.level * c.release;
        if (this.level < 1e-5) { this.level = 0; this.stage = IDLE; }
        break;
    }
    return this.level;
  }
}

// Filtre en échelle 24 dB/oct, modèle de Huovilainen (saturation à chaque étage).
class Ladder {
  constructor () {
    this.s = new Float64Array (4); this.y = new Float64Array (4); this.u = 0;
    this.a = new Float64Array (4); this.b = new Float64Array (4);
  }
  process (x, g, k, bassComp) {
    const slope = v => (Math.abs (v) < 1e-4 ? 1 : fastTanh (v) / v);
    const input = x * (bassComp ? 1 + 0.5 * k : 1) * 0.5;
    const s = this.s, y = this.y, a = this.a, b = this.b;
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
}

class Lfo {
  constructor () { this.phase = 0; this.held = 0; this.rng = new Noise (0x2545f491); }
  process (inc, wave) {
    let out;
    switch (wave) {
      case 0: out = triangle (this.phase); break;
      case 1: out = this.phase < 0.5 ? 1 : -1; break;
      case 2: out = 2 * this.phase - 1; break;
      case 3: out = 1 - 2 * this.phase; break;
      default: out = this.held; break;
    }
    this.phase += inc;
    if (this.phase >= 1) { this.phase -= Math.floor (this.phase); this.held = this.rng.white (); }
    return out;
  }
}

const rectified = (x, offset) => 2 * Math.abs (x + offset) - 1;

//------------------------------------------------------------------------------
class Voice {
  constructor (index) {
    this.osc = [new Oscillator (), new Oscillator (), new Oscillator ()];
    this.drift = [new Drift (), new Drift (), new Drift ()];
    // La voix 0 garde la graine d'origine : en mono, le son est celui du plugin.
    this.noise = new Noise ((0x9e3779b9 ^ (index === 0 ? 0 : (index * 0x85ebca6b) >>> 0)) >>> 0);
    this.ladder = new Ladder ();
    this.filterEnv = new Contour (); this.loudness = new Contour ();
    this.currentNote = 60; this.targetNote = 60; this.velocity = 1;
    this.hasPlayed = false; this.gated = false; this.note = -1; this.age = 0;
  }
  gateOn () { this.gated = true; this.filterEnv.gateOn (); this.loudness.gateOn (); }
  gateOff () { this.gated = false; this.filterEnv.gateOff (); this.loudness.gateOff (); }
  silent () { return ! this.gated && this.loudness.stage === IDLE; }

  tick (f, e) {
    const p = e.p, rate = e.osRate;
    this.currentNote += (this.targetNote - this.currentNote) * e.glideCoef;
    const keyNote = this.currentNote + f.bend;

    const drift3 = (this.drift[2].process (this.noise, rate) - 0.04) * e.driftDepth;
    const note3 = (p.osc3Ctrl ? keyNote : 60) + f.tune + RANGE_OCTAVES[p.range[2]] * 12 + f.osc3Freq + drift3
                + f.osc3Fine + f.lfoPitchAll;
    let o3 = this.osc[2].process (e.increment (note3), p.wave[2], true);
    if (p.rectify[2]) o3 = rectified (o3, p.offset[2]);

    const white = this.noise.white ();
    const pink = this.noise.pink (white);
    const noiseOut = p.pinkNoise ? pink : white;

    const source = p.modFromLfo ? f.lfo : o3 * (1 - f.modMix) + noiseOut * f.modMix;
    const mod = source * f.modDepth;
    const pitchMod = p.oscMod ? mod * 12 : 0;
    const drift1 = this.drift[0].process (this.noise, rate) * e.driftDepth;
    const drift2 = (this.drift[1].process (this.noise, rate) + 0.03) * e.driftDepth;

    const note1 = keyNote + f.tune + RANGE_OCTAVES[p.range[0]] * 12 + drift1 + pitchMod + f.osc1Freq + f.lfoPitchAll;
    const note2 = keyNote + f.tune + RANGE_OCTAVES[p.range[1]] * 12 + f.osc2Freq + drift2 + pitchMod
                + f.osc2Fine + f.lfoPitchAll + f.lfoPitch2;
    const inc1 = e.increment (note1), inc2 = e.increment (note2);

    let o1 = this.osc[0].process (inc1, p.wave[0], false);
    if (p.oscSync && this.osc[0].wrapped && inc1 > 0) this.osc[1].phase = Math.min (0.999, this.osc[0].phase * inc2 / inc1);
    let o2 = this.osc[1].process (inc2, p.wave[1], false);
    if (p.rectify[0]) o1 = rectified (o1, p.offset[0]);
    if (p.rectify[1]) o2 = rectified (o2, p.offset[1]);

    const mixed = o1 * f.level[0] + o2 * f.level[1] + o3 * f.level[2] + noiseOut * f.noiseLevel;
    const driven = fastTanh (mixed * 0.7);

    const fEnv = this.filterEnv.process (e.filterCoefs);
    const lEnv = this.loudness.process (e.loudCoefs);
    const contourAmount = p.velocity > 0 ? f.contour * (1 - p.velocity / 10 * (1 - this.velocity)) : f.contour;

    let octaves = f.cutoffOct + e.keyTrack * (this.currentNote - 60) / 12 + contourAmount * fEnv + (p.filterMod ? mod * 3 : 0);
    if (p.vco3Filter > 0) octaves += o3 * f.vco3Filter;
    if (p.lfoLevel > 0) octaves += f.lfoFilter;
    if (p.aftertouch > 0) octaves += f.pressureOct;

    const fc = clamp (Math.pow (2, octaves), 8, 0.45 * rate);
    const g = Math.tan (Math.PI * fc / rate);
    return this.ladder.process (driven + white * 1e-5, g, f.emphasis, p.bassComp) * lEnv;
  }
}

//------------------------------------------------------------------------------
// Effets : overdrive, chorus BBD, autopan, delay stéréo.
class OnePole {
  constructor () { this.coef = 1; this.state = 0; }
  setCutoff (hz, rate) { this.coef = rcCoef (1 / (TWO_PI * hz), rate, 1); }
  lowPass (x) { this.state += (x - this.state) * this.coef; return this.state; }
  highPass (x) { this.state += (x - this.state) * this.coef; return x - this.state; }
}

class TwoPole {
  constructor (hz, rate) { this.a = new OnePole (); this.b = new OnePole (); this.a.setCutoff (hz, rate); this.b.setCutoff (hz, rate); }
  process (x) { return this.b.lowPass (this.a.lowPass (x)); }
}

class Effects {
  constructor (rate) {
    this.rate = rate;
    this.chorusL = new DelayLine (Math.floor (rate * 0.05)); this.chorusR = new DelayLine (Math.floor (rate * 0.05));
    this.delayL = new DelayLine (Math.floor (rate * 4.5)); this.delayR = new DelayLine (Math.floor (rate * 4.5));
    this.lowCut = new OnePole (); this.highCut = new OnePole ();
    this.chorusInput = new TwoPole (9000, rate); this.chorusOutL = new TwoPole (9000, rate); this.chorusOutR = new TwoPole (9000, rate);
    this.feedbackL = 0; this.feedbackR = 0; this.chorusPhase = 0; this.panPhase = 0;
    this.delaySamplesL = -1; this.delaySamplesR = -1;
    this.smoothCoef = rcCoef (0.05, rate, 1);
    this.chorusIncrement = 0; this.chorusCentre = 0.0035; this.chorusDepth = 0; this.chorusStereo = true;
  }

  setParams (e, bpm, panHz) {
    this.e = e;
    this.panIncrement = panHz / this.rate;
    const beatSeconds = 60 / clamp (bpm, 20, 400);
    const seconds = e.delaySync ? DIVISION_BEATS[clamp (e.delayDivision, 0, 10)] * beatSeconds : e.delayTime;
    this.targetL = clamp (seconds * this.rate, 1, this.rate * 4);
    this.targetR = e.delayRightDouble ? this.targetL * 0.5 : this.targetL;
    if (this.delaySamplesL < 0) { this.delaySamplesL = this.targetL; this.delaySamplesR = this.targetR; }

    const set = (hz, lo, hi, stereo) => {
      this.chorusIncrement = hz / this.rate; this.chorusCentre = 0.5 * (lo + hi);
      this.chorusDepth = 0.5 * (hi - lo); this.chorusStereo = stereo;
    };
    if (e.chorusMode === 1) set (0.513, 0.00166, 0.00535, true);
    else if (e.chorusMode === 2) set (0.863, 0.00166, 0.00535, true);
    else if (e.chorusMode === 3) set (9.75, 0.0033, 0.0037, false);

    this.lowCut.setCutoff (e.driveLowCut, this.rate);
    this.highCut.setCutoff (e.driveHighCut, this.rate);
    this.driveAmount = Math.pow (10, e.driveGain / 10 * 1.5);
    this.driveNorm = 0.5 / Math.tanh (this.driveAmount * 0.5);
  }

  anyOn () { const e = this.e; return e.driveOn || e.chorusMode > 0 || e.autopan || e.delayOn; }

  process (left, right, n) {
    const e = this.e, rate = this.rate;
    const chorusMix = e.chorusLevel / 10, delayWet = e.delayLevel / 10, feedback = e.delayFeedback / 10 * 0.9;
    for (let i = 0; i < n; ++i) {
      let x = left[i];
      if (e.driveOn) {
        x = this.lowCut.highPass (x);
        x = Math.tanh (x * this.driveAmount) * this.driveNorm;
        x = this.highCut.lowPass (x);
      }
      let l = x, r = x;
      if (e.chorusMode > 0) {
        const tri = triangle (this.chorusPhase);
        this.chorusPhase += this.chorusIncrement;
        if (this.chorusPhase >= 1) this.chorusPhase -= Math.floor (this.chorusPhase);
        const input = this.chorusInput.process (x);
        this.chorusL.push (input); this.chorusR.push (input);
        const spread = this.chorusStereo ? -1 : 1;
        const wetL = this.chorusOutL.process (fastTanh (this.chorusL.read ((this.chorusCentre + this.chorusDepth * tri) * rate)));
        const wetR = this.chorusOutR.process (fastTanh (this.chorusR.read ((this.chorusCentre + spread * this.chorusDepth * tri) * rate)));
        const norm = 1 / (1 + 0.5 * chorusMix);
        l = (x + chorusMix * wetL) * norm;
        r = (x + chorusMix * wetR) * norm;
      }
      if (e.autopan) {
        const angle = Math.PI * 0.25 * (1 + Math.sin (TWO_PI * this.panPhase));
        this.panPhase += this.panIncrement;
        if (this.panPhase >= 1) this.panPhase -= Math.floor (this.panPhase);
        l *= Math.SQRT2 * Math.cos (angle);
        r *= Math.SQRT2 * Math.sin (angle);
      }
      if (e.delayOn) {
        this.delaySamplesL += (this.targetL - this.delaySamplesL) * this.smoothCoef;
        this.delaySamplesR += (this.targetR - this.delaySamplesR) * this.smoothCoef;
        const echoL = this.delayL.read (this.delaySamplesL), echoR = this.delayR.read (this.delaySamplesR);
        this.feedbackL += (echoL - this.feedbackL) * 0.35;
        this.feedbackR += (echoR - this.feedbackR) * 0.35;
        this.delayL.push (l + this.feedbackL * feedback);
        this.delayR.push (r + this.feedbackR * feedback);
        l += echoL * delayWet;
        r += echoR * delayWet;
      }
      left[i] = l;
      right[i] = r;
    }
  }
}

//------------------------------------------------------------------------------
const SMOOTHED = ["tune", "osc1Freq", "osc2Freq", "osc3Freq", "osc2Fine", "osc3Fine", "modMix", "modAmount",
                  "level0", "level1", "level2", "noiseLevel", "cutoffOct", "emphasis", "contour", "volume",
                  "lfoLevel", "vco3Filter", "modWheel", "pressure", "tremolo"];

export class ModelPlay {
  constructor (sampleRate) {
    this.baseRate = sampleRate;
    this.osRate = sampleRate * 2;
    this.values = Object.fromEntries (PARAMS.map (p => [p.id, p.def]));
    this.voices = Array.from ({ length: 8 }, (_, i) => new Voice (i));
    this.lfo = new Lfo ();
    this.effects = new Effects (sampleRate);
    this.decimator = new HalfbandDecimator ();
    this.osBuffer = new Float32Array (4096);
    for (const name of SMOOTHED) this[name] = new Smoothed ();
    this.smoothCoef = rcCoef (0.005, this.osRate, 1);
    this.dcCoef = 1 - TWO_PI * 10 / sampleRate;
    this.dcX = 0; this.dcY = 0;
    this.keys = new Uint8Array (128); this.numKeys = 0; this.voiceAge = 0;
    this.pitchBend = 0; this.tempo = 120;
    this.update ();
    for (const name of SMOOTHED) this[name].snap ();
  }

  setParam (id, value) {
    if (! (id in this.values)) return;
    this.values[id] = value;
    this.update ();
  }

  loadPreset () {
    for (const p of PARAMS) this.values[p.id] = p.def;
    this.update ();
    return { ...this.values };
  }

  update () {
    const v = this.values, rate = this.osRate;
    const flag = id => v[id] > 0.5, choice = id => Math.round (v[id]);
    const wasPoly = this.p && this.p.poly;
    const p = this.p = {
      osc3Ctrl: flag ("osc3Ctrl"), oscMod: flag ("oscMod"),
      range: [choice ("osc1Range"), choice ("osc2Range"), choice ("osc3Range")].map (r => clamp (r, 0, 5)),
      wave: [choice ("osc1Wave"), choice ("osc2Wave"), choice ("osc3Wave")],
      pinkNoise: flag ("pinkNoise"), filterMod: flag ("filterMod"), bassComp: flag ("bassComp"),
      bendRange: v.bendRange, poly: flag ("poly"), retrigger: flag ("retrigger"),
      aftertouch: v.aftertouch, velocity: v.velocity, modFromLfo: flag ("modFromLfo"),
      rectify: [flag ("osc1Rect"), flag ("osc2Rect"), flag ("osc3Rect")],
      offset: [v.osc1Offset, v.osc2Offset, v.osc3Offset], oscSync: flag ("oscSync"),
      lfoLevel: v.lfoLevel, lfoWave: choice ("lfoWave"), lfoDest: choice ("lfoDest"),
      vco3Filter: v.vco3Filter,
    };
    if (wasPoly !== undefined && wasPoly !== p.poly) this.allNotesOff ();

    this.tune.target = v.tune;
    this.osc1Freq.target = v.osc1Freq; this.osc2Freq.target = v.osc2Freq; this.osc3Freq.target = v.osc3Freq;
    this.osc2Fine.target = v.osc2Fine; this.osc3Fine.target = v.osc3Fine;
    this.modMix.target = v.modMix / 10;
    this.modAmount.target = v.modAmount / 10;
    this.level0.target = flag ("osc1On") ? levelCurve (v.osc1Level) : 0;
    this.level1.target = flag ("osc2On") ? levelCurve (v.osc2Level) : 0;
    this.level2.target = flag ("osc3On") ? levelCurve (v.osc3Level) : 0;
    this.noiseLevel.target = flag ("noiseOn") ? levelCurve (v.noiseLevel) : 0;
    this.cutoffOct.target = Math.log2 (clamp (v.cutoff, 10, 22000));
    this.emphasis.target = clamp (v.emphasis, 0, 10) * 0.44;
    this.contour.target = v.contour / 10 * 4;
    this.volume.target = 1.5 * (v.volume / 10) ** 2;
    this.lfoLevel.target = v.lfoLevel / 10;
    this.vco3Filter.target = v.vco3Filter / 10 * 3;

    this.keyTrack = (flag ("kbd1") ? 1 / 3 : 0) + (flag ("kbd2") ? 2 / 3 : 0);
    this.driftDepth = v.drift / 10 * 0.15;
    this.glideCoef = v.glide < 0.001 ? 1 : rcCoef (v.glide, rate, 3);

    const coefs = (attack, decay, sustain, release) => {
      const releaseTime = flag ("adsr") ? release : (flag ("decay") ? decay : 0.008);
      return { attack: rcCoef (attack, rate, 1.466), decay: rcCoef (decay, rate, 3),
               sustain: clamp (sustain, 0, 10) / 10, release: rcCoef (releaseTime, rate, 3) };
    };
    this.filterCoefs = coefs (v.fAttack, v.fDecay, v.fSustain, v.fRelease);
    this.loudCoefs = coefs (v.lAttack, v.lDecay, v.lSustain, v.lRelease);

    const lfoHz = flag ("lfoSync") ? this.tempo / 60 / DIVISION_BEATS[clamp (choice ("lfoDivision"), 0, 10)] : v.lfoRate;
    this.lfoIncrement = lfoHz / rate;

    this.effects.setParams ({
      driveOn: flag ("driveOn"), driveGain: v.driveGain, driveLowCut: v.driveLowCut, driveHighCut: v.driveHighCut,
      chorusMode: choice ("chorusMode"), chorusLevel: v.chorusLevel, autopan: flag ("autopan"),
      delayOn: flag ("delayOn"), delayLevel: v.delayLevel, delayTime: v.delayTime, delaySync: flag ("delaySync"),
      delayDivision: choice ("delayDivision"), delayRightDouble: flag ("delayRight2x"), delayFeedback: v.delayFeedback,
    }, this.tempo, lfoHz);
  }

  setTempo (bpm) { this.tempo = clamp (bpm, 20, 400); this.update (); }
  setPitchBend (x) { this.pitchBend = clamp (x, -1, 1); }
  setModWheel (x) { this.modWheel.target = x * x; }
  increment (note) { return clamp (440 * Math.pow (2, (note - 69) / 12) / this.osRate, 0, 0.45); }

  lowestKey () {
    for (let n = 0; n < 128; ++n) if (this.keys[n]) return n;
    return 60;
  }

  // Mono : note la plus grave prioritaire, legato sans redéclenchement (sauf RETRIG).
  noteOn (note, velocity = 1) {
    if (note < 0 || note > 127) return;
    if (this.p.poly) return this.polyNoteOn (note, velocity);
    if (this.keys[note]) return;
    const firstKey = this.numKeys === 0;
    this.keys[note] = 1; ++this.numKeys;
    const v = this.voices[0];
    v.targetNote = this.lowestKey ();
    v.velocity = velocity;
    if (! v.hasPlayed) { v.currentNote = v.targetNote; v.hasPlayed = true; }
    if (firstKey || this.p.retrigger) v.gateOn ();
  }

  noteOff (note) {
    if (note < 0 || note > 127) return;
    if (this.p.poly) {
      for (const v of this.voices) if (v.gated && v.note === note) v.gateOff ();
      return;
    }
    if (! this.keys[note]) return;
    this.keys[note] = 0; --this.numKeys;
    if (this.numKeys === 0) this.voices[0].gateOff ();
    else this.voices[0].targetNote = this.lowestKey ();
  }

  allNotesOff () {
    this.keys.fill (0); this.numKeys = 0;
    for (const v of this.voices) v.gateOff ();
  }

  polyNoteOn (note, velocity) {
    const voices = this.voices;
    let chosen = null;
    for (const v of voices) if (v.note === note && ! v.silent ()) chosen = v;
    if (! chosen) for (const v of voices) if (v.silent () && (! chosen || v.age < chosen.age)) chosen = v;
    if (! chosen) for (const v of voices) if (! v.gated && (! chosen || v.age < chosen.age)) chosen = v;
    if (! chosen) for (const v of voices) if (! chosen || v.age < chosen.age) chosen = v;
    chosen.note = note; chosen.age = ++this.voiceAge; chosen.velocity = velocity; chosen.targetNote = note;
    if (! chosen.hasPlayed) { chosen.currentNote = note; chosen.hasPlayed = true; }
    chosen.gateOn ();
  }

  tick () {
    const a = this.smoothCoef, p = this.p;
    const f = this.frame || (this.frame = { level: [0, 0, 0] });
    f.bend = this.pitchBend * p.bendRange;
    f.tune = this.tune.next (a);
    f.osc1Freq = this.osc1Freq.next (a); f.osc2Freq = this.osc2Freq.next (a); f.osc3Freq = this.osc3Freq.next (a);
    f.osc2Fine = this.osc2Fine.next (a); f.osc3Fine = this.osc3Fine.next (a);
    f.modMix = this.modMix.next (a);
    f.modDepth = this.modWheel.next (a) + this.modAmount.next (a);
    f.level[0] = this.level0.next (a); f.level[1] = this.level1.next (a); f.level[2] = this.level2.next (a);
    f.noiseLevel = this.noiseLevel.next (a);
    f.cutoffOct = this.cutoffOct.next (a);
    f.emphasis = this.emphasis.next (a);
    f.contour = this.contour.next (a);
    f.vco3Filter = this.vco3Filter.next (a);
    f.pressureOct = this.pressure.next (a) * p.aftertouch / 10 * 3;

    f.lfo = this.lfo.process (this.lfoIncrement, p.lfoWave);
    const depth = this.lfoLevel.next (a);
    f.lfoPitchAll = f.lfoPitch2 = f.lfoFilter = 0;
    this.tremolo.target = 1;
    if (p.lfoLevel > 0) {
      switch (p.lfoDest) {
        case 0: f.lfoPitchAll = f.lfo * depth * depth * 12; break;
        case 1: f.lfoPitch2 = f.lfo * depth * depth * 12; break;
        case 2: f.lfoFilter = f.lfo * depth * 3; break;
        default: this.tremolo.target = 1 - depth * (0.5 - 0.5 * f.lfo); break;
      }
    }
    const amp = this.tremolo.next (a);

    let sum;
    if (p.poly) {
      sum = 0;
      for (const v of this.voices) if (! v.silent ()) sum += v.tick (f, this);
      sum *= 0.5;
    } else {
      sum = this.voices[0].tick (f, this);
    }
    const out = sum * this.volume.next (a);
    return p.lfoLevel > 0 && p.lfoDest === 3 ? out * amp : out;
  }

  render (left, right, n) {
    if (this.osBuffer.length < 2 * n) this.osBuffer = new Float32Array (4 * n);
    const os = this.osBuffer;
    for (let i = 0; i < 2 * n; ++i) os[i] = this.tick ();
    this.decimator.process (os, left, n);
    for (let i = 0; i < n; ++i) {
      const x = left[i];
      this.dcY = x - this.dcX + this.dcCoef * this.dcY;
      this.dcX = x;
      left[i] = this.dcY;
    }
    if (this.effects.anyOn ()) this.effects.process (left, right, n);
    else right.set (left.subarray (0, n));
  }
}
