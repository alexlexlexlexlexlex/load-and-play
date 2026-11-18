// Outils DSP communs aux deux synthés, portés à l'identique du C++ des plugins.

export const clamp = (v, lo, hi) => (v < lo ? lo : v > hi ? hi : v);

export function fastTanh (x) {
  if (x > 3) return 1;
  if (x < -3) return -1;
  const x2 = x * x;
  return x * (27 + x2) / (27 + 9 * x2);
}

export function rcCoef (timeConstant, rate) {
  return 1 - Math.exp (-1 / (Math.max (timeConstant, 1e-5) * rate));
}

export function polyBlep (t, dt) {
  if (t < dt) {
    t /= dt;
    return t + t - t * t - 1;
  }
  if (t > 1 - dt) {
    t = (t - 1) / dt;
    return t * t + t + t + 1;
  }
  return 0;
}

export function pulseWave (p, dt, width) {
  let v = p < width ? 1 : -1;
  v += polyBlep (p, dt);
  let p2 = p - width;
  if (p2 < 0) p2 += 1;
  return v - polyBlep (p2, dt);
}

// Table mesurée aux positions 0 / 2,5 / 5 / 7,5 / 10, interpolée en log
// (linéaire si une borne vaut 0).
export function lookup (table, slider) {
  const position = clamp (slider, 0, 10) / 2.5;
  const i = Math.min (3, Math.floor (position));
  const frac = position - i;
  const a = table[i], b = table[i + 1];
  return a > 0 && b > 0 ? a * Math.pow (b / a, frac) : a + (b - a) * frac;
}

export function interpolate (xs, ys, x) {
  if (x <= xs[0]) return ys[0];
  for (let i = 1; i < xs.length; ++i)
    if (x <= xs[i]) return ys[i - 1] + (ys[i] - ys[i - 1]) * (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
  return ys[ys.length - 1];
}

export class Smoothed {
  constructor () { this.current = 0; this.target = 0; }
  snap () { this.current = this.target; }
  next (coef) { this.current += (this.target - this.current) * coef; return this.current; }
}

// Générateur xorshift 32 bits, comme dans le C++.
export class XorShift {
  constructor (seed) { this.state = (seed >>> 0) || 1; }
  white () {
    let s = this.state;
    s ^= s << 13; s >>>= 0;
    s ^= s >>> 17;
    s ^= s << 5; s >>>= 0;
    this.state = s;
    return s * (2 / 4294967296) - 1;
  }
}

//------------------------------------------------------------------------------
// Décimation x2 : les moteurs calculent à deux fois la fréquence de sortie.
// Filtre demi-bande à réponse impulsionnelle finie (fenêtre de Kaiser) : les
// plugins utilisent un demi-bande IIR de JUCE, la coupure est la même.
function besselI0 (x) {
  let sum = 1, term = 1;
  for (let k = 1; k < 40; ++k) {
    term *= (x / (2 * k)) ** 2;
    sum += term;
    if (term < 1e-12 * sum) break;
  }
  return sum;
}

export class HalfbandDecimator {
  constructor (taps = 63, beta = 7.5) {
    const middle = (taps - 1) / 2;
    const h = new Float64Array (taps);
    let sum = 0;
    for (let k = 0; k < taps; ++k) {
      const n = k - middle;
      const ideal = n === 0 ? 0.5 : Math.sin (Math.PI * n / 2) / (Math.PI * n);
      const r = n / middle;
      const window = besselI0 (beta * Math.sqrt (Math.max (0, 1 - r * r))) / besselI0 (beta);
      h[k] = ideal * window;
      sum += h[k];
    }
    // On ne garde que les coefficients non nuls (un sur deux dans un demi-bande).
    this.offsets = [];
    this.coefs = [];
    for (let k = 0; k < taps; ++k) {
      if (Math.abs (h[k]) > 1e-12) { this.offsets.push (k); this.coefs.push (h[k] / sum); }
    }
    this.history = taps - 1;
    this.buffer = new Float32Array (this.history + 8192);
  }

  // input : 2n échantillons suréchantillonnés ; output : n échantillons.
  process (input, output, n) {
    const hist = this.history, buf = this.buffer;
    if (buf.length < hist + 2 * n) {
      const bigger = new Float32Array (hist + 4 * n);
      bigger.set (buf.subarray (0, hist));
      this.buffer = bigger;
      return this.process (input, output, n);
    }
    buf.set (input.subarray (0, 2 * n), hist);
    const offsets = this.offsets, coefs = this.coefs, count = coefs.length;
    for (let i = 0; i < n; ++i) {
      const top = hist + 2 * i + 1;
      let acc = 0;
      for (let k = 0; k < count; ++k) acc += coefs[k] * buf[top - offsets[k]];
      output[i] = acc;
    }
    buf.copyWithin (0, 2 * n, 2 * n + hist);
  }
}

// Ligne à retard à lecture interpolée (chorus, delay).
export class DelayLine {
  constructor (size) { this.buffer = new Float32Array (Math.max (4, size)); this.write = 0; }
  push (x) {
    this.buffer[this.write] = x;
    if (++this.write === this.buffer.length) this.write = 0;
  }
  read (delaySamples) {
    const size = this.buffer.length;
    let pos = this.write - 1 - clamp (delaySamples, 0, size - 2);
    if (pos < 0) pos += size;
    const i0 = Math.floor (pos);
    const i1 = i0 + 1 === size ? 0 : i0 + 1;
    const frac = pos - i0;
    return this.buffer[i0] + frac * (this.buffer[i1] - this.buffer[i0]);
  }
}
