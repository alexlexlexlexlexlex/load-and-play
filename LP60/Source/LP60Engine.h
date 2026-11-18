#pragma once

// Moteur de LP-60 : synthé polyphonique 6 voix inspiré du Roland Juno-60.
//
// Sources des valeurs :
// - notes de service Roland Juno-60 (1983) : LFO 0,3-20 Hz, modulation LFO du
//   DCO +/-300 cents, PWM de 50 % à 95 %, accord +/-50 cents, suivi de clavier
//   0-100 %, résonance jusqu'à l'auto-oscillation, voix attribuées en rotation ;
// - mesures de pendragon-andyh (github.com/pendragon-andyh/Juno60) : durées et
//   courbes de l'enveloppe, retard du LFO, passe-haut, mode GATE, bruit filtré.

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

namespace lp60
{

//==============================================================================
// Réglages de la façade. Comme sur le Juno-60, presque tout va de 0 à 10.
struct Params
{
    // LFO
    float lfoRate = 5.0f;       // 0,3 à 20 Hz
    float lfoDelay = 0.0f;      // attente puis montée progressive
    int lfoMode = 0;            // 0 = AUTO (relancé à chaque nouveau jeu), 1 = MANUAL (molette)

    // DCO
    int range = 1;              // 0 = 16', 1 = 8', 2 = 4'
    float dcoLfo = 0.0f;        // jusqu'à +/-300 cents
    float pwm = 0.0f;           // largeur 50 % à 5 %
    int pwmMode = 1;            // 0 = LFO, 1 = MAN, 2 = ENV
    bool pulse = false;
    bool saw = true;
    float sub = 0.0f;
    float noise = 0.0f;

    // HPF
    int hpf = 0;                // 0 = aucun, 1 = 244 Hz, 2 = 579 Hz, 3 = 1 kHz

    // VCF
    float cutoff = 7.0f;
    float resonance = 0.0f;
    bool envInvert = false;     // polarité de l'enveloppe sur le filtre
    float vcfEnv = 0.0f;
    float vcfLfo = 0.0f;
    float vcfKybd = 0.0f;       // suivi de clavier 0 à 100 %

    // VCA
    bool vcaGate = false;       // GATE : orgue, sinon l'enveloppe
    float level = 7.0f;

    // ENV (une seule enveloppe, partagée par le VCF et le VCA)
    float attack = 0.0f;
    float decay = 5.0f;
    float sustain = 10.0f;
    float release = 3.0f;

    // Jeu
    float tune = 0.0f;          // cents, -50 à +50
    float bendDco = 2.0f;       // demi-tons au maximum du pitch bend
    float bendVcf = 0.0f;       // 0..10, jusqu'à +/-3 octaves sur la coupure
    float lfoMod = 5.0f;        // 0..10, vibrato ajouté par la molette (mode AUTO)
};

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

    inline float rcCoef (float timeConstant, float rate) noexcept
    {
        return 1.0f - std::exp (-1.0f / (std::max (timeConstant, 1.0e-5f) * rate));
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

    inline float pulseWave (float p, float dt, float width) noexcept
    {
        float v = p < width ? 1.0f : -1.0f;
        v += polyBlep (p, dt);
        float p2 = p - width;
        if (p2 < 0.0f)
            p2 += 1.0f;
        return v - polyBlep (p2, dt);
    }

    // Valeur mesurée aux positions 0 / 2,5 / 5 / 7,5 / 10 du curseur,
    // interpolée en échelle logarithmique (linéaire si une borne vaut 0).
    using SliderTable = std::array<float, 5>;

    inline float lookup (const SliderTable& table, float slider) noexcept
    {
        const float position = juce::jlimit (0.0f, 10.0f, slider) / 2.5f;
        const int i = juce::jmin (3, (int) position);
        const float frac = position - (float) i;
        const float a = table[(size_t) i], b = table[(size_t) i + 1];
        return a > 0.0f && b > 0.0f ? a * std::pow (b / a, frac) : a + (b - a) * frac;
    }

    // Interpolation linéaire par morceaux (xs croissants).
    template <size_t N>
    float interpolate (const std::array<float, N>& xs, const std::array<float, N>& ys, float x) noexcept
    {
        if (x <= xs.front()) return ys.front();
        for (size_t i = 1; i < N; ++i)
            if (x <= xs[i])
                return ys[i - 1] + (ys[i] - ys[i - 1]) * (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
        return ys.back();
    }

    // Filtre, mesuré sur le JU-06A (tests B du kit) en ajustant un modèle de
    // filtre en échelle sur la réponse aux harmoniques :
    //  - coupure : 190 Hz à 4, 1,2 kHz à 5, 7,7 kHz à 6, grand ouvert dès 8 ;
    //  - résonance : boucle k et gain d'entrée qui compense en partie la
    //    perte de graves (-2,6 dB à 2,5, -5 dB à 5, -7 dB au maximum).
    inline float cutoffOctaves (float slider) noexcept
    {
        static constexpr std::array<float, 7> xs { 0.0f, 2.0f, 4.0f, 5.0f, 6.0f, 8.0f, 10.0f };
        // Au-delà de 8, la coupure dépasse le haut du spectre (elle est bornée au
        // rendu) ; la garder haute compte quand l'enveloppe inversée la fait descendre.
        static constexpr std::array<float, 7> ys { 3.0f, 4.17f, 7.57f, 10.23f, 12.72f, 16.3f, 16.3f };
        return interpolate (xs, ys, slider);
    }

    inline float cutoffHz (float slider) noexcept { return std::exp2 (cutoffOctaves (slider)); }

    inline float resonanceFeedback (float slider) noexcept
    {
        static constexpr std::array<float, 5> xs { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f };
        static constexpr std::array<float, 5> ys { 0.0f, 0.78f, 2.7f, 3.95f, 4.45f };
        return interpolate (xs, ys, slider);
    }

    inline float resonanceInputGain (float slider) noexcept
    {
        static constexpr std::array<float, 5> xs { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f };
        static constexpr std::array<float, 5> ys { 1.0f, 1.32f, 2.0f, 2.0f, 2.1f };
        return interpolate (xs, ys, slider);
    }

    // Enveloppe, mesurée sur le JU-06A (tests C du kit) :
    //  - durée de l'attaque jusqu'au maximum (secondes) ;
    //  - constante de temps du decay et du release, identique pour les deux
    //    (pente mesurée : 42 dB/s à 5, 7 dB/s à 7,5, 2,3 dB/s à 10).
    constexpr SliderTable attackTimes   { 0.001f, 0.012f, 0.15f, 1.05f, 3.0f };
    constexpr SliderTable envTimeConstants { 0.0015f, 0.0095f, 0.214f, 1.15f, 3.81f };

    // Durée affichée pour decay et release : jusqu'à -30 dB (3,5 constantes de temps).
    inline float envDisplaySeconds (float slider) noexcept { return 3.5f * lookup (envTimeConstants, slider); }

    // Le sustain n'est pas linéaire : à 5, le niveau est à -3,7 dB (mesuré).
    inline float sustainLevel (float slider) noexcept
    {
        return std::pow (juce::jlimit (0.0f, 10.0f, slider) / 10.0f, 0.62f);
    }

    // LFO, mesuré sur le JU-06A (tests E du kit) :
    //  - vitesse : 0,042 Hz à 0, 4,2 Hz à 5, 67 Hz à 10 ;
    //  - retard : attente puis montée linéaire (0,35 s de montée à 5 ; 1,75 s
    //    d'attente puis 2,75 s de montée à 10).
    constexpr SliderTable lfoRates     { 0.042f, 0.42f, 4.23f, 16.8f, 67.0f };
    constexpr SliderTable lfoDelays    { 0.0f, 0.01f, 0.05f, 0.3f, 1.75f };
    constexpr SliderTable lfoFadeTimes { 0.0f, 0.1f, 0.35f, 1.0f, 2.75f };

    //==========================================================================
    // Enveloppe ADSR. L'attaque charge un condensateur vers 2 et s'arrête à 1 :
    // une montée presque linéaire, comme mesuré sur le JU-06A. Decay et release
    // sont des décharges exponentielles (pente constante en dB).
    struct Envelope
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
                    level += (2.0f - level) * attackCoef;
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
                    if (level < 1.0e-4f)
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
    // Mode GATE du VCA : montée en 3 ms, descente en 6 ms (mesuré).
    struct GateRamp
    {
        float level = 0.0f;

        float process (bool gate, float upStep, float downStep) noexcept
        {
            level = gate ? juce::jmin (1.0f, level + upStep) : juce::jmax (0.0f, level - downStep);
            return level;
        }
    };

    //==========================================================================
    // Filtre 24 dB/oct du Juno (puce IR3109) : quatre étages passe-bas en
    // cascade, bouclés par la résonance, chacun avec sa saturation :
    //     dy[i]/dt = wc * (tanh (y[i-1]) - tanh (y[i]))
    // Intégration trapézoïdale (zero-delay feedback), tanh linéarisées autour
    // de l'échantillon précédent. Cette forme reste stable même résonance au
    // maximum et coupure tout en haut, et son auto-oscillation suit la coupure.
    struct OtaFilter
    {
        std::array<float, 4> s {}, y {};
        float u = 0.0f;

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

        float process (float x, float g, float k) noexcept
        {
            const float in = x * 0.5f;

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

    struct Smoothed
    {
        float current = 0.0f, target = 0.0f;

        void snap() noexcept { current = target; }
        float next (float coef) noexcept { current += (target - current) * coef; return current; }
    };
}

//==============================================================================
class Engine
{
public:
    static constexpr int numVoices = 6;

    void prepare (double sampleRate, int maximumBlockSize)
    {
        baseRate = (float) sampleRate;
        osRate = baseRate * 2.0f;
        maxBlock = juce::jmax (1, maximumBlockSize);

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            1, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampler->initProcessing ((size_t) maxBlock);

        smoothCoef = detail::rcCoef (0.005f, osRate);
        noiseCoef = detail::rcCoef (1.0f / (juce::MathConstants<float>::twoPi * 3800.0f), osRate);
        gateUp = 1.0f / (0.003f * osRate);
        gateDown = 1.0f / (0.006f * osRate);
        dcCoef = 1.0f - juce::MathConstants<float>::twoPi * 10.0f / baseRate;
        dcX = dcY = hpfState = 0.0f;

        for (size_t i = 0; i < voices.size(); ++i)
        {
            auto& v = voices[i];
            v = Voice {};
            v.rng = 0x9e3779b9u ^ (uint32_t) (i * 0x85ebca6bu + 1);
            // Petites différences entre les six cartes voix, comme sur un vrai Juno.
            static constexpr float cutoffSpread[] = { 0.02f, -0.03f, 0.01f, -0.01f, 0.03f, -0.02f };
            static constexpr float gainSpread[] = { 1.0f, 0.97f, 1.02f, 0.99f, 1.01f, 0.98f };
            v.cutoffOffset = cutoffSpread[i];
            v.gain = gainSpread[i];
        }

        nextVoice = 0;
        numHeld = 0;
        lfoPhase = lfoTimer = 0.0f;

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
        params = p;

        tune.target = juce::jlimit (-50.0f, 50.0f, p.tune) / 100.0f;
        dcoLfo.target = p.dcoLfo / 10.0f * 3.8f;                 // +/-380 cents (mesuré sur le JU-06A)
        pwm.target = p.pwm / 10.0f;
        subLevel.target = p.sub / 10.0f;
        noiseLevel.target = p.noise / 10.0f;

        cutoffOct.target = detail::cutoffOctaves (p.cutoff);
        resonance.target = detail::resonanceFeedback (p.resonance);
        resonanceGain.target = detail::resonanceInputGain (p.resonance);
        // Profondeur de l'enveloppe sur le filtre, mesurée sur le JU-06A (C14 à C16) :
        // 3,8 octaves à 5, 10,6 octaves à 10.
        vcfEnv.target = (p.envInvert ? -1.0f : 1.0f) * 10.6f * std::pow (juce::jlimit (0.0f, 10.0f, p.vcfEnv) / 10.0f, 1.46f);
        vcfLfo.target = p.vcfLfo / 10.0f * 3.3f;                 // ~40 Hz à 4,5 kHz autour de 440 Hz
        vcfKybd.target = p.vcfKybd / 10.0f;
        level.target = 0.6f * (p.level / 10.0f) * (p.level / 10.0f);

        const float rate = osRate;
        // Charge vers 2 : le niveau 1 est atteint après ln 2 constantes de temps.
        attackCoef = detail::rcCoef (detail::lookup (detail::attackTimes, p.attack) / std::log (2.0f), rate);
        decayCoef = detail::rcCoef (detail::lookup (detail::envTimeConstants, p.decay), rate);
        releaseCoef = detail::rcCoef (detail::lookup (detail::envTimeConstants, p.release), rate);
        sustain = detail::sustainLevel (p.sustain);

        lfoIncrement = detail::lookup (detail::lfoRates, p.lfoRate) / osRate;
        lfoDelaySeconds = detail::lookup (detail::lfoDelays, p.lfoDelay);
        lfoFadeSeconds = detail::lookup (detail::lfoFadeTimes, p.lfoDelay);

        // Passe-haut 6 dB/oct, coupures mesurées sur le JU-06A (tests D du kit).
        static constexpr float hpfHz[] = { 0.0f, 244.0f, 579.0f, 1000.0f };
        const int hpfIndex = juce::jlimit (0, 3, p.hpf);
        hpfCoef = hpfIndex == 0 ? 0.0f
                                : detail::rcCoef (1.0f / (juce::MathConstants<float>::twoPi * hpfHz[hpfIndex]), baseRate);
    }

    //==========================================================================
    // Attribution des voix en rotation (mode ROTARY du Juno-60) : chaque
    // nouvelle note prend la voix suivante, ce qui laisse sonner les relâchements.
    void noteOn (int note) noexcept
    {
        if (note < 0 || note > 127)
            return;

        if (numHeld == 0)
            lfoTimer = 0.0f;   // mode AUTO : le retard du LFO repart à chaque nouveau jeu
        ++numHeld;

        int chosen = -1;
        for (int i = 0; i < numVoices && chosen < 0; ++i)
            if (voices[(size_t) i].gated && voices[(size_t) i].note == note)
                chosen = i;

        for (int i = 0; i < numVoices && chosen < 0; ++i)
        {
            const int candidate = (nextVoice + i) % numVoices;
            if (! voices[(size_t) candidate].gated)
                chosen = candidate;
        }

        if (chosen < 0)
            chosen = nextVoice;

        nextVoice = (chosen + 1) % numVoices;

        auto& v = voices[(size_t) chosen];
        v.note = note;
        v.gated = true;
        v.env.gateOn();
    }

    void noteOff (int note) noexcept
    {
        for (auto& v : voices)
        {
            if (v.gated && v.note == note)
            {
                v.gated = false;
                v.env.gateOff();
                numHeld = juce::jmax (0, numHeld - 1);
            }
        }
    }

    void allNotesOff() noexcept
    {
        for (auto& v : voices)
        {
            v.gated = false;
            v.env.gateOff();
        }
        numHeld = 0;
    }

    void setPitchBend (float normalised) noexcept { pitchBend = juce::jlimit (-1.0f, 1.0f, normalised); }
    void setModWheel (float normalised) noexcept  { modWheel.target = juce::jlimit (0.0f, 1.0f, normalised); }

    int activeVoices() const noexcept
    {
        int count = 0;
        for (const auto& v : voices)
            count += v.isSilent() ? 0 : 1;
        return count;
    }

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
    // Valeurs communes à toutes les voix pour l'échantillon en cours.
    struct Frame
    {
        float pitch;          // décalage commun en demi-tons (accord, bend, LFO)
        float lfo, cutoffOct, resonance, resonanceGain, vcfEnv, vcfKybd, pwm, sub, noise;
    };

    // Niveau du carré et du sous-oscillateur par rapport à la dent de scie :
    // sur le JU-06A, le carré n'est que 3,5 dB plus fort qu'elle (4,8 dB en théorie).
    static constexpr float pulseLevel = 0.86f;

    struct Voice
    {
        float phase = 0.0f, noiseState = 0.0f, cutoffOffset = 0.0f, gain = 1.0f;
        bool subHigh = false, gated = false;
        int note = -1;
        uint32_t rng = 1;
        detail::OtaFilter vcf;
        detail::Envelope env;
        detail::GateRamp gateRamp;

        bool isSilent() const noexcept
        {
            return ! gated && env.stage == detail::Envelope::Stage::idle && gateRamp.level <= 0.0f;
        }

        float white() noexcept
        {
            rng ^= rng << 13;
            rng ^= rng >> 17;
            rng ^= rng << 5;
            return (float) rng * (2.0f / 4294967296.0f) - 1.0f;
        }

        float tick (const Frame& f, const Engine& e) noexcept
        {
            using namespace detail;
            const Params& p = e.params;

            // DCO : une horloge numérique fixe la hauteur, donc pas de dérive.
            static constexpr float rangeSemitones[] = { -12.0f, 0.0f, 12.0f };
            const float pitch = (float) note + rangeSemitones[juce::jlimit (0, 2, p.range)] + f.pitch;
            const float inc = juce::jlimit (0.0f, 0.45f, 440.0f * std::exp2 ((pitch - 69.0f) / 12.0f) / e.osRate);

            const float envLevel = env.process (e.attackCoef, e.decayCoef, e.sustain, e.releaseCoef);

            // PWM, mesuré sur le JU-06A : 49 % au repos (carré légèrement
            // asymétrique), 29 % à mi-course, 4 % au maximum.
            float pwmAmount = f.pwm;
            if (p.pwmMode == 0)      pwmAmount *= 0.6f + 0.52f * f.lfo;   // PWM 7 : 47 % à 16 % (mesuré)
            else if (p.pwmMode == 2) pwmAmount *= envLevel;
            const float a = juce::jlimit (0.0f, 1.0f, pwmAmount);
            const float width = 0.49f - 0.35f * a - 0.1f * a * a;

            const float ph = phase;
            float mix = 0.0f;
            if (p.saw)
                mix += 2.0f * ph - 1.0f - polyBlep (ph, inc);

            // L'impulsion est haute en fin de rampe, quand la dent de scie dépasse le
            // seuil du PWM : les deux ondes s'additionnent au lieu de s'annuler.
            // Son niveau efficace reste constant quelle que soit sa largeur (mesuré).
            if (p.pulse)
            {
                float q = ph + width;
                if (q >= 1.0f)
                    q -= 1.0f;
                const float v = pulseWave (q, inc, width) - (2.0f * width - 1.0f);
                mix += v * pulseLevel / (2.0f * std::sqrt (width * (1.0f - width)));
            }

            // Sous-oscillateur : carré une octave plus bas, divisé par bascule à chaque cycle.
            if (f.sub > 0.0f)
                mix += f.sub * pulseLevel * pulseWave (0.5f * (ph + (subHigh ? 1.0f : 0.0f)), inc * 0.5f, 0.5f);

            phase += inc;
            if (phase >= 1.0f)
            {
                phase -= 1.0f;
                subHigh = ! subHigh;
            }

            // Bruit blanc adouci au-dessus de 4 kHz (mesuré sur le JU-06A).
            noiseState += (white() - noiseState) * e.noiseCoef;
            if (f.noise > 0.0f)
                mix += f.noise * noiseState * 2.0f;

            // VCF
            const float octaves = f.cutoffOct + cutoffOffset
                                + f.vcfKybd * ((float) note - 60.0f) / 12.0f
                                + f.vcfEnv * envLevel;
            const float fc = juce::jlimit (8.0f, 0.45f * e.osRate, std::exp2 (octaves));
            const float g = std::tan (juce::MathConstants<float>::pi * fc / e.osRate);
            const float filtered = vcf.process ((mix * 0.4f + white() * 1.0e-5f) * f.resonanceGain, g, f.resonance);

            // VCA : enveloppe, ou porte quasi instantanée en mode GATE.
            const float gateLevel = gateRamp.process (gated, e.gateUp, e.gateDown);
            return filtered * (p.vcaGate ? gateLevel : envLevel) * gain;
        }
    };

    std::array<detail::Smoothed*, 15> allSmoothed() noexcept
    {
        return { &tune, &dcoLfo, &pwm, &subLevel, &noiseLevel, &cutoffOct, &resonance, &resonanceGain,
                 &vcfEnv, &vcfLfo, &vcfKybd, &level, &modWheel, &lfoGain, &bendSmoothed };
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

        for (int i = 0; i < numSamples; ++i)
        {
            float x = out[i];

            // Passe-haut 6 dB/oct commun aux six voix (positions 1 à 3).
            if (hpfCoef > 0.0f)
            {
                hpfState += (x - hpfState) * hpfCoef;
                x -= hpfState;
            }

            // Retire la composante continue (pulse étroite, sous-oscillateur).
            dcY = x - dcX + dcCoef * dcY;
            dcX = x;
            out[i] = dcY;
        }
    }

    float tick() noexcept
    {
        const float a = smoothCoef;

        // LFO triangle unique pour les six voix.
        const float triangle = 1.0f - 4.0f * std::abs (lfoPhase - 0.5f);
        lfoPhase += lfoIncrement;
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        // Mode AUTO : silence pendant le retard, puis montée progressive.
        // Mode MANUAL : la molette dose le LFO, comme le bouton du levier du Juno.
        float gainTarget;
        if (params.lfoMode == 0)
        {
            lfoTimer = juce::jmin (lfoTimer + 1.0f / osRate, 60.0f);
            const float t = lfoTimer - lfoDelaySeconds;
            gainTarget = t <= 0.0f ? 0.0f : (lfoFadeSeconds <= 0.0f ? 1.0f : juce::jmin (1.0f, t / lfoFadeSeconds));
        }
        else
        {
            gainTarget = modWheel.target;
        }
        lfoGain.target = gainTarget;
        const float lfo = triangle * lfoGain.next (a);
        const float wheel = modWheel.next (a);

        bendSmoothed.target = pitchBend;
        const float bend = bendSmoothed.next (a);

        Frame f;
        f.lfo = lfo;
        f.pitch = tune.next (a) + bend * params.bendDco + lfo * dcoLfo.next (a);
        if (params.lfoMode == 0)
            f.pitch += triangle * wheel * params.lfoMod / 10.0f * 3.0f;   // vibrato à la molette
        f.cutoffOct = cutoffOct.next (a) + bend * params.bendVcf / 10.0f * 3.0f + lfo * vcfLfo.next (a);
        f.resonance = resonance.next (a);
        f.resonanceGain = resonanceGain.next (a);
        f.vcfEnv = vcfEnv.next (a);
        f.vcfKybd = vcfKybd.next (a);
        f.pwm = pwm.next (a);
        f.sub = subLevel.next (a);
        f.noise = noiseLevel.next (a);

        float sum = 0.0f;
        for (auto& v : voices)
            if (! v.isSilent())
                sum += v.tick (f, *this);

        return sum * level.next (a);
    }

    //==========================================================================
    Params params;

    float baseRate = 44100.0f, osRate = 88200.0f;
    int maxBlock = 512;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    std::array<Voice, numVoices> voices;
    int nextVoice = 0, numHeld = 0;

    detail::Smoothed tune, dcoLfo, pwm, subLevel, noiseLevel, cutoffOct, resonance, resonanceGain, vcfEnv, vcfLfo, vcfKybd,
                     level, modWheel, lfoGain, bendSmoothed;

    float smoothCoef = 0.01f, noiseCoef = 0.3f, gateUp = 0.01f, gateDown = 0.01f;
    float attackCoef = 0.1f, decayCoef = 0.001f, releaseCoef = 0.001f, sustain = 1.0f;
    float lfoPhase = 0.0f, lfoIncrement = 0.0f, lfoTimer = 0.0f, lfoDelaySeconds = 0.0f, lfoFadeSeconds = 0.0f;
    float pitchBend = 0.0f;
    float hpfCoef = 0.0f, hpfState = 0.0f;
    float dcCoef = 0.999f, dcX = 0.0f, dcY = 0.0f;
};

} // namespace lp60
