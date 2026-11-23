// Processeur audio du navigateur : fait tourner le moteur du LP-60, de Model
// Play ou de LP-303 dans le fil audio, avec des événements calés à l'échantillon.

import { LP60 } from "./lp60.js";
import { ModelPlay } from "./modelplay.js";
import { LP303 } from "./lp303.js";

const ENGINES = { lp60: LP60, modelplay: ModelPlay, lp303: LP303 };

class SynthProcessor extends AudioWorkletProcessor {
  constructor (options) {
    super ();
    const synth = options.processorOptions.synth;
    this.engine = new (ENGINES[synth] || ModelPlay) (sampleRate);
    this.queue = [];            // événements datés { frame, apply }
    this.port.onmessage = e => this.receive (e.data);
    this.port.postMessage ({ type: "values", values: { ...this.engine.values } });
  }

  receive (m) {
    const e = this.engine;
    const now = { param: () => e.setParam (m.id, m.value),
                  preset: () => this.port.postMessage ({ type: "values", values: e.loadPreset (m.index) }),
                  allOff: () => { this.queue.length = 0; e.allNotesOff (); },
                  bend: () => e.setPitchBend (m.value),
                  mod: () => e.setModWheel (m.value),
                  tempo: () => e.setTempo (m.bpm) };
    if (now[m.type]) { now[m.type] (); return; }

    if (m.type === "note") {
      const apply = m.on ? () => e.noteOn (m.note, m.velocity ?? 1) : () => e.noteOff (m.note);
      const frame = m.time ? Math.round (m.time * sampleRate) : 0;
      this.queue.push ({ frame, apply });
      this.queue.sort ((a, b) => a.frame - b.frame);
    }
  }

  process (_, outputs) {
    const out = outputs[0];
    const left = out[0], right = out[1] || out[0];
    const n = left.length;
    let pos = 0;
    while (pos < n) {
      while (this.queue.length && this.queue[0].frame <= currentFrame + pos) this.queue.shift ().apply ();
      let end = n;
      if (this.queue.length) end = Math.min (n, Math.max (pos + 1, this.queue[0].frame - currentFrame));
      this.engine.render (left.subarray (pos, end), right.subarray (pos, end), end - pos);
      pos = end;
    }
    return true;
  }
}

registerProcessor ("load-and-play-synth", SynthProcessor);
