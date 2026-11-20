// Chop Chop : hache une voix au tempo, façon « repeater » distordu.
//
// Le moteur écoute la voix en continu (4 s de mémoire) et, quand un pas le
// demande, rejoue le dernier bout capturé : répété, en « roll » qui se resserre,
// à l'envers, une octave au-dessus ou en dessous (par la vitesse, comme un
// sampler), ou coupé. Le début du bout est calé sur le début d'une syllabe.
//
// Ce qui déclenche les bouts :
// - PATTERN : un séquenceur de 16 pas calé sur le tempo du DAW ;
// - DRUMS : la piste de drums, sur l'entrée sidechain ; chaque coup joue le pas
//   suivant du séquenceur pendant une durée réglable ;
// - MIDI : chaque note rejoue le bout, à la hauteur de la note (do 3 = original).
//
// Puis : saturation / fuzz / bitcrush (plus fort sur chaque coup de drums),
// filtre modulé par les drums, delay au tempo, reverb, pompage sur le kick.
// Sans latence. Ne dépend que de juce_audio_basics : testé hors DAW.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <random>
#include <vector>

namespace chopchop
{

enum class Trigger { pattern, drums, midi };
enum class Step { thru, repeat, roll, reverse, up, down, mute };
enum class Dirt { warm, fuzz, crush };
enum class FilterType { off, lowpass, highpass, bandpass };
enum class DrumBand { kick, snare, all };

inline constexpr int numSteps = 16;
inline constexpr int numStepTypes = 7;
inline const char* const stepNames[] { "THRU", "REPEAT", "ROLL", "REVERSE", "+12", "-12", "MUTE" };

// Durées en temps (noires).
inline constexpr std::array<double, 3> stepRateBeats { 0.5, 0.25, 0.125 };
inline const char* const stepRateNames[] { "1/8", "1/16", "1/32" };
inline constexpr std::array<double, 5> sizeBeats { 1.0, 0.5, 0.25, 0.125, 0.0625 };
inline const char* const sizeNames[] { "1/4", "1/8", "1/16", "1/32", "1/64" };
inline constexpr std::array<double, 5> hitBeats { 0.125, 0.25, 0.5, 1.0, 2.0 };
inline const char* const hitNames[] { "1/32", "1/16", "1/8", "1/4", "1/2" };
inline constexpr std::array<double, 6> delayBeats { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
inline const char* const delayNames[] { "1/16", "1/8", "1/8.", "1/4", "1/4.", "1/2" };

// Motif écrit en lettres : - passe, R répète, L roll, < envers, U +12, D -12, X coupe.
inline std::array<Step, numSteps> patternFromString (const char* text)
{
    std::array<Step, numSteps> steps {};
    for (int i = 0; i < numSteps && text[i] != 0; ++i)
    {
        switch (text[i])
        {
            case 'R': steps[(size_t) i] = Step::repeat; break;
            case 'L': steps[(size_t) i] = Step::roll; break;
            case '<': steps[(size_t) i] = Step::reverse; break;
            case 'U': steps[(size_t) i] = Step::up; break;
            case 'D': steps[(size_t) i] = Step::down; break;
            case 'X': steps[(size_t) i] = Step::mute; break;
            default:  steps[(size_t) i] = Step::thru; break;
        }
    }
    return steps;
}

struct Params
{
    Trigger trigger = Trigger::pattern;
    int stepRate = 1;                   // index dans stepRateBeats
    std::array<Step, numSteps> pattern {};

    int size = 2;                       // longueur du bout répété (index dans sizeBeats)
    float rise = 0.0f;                  // demi-tons ajoutés à chaque répétition
    float decay = 1.0f;                 // volume gardé à chaque répétition (1 = constant)
    float chance = 1.0f;                // probabilité qu'un pas se déclenche
    float pitch = 0.0f;                 // demi-tons, pour tous les bouts
    bool snap = true;                   // caler le bout sur le début d'une syllabe

    DrumBand band = DrumBand::kick;     // ce qui déclenche en mode DRUMS
    float sensitivityDb = -30.0f;       // seuil de détection des coups
    int hitLength = 2;                  // durée d'un bout déclenché par un coup

    Dirt dirt = Dirt::warm;
    float driveDb = 0.0f;               // 0 = pas de distorsion
    float drumDriveDb = 0.0f;           // distorsion en plus sur les coups de drums

    FilterType filter = FilterType::off;
    float cutoff = 1000.0f;
    float resonance = 0.7f;
    float filterMod = 0.0f;             // -1..1 : les drums ferment ou ouvrent le filtre (±4 octaves)

    float pumpDb = 0.0f;                // pompage sur le kick
    float pumpReleaseMs = 150.0f;

    int delayDivision = 2;
    float feedback = 0.35f;
    float echo = 0.0f;                  // niveau du delay
    float reverbSize = 0.5f;
    float reverb = 0.0f;                // niveau de la reverb

    float mix = 1.0f;
    float outputDb = 0.0f;
};

struct Transport
{
    double bpm = 120.0;
    double ppq = 0.0;
    bool playing = false;
};

//==============================================================================
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

// Filtre d'état variable (topologie TPT) : stable même modulé à chaque échantillon.
struct Svf
{
    double ic1 = 0.0, ic2 = 0.0;

    struct Coefficients { double a1 = 1.0, a2 = 0.0, a3 = 0.0, k = 1.4; };

    static Coefficients make (double freq, double q, double rate)
    {
        Coefficients c;
        const double g = std::tan (juce::MathConstants<double>::pi * std::clamp (freq, 20.0, rate * 0.45) / rate);
        c.k = 1.0 / std::max (0.3, q);
        c.a1 = 1.0 / (1.0 + g * (g + c.k));
        c.a2 = g * c.a1;
        c.a3 = g * c.a2;
        return c;
    }

    float process (float x, const Coefficients& c, FilterType type) noexcept
    {
        const double v3 = x - ic2;
        const double v1 = c.a1 * ic1 + c.a2 * v3;
        const double v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        switch (type)
        {
            case FilterType::lowpass:  return (float) v2;
            case FilterType::highpass: return (float) (x - c.k * v1 - v2);
            case FilterType::bandpass: return (float) (v1 * c.k);
            case FilterType::off:      break;
        }
        return x;
    }

    void reset() { ic1 = ic2 = 0.0; }
};

// Détection d'attaques : une enveloppe rapide qui passe nettement au-dessus d'une lente.
// Les deux suivent les crêtes ; la lente retombe moins vite, si bien que la traîne
// d'un coup n'est jamais prise pour un nouveau coup.
struct OnsetDetector
{
    float fast = 0.0f, slow = 0.0f, fastUp = 0.0f, fastDown = 0.0f, slowUp = 0.0f, slowDown = 0.0f;
    int refractory = 0, refractorySamples = 0;

    void prepare (double rate, double refractorySeconds)
    {
        fastUp = (float) std::exp (-1.0 / (0.0005 * rate));
        fastDown = (float) std::exp (-1.0 / (0.020 * rate));
        slowUp = (float) std::exp (-1.0 / (0.010 * rate));
        slowDown = (float) std::exp (-1.0 / (0.150 * rate));
        refractorySamples = (int) (refractorySeconds * rate);
    }

    bool process (float x, float threshold) noexcept
    {
        const float a = std::abs (x);
        fast = a > fast ? a + (fast - a) * fastUp : a + (fast - a) * fastDown;
        slow = a > slow ? a + (slow - a) * slowUp : a + (slow - a) * slowDown;
        if (refractory > 0) { --refractory; return false; }
        if (fast > threshold && fast > 2.0f * slow + 1.0e-5f)
        {
            refractory = refractorySamples;
            return true;
        }
        return false;
    }

    void reset() { fast = slow = 0.0f; refractory = 0; }
};

// Mémoire de la voix : un anneau par canal.
struct History
{
    std::array<std::vector<float>, 2> data;
    long long written = 0;
    int mask = 0;

    void prepare (int minSize)
    {
        const int size = juce::nextPowerOfTwo (minSize);
        for (auto& d : data) d.assign ((size_t) size, 0.0f);
        mask = size - 1;
        written = 0;
    }

    void push (float l, float r) noexcept
    {
        data[0][(size_t) (written & mask)] = l;
        data[1][(size_t) (written & mask)] = r;
        ++written;
    }

    float at (int ch, long long index) const noexcept { return data[(size_t) ch][(size_t) (index & mask)]; }
    int capacity() const { return mask + 1; }
};

// Lecture d'un bout capturé : boucle, vitesse, sens, fondus aux raccords.
struct SlicePlayer
{
    std::array<std::vector<float>, 2> data;
    int length = 0, loopLength = 0, fade = 64;
    double position = 0.0, rate = 1.0, riseFactor = 1.0, riseMultiplier = 1.0;
    float gain = 1.0f, decay = 1.0f;
    bool reverse = false, roll = false;
    int repeats = 0;

    void prepare (int maxLength, double sampleRate)
    {
        for (auto& d : data) d.assign ((size_t) maxLength + 2, 0.0f);
        fade = std::max (16, (int) (0.0015 * sampleRate));
    }

    void capture (const History& history, long long start, int n)
    {
        length = std::clamp (n, 32, (int) data[0].size() - 2);
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < length; ++i)
                data[(size_t) ch][(size_t) i] = history.at (ch, start + i);
            data[(size_t) ch][(size_t) length] = data[(size_t) ch][(size_t) length - 1];
        }
        loopLength = length;
        position = 0.0;
        repeats = 0;
        riseMultiplier = 1.0;
        gain = 1.0f;
    }

    void read (float& left, float& right) noexcept
    {
        const double at = reverse ? (double) loopLength - 1.0 - position : position;
        const int i = std::clamp ((int) at, 0, length - 1);
        const float frac = (float) (at - i);
        const float edge = (float) std::min ({ 1.0, position / fade, ((double) loopLength - position) / fade });
        const float g = gain * std::max (0.0f, edge);
        left = (data[0][(size_t) i] + (data[0][(size_t) i + 1] - data[0][(size_t) i]) * frac) * g;
        right = (data[1][(size_t) i] + (data[1][(size_t) i + 1] - data[1][(size_t) i]) * frac) * g;

        position += rate * riseMultiplier;
        if (position >= loopLength)
        {
            position = std::fmod (position - loopLength, (double) std::max (1, loopLength));
            ++repeats;
            riseMultiplier *= riseFactor;
            riseMultiplier = std::clamp (riseMultiplier, 0.125, 8.0);
            gain *= decay;
            if (roll)
                loopLength = std::max (fade * 4, loopLength / 2);
        }
    }
};

struct DelayLine
{
    std::vector<float> data;
    int write = 0, mask = 0;

    void prepare (int maxDelay)
    {
        data.assign ((size_t) juce::nextPowerOfTwo (maxDelay + 4), 0.0f);
        mask = (int) data.size() - 1;
        write = 0;
    }

    void push (float x) noexcept { data[(size_t) write] = x; write = (write + 1) & mask; }

    float read (double delay) const noexcept
    {
        const double at = (double) write - 1.0 - delay;
        const int i = (int) std::floor (at);
        const float frac = (float) (at - i);
        const float a = data[(size_t) (i & mask)], b = data[(size_t) ((i + 1) & mask)];
        return a + (b - a) * frac;
    }

    void reset() { std::fill (data.begin(), data.end(), 0.0f); }
};

//==============================================================================
struct HistoryPoint
{
    float input = 0.0f, output = 0.0f, drums = 0.0f;
    int state = 0;          // type du pas joué (Step), -1 si rien
    bool hit = false;       // coup de drums détecté
};

class ChopEngine
{
public:
    void prepare (double sampleRate)
    {
        rate = sampleRate;
        history.prepare ((int) (4.0 * sampleRate));
        slice.prepare ((int) (2.0 * sampleRate), sampleRate);
        vocalOnsets.prepare (sampleRate, 0.06);
        drumOnsets.prepare (sampleRate, 0.05);
        for (auto& d : delays) d.prepare ((int) (2.6 * sampleRate));
        reverb.setSampleRate (sampleRate);
        drumFilters[0].set (false, 150.0, 0.7071, sampleRate);       // kick
        drumFilters[1].set (true, 1500.0, 0.7071, sampleRate);       // caisse claire, charleston
        kickFilter.set (false, 150.0, 0.7071, sampleRate);
        envUp = (float) std::exp (-1.0 / (0.002 * sampleRate));
        envDown = (float) std::exp (-1.0 / (0.120 * sampleRate));
        ramp = 1.0f / (float) std::max (1.0, 0.003 * sampleRate);
        historyStep = std::max (1, (int) (sampleRate / 100.0));
        reset();
    }

    void reset()
    {
        for (auto& f : filters) f.reset();
        for (auto& d : delays) d.reset();
        for (auto& f : delayDamp) f = 0.0f;
        for (auto& d : dcIn) d = 0.0f;
        for (auto& d : dcOut) d = 0.0f;
        for (auto& h : crushHold) h = 0.0f;
        vocalOnsets.reset();
        drumOnsets.reset();
        drumFilters[0].reset();
        drumFilters[1].reset();
        kickFilter.reset();
        reverb.reset();
        play = Play::thru;
        sliceMix = 0.0f;
        muteGain = 1.0f;
        drumEnv = kickEnv = pumpEnv = 0.0f;
        lastStepIndex = -1;
        hitRemaining = 0;
        notesHeld = 0;
        numOnsets = 0;
        crushCounter = 0;
    }

    void setParams (const Params& p)
    {
        params = p;
        juce::Reverb::Parameters r;
        r.roomSize = std::clamp (p.reverbSize, 0.0f, 1.0f);
        r.damping = 0.45f;
        r.wetLevel = 0.33f;
        r.dryLevel = 0.0f;
        r.width = 1.0f;
        reverb.setParameters (r);
        slice.decay = std::clamp (p.decay, 0.0f, 1.0f);
        slice.riseFactor = std::pow (2.0, p.rise / 12.0);
        pumpCoef = (float) std::exp (-1.0 / (std::max (5.0f, p.pumpReleaseMs) * 0.001 * rate));
    }

    // MIDI (l'appelant découpe le bloc à l'instant de la note).
    void noteOn (int note, float velocity)
    {
        if (params.trigger != Trigger::midi) return;
        midiSemitones = (float) (note - 60);
        if (notesHeld++ == 0 || play != Play::slice)
            startSlice();
        slice.gain = std::clamp (velocity, 0.05f, 1.0f);
        updateRate (Step::repeat);
    }

    void noteOff (int)
    {
        if (params.trigger != Trigger::midi) return;
        notesHeld = std::max (0, notesHeld - 1);
        if (notesHeld == 0) play = Play::thru;
    }

    void process (float* const* main, int numMain, const float* const* side, int numSide,
                  int numSamples, const Transport& transport) noexcept
    {
        const auto& p = params;
        numMain = std::min (numMain, 2);
        numSide = std::min (numSide, 2);
        const double samplesPerBeat = rate * 60.0 / std::max (20.0, transport.bpm);
        const double beatsPerSample = 1.0 / samplesPerBeat;
        const double stepBeats = stepRateBeats[(size_t) std::clamp (p.stepRate, 0, 2)];
        const double delaySamples = std::min (2.5 * rate, delayBeats[(size_t) std::clamp (p.delayDivision, 0, 5)] * samplesPerBeat);
        const float sensitivity = dbToGain (p.sensitivityDb);
        const float output = dbToGain (p.outputDb);

        currentSamplesPerBeat = samplesPerBeat;
        if (transport.playing)
            beat = transport.ppq;

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = numMain > 0 ? main[0][i] : 0.0f;
            const float inR = numMain > 1 ? main[1][i] : inL;
            history.push (inL, inR);

            // Débuts de syllabes de la voix.
            if (vocalOnsets.process (0.5f * (inL + inR), 0.006f))
            {
                onsets[(size_t) (numOnsets % (int) onsets.size())] = history.written - 1;
                ++numOnsets;
            }

            // Drums : bande choisie pour les coups, enveloppe pour la saleté et le filtre, kick pour le pompage.
            float drum = 0.0f;
            for (int ch = 0; ch < numSide; ++ch)
                drum = std::abs (side[ch][i]) > std::abs (drum) ? side[ch][i] : drum;
            const float kick = kickFilter.process (drum);
            const float detect = p.band == DrumBand::kick ? drumFilters[0].process (drum)
                               : (p.band == DrumBand::snare ? drumFilters[1].process (drum) : drum);
            if (p.band != DrumBand::kick) drumFilters[0].process (drum);
            if (p.band != DrumBand::snare) drumFilters[1].process (drum);
            const bool hit = numSide > 0 && drumOnsets.process (detect, sensitivity);

            const float a = std::abs (drum);
            drumEnv = a > drumEnv ? a + (drumEnv - a) * envUp : a + (drumEnv - a) * envDown;
            const float drumAmount = std::clamp ((gainToDb (drumEnv) - p.sensitivityDb) / 24.0f, 0.0f, 1.0f);
            kickEnv = std::max (std::abs (kick), kickEnv * pumpCoef);

            // Déclenchements.
            if (p.trigger == Trigger::pattern)
            {
                const auto index = (int) (((long long) std::floor (beat / stepBeats + 1.0e-9) % numSteps + numSteps) % numSteps);
                if (index != lastStepIndex)
                {
                    lastStepIndex = index;
                    currentStep = index;
                    applyStep (p.pattern[(size_t) index], false);
                }
            }
            else if (p.trigger == Trigger::drums)
            {
                if (hit)
                {
                    const auto step = nextActiveStep();
                    applyStep (step, true);
                    hitRemaining = (int) (hitBeats[(size_t) std::clamp (p.hitLength, 0, 4)] * samplesPerBeat);
                }
                else if (hitRemaining > 0 && --hitRemaining == 0)
                    applyStep (Step::thru, false);
            }
            beat += beatsPerSample;

            // Repeater : voix directe ou bout rejoué, avec un fondu de 3 ms entre les deux.
            float sliceL = 0.0f, sliceR = 0.0f;
            if (play == Play::slice || sliceMix > 0.0f)
                slice.read (sliceL, sliceR);
            sliceMix = play == Play::slice ? std::min (1.0f, sliceMix + ramp) : std::max (0.0f, sliceMix - ramp);
            muteGain = play == Play::mute ? std::max (0.0f, muteGain - ramp) : std::min (1.0f, muteGain + ramp);
            float l = (inL + (sliceL - inL) * sliceMix) * muteGain;
            float r = (inR + (sliceR - inR) * sliceMix) * muteGain;

            // Saleté.
            const float drive = p.driveDb + p.drumDriveDb * drumAmount;
            if (drive > 0.01f)
            {
                l = distort (l, 0, drive, p.dirt);
                r = distort (r, 1, drive, p.dirt);
            }
            if (p.dirt == Dirt::crush && drive > 0.01f)
                ++crushCounter;

            // Filtre.
            if (p.filter != FilterType::off)
            {
                const double freq = p.cutoff * std::pow (2.0, 4.0 * p.filterMod * drumAmount);
                const auto c = Svf::make (freq, p.resonance, rate);
                l = filters[0].process (l, c, p.filter);
                r = filters[1].process (r, c, p.filter);
            }

            // Delay au tempo (aller-retour gauche / droite, échos assombris) et reverb.
            if (p.echo > 0.0f || p.feedback > 0.0f)
            {
                // Lu avant l'écriture de l'échantillon courant : décalage d'un échantillon en moins.
                const float dl = delays[0].read (delaySamples - 1.0), dr = delays[1].read (delaySamples - 1.0);
                delayDamp[0] += (dr - delayDamp[0]) * 0.35f;
                delayDamp[1] += (dl - delayDamp[1]) * 0.35f;
                delays[0].push (l + std::tanh (delayDamp[0] * p.feedback));
                delays[1].push (r + std::tanh (delayDamp[1] * p.feedback));
                l += dl * p.echo;
                r += dr * p.echo;
            }
            if (p.reverb > 0.0f)
            {
                float wl = l, wr = r;
                reverb.processStereo (&wl, &wr, 1);
                l += wl * p.reverb;
                r += wr * p.reverb;
            }

            // Pompage sur le kick.
            if (p.pumpDb > 0.0f)
            {
                const float amount = std::clamp ((gainToDb (kickEnv) - p.sensitivityDb) / 12.0f, 0.0f, 1.0f);
                const float g = dbToGain (-p.pumpDb * amount);
                l *= g;
                r *= g;
            }

            const float outL = (inL + (l - inL) * p.mix) * output;
            const float outR = (inR + (r - inR) * p.mix) * output;
            if (numMain > 0) main[0][i] = outL;
            if (numMain > 1) main[1][i] = outR;

            // Historique pour l'affichage.
            pending.input = std::max (pending.input, std::max (std::abs (inL), std::abs (inR)));
            pending.output = std::max (pending.output, std::max (std::abs (outL), std::abs (outR)));
            pending.drums = std::max (pending.drums, a);
            pending.hit = pending.hit || hit;
            if (play == Play::slice) pending.state = (int) playingStep;
            else if (play == Play::mute) pending.state = (int) Step::mute;
            if (++historyCount >= historyStep)
            {
                pushHistory (pending);
                pending = {};
                pending.state = -1;
                historyCount = 0;
            }
        }
    }

    // Affichage (fil de l'interface).
    int readHistory (HistoryPoint* dest, int maxPoints)
    {
        const auto scope = historyFifo.read (std::min (maxPoints, historyFifo.getNumReady()));
        int n = 0;
        for (int i = 0; i < scope.blockSize1; ++i) dest[n++] = historyRing[(size_t) (scope.startIndex1 + i)];
        for (int i = 0; i < scope.blockSize2; ++i) dest[n++] = historyRing[(size_t) (scope.startIndex2 + i)];
        return n;
    }

    std::atomic<int> currentStep { -1 };

    const Params& currentParams() const { return params; }

private:
    enum class Play { thru, slice, mute };

    void applyStep (Step step, bool fresh)
    {
        if (step == Step::thru) { play = Play::thru; return; }
        if (step == Step::mute) { play = Play::mute; return; }

        // Une suite de pas actifs garde le même bout ; un nouveau bout après une pause.
        if (fresh || play != Play::slice)
        {
            if (params.chance < 1.0f && random (generator) > params.chance)
            {
                play = Play::thru;
                return;
            }
            startSlice();
        }
        updateRate (step);
    }

    void startSlice()
    {
        const int length = std::clamp ((int) (sizeBeats[(size_t) std::clamp (params.size, 0, 4)] * currentSamplesPerBeat), 64,
                                       std::min ((int) (2.0 * rate), history.capacity() / 2));
        const long long now = history.written;
        long long start = now - length;

        // Caler le début sur la dernière syllabe qui laisse un bout complet.
        if (params.snap)
        {
            const long long earliest = now - std::min ((long long) (1.5 * rate), (long long) history.capacity() / 2 - length);
            long long best = -1;
            for (int k = 0; k < std::min (numOnsets, (int) onsets.size()); ++k)
            {
                const long long o = onsets[(size_t) ((numOnsets - 1 - k) % (int) onsets.size())];
                if (o <= start && o >= earliest) { best = o; break; }
            }
            if (best >= 0) start = best;
        }

        // Le nouveau bout démarre par un fondu court : pas de clic, même enchaîné à un autre.
        slice.capture (history, start, length);
        play = Play::slice;
    }

    void updateRate (Step step)
    {
        slice.reverse = step == Step::reverse;
        const bool wasRoll = slice.roll;
        slice.roll = step == Step::roll;
        if (slice.roll && ! wasRoll)
            slice.loopLength = std::max (slice.fade * 4, slice.loopLength);
        if (! slice.roll)
            slice.loopLength = slice.length;
        const double octave = step == Step::up ? 2.0 : (step == Step::down ? 0.5 : 1.0);
        const float semitones = params.pitch + (params.trigger == Trigger::midi ? midiSemitones : 0.0f);
        slice.rate = octave * std::pow (2.0, semitones / 12.0);
        playingStep = step;
    }

    // Mode DRUMS : le pas actif suivant du séquenceur (répéter si le motif est vide).
    Step nextActiveStep()
    {
        for (int k = 0; k < numSteps; ++k)
        {
            const auto s = params.pattern[(size_t) drumPointer];
            currentStep = drumPointer;
            drumPointer = (drumPointer + 1) % numSteps;
            if (s != Step::thru) return s;
        }
        return Step::repeat;
    }

    float distort (float x, int ch, float driveDb, Dirt type) noexcept
    {
        const float g = dbToGain (driveDb);
        const float makeup = 1.0f / std::sqrt (g);
        float y;
        switch (type)
        {
            case Dirt::fuzz:
            {
                // Écrêtage asymétrique (fuzz), puis on retire le continu qu'il crée.
                const float v = g * x;
                y = v >= 0.0f ? 1.0f - std::exp (-v) : -0.7f * (1.0f - std::exp (1.6f * v));
                const float blocked = y - dcIn[(size_t) ch] + 0.995f * dcOut[(size_t) ch];
                dcIn[(size_t) ch] = y;
                dcOut[(size_t) ch] = blocked;
                y = blocked * 1.3f;
                break;
            }
            case Dirt::crush:
            {
                // Résolution et fréquence d'échantillonnage réduites selon la saleté.
                const float amount = std::clamp (driveDb / 40.0f, 0.0f, 1.0f);
                const int hold = 1 + (int) (amount * 15.0f);
                if (crushCounter % hold == 0) crushHold[(size_t) ch] = x;
                const float levels = std::pow (2.0f, 15.0f - amount * 11.0f);
                y = std::round (std::tanh (crushHold[(size_t) ch] * std::sqrt (g)) * levels) / levels;
                return y;
            }
            case Dirt::warm:
            default:
                y = std::tanh (g * x);
                break;
        }
        return y * makeup;
    }

    void pushHistory (const HistoryPoint& point)
    {
        const auto scope = historyFifo.write (1);
        if (scope.blockSize1 > 0) historyRing[(size_t) scope.startIndex1] = point;
        else if (scope.blockSize2 > 0) historyRing[(size_t) scope.startIndex2] = point;
    }

    double rate = 44100.0, beat = 0.0, currentSamplesPerBeat = 22050.0;
    Params params;
    History history;
    SlicePlayer slice;
    OnsetDetector vocalOnsets, drumOnsets;
    std::array<long long, 64> onsets {};
    int numOnsets = 0;

    Play play = Play::thru;
    Step playingStep = Step::thru;
    float sliceMix = 0.0f, muteGain = 1.0f, ramp = 0.01f;
    int lastStepIndex = -1, drumPointer = 0, hitRemaining = 0, notesHeld = 0;
    float midiSemitones = 0.0f;

    std::array<Biquad, 2> drumFilters;
    Biquad kickFilter;
    float drumEnv = 0.0f, kickEnv = 0.0f, pumpEnv = 0.0f, envUp = 0.0f, envDown = 0.0f, pumpCoef = 0.999f;

    std::array<Svf, 2> filters;
    std::array<DelayLine, 2> delays;
    std::array<float, 2> delayDamp {}, dcIn {}, dcOut {}, crushHold {};
    long long crushCounter = 0;
    juce::Reverb reverb;

    std::mt19937 generator { 1234 };
    std::uniform_real_distribution<float> random { 0.0f, 1.0f };

    static constexpr int historySize = 2048;
    std::array<HistoryPoint, historySize> historyRing;
    juce::AbstractFifo historyFifo { historySize };
    HistoryPoint pending;
    int historyCount = 0, historyStep = 441;

};

} // namespace chopchop
