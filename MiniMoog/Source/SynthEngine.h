#pragma once

// Moteur sonore de Model D, inspiré du Minimoog Model D (monophonique, ou 8 voix en option).
// Indépendant du format plugin : il peut tourner tel quel dans un VST3/AU,
// dans le programme de test, ou plus tard sur un Raspberry Pi.

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

namespace modeld
{

//==============================================================================
// Réglages de la façade. Les unités suivent l'instrument : la plupart des
// potards vont de 0 à 10, les temps sont en secondes, la coupure en Hz.
struct Params
{
    // Controllers
    float tune = 0.0f;          // demi-tons
    float glide = 0.0f;         // secondes
    float modMix = 0.0f;        // 0 = osc 3, 10 = bruit
    bool oscMod = false;        // modulation -> hauteur des osc 1 et 2
    bool osc3Ctrl = true;       // osc 3 suit le clavier

    // Oscillator bank : range 0 = LO, 1 = 32', 2 = 16', 3 = 8', 4 = 4', 5 = 2'
    std::array<int, 3> range { 3, 3, 3 };
    std::array<int, 3> wave { 2, 2, 0 };
    float osc2Freq = 0.1f;      // demi-tons, -7..+7
    float osc3Freq = -0.1f;

    // Mixer
    std::array<bool, 3> oscOn { true, true, false };
    std::array<float, 3> oscLevel { 8.0f, 8.0f, 5.0f };
    bool noiseOn = false;
    float noiseLevel = 0.0f;
    bool pinkNoise = false;

    // Modifiers
    float cutoff = 1200.0f;     // Hz
    float emphasis = 2.0f;      // 0..10, auto-oscillation vers 10
    float contour = 5.0f;       // 0..10, profondeur de l'enveloppe de filtre
    bool filterMod = false;     // modulation -> coupure
    bool kbd1 = false;          // suivi de clavier 1/3
    bool kbd2 = false;          // suivi de clavier 2/3
    bool bassComp = false;      // compense la perte de graves due à la résonance (absent de l'original)
    float filterAttack = 0.005f, filterDecay = 0.4f, filterSustain = 3.0f;
    float loudAttack = 0.002f, loudDecay = 0.3f, loudSustain = 8.0f;
    bool decaySwitch = false;   // relâchement = decay si activé, sinon très court

    // Output
    float volume = 7.0f;
    float bendRange = 2.0f;     // demi-tons
    float drift = 3.0f;         // instabilité analogique des oscillateurs

    //--------------------------------------------------------------------------
    // Options supplémentaires, absentes du Minimoog d'origine. À leurs valeurs par
    // défaut, le son est strictement identique à celui du Model D émulé.

    // Jeu
    bool poly = false;          // 8 voix au lieu d'une
    bool retrigger = false;     // relance les enveloppes à chaque note (sinon legato)
    float aftertouch = 0.0f;    // 0..10, pression -> coupure
    float velocity = 0.0f;      // 0..10, vélocité -> profondeur du contour de filtre
    float modAmount = 0.0f;     // 0..10, modulation permanente en plus de la molette
    bool modFromLfo = false;    // source de modulation : LFO au lieu d'osc 3 / bruit

    // Oscillateurs
    float osc1Freq = 0.0f;      // demi-tons, -7..+7
    float osc2Fine = 0.0f;      // demi-tons, -0.5..+0.5
    float osc3Fine = 0.0f;
    std::array<bool, 3> rectify { false, false, false };
    std::array<float, 3> offset { 0.0f, 0.0f, 0.0f };   // -1..1, agit en mode rectify
    bool oscSync = false;       // osc 2 resynchronisé par osc 1

    // LFO
    float lfoLevel = 0.0f;      // 0..10
    int lfoWave = 0;            // triangle, carré, dent montante, dent descendante, S&H
    float lfoRate = 2.0f;       // Hz, quand il n'est pas synchronisé
    bool lfoSync = false;       // calé sur le tempo et le début de mesure
    int lfoDivision = 4;        // index dans divisionBeats()
    int lfoDest = 0;            // toutes les osc, osc 2, filtre, volume

    // Filtre et contours
    float vco3Filter = 0.0f;    // 0..10, osc 3 -> coupure en permanence
    bool adsr = false;          // relâchement réglable au lieu de l'interrupteur Decay
    float filterRelease = 0.3f;
    float loudRelease = 0.3f;
};

//==============================================================================
// Valeurs de note pour tout ce qui se cale sur le tempo (LFO, delay, arpège).
inline const juce::StringArray& divisionNames()
{
    static const juce::StringArray names { "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                                           "1/4T", "1/8T", "1/16T" };
    return names;
}

// Durée en temps (noires).
inline double divisionBeats (int index) noexcept
{
    static constexpr double beats[] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                        2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
    return beats[juce::jlimit (0, 10, index)];
}

//==============================================================================
namespace detail
{
    inline float fastTanh (float x) noexcept
    {
        if (x > 3.0f)  return 1.0f;
        if (x < -3.0f) return -1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float noteToHz (float note) noexcept
    {
        return 440.0f * std::exp2 ((note - 69.0f) / 12.0f);
    }

    // Coefficient d'un filtre RC à un pôle. ratio fixe à quel point de la
    // courbe exponentielle on considère que la durée est écoulée.
    inline float rcCoef (float seconds, float rate, float ratio) noexcept
    {
        return 1.0f - std::exp (-ratio / (std::max (seconds, 1.0e-4f) * rate));
    }

    inline float rangeOctaves (int range) noexcept
    {
        static constexpr float octaves[] = { -7.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f };
        return octaves[juce::jlimit (0, 5, range)];
    }

    inline float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    //==========================================================================
    // Oscillateur libre (jamais remis en phase, comme sur l'original), avec
    // anti-repliement PolyBLEP sur les fronts raides.
    struct Oscillator
    {
        float phase = 0.0f;
        bool wrapped = false;   // vrai si le cycle vient de recommencer (pour la synchro)

        float process (float inc, int wave, bool isOsc3) noexcept
        {
            const float p = phase;
            float out;

            switch (wave)
            {
                case 0:  out = triangle (p); break;
                // Sur le Model D, la forme tri-saw est un mélange des sorties
                // triangle et dent de scie au niveau du sélecteur.
                case 1:  out = isOsc3 ? -saw (p, inc) : 0.5f * (triangle (p) + saw (p, inc)); break;
                case 2:  out = saw (p, inc); break;
                case 3:  out = pulse (p, inc, 0.5f); break;
                case 4:  out = pulse (p, inc, 0.35f); break;   // largeurs couramment citées pour le Model D
                default: out = pulse (p, inc, 0.15f); break;
            }

            phase += inc;
            wrapped = phase >= 1.0f;
            if (wrapped)
                phase -= 1.0f;

            return out;
        }

        static float triangle (float p) noexcept { return 1.0f - 4.0f * std::abs (p - 0.5f); }
        static float saw (float p, float dt) noexcept { return 2.0f * p - 1.0f - polyBlep (p, dt); }

        static float pulse (float p, float dt, float width) noexcept
        {
            float v = p < width ? 1.0f : -1.0f;
            v += polyBlep (p, dt);
            float p2 = p - width;
            if (p2 < 0.0f)
                p2 += 1.0f;
            v -= polyBlep (p2, dt);
            return v - (2.0f * width - 1.0f);
        }
    };

    //==========================================================================
    struct Noise
    {
        uint32_t state = 0x9e3779b9u;
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;

        float white() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return (float) state * (2.0f / 4294967296.0f) - 1.0f;
        }

        // Filtre rose de Paul Kellet.
        float pink (float w) noexcept
        {
            b0 = 0.99886f * b0 + w * 0.0555179f;
            b1 = 0.99332f * b1 + w * 0.0750759f;
            b2 = 0.96900f * b2 + w * 0.1538520f;
            b3 = 0.86650f * b3 + w * 0.3104856f;
            b4 = 0.55000f * b4 + w * 0.5329522f;
            b5 = -0.7616f * b5 - w * 0.0168980f;
            const float out = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
            b6 = w * 0.115926f;
            return out * 0.11f;
        }
    };

    //==========================================================================
    // Dérive lente et aléatoire de la hauteur, entre -1 et 1.
    struct Drift
    {
        float value = 0.0f, target = 0.0f;
        int countdown = 0;

        float process (Noise& noise, float rate) noexcept
        {
            if (--countdown <= 0)
            {
                target = noise.white();
                countdown = (int) (rate * (0.3f + 0.35f * (noise.white() + 1.0f)));
            }
            value += (target - value) * (1.0f / (rate * 0.8f));
            return value;
        }
    };

    //==========================================================================
    // Générateur de contour : attaque, decay, sustain. Comme sur le Model D,
    // le relâchement réutilise le decay quand l'interrupteur Decay est activé.
    struct Contour
    {
        enum class Stage { idle, attack, decay, release };

        Stage stage = Stage::idle;
        float level = 0.0f;

        void gateOn() noexcept  { stage = Stage::attack; }
        void gateOff() noexcept { if (stage != Stage::idle) stage = Stage::release; }

        float process (float attackCoef, float decayCoef, float sustain, float releaseCoef) noexcept
        {
            switch (stage)
            {
                case Stage::attack:
                    // Vise au-dessus de 1, comme un condensateur qui charge vers une tension plus haute.
                    level += (1.3f - level) * attackCoef;
                    if (level >= 1.0f)
                    {
                        level = 1.0f;
                        stage = Stage::decay;
                    }
                    break;

                case Stage::decay:
                    level += (sustain - level) * decayCoef;
                    break;

                case Stage::release:
                    level -= level * releaseCoef;
                    if (level < 1.0e-5f)
                    {
                        level = 0.0f;
                        stage = Stage::idle;
                    }
                    break;

                case Stage::idle:
                    break;
            }
            return level;
        }
    };

    //==========================================================================
    // Filtre en échelle 24 dB/oct (ladder), modèle de Huovilainen : chacun des
    // 4 étages est un intégrateur dont l'entrée passe par une tanh, comme les
    // paires de transistors du circuit Moog :
    //     dy[i]/dt = wc * (tanh (y[i-1]) - tanh (y[i]))
    // Intégration trapézoïdale (TPT, zero-delay feedback). Les tanh sont
    // linéarisées autour de l'échantillon précédent (tanh (v) = pente * v),
    // ce qui permet de résoudre la boucle de résonance exactement à chaque
    // échantillon tout en gardant la saturation propre à chaque étage.
    struct Ladder
    {
        std::array<float, 4> s {};   // états des intégrateurs
        std::array<float, 4> y {};   // sorties des étages à l'échantillon précédent
        float u = 0.0f;              // entrée de l'échelle à l'échantillon précédent

        void reset() noexcept
        {
            s.fill (0.0f);
            y.fill (0.0f);
            u = 0.0f;
        }

        static float slope (float v) noexcept
        {
            return std::abs (v) < 1.0e-4f ? 1.0f : fastTanh (v) / v;
        }

        // bassComp : compense en partie la perte de graves quand la résonance
        // monte. Désactivé, le filtre se comporte comme l'original.
        float process (float x, float g, float k, bool bassComp) noexcept
        {
            // Le signal entre à mi-niveau pour que la saturation reste douce sur
            // un son normal mais laisse une auto-oscillation bien audible.
            const float in = x * (bassComp ? 1.0f + 0.5f * k : 1.0f) * 0.5f;

            // Chaque étage vaut y[i] = a[i] * entrée + b[i] ; on compose les
            // quatre pour exprimer la sortie en fonction de u.
            std::array<float, 4> a {}, b {};
            float inputSlope = slope (u);
            float A = 1.0f, B = 0.0f;

            for (size_t i = 0; i < 4; ++i)
            {
                const float denominator = 1.0f + g * slope (y[i]);
                a[i] = g * inputSlope / denominator;
                b[i] = s[i] / denominator;
                A = a[i] * A;
                B = a[i] * B + b[i];
                inputSlope = slope (y[i]);
            }

            // Boucle de résonance : u = in - k * (A * u + B).
            u = (in - k * B) / (1.0f + k * A);

            float stageInput = u;
            for (size_t i = 0; i < 4; ++i)
            {
                y[i] = a[i] * stageInput + b[i];
                s[i] = 2.0f * y[i] - s[i];
                stageInput = y[i];
            }
            return y[3] * 2.0f;
        }
    };

    //==========================================================================
    struct Smoothed
    {
        float current = 0.0f, target = 0.0f;

        void snap() noexcept { current = target; }
        float next (float coef) noexcept { current += (target - current) * coef; return current; }
    };

    //==========================================================================
    // LFO d'appoint (absent du Model D, où c'est l'osc 3 qui en fait office).
    struct Lfo
    {
        float phase = 0.0f;
        float held = 0.0f;
        uint32_t rng = 0x2545f491u;

        float process (float inc, int wave) noexcept
        {
            float out;
            switch (wave)
            {
                case 0:  out = Oscillator::triangle (phase); break;
                case 1:  out = phase < 0.5f ? 1.0f : -1.0f; break;
                case 2:  out = 2.0f * phase - 1.0f; break;
                case 3:  out = 1.0f - 2.0f * phase; break;
                default: out = held; break;
            }

            phase += inc;
            if (phase >= 1.0f)
            {
                phase -= std::floor (phase);
                rng ^= rng << 13;
                rng ^= rng >> 17;
                rng ^= rng << 5;
                held = (float) rng * (2.0f / 4294967296.0f) - 1.0f;
            }
            return out;
        }
    };
}

//==============================================================================
class SynthEngine
{
public:
    static constexpr int maxVoices = 8;

    SynthEngine()
    {
        // La voix 0 garde la graine d'origine : en mono, le son est inchangé.
        for (size_t i = 1; i < voices.size(); ++i)
            voices[i].noise.state ^= (uint32_t) (i * 0x85ebca6bu);
    }

    void prepare (double sampleRate, int maximumBlockSize)
    {
        baseRate = (float) sampleRate;
        osRate = baseRate * 2.0f;
        maxBlock = juce::jmax (1, maximumBlockSize);

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            1, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampler->initProcessing ((size_t) maxBlock);

        smoothCoef = detail::rcCoef (0.005f, osRate, 1.0f);
        dcCoef = 1.0f - juce::MathConstants<float>::twoPi * 10.0f / baseRate;
        dcX = dcY = 0.0f;

        for (auto& v : voices)
            v.reset();

        keys.fill (false);
        numKeys = 0;
        voiceAge = 0;

        setParams (params);
        for (auto* s : allSmoothed())
            s->snap();
    }

    int getLatencySamples() const
    {
        return oversampler != nullptr ? (int) std::round (oversampler->getLatencyInSamples()) : 0;
    }

    void setParams (const Params& p) noexcept
    {
        if (p.poly != params.poly)
            allNotesOff();

        params = p;

        tune.target = p.tune;
        osc1Freq.target = p.osc1Freq;
        osc2Freq.target = p.osc2Freq;
        osc3Freq.target = p.osc3Freq;
        osc2Fine.target = p.osc2Fine;
        osc3Fine.target = p.osc3Fine;
        modMix.target = p.modMix / 10.0f;
        modAmount.target = p.modAmount / 10.0f;

        for (size_t i = 0; i < 3; ++i)
            level[i].target = p.oscOn[i] ? levelCurve (p.oscLevel[i]) : 0.0f;
        noiseLevel.target = p.noiseOn ? levelCurve (p.noiseLevel) : 0.0f;

        cutoffOct.target = std::log2 (juce::jlimit (10.0f, 22000.0f, p.cutoff));
        emphasis.target = juce::jlimit (0.0f, 10.0f, p.emphasis) * 0.44f;
        contour.target = p.contour / 10.0f * 4.0f;   // jusqu'à 4 octaves
        volume.target = 1.5f * (p.volume / 10.0f) * (p.volume / 10.0f);
        lfoLevel.target = p.lfoLevel / 10.0f;
        vco3Filter.target = p.vco3Filter / 10.0f * 3.0f;

        keyTrack = (p.kbd1 ? 1.0f / 3.0f : 0.0f) + (p.kbd2 ? 2.0f / 3.0f : 0.0f);
        driftDepth = p.drift / 10.0f * 0.15f;        // jusqu'à 15 cents

        glideCoef = p.glide < 0.001f ? 1.0f : detail::rcCoef (p.glide, osRate, 3.0f);

        filterCoefs = makeCoefs (p.filterAttack, p.filterDecay, p.filterSustain, p.filterRelease, p);
        loudCoefs   = makeCoefs (p.loudAttack, p.loudDecay, p.loudSustain, p.loudRelease, p);

        updateLfoIncrement();
    }

    // Tempo et position de l'hôte, pour le LFO synchronisé.
    void setTempo (double bpm) noexcept
    {
        tempo = juce::jlimit (20.0, 400.0, bpm);
        updateLfoIncrement();
    }

    void setTransport (bool isPlaying, double ppqPosition) noexcept
    {
        if (params.lfoSync && isPlaying)
        {
            const double cycle = divisionBeats (params.lfoDivision);
            lfo.phase = (float) (ppqPosition / cycle - std::floor (ppqPosition / cycle));
        }
    }

    //==========================================================================
    // En mono : priorité à la note la plus basse et déclenchement unique, comme
    // le Model D. Jouer legato change la hauteur sans relancer les enveloppes,
    // sauf si RETRIG est activé.
    void noteOn (int note, float velocity = 1.0f) noexcept
    {
        if (note < 0 || note > 127)
            return;

        if (params.poly)
        {
            polyNoteOn (note, velocity);
            return;
        }

        if (keys[(size_t) note])
            return;

        const bool firstKey = numKeys == 0;
        keys[(size_t) note] = true;
        ++numKeys;

        auto& v = voices[0];
        v.targetNote = (float) lowestKey();
        v.velocity = velocity;

        if (! v.hasPlayed)
        {
            v.currentNote = v.targetNote;
            v.hasPlayed = true;
        }

        if (firstKey || params.retrigger)
            v.gateOn();
    }

    void noteOff (int note) noexcept
    {
        if (note < 0 || note > 127)
            return;

        if (params.poly)
        {
            for (auto& v : voices)
                if (v.gated && v.note == note)
                    v.gateOff();
            return;
        }

        if (! keys[(size_t) note])
            return;

        keys[(size_t) note] = false;
        --numKeys;

        if (numKeys == 0)
            voices[0].gateOff();
        else
            voices[0].targetNote = (float) lowestKey();
    }

    void allNotesOff() noexcept
    {
        keys.fill (false);
        numKeys = 0;
        for (auto& v : voices)
            v.gateOff();
    }

    void setPitchBend (float normalised) noexcept { pitchBend = juce::jlimit (-1.0f, 1.0f, normalised); }
    void setModWheel (float normalised) noexcept  { modWheel.target = normalised * normalised; }
    void setAftertouch (float normalised) noexcept { pressure.target = juce::jlimit (0.0f, 1.0f, normalised); }

    //==========================================================================
    // Rend numSamples échantillons mono à la fréquence de l'hôte.
    void render (float* out, int numSamples) noexcept
    {
        while (numSamples > 0)
        {
            const int chunk = juce::jmin (numSamples, maxBlock);
            renderChunk (out, chunk);
            out += chunk;
            numSamples -= chunk;
        }
    }

private:
    struct EnvCoefs { float attack, decay, sustain, release; };

    // Valeurs communes à toutes les voix pour l'échantillon en cours.
    struct Frame
    {
        float bend, tune, osc1Freq, osc2Freq, osc3Freq, osc2Fine, osc3Fine;
        float modMix, modDepth, lfo, lfoPitchAll, lfoPitch2, lfoFilter;
        std::array<float, 3> level;
        float noiseLevel, cutoffOct, emphasis, contour, vco3Filter, pressureOct;
    };

    //==========================================================================
    struct Voice
    {
        std::array<detail::Oscillator, 3> osc;
        std::array<detail::Drift, 3> drift;
        detail::Noise noise;
        detail::Ladder ladder;
        detail::Contour filterEnv, loudness;

        float currentNote = 60.0f, targetNote = 60.0f, velocity = 1.0f;
        bool hasPlayed = false, gated = false;
        int note = -1;
        uint32_t age = 0;

        void reset() noexcept
        {
            ladder.reset();
            loudness = {};
            filterEnv = {};
            hasPlayed = gated = false;
            note = -1;
        }

        void gateOn() noexcept
        {
            gated = true;
            filterEnv.gateOn();
            loudness.gateOn();
        }

        void gateOff() noexcept
        {
            gated = false;
            filterEnv.gateOff();
            loudness.gateOff();
        }

        bool isSilent() const noexcept
        {
            return ! gated && loudness.stage == detail::Contour::Stage::idle;
        }

        float tick (const Frame& f, const SynthEngine& e) noexcept
        {
            using namespace detail;
            const Params& p = e.params;
            const float rate = e.osRate;

            currentNote += (targetNote - currentNote) * e.glideCoef;

            const float keyNote = currentNote + f.bend;

            // Oscillateur 3 en premier : il sert aussi de source de modulation.
            const float drift3 = (drift[2].process (noise, rate) - 0.04f) * e.driftDepth;
            const float note3 = (p.osc3Ctrl ? keyNote : 60.0f) + f.tune
                              + rangeOctaves (p.range[2]) * 12.0f + f.osc3Freq + drift3
                              + f.osc3Fine + f.lfoPitchAll;
            float o3 = osc[2].process (e.increment (note3), p.wave[2], true);
            if (p.rectify[2])
                o3 = rectified (o3, p.offset[2]);

            const float white = noise.white();
            const float pink = noise.pink (white);
            const float noiseOut = p.pinkNoise ? pink : white;

            const float source = p.modFromLfo ? f.lfo : o3 * (1.0f - f.modMix) + noiseOut * f.modMix;
            const float mod = source * f.modDepth;

            const float pitchMod = p.oscMod ? mod * 12.0f : 0.0f;
            const float drift1 = drift[0].process (noise, rate) * e.driftDepth;
            const float drift2 = (drift[1].process (noise, rate) + 0.03f) * e.driftDepth;

            const float note1 = keyNote + f.tune + rangeOctaves (p.range[0]) * 12.0f + drift1 + pitchMod
                              + f.osc1Freq + f.lfoPitchAll;
            const float note2 = keyNote + f.tune + rangeOctaves (p.range[1]) * 12.0f
                              + f.osc2Freq + drift2 + pitchMod
                              + f.osc2Fine + f.lfoPitchAll + f.lfoPitch2;

            const float inc1 = e.increment (note1);
            const float inc2 = e.increment (note2);

            float o1 = osc[0].process (inc1, p.wave[0], false);

            // Synchro : osc 2 repart quand osc 1 boucle, au même point de son cycle.
            if (p.oscSync && osc[0].wrapped && inc1 > 0.0f)
                osc[1].phase = juce::jmin (0.999f, osc[0].phase * inc2 / inc1);

            float o2 = osc[1].process (inc2, p.wave[1], false);

            if (p.rectify[0]) o1 = rectified (o1, p.offset[0]);
            if (p.rectify[1]) o2 = rectified (o2, p.offset[1]);

            // Mixer : sature doucement quand on pousse les niveaux, une bonne part
            // du grain « gras » de l'original vient de là.
            const float mixed = o1 * f.level[0] + o2 * f.level[1]
                              + o3 * f.level[2] + noiseOut * f.noiseLevel;
            const float driven = fastTanh (mixed * 0.7f);

            const float fEnv = filterEnv.process (e.filterCoefs.attack, e.filterCoefs.decay,
                                                  e.filterCoefs.sustain, e.filterCoefs.release);
            const float lEnv = loudness.process (e.loudCoefs.attack, e.loudCoefs.decay,
                                                 e.loudCoefs.sustain, e.loudCoefs.release);

            const float contourAmount = p.velocity > 0.0f
                ? f.contour * (1.0f - p.velocity / 10.0f * (1.0f - velocity))
                : f.contour;

            float octaves = f.cutoffOct
                          + e.keyTrack * (currentNote - 60.0f) / 12.0f
                          + contourAmount * fEnv
                          + (p.filterMod ? mod * 3.0f : 0.0f);

            if (p.vco3Filter > 0.0f)  octaves += o3 * f.vco3Filter;
            if (p.lfoLevel > 0.0f)    octaves += f.lfoFilter;
            if (p.aftertouch > 0.0f)  octaves += f.pressureOct;

            const float fc = juce::jlimit (8.0f, 0.45f * rate, std::exp2 (octaves));
            const float g = std::tan (juce::MathConstants<float>::pi * fc / rate);

            // Un souffle infime permet au filtre d'auto-osciller même sans entrée.
            const float filtered = ladder.process (driven + white * 1.0e-5f, g, f.emphasis, p.bassComp);

            return filtered * lEnv;
        }

        // Mode rectify : redressement double alternance autour d'un décalage.
        static float rectified (float x, float offset) noexcept
        {
            return 2.0f * std::abs (x + offset) - 1.0f;
        }
    };

    //==========================================================================
    static float levelCurve (float level0to10) noexcept
    {
        return std::pow (juce::jlimit (0.0f, 10.0f, level0to10) / 10.0f, 1.7f);
    }

    EnvCoefs makeCoefs (float attack, float decay, float sustain, float release, const Params& p) const noexcept
    {
        const float releaseTime = p.adsr ? release : (p.decaySwitch ? decay : 0.008f);
        return { detail::rcCoef (attack, osRate, 1.466f),
                 detail::rcCoef (decay, osRate, 3.0f),
                 juce::jlimit (0.0f, 10.0f, sustain) / 10.0f,
                 detail::rcCoef (releaseTime, osRate, 3.0f) };
    }

    void updateLfoIncrement() noexcept
    {
        const float hz = params.lfoSync ? (float) (tempo / 60.0 / divisionBeats (params.lfoDivision))
                                        : params.lfoRate;
        lfoIncrement = hz / osRate;
    }

    int lowestKey() const noexcept
    {
        for (int n = 0; n < 128; ++n)
            if (keys[(size_t) n])
                return n;
        return 60;
    }

    void polyNoteOn (int note, float velocity) noexcept
    {
        Voice* chosen = nullptr;

        // Même note déjà jouée : on la relance sur la même voix.
        for (auto& v : voices)
            if (v.note == note && ! v.isSilent())
                chosen = &v;

        // Sinon une voix muette, puis la plus ancienne relâchée, puis la plus ancienne tout court.
        if (chosen == nullptr)
            for (auto& v : voices)
                if (v.isSilent() && (chosen == nullptr || v.age < chosen->age))
                    chosen = &v;

        if (chosen == nullptr)
            for (auto& v : voices)
                if (! v.gated && (chosen == nullptr || v.age < chosen->age))
                    chosen = &v;

        if (chosen == nullptr)
            for (auto& v : voices)
                if (chosen == nullptr || v.age < chosen->age)
                    chosen = &v;

        auto& v = *chosen;
        v.note = note;
        v.age = ++voiceAge;
        v.velocity = velocity;
        v.targetNote = (float) note;

        if (! v.hasPlayed)
        {
            v.currentNote = v.targetNote;
            v.hasPlayed = true;
        }

        v.gateOn();
    }

    std::array<detail::Smoothed*, 21> allSmoothed() noexcept
    {
        return { &tune, &osc1Freq, &osc2Freq, &osc3Freq, &osc2Fine, &osc3Fine, &modMix, &modAmount,
                 &level[0], &level[1], &level[2], &noiseLevel, &cutoffOct, &emphasis, &contour, &volume,
                 &lfoLevel, &vco3Filter, &modWheel, &pressure, &tremolo };
    }

    float increment (float note) const noexcept
    {
        return juce::jlimit (0.0f, 0.45f, detail::noteToHz (note) / osRate);
    }

    void renderChunk (float* out, int numSamples) noexcept
    {
        juce::FloatVectorOperations::clear (out, numSamples);

        float* channels[] = { out };
        juce::dsp::AudioBlock<float> block (channels, 1, (size_t) numSamples);

        auto upsampled = oversampler->processSamplesUp (block);
        auto* data = upsampled.getChannelPointer (0);

        for (size_t i = 0; i < upsampled.getNumSamples(); ++i)
            data[i] = tick();

        oversampler->processSamplesDown (block);

        // Retire la composante continue (ondes pulse étroites, saturation).
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = out[i];
            dcY = x - dcX + dcCoef * dcY;
            dcX = x;
            out[i] = dcY;
        }
    }

    float tick() noexcept
    {
        const float a = smoothCoef;

        Frame f;
        f.bend = pitchBend * params.bendRange;
        f.tune = tune.next (a);
        f.osc1Freq = osc1Freq.next (a);
        f.osc2Freq = osc2Freq.next (a);
        f.osc3Freq = osc3Freq.next (a);
        f.osc2Fine = osc2Fine.next (a);
        f.osc3Fine = osc3Fine.next (a);
        f.modMix = modMix.next (a);
        f.modDepth = modWheel.next (a) + modAmount.next (a);
        for (size_t i = 0; i < 3; ++i)
            f.level[i] = level[i].next (a);
        f.noiseLevel = noiseLevel.next (a);
        f.cutoffOct = cutoffOct.next (a);
        f.emphasis = emphasis.next (a);
        f.contour = contour.next (a);
        f.vco3Filter = vco3Filter.next (a);
        f.pressureOct = pressure.next (a) * params.aftertouch / 10.0f * 3.0f;

        // LFO : n'agit que si son niveau n'est pas à zéro.
        f.lfo = lfo.process (lfoIncrement, params.lfoWave);
        const float depth = lfoLevel.next (a);
        f.lfoPitchAll = f.lfoPitch2 = f.lfoFilter = 0.0f;
        tremolo.target = 1.0f;

        if (params.lfoLevel > 0.0f)
        {
            switch (params.lfoDest)
            {
                case 0:  f.lfoPitchAll = f.lfo * depth * depth * 12.0f; break;
                case 1:  f.lfoPitch2 = f.lfo * depth * depth * 12.0f; break;
                case 2:  f.lfoFilter = f.lfo * depth * 3.0f; break;
                default: tremolo.target = 1.0f - depth * (0.5f - 0.5f * f.lfo); break;
            }
        }
        const float amp = tremolo.next (a);

        float sum;
        if (params.poly)
        {
            sum = 0.0f;
            for (auto& v : voices)
                if (! v.isSilent())
                    sum += v.tick (f, *this);
            sum *= 0.5f;
        }
        else
        {
            sum = voices[0].tick (f, *this);
        }

        const float out = sum * volume.next (a);
        return params.lfoLevel > 0.0f && params.lfoDest == 3 ? out * amp : out;
    }

    //==========================================================================
    Params params;

    float baseRate = 44100.0f, osRate = 88200.0f;
    int maxBlock = 512;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    std::array<Voice, maxVoices> voices;
    detail::Lfo lfo;
    EnvCoefs filterCoefs {}, loudCoefs {};

    detail::Smoothed tune, osc1Freq, osc2Freq, osc3Freq, osc2Fine, osc3Fine, modMix, modAmount, noiseLevel,
                     cutoffOct, emphasis, contour, volume, lfoLevel, vco3Filter, modWheel, pressure, tremolo;
    std::array<detail::Smoothed, 3> level;

    float smoothCoef = 0.01f, glideCoef = 1.0f, keyTrack = 0.0f, driftDepth = 0.0f;
    float pitchBend = 0.0f, lfoIncrement = 0.0f;
    double tempo = 120.0;
    float dcCoef = 0.999f, dcX = 0.0f, dcY = 0.0f;

    std::array<bool, 128> keys {};
    int numKeys = 0;
    uint32_t voiceAge = 0;
};

} // namespace modeld
