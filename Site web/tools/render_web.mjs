// Rend un scénario avec le moteur web (JavaScript) d'un synthé, en WAV 24 bits,
// pour le comparer au rendu du plugin.
//
// Utilisation : node render_web.mjs <lp60|modelplay> <scenario.json> <sortie.wav>
// Scénario : { "rate": 44100, "duration": 4, "params": { "cutoff": 5 }, "preset": 2,
//              "notes": [[48, 0.5, 2.0], ...] }   (note, début s, durée s)

import { readFileSync, writeFileSync } from "node:fs";

const [, , synth, scenarioPath, output] = process.argv;
const scenario = JSON.parse (readFileSync (scenarioPath, "utf8"));
const rate = scenario.rate || 44100;

const module = await import (synth === "lp60" ? "../assets/js/dsp/lp60.js" : "../assets/js/dsp/modelplay.js");
const Engine = synth === "lp60" ? module.LP60 : module.ModelPlay;
const engine = new Engine (rate);

if (scenario.preset !== undefined) engine.loadPreset (scenario.preset);
for (const [id, value] of Object.entries (scenario.params || {})) engine.setParam (id, value);

const total = Math.round (scenario.duration * rate);
const events = [];
for (const [note, start, length] of scenario.notes || []) {
  events.push ({ at: Math.round (start * rate), on: true, note });
  events.push ({ at: Math.round ((start + length) * rate), on: false, note });
}
events.sort ((a, b) => a.at - b.at || (a.on ? 1 : 0) - (b.on ? 1 : 0));

const left = new Float32Array (total), right = new Float32Array (total);
const block = 128;
const bl = new Float32Array (block), br = new Float32Array (block);
let next = 0;
for (let pos = 0; pos < total;) {
  while (next < events.length && events[next].at <= pos) {
    const e = events[next++];
    e.on ? engine.noteOn (e.note, 1) : engine.noteOff (e.note);
  }
  let n = Math.min (block, total - pos);
  if (next < events.length) n = Math.min (n, Math.max (1, events[next].at - pos));
  engine.render (bl, br, n);
  left.set (bl.subarray (0, n), pos);
  right.set (br.subarray (0, n), pos);
  pos += n;
}

// WAV PCM 24 bits stéréo
const data = Buffer.alloc (total * 6);
for (let i = 0; i < total; ++i) {
  for (const [c, ch] of [[0, left], [1, right]]) {
    const v = Math.max (-8388608, Math.min (8388607, Math.round (ch[i] * 8388607)));
    data.writeIntLE (v, i * 6 + c * 3, 3);
  }
}
const header = Buffer.alloc (44);
header.write ("RIFF", 0); header.writeUInt32LE (36 + data.length, 4); header.write ("WAVE", 8);
header.write ("fmt ", 12); header.writeUInt32LE (16, 16); header.writeUInt16LE (1, 20); header.writeUInt16LE (2, 22);
header.writeUInt32LE (rate, 24); header.writeUInt32LE (rate * 6, 28); header.writeUInt16LE (6, 32); header.writeUInt16LE (24, 34);
header.write ("data", 36); header.writeUInt32LE (data.length, 40);
writeFileSync (output, Buffer.concat ([header, data]));
