// Duck Hunt : baisse le volume d'une piste quand une autre joue (« ducking »),
// sans compresseur. La piste de référence arrive par l'entrée sidechain ; on
// peut aussi déclencher le duck par des notes MIDI ou en rythme au tempo.
//
// Trois façons de baisser le volume :
// - pleine bande : tout le signal baisse ;
// - 3 bandes : graves, médiums et aigus baissent chacun selon la même bande de
//   la référence (filtres de Linkwitz-Riley, somme plate, sans latence) ;
// - spectral : la piste ne se creuse qu'aux fréquences où la référence joue
//   (analyse FFT, latence d'une fenêtre, compensée par le DAW).
//
// Le moteur ne dépend que de juce_dsp (pour la FFT) : il est testé hors DAW.

#pragma once

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace duckhunt
{

enum class Trigger { sidechain, midi, sync };
enum class Mode { fullBand, threeBands, spectral };

// Divisions du mode tempo, en temps (noires).
inline constexpr std::array<double, 10> divisionBeats { 4.0, 2.0, 1.0, 0.5, 0.25, 1.5, 0.75, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
inline const char* const divisionNames[] { "1/1", "1/2", "1/4", "1/8", "1/16", "1/4.", "1/8.", "1/4T", "1/8T", "1/16T" };

inline constexpr float maxLookaheadMs = 20.0f;

struct Params
{
    Trigger trigger = Trigger::sidechain;
    Mode mode = Mode::fullBand;

    float depthDb = 12.0f;        // baisse maximale
    float thresholdDb = -30.0f;   // niveau de la référence où le duck commence
    float rangeDb = 12.0f;        // dB au-dessus du seuil pour atteindre la baisse maximale
    float attackMs = 5.0f;
    float holdMs = 20.0f;
    float releaseMs = 200.0f;
    float curve = 0.0f;           // -1 : remonte vite puis traîne ; +1 : reste bas puis remonte d'un coup
    float lookaheadMs = 0.0f;
    int division = 2;             // index dans divisionBeats
    bool velocity = true;         // MIDI : la vélocité dose la baisse

    float scLowCut = 20.0f;       // filtre de la référence (pleine bande)
    float scHighCut = 20000.0f;

    float crossoverLow = 200.0f;  // 3 bandes
    float crossoverHigh = 2500.0f;
    std::array<float, 3> bandAmount { 1.0f, 1.0f, 1.0f };

    float focusLow = 20.0f;       // spectral : zone de fréquences concernée
    float focusHigh = 20000.0f;
    float smoothing = 0.5f;       // spectral : largeur de l'analyse, 0 = fine, 1 = une octave

    float mix = 1.0f;
    float outputDb = 0.0f;
    bool invert = false;          // baisse quand la référence se tait
    bool listen = false;          // écouter la référence filtrée
};

struct Transport
{
    double bpm = 120.0;
    double ppq = 0.0;
    bool playing = false;
};

//==============================================================================
// Petits outils

inline float dbToGain (float db) { return std::exp (db * 0.11512925f); }
inline float gainToDb (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-6f)); }

struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, z1 = 0.0, z2 = 0.0;

    void set (bool highpass, double freq, double q, double rate)
    {
        const double w = juce::MathConstants<double>::twoPi * std::clamp (freq, 10.0, rate * 0.45) / rate;
        const double cosw = std::cos (w), alpha = std::sin (w) / (2.0 * q), a0 = 1.0 + alpha;
        const double side = highpass ? (1.0 + cosw) * 0.5 : (1.0 - cosw) * 0.5;
        b0 = side / a0;
        b1 = (highpass ? -2.0 : 2.0) * side / a0;
        b2 = side / a0;
        a1 = -2.0 * cosw / a0;
        a2 = (1.0 - alpha) / a0;
    }

    float process (float x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

    void reset() { z1 = z2 = 0.0; }
};

// Filtre de Linkwitz-Riley 24 dB/oct : passe-bas + passe-haut dont la somme est
// un passe-tout (même niveau à toutes les fréquences).
struct Crossover
{
    std::array<Biquad, 2> low, high;

    void set (double freq, double rate)
    {
        for (auto& f : low) f.set (false, freq, juce::MathConstants<double>::sqrt2 * 0.5, rate);
        for (auto& f : high) f.set (true, freq, juce::MathConstants<double>::sqrt2 * 0.5, rate);
    }

    void split (float x, float& lowOut, float& highOut) noexcept
    {
        lowOut = low[1].process (low[0].process (x));
        highOut = high[1].process (high[0].process (x));
    }

    float allpass (float x) noexcept { float l, h; split (x, l, h); return l + h; }

    void reset() { for (auto& f : low) f.reset(); for (auto& f : high) f.reset(); }
};

// Niveau de la référence : crête qui monte aussitôt et retombe en 15 ms.
struct LevelFollower
{
    float level = 0.0f, fall = 0.999f;

    void prepare (double rate) { fall = (float) std::exp (-1.0 / (0.015 * rate)); }
    float process (float x) noexcept { level = std::max (std::abs (x), level * fall); return level; }
    void reset() { level = 0.0f; }
};

// Forme du duck : la valeur (0 à 1) monte et descend en ligne droite aux
// vitesses d'attaque et de relâchement, attend « hold » avant de redescendre,
// puis passe par la courbe.
struct Shaper
{
    float value = 0.0f, up = 1.0f, down = 1.0f, exponent = 1.0f, gate = 0.0f;
    int holdSamples = 0, holdLeft = 0;

    void setTimes (float attackS, float holdS, float releaseS, float curve, double rate)
    {
        up = 1.0f / std::max (1.0f, attackS * (float) rate);
        down = 1.0f / std::max (1.0f, releaseS * (float) rate);
        holdSamples = (int) (holdS * rate);
        exponent = std::pow (2.0f, -2.0f * curve);
    }

    // Déclenchement (MIDI, tempo) : monte jusqu'au niveau demandé, attend, redescend.
    void trigger (float level) { gate = std::max (gate, level); }

    float process (float target) noexcept
    {
        if (gate > 0.0f)
        {
            target = std::max (target, gate);
            if (value >= gate - 1.0e-6f)
                gate = 0.0f;
        }

        if (target >= value)
        {
            value = std::min (target, value + up);
            holdLeft = holdSamples;
        }
        else if (holdLeft > 0)
            --holdLeft;
        else
            value = std::max (target, value - down);

        return shaped();
    }

    float shaped() const noexcept { return juce::exactlyEqual (exponent, 1.0f) ? value : std::pow (value, exponent); }

    void reset() { value = gate = 0.0f; holdLeft = 0; }
};

// Ligne à retard (anticipation et compensation de latence du signal sec).
struct DelayLine
{
    std::vector<float> data;
    int write = 0;

    void prepare (int maxDelay) { data.assign ((size_t) juce::nextPowerOfTwo (maxDelay + 2), 0.0f); write = 0; }
    void push (float x) noexcept { data[(size_t) write] = x; write = (write + 1) & (int) (data.size() - 1); }
    float read (int delay) const noexcept { return data[(size_t) ((write - 1 - delay) & (int) (data.size() - 1))]; }
    void reset() { std::fill (data.begin(), data.end(), 0.0f); }
};

//==============================================================================
// Ce que l'interface affiche.

struct HistoryPoint
{
    float main = 0.0f, side = 0.0f;           // crêtes (linéaires)
    std::array<float, 3> reductionDb {};      // baisse par bande (pleine bande : la première)
};

struct SpectrumView
{
    static constexpr int numPoints = 160;     // de 20 Hz à 20 kHz, en échelle logarithmique
    std::array<float, numPoints> mainDb {}, sideDb {}, reductionDb {};
};

//==============================================================================
// Duck spectral : FFT à recouvrement de 75 %, fenêtres racine de Hann à
// l'analyse et à la synthèse (reconstruction parfaite sans traitement).
class SpectralDucker
{
public:
    void prepare (double sampleRate)
    {
        rate = sampleRate;
        const int order = sampleRate <= 50000.0 ? 10 : (sampleRate <= 100000.0 ? 11 : 12);
        fft = std::make_unique<juce::dsp::FFT> (order);
        size = 1 << order;
        hop = size / 4;
        bins = size / 2 + 1;

        window.resize ((size_t) size);
        windowPower = 0.0;
        for (int i = 0; i < size; ++i)
        {
            window[(size_t) i] = (float) std::sqrt (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / size));
            windowPower += (double) window[(size_t) i] * window[(size_t) i];
        }

        for (auto* buffers : { &mainIn, &mainOut, &sideIn })
            for (auto& b : *buffers)
                b.assign ((size_t) size, 0.0f);

        work.assign ((size_t) size * 2, 0.0f);
        sidePower.assign ((size_t) bins, 0.0);
        mainPower.assign ((size_t) bins, 0.0);
        prefix.assign ((size_t) bins + 1, 0.0);
        value.assign ((size_t) bins, 0.0f);
        holdLeft.assign ((size_t) bins, 0);
        reduction.assign ((size_t) bins, 0.0f);
        sideLevel.assign ((size_t) bins, -120.0f);
        mainLevel.assign ((size_t) bins, -120.0f);

        // Échelle de la FFT inverse selon le moteur de la plateforme : mesurée une fois.
        std::fill (work.begin(), work.end(), 0.0f);
        work[1] = 1.0f;
        fft->performRealOnlyForwardTransform (work.data(), true);
        fft->performRealOnlyInverseTransform (work.data());
        inverseScale = std::abs (work[1]) > 1.0e-9f ? 1.0f / work[1] : 1.0f;
        inverseScale *= 0.5f;   // fenêtre de Hann au carré à 75 % de recouvrement : somme 2

        reset();
    }

    void reset()
    {
        for (auto* buffers : { &mainIn, &mainOut, &sideIn })
            for (auto& b : *buffers)
                std::fill (b.begin(), b.end(), 0.0f);
        std::fill (value.begin(), value.end(), 0.0f);
        std::fill (holdLeft.begin(), holdLeft.end(), 0);
        std::fill (reduction.begin(), reduction.end(), 0.0f);
        count = 0;
    }

    int latency() const { return size; }

    // Un échantillon par canal à l'entrée et à la sortie ; « shaped » est la forme
    // du duck pour les déclenchements MIDI et tempo (négatif : suivre la référence).
    void process (const float* main, int numMain, const float* side, int numSide, float* out,
                  const Params& p, float shaped) noexcept
    {
        const int at = size - hop + count;
        for (int ch = 0; ch < 2; ++ch)
        {
            mainIn[(size_t) ch][(size_t) at] = ch < numMain ? main[ch] : 0.0f;
            sideIn[(size_t) ch][(size_t) at] = ch < numSide ? side[ch] : 0.0f;
            out[ch] = mainOut[(size_t) ch][(size_t) count];
        }

        if (++count == hop)
        {
            frame (p, shaped, numMain, numSide);
            count = 0;
        }
    }

    // Pour l'affichage (appelé sur le fil audio, juste après une analyse).
    void fillView (SpectrumView& view) const
    {
        for (int i = 0; i < SpectrumView::numPoints; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, (double) i / (SpectrumView::numPoints - 1));
            const int k = std::clamp ((int) std::round (f * size / rate), 1, bins - 1);
            view.mainDb[(size_t) i] = mainLevel[(size_t) k];
            view.sideDb[(size_t) i] = sideLevel[(size_t) k];
            view.reductionDb[(size_t) i] = reduction[(size_t) k];
        }
    }

    bool frameReady = false;

private:
    void frame (const Params& p, float shaped, int numMain, int numSide)
    {
        // Spectre de puissance de la référence (moyenne des canaux) et de la piste.
        analyse (sideIn, std::max (1, numSide), sidePower);
        analyse (mainIn, std::max (1, numMain), mainPower);

        // Largeur d'analyse autour de chaque fréquence : de 1/12 d'octave à 1 octave.
        prefix[0] = 0.0;
        for (int k = 0; k < bins; ++k)
            prefix[(size_t) k + 1] = prefix[(size_t) k] + sidePower[(size_t) k];

        const double width = std::pow (2.0, (1.0 / 12.0 + p.smoothing * (11.0 / 12.0)) * 0.5);
        const double toAmplitude = 4.0 / ((double) size * windowPower);
        const float hopSeconds = (float) hop / (float) rate;
        const float up = hopSeconds / std::max (hopSeconds, p.attackMs * 0.001f);
        const float down = hopSeconds / std::max (hopSeconds, p.releaseMs * 0.001f);
        const int holdFrames = (int) std::round (p.holdMs * 0.001f / hopSeconds);
        const float exponent = std::pow (2.0f, -2.0f * p.curve);
        const double binHz = rate / size;

        for (int k = 0; k < bins; ++k)
        {
            const double f = std::max (1, k) * binHz;
            const int lo = std::clamp ((int) std::floor (f / width / binHz), 1, bins - 1);
            const int hi = std::clamp ((int) std::ceil (f * width / binHz), lo, bins - 1);
            const double energy = prefix[(size_t) hi + 1] - prefix[(size_t) lo];
            const float level = (float) (10.0 * std::log10 (energy * toAmplitude + 1.0e-14));
            sideLevel[(size_t) k] = level;
            mainLevel[(size_t) k] = (float) (10.0 * std::log10 (mainPower[(size_t) k] * toAmplitude + 1.0e-14));

            float amount;
            if (shaped >= 0.0f)
                amount = shaped;
            else
            {
                const float target = std::clamp ((level - p.thresholdDb) / std::max (0.1f, p.rangeDb), 0.0f, 1.0f);
                float& v = value[(size_t) k];
                int& hold = holdLeft[(size_t) k];
                if (target >= v) { v = std::min (target, v + up); hold = holdFrames; }
                else if (hold > 0) --hold;
                else v = std::max (target, v - down);
                amount = juce::exactlyEqual (exponent, 1.0f) ? v : std::pow (v, exponent);
            }

            if (p.invert) amount = 1.0f - amount;
            reduction[(size_t) k] = p.depthDb * amount * focusWeight ((float) f, p);
        }

        // La piste : même analyse, chaque fréquence baissée, puis resynthèse.
        for (int ch = 0; ch < 2; ++ch)
        {
            if (ch < numMain)
            {
                auto& in = mainIn[(size_t) ch];
                for (int i = 0; i < size; ++i)
                    work[(size_t) i] = in[(size_t) i] * window[(size_t) i];
                std::fill (work.begin() + size, work.end(), 0.0f);
                fft->performRealOnlyForwardTransform (work.data(), true);

                for (int k = 0; k < bins; ++k)
                {
                    const float g = dbToGain (-reduction[(size_t) std::max (1, k)]);
                    work[(size_t) (2 * k)] *= g;
                    work[(size_t) (2 * k + 1)] *= g;
                }

                fft->performRealOnlyInverseTransform (work.data());
            }

            auto& out = mainOut[(size_t) ch];
            std::move (out.begin() + hop, out.end(), out.begin());
            std::fill (out.end() - hop, out.end(), 0.0f);
            if (ch < numMain)
                for (int i = 0; i < size; ++i)
                    out[(size_t) i] += work[(size_t) i] * window[(size_t) i] * inverseScale;
        }

        for (auto* buffers : { &mainIn, &sideIn })
            for (auto& b : *buffers)
                std::move (b.begin() + hop, b.end(), b.begin());

        frameReady = true;
    }

    void analyse (std::array<std::vector<float>, 2>& input, int channels, std::vector<double>& power)
    {
        std::fill (power.begin(), power.end(), 0.0);
        for (int ch = 0; ch < std::min (2, channels); ++ch)
        {
            for (int i = 0; i < size; ++i)
                work[(size_t) i] = input[(size_t) ch][(size_t) i] * window[(size_t) i];
            std::fill (work.begin() + size, work.end(), 0.0f);
            fft->performRealOnlyForwardTransform (work.data(), true);
            for (int k = 0; k < bins; ++k)
            {
                const double re = work[(size_t) (2 * k)], im = work[(size_t) (2 * k + 1)];
                power[(size_t) k] += (re * re + im * im) / channels;
            }
        }
    }

    // 1 dans la zone choisie, fondu d'une demi-octave de part et d'autre.
    static float focusWeight (float f, const Params& p)
    {
        const float lo = std::min (p.focusLow, p.focusHigh), hi = std::max (p.focusLow, p.focusHigh);
        float w = 1.0f;
        if (lo > 20.5f && f < lo) w = std::clamp (1.0f + 2.0f * std::log2 (f / lo), 0.0f, 1.0f);
        if (hi < 19999.0f && f > hi) w = std::min (w, std::clamp (1.0f - 2.0f * std::log2 (f / hi), 0.0f, 1.0f));
        return w;
    }

    double rate = 44100.0, windowPower = 1.0;
    std::unique_ptr<juce::dsp::FFT> fft;
    int size = 1024, hop = 256, bins = 513, count = 0;
    float inverseScale = 1.0f;
    std::vector<float> window, work, value, reduction, sideLevel, mainLevel;
    std::vector<double> sidePower, mainPower, prefix;
    std::vector<int> holdLeft;
    std::array<std::vector<float>, 2> mainIn, mainOut, sideIn;
};

//==============================================================================
class DuckEngine
{
public:
    void prepare (double sampleRate)
    {
        rate = sampleRate;
        spectral.prepare (sampleRate);
        const int maxDelay = (int) std::ceil (maxLookaheadMs * 0.001 * sampleRate) + spectral.latency() + 8;
        for (auto& d : lookahead) d.prepare (maxDelay);
        for (auto& d : dry) d.prepare (maxDelay);
        for (auto& f : followers) f.prepare (sampleRate);
        historyStep = std::max (1, (int) (sampleRate / 100.0));
        filtersDirty = true;
        updateFilters();
        reset();
    }

    void reset()
    {
        for (auto& d : lookahead) d.reset();
        for (auto& d : dry) d.reset();
        for (auto& f : followers) f.reset();
        for (auto& s : shapers) s.reset();
        for (auto& c : mainSplit) { c.low.reset(); c.high.reset(); c.compensate.reset(); }
        for (auto& c : sideSplit) { c.low.reset(); c.high.reset(); }
        for (auto& pair : sideFilters) for (auto& f : pair) f.reset();
        spectral.reset();
        lastDivisionIndex = -1;
    }

    // Latence totale : anticipation + fenêtre FFT en mode spectral.
    static int latencyFor (const Params& p, double sampleRate, int spectralSize)
    {
        return (int) std::round (std::clamp (p.lookaheadMs, 0.0f, maxLookaheadMs) * 0.001 * sampleRate)
             + (p.mode == Mode::spectral ? spectralSize : 0);
    }

    int spectralSize() const { return spectral.latency(); }

    void setParams (const Params& p)
    {
        if (p.mode != params.mode)
        {
            for (auto& s : shapers) s.reset();
            spectral.reset();
        }
        params = p;
        updateFilters();
        for (auto& s : shapers)
            s.setTimes (p.attackMs * 0.001f, p.holdMs * 0.001f, p.releaseMs * 0.001f, p.curve, rate);
    }

    // Note MIDI (l'appelant découpe le bloc à l'instant de la note).
    void noteOn (float velocity)
    {
        if (params.trigger == Trigger::midi)
            for (auto& s : shapers)
                s.trigger (params.velocity ? velocity : 1.0f);
    }

    void process (float* const* main, int numMain, const float* const* side, int numSide,
                  int numSamples, const Transport& transport, bool bypassed = false) noexcept
    {
        const auto& p = params;
        const int ahead = (int) std::round (std::clamp (p.lookaheadMs, 0.0f, maxLookaheadMs) * 0.001 * rate);
        const int dryDelay = latencyFor (p, rate, spectral.latency());
        const float wet = bypassed ? 0.0f : p.mix, output = bypassed ? 1.0f : dbToGain (p.outputDb);
        const double beatsPerSample = transport.bpm / 60.0 / rate;
        const double division = divisionBeats[(size_t) std::clamp (p.division, 0, (int) divisionBeats.size() - 1)];

        if (p.trigger == Trigger::sync && transport.playing)
            beat = transport.ppq;

        numMain = std::min (numMain, 2);
        numSide = std::min (numSide, 2);

        for (int i = 0; i < numSamples; ++i)
        {
            float in[2] {}, delayed[2] {}, sc[2] {}, out[2] {};
            for (int ch = 0; ch < numMain; ++ch)
            {
                in[ch] = main[ch][i];
                lookahead[(size_t) ch].push (in[ch]);
                dry[(size_t) ch].push (in[ch]);
                delayed[ch] = lookahead[(size_t) ch].read (ahead);
            }
            for (int ch = 0; ch < numSide; ++ch)
                sc[ch] = side[ch][i];

            // Tempo : un déclenchement à chaque division.
            if (p.trigger == Trigger::sync)
            {
                const auto index = (long long) std::floor (beat / division + 1.0e-9);
                if (index != lastDivisionIndex)
                {
                    for (auto& s : shapers) s.trigger (1.0f);
                    lastDivisionIndex = index;
                }
                beat += beatsPerSample;
            }

            // Référence filtrée (pleine bande, écoute).
            float filtered = 0.0f;
            for (int ch = 0; ch < numSide; ++ch)
            {
                const float y = sideFilters[(size_t) ch][1].process (sideFilters[(size_t) ch][0].process (sc[ch]));
                filtered = std::abs (y) > std::abs (filtered) ? y : filtered;
            }

            const bool followSide = p.trigger == Trigger::sidechain;
            std::array<float, 3> reductionDb {};

            if (p.mode == Mode::fullBand)
            {
                const float level = followers[0].process (filtered);
                const float amount = finish (shapers[0].process (followSide ? toTarget (level) : 0.0f));
                reductionDb[0] = p.depthDb * amount;
                const float g = dbToGain (-reductionDb[0]);
                for (int ch = 0; ch < numMain; ++ch)
                    out[ch] = delayed[ch] * g;
            }
            else if (p.mode == Mode::threeBands)
            {
                // Niveau de chaque bande de la référence.
                std::array<float, 3> sideBand {};
                for (int ch = 0; ch < numSide; ++ch)
                {
                    float low, rest, mid, high;
                    sideSplit[(size_t) ch].low.split (sc[ch], low, rest);
                    sideSplit[(size_t) ch].high.split (rest, mid, high);
                    sideBand[0] = std::max (sideBand[0], std::abs (low));
                    sideBand[1] = std::max (sideBand[1], std::abs (mid));
                    sideBand[2] = std::max (sideBand[2], std::abs (high));
                }

                std::array<float, 3> gain {};
                const float common = followSide ? 0.0f : finish (shapers[0].process (0.0f));
                for (int b = 0; b < 3; ++b)
                {
                    const float level = followers[(size_t) b].process (sideBand[(size_t) b]);
                    const float amount = followSide ? finish (shapers[(size_t) b].process (toTarget (level))) : common;
                    reductionDb[(size_t) b] = p.depthDb * amount * p.bandAmount[(size_t) b];
                    gain[(size_t) b] = dbToGain (-reductionDb[(size_t) b]);
                }

                for (int ch = 0; ch < numMain; ++ch)
                {
                    auto& split = mainSplit[(size_t) ch];
                    float low, rest, mid, high;
                    split.low.split (delayed[ch], low, rest);
                    split.high.split (rest, mid, high);
                    low = split.compensate.allpass (low);
                    out[ch] = low * gain[0] + mid * gain[1] + high * gain[2];
                }
            }
            else
            {
                const float shaped = followSide ? -1.0f : shapers[0].process (0.0f);
                spectral.process (delayed, numMain, sc, numSide, out, p, shaped);
                if (spectral.frameReady)
                {
                    spectral.frameReady = false;
                    if (spectrumLock.tryEnter())
                    {
                        spectral.fillView (spectrum);
                        spectrumLock.exit();
                    }
                }
                reductionDb[0] = lastSpectralReduction (shaped);
            }

            for (int ch = 0; ch < numMain; ++ch)
            {
                const float d = dry[(size_t) ch].read (dryDelay);
                float y = (d + (out[ch] - d) * wet) * output;
                if (p.listen && ! bypassed)
                    y = filtered;
                main[ch][i] = y;
            }

            // Historique pour l'affichage.
            pending.main = std::max (pending.main, std::max (std::abs (in[0]), std::abs (in[1])));
            pending.side = std::max (pending.side, std::abs (filtered));
            for (int b = 0; b < 3; ++b)
                pending.reductionDb[(size_t) b] = std::max (pending.reductionDb[(size_t) b], reductionDb[(size_t) b]);
            if (++historyCount >= historyStep)
            {
                pushHistory (pending);
                pending = {};
                historyCount = 0;
            }
        }
    }

    //--------------------------------------------------------------------------
    // Affichage (fil de l'interface)
    int readHistory (HistoryPoint* dest, int maxPoints)
    {
        const auto scope = historyFifo.read (std::min (maxPoints, historyFifo.getNumReady()));
        int n = 0;
        for (int i = 0; i < scope.blockSize1; ++i) dest[n++] = history[(size_t) (scope.startIndex1 + i)];
        for (int i = 0; i < scope.blockSize2; ++i) dest[n++] = history[(size_t) (scope.startIndex2 + i)];
        return n;
    }

    void readSpectrum (SpectrumView& dest)
    {
        const juce::SpinLock::ScopedLockType lock (spectrumLock);
        dest = spectrum;
    }

    const Params& currentParams() const { return params; }

private:
    float toTarget (float level) const noexcept
    {
        return std::clamp ((gainToDb (level) - params.thresholdDb) / std::max (0.1f, params.rangeDb), 0.0f, 1.0f);
    }

    float finish (float amount) const noexcept { return params.invert ? 1.0f - amount : amount; }

    float lastSpectralReduction (float shaped) const noexcept
    {
        if (shaped >= 0.0f) return params.depthDb * finish (shaped);
        float m = 0.0f;
        for (auto r : spectrum.reductionDb) m = std::max (m, r);
        return m;
    }

    void updateFilters()
    {
        const auto& p = params;
        if (filtersDirty || ! juce::exactlyEqual (p.scLowCut, cached.scLowCut) || ! juce::exactlyEqual (p.scHighCut, cached.scHighCut))
            for (auto& pair : sideFilters)
            {
                pair[0].set (true, p.scLowCut, 0.7071, rate);
                pair[1].set (false, p.scHighCut, 0.7071, rate);
            }

        if (filtersDirty || ! juce::exactlyEqual (p.crossoverLow, cached.crossoverLow) || ! juce::exactlyEqual (p.crossoverHigh, cached.crossoverHigh))
        {
            const double low = std::min (p.crossoverLow, p.crossoverHigh * 0.8f);
            const double high = std::max (p.crossoverHigh, p.crossoverLow * 1.25f);
            for (auto& c : mainSplit) { c.low.set (low, rate); c.high.set (high, rate); c.compensate.set (high, rate); }
            for (auto& c : sideSplit) { c.low.set (low, rate); c.high.set (high, rate); }
        }
        cached = p;
        filtersDirty = false;
    }

    void pushHistory (const HistoryPoint& point)
    {
        const auto scope = historyFifo.write (1);
        if (scope.blockSize1 > 0) history[(size_t) scope.startIndex1] = point;
        else if (scope.blockSize2 > 0) history[(size_t) scope.startIndex2] = point;
    }

    struct MainSplit { Crossover low, high, compensate; };
    struct SideSplit { Crossover low, high; };

    double rate = 44100.0;
    Params params, cached;
    bool filtersDirty = true;
    std::array<DelayLine, 2> lookahead, dry;
    std::array<LevelFollower, 3> followers;
    std::array<Shaper, 3> shapers;
    std::array<MainSplit, 2> mainSplit;
    std::array<SideSplit, 2> sideSplit;
    std::array<std::array<Biquad, 2>, 2> sideFilters;
    SpectralDucker spectral;

    double beat = 0.0;
    long long lastDivisionIndex = -1;

    static constexpr int historySize = 2048;
    std::array<HistoryPoint, historySize> history;
    juce::AbstractFifo historyFifo { historySize };
    HistoryPoint pending;
    int historyCount = 0, historyStep = 441;

    SpectrumView spectrum;
    juce::SpinLock spectrumLock;
};

} // namespace duckhunt
