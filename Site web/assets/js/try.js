// « Click to try » : l'instrument jouable dans la page (moteur web identique
// au plugin), son clavier et un petit séquenceur MIDI.

import { PARAMS as LP60_PARAMS, PRESETS as LP60_PRESETS, cutoffHz, attackSeconds, envSeconds, lfoHz } from "./dsp/lp60.js";
import { PARAMS as MODELPLAY_PARAMS } from "./dsp/modelplay.js";
import { PARAMS as LP303_PARAMS, PRESETS as LP303_PRESETS, PATTERNS as LP303_PATTERNS, BASE_NOTE as LP303_BASE,
         factoryPattern, randomPattern, cutoffHz as acidCutoffHz, decaySeconds } from "./dsp/lp303.js";

const DEFAULTS = {
  lp60: Object.fromEntries (LP60_PARAMS.map (p => [p.id, p.def])),
  modelplay: Object.fromEntries (MODELPLAY_PARAMS.map (p => [p.id, p.def])),
  lp303: Object.fromEntries (LP303_PARAMS.map (p => [p.id, p.def])),
};

const en = () => document.documentElement.dataset.uiLang === "en";
const t = (fr, english) => (en () ? english : fr);

//------------------------------------------------------------------------------
// Formats d'affichage
const fmtTime = s => (s < 1 ? `${Math.round (s * 1000)} ms` : `${s.toFixed (2)} s`);
const fmtHz = f => (f < 1000 ? `${Math.round (f)} Hz` : `${(f / 1000).toFixed (2)} kHz`);
const fmt1 = v => v.toFixed (1);

//------------------------------------------------------------------------------
// Les réglages proposés en ligne, par section (un sous-ensemble des plugins).
// type : slider (curseur), knob (bouton, éventuellement à crans avec choices),
// toggle, choice. Dans une section, chaque ligne de « rows » est une rangée de
// réglages ; un tableau dans une rangée empile ses réglages en colonne, null
// laisse une place vide.
const RANGES = ["LO", "32'", "16'", "8'", "4'", "2'"];
const WAVES = ["Tri", "Tri-Saw", "Saw", "Square", "Wide", "Narrow"];
const semitones = v => `${v.toFixed (2)} st`;

const SPECS = {
  lp60: {
    style: "juno",
    sections: [
      { title: "ARPEGGIO", colour: "white", rows: [[
        { id: "arpOn", label: "ON", type: "toggle" },
        { id: "arpMode", label: "MODE", type: "choice", choices: ["UP", "U&D", "DOWN"] },
        { id: "arpRange", label: "RANGE", type: "choice", choices: ["1", "2", "3"] },
        { id: "arpRate", label: "RATE", type: "slider", min: 0, max: 10 } ]] },
      { title: "LFO", colour: "white", rows: [[
        { id: "lfoRate", label: "RATE", type: "slider", min: 0, max: 10, show: v => `${lfoHz (v).toFixed (2)} Hz` },
        { id: "lfoDelay", label: "DELAY", type: "slider", min: 0, max: 10 },
        { id: "lfoMode", label: "TRIG", type: "choice", choices: ["AUTO", "MAN"] } ]] },
      { title: "DCO", colour: "blue", rows: [[
        { id: "range", label: "RANGE", type: "choice", choices: ["16'", "8'", "4'"] },
        { id: "dcoLfo", label: "LFO", type: "slider", min: 0, max: 10 },
        { id: "pwm", label: "PWM", type: "slider", min: 0, max: 10 },
        { id: "pwmMode", label: "PWM SRC", type: "choice", choices: ["LFO", "MAN", "ENV"] },
        [ { id: "pulse", label: "PULSE", type: "toggle" },
          { id: "saw", label: "SAW", type: "toggle" },
          { id: "subOn", label: "SUB", type: "toggle" } ],
        { id: "sub", label: "SUB", type: "slider", min: 0, max: 10 },
        { id: "noise", label: "NOISE", type: "slider", min: 0, max: 10 } ]] },
      { title: "HPF", colour: "red", rows: [[
        { id: "hpf", label: "FREQ", type: "slider", min: 0, max: 3, step: 1, show: v => String (v) } ]] },
      { title: "VCF", colour: "red", rows: [[
        { id: "cutoff", label: "FREQ", type: "slider", min: 0, max: 10, show: v => fmtHz (Math.min (20000, cutoffHz (v))) },
        { id: "resonance", label: "RES", type: "slider", min: 0, max: 10 },
        { id: "envPolarity", label: "ENV", type: "choice", choices: ["+", "−"] },
        { id: "vcfEnv", label: "ENV", type: "slider", min: 0, max: 10 },
        { id: "vcfLfo", label: "LFO", type: "slider", min: 0, max: 10 },
        { id: "vcfKybd", label: "KYBD", type: "slider", min: 0, max: 10 } ]] },
      { title: "VCA", colour: "red", rows: [[
        { id: "vcaMode", label: "MODE", type: "choice", choices: ["ENV", "GATE"] },
        { id: "level", label: "LEVEL", type: "slider", min: 0, max: 10 } ]] },
      { title: "ENV", colour: "red", rows: [[
        { id: "attack", label: "A", type: "slider", min: 0, max: 10, show: v => fmtTime (attackSeconds (v)) },
        { id: "decay", label: "D", type: "slider", min: 0, max: 10, show: v => fmtTime (envSeconds (v)) },
        { id: "sustain", label: "S", type: "slider", min: 0, max: 10 },
        { id: "release", label: "R", type: "slider", min: 0, max: 10, show: v => fmtTime (envSeconds (v)) } ]] },
      { title: "CHORUS", colour: "blue", rows: [[
        { id: "chorus1", label: "I", type: "toggle" },
        { id: "chorus2", label: "II", type: "toggle" },
        { id: "chorusHiss", label: "NOISE", type: "slider", min: 0, max: 10 } ]] },
    ],
    // Boutons de jeu, à gauche du clavier comme sur le plugin.
    performance: [
      { id: "tune", label: "TUNE", type: "knob", min: -50, max: 50, show: v => `${Math.round (v)} ct` },
      { id: "bendDco", label: "BEND DCO", type: "knob", min: 0, max: 12, step: 1, show: v => `${v} st` },
      { id: "bendVcf", label: "BEND VCF", type: "knob", min: 0, max: 10 },
      { id: "lfoMod", label: "MOD LFO", type: "knob", min: 0, max: 10 } ],
    presets: LP60_PRESETS.map (p => p[0]),
  },

  modelplay: {
    style: "moog",
    sections: [
      { title: "CONTROLLERS", rows: [
        [ { id: "tune", label: "TUNE", type: "knob", min: -1, max: 1, show: semitones },
          { id: "modAmount", label: "MOD AMOUNT", type: "knob", min: 0, max: 10 },
          { id: "oscMod", label: "OSC MOD", type: "toggle" } ],
        [ { id: "glide", label: "GLIDE", type: "knob", min: 0, max: 5, curve: "pow", show: fmtTime },
          { id: "modMix", label: "MOD MIX", type: "knob", min: 0, max: 10 },
          { id: "osc3Ctrl", label: "OSC 3 CTRL", type: "toggle" } ],
        [ { id: "drift", label: "DRIFT", type: "knob", min: 0, max: 10 }, null,
          { id: "poly", label: "POLY", type: "toggle" } ] ] },
      { title: "OSCILLATOR BANK", rows: [
        [ { id: "osc1Range", label: "RANGE", type: "knob", choices: RANGES },
          { id: "osc1Freq", label: "FREQUENCY", type: "knob", min: -7, max: 7, show: semitones },
          { id: "osc1Wave", label: "WAVEFORM", type: "knob", choices: WAVES } ],
        [ { id: "osc2Range", label: "RANGE", type: "knob", choices: RANGES },
          { id: "osc2Freq", label: "FREQUENCY", type: "knob", min: -7, max: 7, show: semitones },
          { id: "osc2Wave", label: "WAVEFORM", type: "knob", choices: WAVES } ],
        [ { id: "osc3Range", label: "RANGE", type: "knob", choices: RANGES },
          { id: "osc3Freq", label: "FREQUENCY", type: "knob", min: -7, max: 7, show: semitones },
          { id: "osc3Wave", label: "WAVEFORM", type: "knob", choices: ["Tri", "Rev Saw", "Saw", "Square", "Wide", "Narrow"] } ] ] },
      { title: "MIXER", rows: [
        [ { id: "osc1Level", label: "OSC 1", type: "knob", min: 0, max: 10 },
          { id: "osc1On", label: "ON", type: "toggle" },
          { id: "noiseLevel", label: "NOISE", type: "knob", min: 0, max: 10 } ],
        [ { id: "osc2Level", label: "OSC 2", type: "knob", min: 0, max: 10 },
          { id: "osc2On", label: "ON", type: "toggle" },
          { id: "noiseOn", label: "ON", type: "toggle" } ],
        [ { id: "osc3Level", label: "OSC 3", type: "knob", min: 0, max: 10 },
          { id: "osc3On", label: "ON", type: "toggle" },
          { id: "pinkNoise", label: "PINK", type: "toggle" } ] ] },
      { title: "MODIFIERS", rows: [
        [ { id: "cutoff", label: "CUTOFF", type: "knob", min: 20, max: 20000, curve: "log", show: fmtHz },
          { id: "emphasis", label: "EMPHASIS", type: "knob", min: 0, max: 10 },
          { id: "contour", label: "CONTOUR AMT", type: "knob", min: 0, max: 10 },
          { id: "kbd1", label: "KBD 1", type: "toggle" } ],
        [ { id: "fAttack", label: "F. ATTACK", type: "knob", min: 0.001, max: 10, curve: "log", show: fmtTime },
          { id: "fDecay", label: "F. DECAY", type: "knob", min: 0.004, max: 35, curve: "log", show: fmtTime },
          { id: "fSustain", label: "F. SUSTAIN", type: "knob", min: 0, max: 10 },
          { id: "kbd2", label: "KBD 2", type: "toggle" } ],
        [ { id: "lAttack", label: "L. ATTACK", type: "knob", min: 0.001, max: 10, curve: "log", show: fmtTime },
          { id: "lDecay", label: "L. DECAY", type: "knob", min: 0.004, max: 35, curve: "log", show: fmtTime },
          { id: "lSustain", label: "L. SUSTAIN", type: "knob", min: 0, max: 10 },
          { id: "decay", label: "DECAY", type: "toggle" } ] ] },
      { title: "OUTPUT", rows: [
        [ { id: "volume", label: "VOLUME", type: "knob", min: 0, max: 10 } ] ] },
      { title: "LFO", newRow: true, rows: [[
        { id: "lfoLevel", label: "LEVEL", type: "knob", min: 0, max: 10 },
        { id: "lfoWave", label: "WAVE", type: "knob", choices: ["Triangle", "Square", "Saw Up", "Saw Down", "S&H"] },
        { id: "lfoRate", label: "RATE", type: "knob", min: 0.05, max: 30, curve: "log", show: v => `${v.toFixed (2)} Hz` },
        { id: "lfoDest", label: "DESTINATION", type: "knob", choices: ["All Osc", "Osc 2", "Filter", "Volume"] } ]] },
      { title: "OVERDRIVE", rows: [[
        { id: "driveOn", label: "ON", type: "toggle" },
        { id: "driveGain", label: "GAIN", type: "knob", min: 0, max: 10 } ]] },
      { title: "CHORUS", rows: [[
        { id: "chorusMode", label: "MODE", type: "knob", choices: ["Off", "I", "II", "I+II"] } ]] },
      { title: "DELAY", rows: [[
        { id: "delayOn", label: "ON", type: "toggle" },
        { id: "delayLevel", label: "LEVEL", type: "knob", min: 0, max: 10 },
        { id: "delayTime", label: "TIME", type: "knob", min: 0.01, max: 2, curve: "log", show: fmtTime },
        { id: "delayFeedback", label: "FEEDBACK", type: "knob", min: 0, max: 10 } ]] },
    ],
    performance: [],
    presets: null,
  },

  lp303: {
    style: "acid",
    sections: [
      { title: "VCO", rows: [[
        { id: "tune", label: "TUNE", type: "knob", min: -12, max: 12, show: semitones },
        { id: "wave", label: "WAVEFORM", type: "choice", choices: ["SAW", "SQUARE"] } ]] },
      { title: "FILTER", rows: [[
        { id: "cutoff", label: "CUTOFF", type: "knob", min: 0, max: 10, show: v => fmtHz (acidCutoffHz (v)) },
        { id: "resonance", label: "RESONANCE", type: "knob", min: 0, max: 10 },
        { id: "envMod", label: "ENV MOD", type: "knob", min: 0, max: 10 },
        { id: "decay", label: "DECAY", type: "knob", min: 0, max: 10, show: v => fmtTime (decaySeconds (v)) } ]] },
      { title: "ACCENT", rows: [[ { id: "accent", label: "AMOUNT", type: "knob", min: 0, max: 10 } ]] },
      { title: "SLIDE", rows: [[ { id: "slide", label: "TIME", type: "knob", min: 0.02, max: 0.5, curve: "log", show: fmtTime } ]] },
      { title: "DRIVE", rows: [[ { id: "drive", label: "AMOUNT", type: "knob", min: 0, max: 10 } ]] },
      { title: "OUTPUT", rows: [[ { id: "volume", label: "VOLUME", type: "knob", min: 0, max: 10 } ]] },
    ],
    performance: [],
    presets: LP303_PRESETS.map (p => p[0]),
  },
};

// Un bouton à crans parcourt ses choix comme un sélecteur rotatif.
for (const spec of Object.values (SPECS))
  for (const section of spec.sections)
    for (const c of section.rows.flat (2))
      if (c && c.type === "knob" && c.choices) {
        Object.assign (c, { min: 0, max: c.choices.length - 1, step: 1 });
        c.show = v => c.choices[Math.round (v)];
      }

// Motifs de démonstration pour le séquenceur : [note, pas de début, longueur en pas].
const DEMOS = {
  lp60: [[48, 0, 8], [52, 0, 8], [55, 0, 8], [59, 0, 8], [52, 8, 8], [55, 8, 8], [57, 8, 8], [60, 8, 8],
         [53, 16, 8], [57, 16, 8], [60, 16, 8], [64, 16, 8], [55, 24, 8], [59, 24, 8], [62, 24, 8], [67, 24, 8]],
  modelplay: [[36, 0, 2], [36, 3, 1], [48, 4, 1], [36, 6, 2], [43, 8, 2], [46, 11, 1], [48, 12, 2], [36, 14, 2],
              [36, 16, 2], [36, 19, 1], [48, 20, 1], [36, 22, 2], [41, 24, 2], [43, 27, 1], [46, 28, 2], [48, 30, 2]],
};

//------------------------------------------------------------------------------
// Audio : un contexte partagé, démarré au premier geste de l'utilisateur.
let contextPromise = null;
function audioContext () {
  if (! contextPromise) {
    contextPromise = (async () => {
      const ctx = new AudioContext ({ latencyHint: "interactive" });
      await ctx.audioWorklet.addModule (new URL ("./dsp/worklet.js", import.meta.url));
      return ctx;
    }) ();
  }
  return contextPromise;
}

//------------------------------------------------------------------------------
// Conversions position (0..1) <-> valeur, pour les boutons et curseurs.
function toValue (c, x) {
  if (c.curve === "log") return c.min * Math.pow (c.max / c.min, x);
  if (c.curve === "pow") return c.min + (c.max - c.min) * x * x * x;
  const v = c.min + (c.max - c.min) * x;
  return c.step ? Math.round (v / c.step) * c.step : v;
}

function toPosition (c, v) {
  if (c.curve === "log") return Math.log (v / c.min) / Math.log (c.max / c.min);
  if (c.curve === "pow") return Math.cbrt ((v - c.min) / (c.max - c.min));
  return (v - c.min) / (c.max - c.min);
}

const el = (tag, cls, text) => {
  const e = document.createElement (tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
};

//------------------------------------------------------------------------------
class TryPanel {
  constructor (panel) {
    this.panel = panel;
    this.synth = panel.dataset.instrument;
    this.spec = SPECS[this.synth];
    this.values = {};
    this.widgets = new Map ();
    this.node = null;
    this.build ();
  }

  //----------------------------------------------------------------------------
  build () {
    const p = this.panel;
    p.innerHTML = "";
    p.classList.add ("synth-try", `style-${this.spec.style}`);

    // Façade : bandeau du haut, sections, puis boutons de jeu et clavier.
    const face = el ("div", "face");
    const body = el ("div", "face-body");
    face.append (body);
    const top = el ("div", "face-top");
    const power = el ("button", "power");
    power.type = "button";
    power.addEventListener ("click", () => this.start ());
    this.power = power;

    if (this.synth === "lp60" || this.synth === "lp303") {
      if (this.synth === "lp60") body.append (el ("div", "face-grille"));
      const presets = el ("div", "preset-box");
      const select = el ("select", "preset");
      select.setAttribute ("aria-label", "Presets");
      this.spec.presets.forEach ((name, i) => select.append (new Option (`${String (i + 1).padStart (2, "0")}  ${name}`, i)));
      const load = async index => {
        select.value = index;
        await this.start ();
        this.send ({ type: "preset", index });
      };
      select.addEventListener ("change", () => load (Number (select.value)));
      const count = this.spec.presets.length;
      const step = (label, delta) => {
        const b = el ("button", "preset-step", label);
        b.type = "button";
        b.addEventListener ("click", () => load ((Number (select.value) + delta + count) % count));
        return b;
      };
      presets.append (step ("‹", -1), select, step ("›", 1));
      top.append (el ("span", "brand-box", "LOAD & PLAY"), presets, power,
                  el ("span", "face-logo", this.synth === "lp60" ? "LP-60" : "LP-303"));
    } else {
      top.append (el ("span", "face-name", "Model Play"), el ("span", "face-sub", "ANALOG-STYLE SYNTHESIZER"),
                  power, el ("span", "brand", "LOAD & PLAY"));
    }
    body.append (top);

    const sections = el ("div", "sections");
    for (const section of this.spec.sections) {
      if (section.newRow) sections.append (el ("div", "row-break"));
      const s = el ("div", `section ${section.colour || ""}`);
      s.append (el ("div", "section-title", section.title));
      const rows = el ("div", "rows");
      for (const line of section.rows) {
        const row = el ("div", "controls");
        for (const item of line) {
          if (item === null) row.append (el ("div", "ctl spacer"));
          else if (Array.isArray (item)) {
            const stack = el ("div", "ctl-stack");
            for (const c of item) stack.append (this.widget (c));
            row.append (stack);
          } else row.append (this.widget (item));
        }
        rows.append (row);
      }
      s.append (rows);
      sections.append (s);
    }
    if (this.synth === "lp303") body.append (sections, this.acidSequencer (), this.keyboard ());
    else body.append (sections, this.keyboard ());
    p.append (face);

    // Si la façade dépasse un peu, on la réduit pour qu'elle tienne ; trop
    // étroit (téléphone), elle garde sa taille et défile de côté.
    let lastWidth = 0;
    new ResizeObserver (() => {
      if (face.clientWidth === lastWidth) return;
      lastWidth = face.clientWidth;
      body.style.zoom = "";
      const cs = getComputedStyle (face);
      const room = face.clientWidth - parseFloat (cs.paddingLeft) - parseFloat (cs.paddingRight);
      const scale = room / body.scrollWidth;
      if (scale < 1 && scale > 0.75) body.style.zoom = scale;
    }).observe (face);
    p.append (this.keyHint);

    // Séquenceur (celui de LP-303 est dans la façade, comme sur le plugin)
    if (this.synth !== "lp303") p.append (this.sequencer ());

    this.applyValues (DEFAULTS[this.synth]);
    this.refreshTexts ();
    document.addEventListener ("languagechange", () => this.refreshTexts ());
  }

  refreshTexts () {
    this.power.textContent = this.node ? t ("● son actif", "● sound on") : t ("▶ Activer le son", "▶ Turn sound on");
    this.power.classList.toggle ("on", !! this.node);
    if (this.seq) {
      this.seq.play.textContent = this.seq.playing ? "■ Stop" : "▶ Play";
      this.seq.clear.textContent = t ("Effacer", "Clear");
      this.seq.demo.textContent = t ("Démo", "Demo");
      this.seq.tempoLabel.textContent = "Tempo";
      this.seq.hint.textContent = t ("Cliquez dans la grille pour poser une note, glissez pour l'allonger, recliquez pour l'effacer.",
                                     "Click the grid to place a note, drag to lengthen it, click again to delete it.");
    }
    if (this.keyHint)
      this.keyHint.textContent = t ("Clavier de l'ordinateur : rangée Q/A S D F… (blanches) et Z/W E T Y U (noires) · ← → octave",
                                    "Computer keyboard: A S D F… row (white) and W E T Y U (black) · ← → octave")
        + (this.synth === "lp303"
           ? t (" · Maj = accent, notes liées = slide · pendant la lecture, une touche transpose le motif (do 4 = sans transposition)",
                " · Shift = accent, tied notes = slide · while playing, a key transposes the pattern (C4 = no transposition)")
           : "");
    if (this.acid) {
      const a = this.acid;
      a.play.textContent = a.playing ? "■ STOP" : "▶ RUN";
      a.random.textContent = "RANDOM";
      a.clear.textContent = "CLEAR";
      a.hint.textContent = t ("Clic : note (re-clic : silence) · glisser pour peindre · clic sur un numéro : longueur du motif",
                              "Click: note (click again: rest) · drag to paint · click a step number: pattern length");
    }
  }

  //----------------------------------------------------------------------------
  widget (c) {
    const w = el ("div", `ctl ctl-${c.type}`);
    const label = el ("div", "ctl-label", c.label);
    const value = el ("div", "ctl-value");
    let set;

    if (c.type === "toggle") {
      const b = el ("button", "toggle");
      b.type = "button";
      b.append (el ("span", "led"));
      b.addEventListener ("click", () => this.change (c, this.values[c.id] > 0.5 ? 0 : 1));
      w.append (b, label);
      set = v => b.classList.toggle ("on", v > 0.5);
    } else if (c.type === "choice") {
      // Rangée de petits boutons avec leur voyant, comme sur le Juno.
      const group = el ("div", "choice");
      const buttons = c.choices.map ((name, i) => {
        const b = el ("button");
        b.append (el ("span", "led"), el ("span", "sq"), el ("span", "nm", name));
        b.type = "button";
        b.addEventListener ("click", () => this.change (c, i));
        group.append (b);
        return b;
      });
      w.append (label, group);
      set = v => buttons.forEach ((b, i) => b.classList.toggle ("on", i === Math.round (v)));
    } else {
      // Curseur vertical (LP-60) ou bouton rotatif (Model Play) : on glisse verticalement.
      const surface = el ("div", c.type === "slider" ? "slider" : "knob");
      const handle = el ("div", c.type === "slider" ? "cap" : "pointer");
      if (c.type === "knob") surface.append (el ("div", "scale"));
      surface.append (handle);
      if (c.type === "knob") surface.append (el ("div", "dome"));
      surface.tabIndex = 0;
      surface.setAttribute ("role", "slider");
      surface.setAttribute ("aria-label", c.label);
      w.append (label, surface, value);

      let startY = 0, startX = 0;
      const move = e => {
        const range = c.type === "slider" ? surface.clientHeight : 160;
        const x = Math.min (1, Math.max (0, startX + (startY - e.clientY) / range));
        this.change (c, toValue (c, x));
      };
      surface.addEventListener ("pointerdown", e => {
        surface.setPointerCapture (e.pointerId);
        startY = e.clientY;
        if (c.type === "slider") {
          const r = surface.getBoundingClientRect ();
          startX = Math.min (1, Math.max (0, (r.bottom - e.clientY) / r.height));
          this.change (c, toValue (c, startX));
        } else {
          startX = toPosition (c, this.values[c.id]);
        }
        surface.addEventListener ("pointermove", move);
      });
      surface.addEventListener ("pointerup", () => surface.removeEventListener ("pointermove", move));
      surface.addEventListener ("dblclick", () => this.change (c, DEFAULTS[this.synth][c.id]));
      surface.addEventListener ("keydown", e => {
        const step = e.shiftKey ? 0.1 : 0.02;
        const x = toPosition (c, this.values[c.id]);
        if (e.key === "ArrowUp" || e.key === "ArrowRight") { this.change (c, toValue (c, Math.min (1, x + step))); e.preventDefault (); }
        if (e.key === "ArrowDown" || e.key === "ArrowLeft") { this.change (c, toValue (c, Math.max (0, x - step))); e.preventDefault (); }
      });

      set = v => {
        const x = Math.min (1, Math.max (0, toPosition (c, v)));
        if (c.type === "slider") handle.style.bottom = `calc(${x * 100}% - ${x * 12}px)`;
        else handle.style.transform = `rotate(${-135 + 270 * x}deg)`;
        value.textContent = c.show ? c.show (v) : fmt1 (v);
        surface.setAttribute ("aria-valuetext", value.textContent);
      };
    }

    this.widgets.set (c.id, set);
    return w;
  }

  change (c, v) {
    this.values[c.id] = v;
    this.widgets.get (c.id)?.(v);
    this.send ({ type: "param", id: c.id, value: v });
    if (! this.node) this.start ();
  }

  applyValues (values) {
    this.values = { ...this.values, ...values };
    for (const [id, set] of this.widgets) if (id in values) set (values[id]);
  }

  //----------------------------------------------------------------------------
  // Démarrage du son (premier geste) : contexte audio, moteur, MIDI.
  async start () {
    if (this.node || this.starting) return this.starting;
    this.starting = (async () => {
      const ctx = await audioContext ();
      if (ctx.state !== "running") await ctx.resume ();
      const node = new AudioWorkletNode (ctx, "load-and-play-synth", {
        numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2],
        processorOptions: { synth: this.synth } });
      const gain = ctx.createGain ();
      gain.gain.value = 0.9;
      node.connect (gain).connect (ctx.destination);
      node.onprocessorerror = e => console.error ("Load & Play : erreur du moteur audio", e);
      this.ctx = ctx;
      this.node = node;
      const ready = new Promise (resolve => {
        node.port.onmessage = e => {
          if (e.data.type === "values") { this.applyValues ({ ...e.data.values, ...(this.pending || {}) }); resolve (); }
        };
      });
      await ready;
      // Les réglages touchés avant le démarrage sont renvoyés au moteur.
      for (const [id, v] of Object.entries (this.pending || {})) node.port.postMessage ({ type: "param", id, value: v });
      this.pending = null;
      this.setupMidi ();
      this.refreshTexts ();
    }) ();
    return this.starting;
  }

  send (message) {
    if (this.node) this.node.port.postMessage (message);
    else if (message.type === "param") (this.pending ||= {})[message.id] = message.value;
  }

  noteOn (note, velocity = 1, time = 0) {
    if (! this.node) { this.start ().then (() => this.noteOn (note, velocity)); return; }
    // LP-303 en lecture : les touches transposent le motif au lieu de jouer.
    if (! time && this.acid?.playing) {
      this.acid.transpose = Math.max (-12, Math.min (12, note - 60));
      this.acid.showTranspose ();
      this.lightKey (note, true);
      return;
    }
    this.send ({ type: "note", on: true, note, velocity, time });
    if (! time) this.lightKey (note, true);
  }

  noteOff (note, time = 0) {
    if (! time && this.acid?.playing) { this.lightKey (note, false); return; }
    this.send ({ type: "note", on: false, note, time });
    if (! time) this.lightKey (note, false);
  }

  async setupMidi () {
    if (! navigator.requestMIDIAccess || this.midiReady) return;
    this.midiReady = true;
    try {
      const access = await navigator.requestMIDIAccess ();
      const listen = input => {
        input.onmidimessage = e => {
          const [status, a, b] = e.data, type = status & 0xf0;
          if (type === 0x90 && b > 0) this.noteOn (a, b / 127);
          else if (type === 0x80 || (type === 0x90 && b === 0)) this.noteOff (a);
          else if (type === 0xe0) this.send ({ type: "bend", value: ((b << 7 | a) - 8192) / 8192 });
          else if (type === 0xb0 && a === 1) this.send ({ type: "mod", value: b / 127 });
        };
      };
      access.inputs.forEach (listen);
      access.onstatechange = () => access.inputs.forEach (listen);
    } catch (e) { /* MIDI refusé ou indisponible : le clavier de l'écran reste là */ }
  }

  //----------------------------------------------------------------------------
  // Clavier à l'écran + clavier de l'ordinateur (touches physiques, donc AZERTY comme QWERTY).
  keyboard () {
    const wrap = el ("div", "keys-wrap");
    const keys = el ("div", "keys");
    this.keyEls = new Map ();
    const bass = this.synth === "lp303";
    this.octave = bass ? 3 : 4;
    const first = bass ? 24 : 36, last = bass ? 72 : 84;
    // LP-303 : vélocité normale, accent avec Maj (le seuil d'accent est à 100).
    const velocity = e => (bass ? (e?.shiftKey ? 1 : 0.7) : 1);
    let pointerNote = null;
    for (let n = first; n <= last; ++n) {
      const black = [1, 3, 6, 8, 10].includes (n % 12);
      const k = el ("div", black ? "key black" : "key white");
      k.dataset.note = n;
      if (n % 12 === 0) k.append (el ("span", "key-name", `C${Math.floor (n / 12) - 1}`));
      keys.append (k);
      this.keyEls.set (n, k);
    }
    keys.addEventListener ("pointerdown", e => {
      const k = e.target.closest (".key");
      if (! k) return;
      keys.setPointerCapture (e.pointerId);
      pointerNote = Number (k.dataset.note);
      this.noteOn (pointerNote, velocity (e));
    });
    keys.addEventListener ("pointermove", e => {
      if (pointerNote === null) return;
      const k = document.elementFromPoint (e.clientX, e.clientY)?.closest (".key");
      if (k && Number (k.dataset.note) !== pointerNote) {
        this.noteOff (pointerNote);
        pointerNote = Number (k.dataset.note);
        this.noteOn (pointerNote, velocity (e));
      }
    });
    const release = () => { if (pointerNote !== null) { this.noteOff (pointerNote); pointerNote = null; } };
    keys.addEventListener ("pointerup", release);
    keys.addEventListener ("pointercancel", release);

    // Pitch bend et molette de modulation
    const wheels = el ("div", "wheels");
    for (const [kind, label] of [["bend", "PITCH"], ["mod", "MOD"]]) {
      const box = el ("div", "wheel-box");
      const input = el ("input", "wheel");
      input.type = "range"; input.min = kind === "bend" ? -1 : 0; input.max = 1; input.step = 0.01; input.value = 0;
      input.setAttribute ("aria-label", label);
      input.addEventListener ("input", () => this.send ({ type: kind, value: Number (input.value) }));
      if (kind === "bend") input.addEventListener ("change", () => { input.value = 0; this.send ({ type: "bend", value: 0 }); });
      box.append (input, el ("span", "wheel-label", label));
      wheels.append (box);
    }

    if (bass) wheels.lastChild.remove ();          // pas de molette de modulation sur LP-303
    if (this.spec.performance.length) {
      const perf = el ("div", "performance");
      for (const c of this.spec.performance) perf.append (this.widget (c));
      wrap.append (perf);
    }
    wrap.append (wheels, keys);
    this.keyHint = el ("p", "key-hint");

    // Touches de l'ordinateur : actives quand la souris est au-dessus du bloc.
    const map = { KeyA: 0, KeyW: 1, KeyS: 2, KeyE: 3, KeyD: 4, KeyF: 5, KeyT: 6, KeyG: 7, KeyY: 8, KeyH: 9, KeyU: 10,
                  KeyJ: 11, KeyK: 12, KeyO: 13, KeyL: 14, KeyP: 15, Semicolon: 16, Quote: 17 };
    const down = new Map ();
    this.panel.addEventListener ("pointerenter", () => (this.active = true));
    this.panel.addEventListener ("pointerleave", () => (this.active = false));
    window.addEventListener ("keydown", e => {
      if (! this.active || e.repeat || e.metaKey || e.ctrlKey || e.target.closest?.("input, select, textarea")) return;
      if (e.code === "ArrowLeft") { this.octave = Math.max (1, this.octave - 1); e.preventDefault (); return; }
      if (e.code === "ArrowRight") { this.octave = Math.min (7, this.octave + 1); e.preventDefault (); return; }
      if (! (e.code in map)) return;
      const note = 12 * (this.octave + 1) + map[e.code] - 12;
      down.set (e.code, note);
      this.noteOn (note, velocity (e));
      e.preventDefault ();
    });
    window.addEventListener ("keyup", e => {
      if (! down.has (e.code)) return;
      this.noteOff (down.get (e.code));
      down.delete (e.code);
    });
    return wrap;
  }

  lightKey (note, on) { this.keyEls?.get (note)?.classList.toggle ("down", on); }

  // Bloc refermé : on coupe le séquenceur et les notes en cours.
  close () {
    if (this.seq?.playing) this.seq.stop ();
    if (this.acid?.playing) this.acid.stop ();
    this.send ({ type: "allOff" });
    this.keyEls?.forEach (k => k.classList.remove ("down"));
  }

  //----------------------------------------------------------------------------
  // Séquenceur : grille de 32 doubles-croches sur deux octaves.
  sequencer () {
    const steps = 32, low = 36 + (this.synth === "lp60" ? 12 : 0), rows = 25;
    const seq = this.seq = { notes: [], playing: false, step: 0, tempo: 110 };
    const block = el ("div", "seq");

    const bar = el ("div", "seq-bar");
    seq.play = el ("button", "seq-btn"); seq.play.type = "button";
    seq.demo = el ("button", "seq-btn"); seq.demo.type = "button";
    seq.clear = el ("button", "seq-btn"); seq.clear.type = "button";
    seq.tempoLabel = el ("label", "seq-tempo-label");
    const tempo = el ("input", "seq-tempo");
    tempo.type = "number"; tempo.min = 50; tempo.max = 200; tempo.value = seq.tempo;
    const tempoBox = el ("span", "seq-tempo-box");
    tempoBox.append (seq.tempoLabel, tempo, el ("span", "", "BPM"));
    bar.append (seq.play, seq.demo, seq.clear, tempoBox);
    block.append (bar);

    const grid = el ("div", "seq-grid");
    grid.style.setProperty ("--steps", steps);
    grid.style.setProperty ("--rows", rows);
    const names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
    for (let r = 0; r < rows; ++r) {
      const note = low + rows - 1 - r;
      const label = el ("div", `seq-note${names[note % 12].includes ("#") ? " sharp" : ""}`,
                        `${names[note % 12]}${Math.floor (note / 12) - 1}`);
      label.style.gridRow = r + 1;
      grid.append (label);
    }
    const cells = el ("div", "seq-cells");
    cells.style.gridRow = `1 / span ${rows}`;
    grid.append (cells);
    const playhead = el ("div", "seq-playhead");
    cells.append (playhead);
    block.append (grid);
    seq.hint = el ("p", "seq-hint");
    block.append (seq.hint);

    const draw = () => {
      cells.querySelectorAll (".seq-n").forEach (n => n.remove ());
      for (const n of seq.notes) {
        const d = el ("div", "seq-n");
        d.style.left = `${(n.start / steps) * 100}%`;
        d.style.width = `${(n.length / steps) * 100}%`;
        d.style.top = `${((low + rows - 1 - n.note) / rows) * 100}%`;
        d.style.height = `${100 / rows}%`;
        cells.append (d);
      }
    };

    const cellAt = e => {
      const r = cells.getBoundingClientRect ();
      const step = Math.min (steps - 1, Math.max (0, Math.floor ((e.clientX - r.left) / r.width * steps)));
      const row = Math.min (rows - 1, Math.max (0, Math.floor ((e.clientY - r.top) / r.height * rows)));
      return { step, note: low + rows - 1 - row };
    };

    let editing = null;
    cells.addEventListener ("pointerdown", e => {
      const { step, note } = cellAt (e);
      const hit = seq.notes.findIndex (n => n.note === note && step >= n.start && step < n.start + n.length);
      if (hit >= 0) { seq.notes.splice (hit, 1); draw (); return; }
      editing = { note, start: step, length: 1 };
      seq.notes.push (editing);
      cells.setPointerCapture (e.pointerId);
      draw ();
      this.noteOn (note); setTimeout (() => this.noteOff (note), 150);
    });
    cells.addEventListener ("pointermove", e => {
      if (! editing) return;
      const { step } = cellAt (e);
      editing.length = Math.max (1, step - editing.start + 1);
      draw ();
    });
    cells.addEventListener ("pointerup", () => (editing = null));

    seq.play.addEventListener ("click", () => (seq.playing ? stop () : play ()));
    seq.clear.addEventListener ("click", () => { seq.notes = []; draw (); });
    seq.demo.addEventListener ("click", () => {
      seq.notes = DEMOS[this.synth].map (([note, start, length]) => ({ note, start, length }));
      draw ();
      if (! seq.playing) play ();
    });
    tempo.addEventListener ("change", () => {
      seq.tempo = Math.min (200, Math.max (50, Number (tempo.value) || 110));
      tempo.value = seq.tempo;
      this.send ({ type: "tempo", bpm: seq.tempo });
    });

    // Lecture : planification avec un peu d'avance, événements datés pour le moteur audio.
    let timer = null, nextTime = 0;
    const stepSeconds = () => 60 / seq.tempo / 4;
    const schedule = () => {
      const ctx = this.ctx;
      while (nextTime < ctx.currentTime + 0.12) {
        for (const n of seq.notes) {
          if (n.start !== seq.step) continue;
          this.noteOn (n.note, 0.85, nextTime);
          this.noteOff (n.note, nextTime + n.length * stepSeconds () - 0.005);
        }
        const step = seq.step, at = nextTime;
        setTimeout (() => {
          playhead.style.left = `${(step / steps) * 100}%`;
          playhead.style.width = `${100 / steps}%`;
        }, Math.max (0, (at - ctx.currentTime) * 1000));
        seq.step = (seq.step + 1) % steps;
        nextTime += stepSeconds ();
      }
    };
    const play = async () => {
      await this.start ();
      seq.playing = true;
      seq.step = 0;
      nextTime = this.ctx.currentTime + 0.05;
      this.send ({ type: "tempo", bpm: seq.tempo });
      schedule ();
      timer = setInterval (schedule, 25);
      playhead.hidden = false;
      this.refreshTexts ();
    };
    const stop = () => {
      seq.playing = false;
      clearInterval (timer);
      this.send ({ type: "allOff" });
      playhead.hidden = true;
      this.refreshTexts ();
    };
    playhead.hidden = true;
    seq.stop = stop;
    draw ();
    return block;
  }

  //----------------------------------------------------------------------------
  // Séquenceur de LP-303 : 8 motifs de 16 pas, comme dans le plugin.
  acidSequencer () {
    const a = this.acid = {
      patterns: LP303_PATTERNS.map ((_, i) => factoryPattern (i)),
      current: 0, requested: 0, playing: false, transpose: 0, tempo: 125, swing: 0,
    };
    const NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C'"];
    const TOGGLES = [["OCT +", "up"], ["OCT −", "down"], ["ACCENT", "accent"], ["SLIDE", "slide"]];
    const black = n => [1, 3, 6, 8, 10].includes (n);

    const box = el ("div", "acid-seq");
    const side = el ("div", "acid-side");
    side.append (el ("div", "acid-title", "SEQUENCER"));
    a.play = el ("button", "acid-run"); a.play.type = "button";
    a.random = el ("button", "acid-tool"); a.random.type = "button";
    a.clear = el ("button", "acid-tool"); a.clear.type = "button";

    const patternRow = el ("div", "acid-patterns");
    const patternButtons = a.patterns.map ((pat, i) => {
      const b = el ("button", "", String (i + 1));
      b.type = "button";
      b.title = pat.name;
      b.addEventListener ("click", () => { a.requested = i; if (! a.playing) a.current = i; refresh (); });
      patternRow.append (b);
      return b;
    });
    const patternName = el ("div", "acid-name");

    const tempo = el ("input", "acid-tempo");
    tempo.type = "number"; tempo.min = 60; tempo.max = 200; tempo.value = a.tempo;
    tempo.setAttribute ("aria-label", "Tempo");
    tempo.addEventListener ("change", () => { a.tempo = Math.min (200, Math.max (60, Number (tempo.value) || 125)); tempo.value = a.tempo; });
    const swing = el ("input", "acid-swing");
    swing.type = "range"; swing.min = 0; swing.max = 1; swing.step = 0.05; swing.value = 0;
    swing.setAttribute ("aria-label", "Swing");
    swing.addEventListener ("input", () => (a.swing = Number (swing.value)));
    const transpose = el ("div", "acid-transpose");
    a.showTranspose = () => { transpose.textContent = `TRANSP ${a.transpose > 0 ? "+" : ""}${a.transpose}`; };
    transpose.title = "Clic : 0 / Click: 0";
    transpose.addEventListener ("click", () => { a.transpose = 0; a.showTranspose (); });

    const tempoRow = el ("div", "acid-row");
    tempoRow.append (el ("span", "acid-label", "TEMPO"), tempo, el ("span", "acid-label", "SWING"), swing);
    const tools = el ("div", "acid-row");
    tools.append (a.random, a.clear);
    side.append (a.play, el ("div", "acid-label", "PATTERN"), patternRow, patternName, tempoRow, transpose, tools);

    // Grille : en-tête (numéros), 13 notes, 4 rangées d'options.
    const grid = el ("div", "acid-grid");
    const cells = [];
    const rowLabel = (text, row, cls = "") => {
      const l = el ("div", `acid-rowlabel ${cls}`, text);
      l.style.gridRow = row; grid.append (l);
    };
    grid.append (Object.assign (el ("div", "acid-corner"), { style: "grid-row: 1" }));
    for (let n = 12; n >= 0; --n) rowLabel (NAMES[n], 2 + 12 - n, black (n) ? "sharp" : "");
    TOGGLES.forEach (([name], i) => rowLabel (name, 15 + i, "opt"));
    const heads = [];
    for (let s = 0; s < 16; ++s) {
      const head = el ("div", "acid-head", String (s + 1));
      head.style.gridColumn = s + 2; head.style.gridRow = 1;
      head.dataset.step = s; head.dataset.kind = "head";
      grid.append (head); heads.push (head);
      const column = [];
      for (let n = 12; n >= 0; --n) {
        const c = el ("div", `acid-cell${black (n) ? " sharp" : ""}${s % 4 === 0 ? " beat" : ""}`);
        c.style.gridColumn = s + 2; c.style.gridRow = 2 + 12 - n;
        c.dataset.step = s; c.dataset.note = n; c.dataset.kind = "note";
        grid.append (c); column[n] = c;
      }
      TOGGLES.forEach (([, key], i) => {
        const c = el ("div", `acid-opt ${key}`);
        c.style.gridColumn = s + 2; c.style.gridRow = 15 + i;
        c.dataset.step = s; c.dataset.kind = key;
        grid.append (c); column[key] = c;
      });
      cells.push (column);
    }
    a.hint = el ("p", "acid-hint");
    const main = el ("div", "acid-main");
    main.append (grid, a.hint);
    box.append (side, main);

    const pattern = () => a.patterns[a.current];
    const refresh = () => {
      const pat = pattern ();
      patternButtons.forEach ((b, i) => {
        b.classList.toggle ("on", i === a.current);
        b.classList.toggle ("pending", a.playing && i === a.requested && i !== a.current);
      });
      patternName.textContent = `${pat.name} · ${pat.length} ${t ("pas", "steps")}`;
      for (let s = 0; s < 16; ++s) {
        const st = pat.steps[s], col = cells[s];
        for (let n = 0; n <= 12; ++n) {
          const on = st.gate && st.note === n;
          col[n].classList.toggle ("on", on);
          col[n].classList.toggle ("accent", on && st.accent);
          col[n].textContent = on && st.octave ? (st.octave > 0 ? "▲" : "▼") : "";
        }
        col.up.classList.toggle ("on", st.octave > 0);
        col.down.classList.toggle ("on", st.octave < 0);
        col.accent.classList.toggle ("on", st.accent);
        col.slide.classList.toggle ("on", st.slide);
        const outside = s >= pat.length;
        heads[s].classList.toggle ("out", outside);
        Object.values (col).forEach (c => c.classList.toggle ("out", outside));
      }
    };
    a.refresh = refresh;

    // Édition : clic, puis glisser pour peindre la même valeur.
    let drag = null;
    const apply = (target, first) => {
      const kind = target.dataset.kind, s = Number (target.dataset.step), st = pattern ().steps[s];
      if (kind === "head") { if (first) pattern ().length = s + 1; }
      else if (kind === "note") {
        const n = Number (target.dataset.note);
        if (first) drag.value = ! (st.gate && st.note === n);
        st.gate = drag.value;
        if (drag.value) st.note = n;
        if (first && drag.value && ! a.playing) {
          const note = LP303_BASE + n + 12 * st.octave;
          this.noteOn (note, st.accent ? 1 : 0.7);
          setTimeout (() => this.noteOff (note), 160);
        }
      } else {
        const get = { up: () => st.octave > 0, down: () => st.octave < 0, accent: () => st.accent, slide: () => st.slide }[kind];
        if (first) drag.value = ! get ();
        if (kind === "up") st.octave = drag.value ? 1 : (st.octave > 0 ? 0 : st.octave);
        else if (kind === "down") st.octave = drag.value ? -1 : (st.octave < 0 ? 0 : st.octave);
        else st[kind] = drag.value;
      }
      drag.last = `${kind}${s}${target.dataset.note ?? ""}`;
      refresh ();
    };
    grid.addEventListener ("pointerdown", e => {
      const target = e.target.closest ("[data-kind]");
      if (! target) return;
      drag = { kind: target.dataset.kind, value: true, last: "" };
      grid.setPointerCapture (e.pointerId);
      apply (target, true);
    });
    grid.addEventListener ("pointermove", e => {
      if (! drag || drag.kind === "head") return;
      const target = document.elementFromPoint (e.clientX, e.clientY)?.closest ("[data-kind]");
      if (! target || target.dataset.kind !== drag.kind) return;
      if (`${target.dataset.kind}${target.dataset.step}${target.dataset.note ?? ""}` !== drag.last) apply (target, false);
    });
    const endDrag = () => (drag = null);
    grid.addEventListener ("pointerup", endDrag);
    grid.addEventListener ("pointercancel", endDrag);

    a.random.addEventListener ("click", () => { const r = randomPattern (); Object.assign (pattern (), { steps: r.steps, length: 16 }); refresh (); });
    a.clear.addEventListener ("click", () => {
      Object.assign (pattern (), { length: 16, steps: Array.from ({ length: 16 }, () => ({ note: 0, octave: 0, accent: false, slide: false, gate: true })) });
      refresh ();
    });

    // Lecture : comme le plugin. Une note dure une demi double croche ; un slide
    // envoie la note suivante avant de relâcher la précédente (legato).
    let timer = null, nextTime = 0, k = 0, position = 0, sounding = -1, tie = false;
    const stepSeconds = () => 60 / a.tempo / 4;
    const schedule = () => {
      const ctx = this.ctx;
      while (nextTime < ctx.currentTime + 0.12) {
        const swingDelay = k % 2 ? a.swing * 0.5 * stepSeconds () : 0;
        const at = nextTime + swingDelay;
        if (position === 0 && a.requested !== a.current) { a.current = a.requested; setTimeout (refresh, Math.max (0, (at - ctx.currentTime) * 1000)); }
        const pat = pattern ();
        if (position >= pat.length) position = 0;
        const st = pat.steps[position];
        if (st.gate) {
          const note = Math.max (0, Math.min (127, LP303_BASE + st.note + 12 * st.octave + a.transpose));
          const velocity = st.accent ? 1 : 90 / 127;
          if (tie && sounding >= 0) {
            if (note !== sounding) { this.noteOn (note, velocity, at); this.noteOff (sounding, at); }
          } else {
            if (sounding >= 0) this.noteOff (sounding, at);
            this.noteOn (note, velocity, at);
          }
          sounding = note;
          tie = st.slide;
          if (! st.slide) {
            const nextStart = nextTime + stepSeconds () + ((k + 1) % 2 ? a.swing * 0.5 * stepSeconds () : 0);
            this.noteOff (note, at + 0.5 * (nextStart - at));
            sounding = -1;
          }
        } else {
          if (sounding >= 0) this.noteOff (sounding, at);
          sounding = -1; tie = false;
        }
        const shown = position;
        setTimeout (() => {
          heads.forEach ((h, i) => h.classList.toggle ("now", i === shown && a.playing));
          cells.forEach ((col, i) => Object.values (col).forEach (c => c.classList.toggle ("now", i === shown && a.playing)));
        }, Math.max (0, (at - ctx.currentTime) * 1000));
        position = (position + 1) % pat.length;
        k += 1;
        nextTime += stepSeconds ();
      }
    };
    const play = async () => {
      await this.start ();
      this.send ({ type: "allOff" });
      a.playing = true;
      k = 0; position = 0; sounding = -1; tie = false;
      a.current = a.requested;
      nextTime = this.ctx.currentTime + 0.05;
      schedule ();
      timer = setInterval (schedule, 25);
      this.refreshTexts ();
      refresh ();
    };
    const stop = () => {
      a.playing = false;
      clearInterval (timer);
      this.send ({ type: "allOff" });
      heads.forEach (h => h.classList.remove ("now"));
      cells.forEach (col => Object.values (col).forEach (c => c.classList.remove ("now")));
      this.refreshTexts ();
      refresh ();
    };
    a.stop = stop;
    a.play.addEventListener ("click", () => (a.playing ? stop () : play ()));
    a.showTranspose ();
    refresh ();
    return box;
  }
}

//------------------------------------------------------------------------------
for (const panel of document.querySelectorAll (".try-panel[data-instrument]")) {
  let instance = null;
  const open = () => { if (! instance) instance = new TryPanel (panel); };
  panel.addEventListener ("tryopen", open);
  panel.addEventListener ("tryclose", () => instance?.close ());
  if (! panel.hidden) open ();
}
