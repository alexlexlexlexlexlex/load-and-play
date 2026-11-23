#pragma once

// Moteur sonore de LP-303 : une voix monophonique de basse « acide ».
//
//   oscillateur (dents de scie ou carré) -> filtre passe-bas à diodes 4 pôles
//   -> ampli (VCA) -> saturation optionnelle -> volume
//
// Tout le trajet du son tourne en suréchantillonnage x4 : le filtre est non
// linéaire et sa résonance peut monter jusqu'au sifflement, ce qui créerait
// sinon du repliement (aliasing) audible dans les aigus.
//
// Le jeu suit la logique de la machine d'origine :
//   - note liée (legato) = slide : la hauteur glisse, les enveloppes ne
//     repartent pas ;
//   - vélocité haute (>= 100) = accent : plus de volume, une décroissance
//     du filtre la plus courte possible, et un coup de cutoff qui s'accumule
//     d'une note accentuée à l'autre (plus la résonance est haute, plus il
//     est lent et marqué).

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>

namespace lp303
{

struct Params
{
    int wave = 0;              // 0 = dents de scie, 1 = carré
    float tune = 0.0f;         // demi-tons, -12 à +12
    float cutoff = 3.5f;       // 0 à 10
    float resonance = 7.0f;    // 0 à 10
    float envMod = 6.0f;       // 0 à 10
    float decay = 4.0f;        // 0 à 10
    float accent = 6.0f;       // 0 à 10
    float slide = 0.08f;       // durée du glissement, en secondes
    float drive = 0.0f;        // saturation, 0 (coupée) à 10
    float volume = 7.0f;       // 0 à 10
    float bendRange = 2.0f;    // demi-tons
};

namespace detail
{
    inline float noteToHz (float note) noexcept
    {
        return 440.0f * std::exp2 ((note - 69.0f) / 12.0f);
    }

    // Coefficient d'un filtre à un pôle (lissage RC) de constante de temps donnée.
    inline float rcCoef (float seconds, float rate) noexcept
    {
        return seconds <= 0.0f ? 1.0f : 1.0f - std::exp (-1.0f / (seconds * rate));
    }

    // tanh approchée (Padé), exacte à 1 % près, plafonnée à +-1.
    inline float fastTanh (float x) noexcept
    {
        if (x > 3.0f)  return 1.0f;
        if (x < -3.0f) return -1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // Pente moyenne tanh (v) / v, utilisée pour linéariser la saturation
    // autour de l'échantillon précédent. Toujours comprise entre 0 et 1.
    inline float tanhSlope (float v) noexcept
    {
        const float a = std::abs (v);
        if (a >= 3.0f)
            return 1.0f / a;
        const float v2 = v * v;
        return (27.0f + v2) / (27.0f + 9.0f * v2);
    }

}

//==============================================================================
// Oscillateur à tables d'onde à bande limitée : une table par octave, dont les
// harmoniques s'arrêtent bien avant la moitié de la fréquence suréchantillonnée.
// Aucune harmonique ne se replie, même pour les notes aiguës, et le spectre
// audible reste complet (au moins 35 kHz de bande à 44,1 kHz).
class WaveTables
{
public:
    static constexpr int tableOrder = 12;
    static constexpr int tableSize = 1 << tableOrder;
    static constexpr int numTables = 12;          // octaves à partir de 8 Hz
    static constexpr float lowestHz = 8.0f;

    void build (float oversampledRate)
    {
        if (std::abs (oversampledRate - builtFor) < 1.0f)
            return;
        builtFor = oversampledRate;

        juce::dsp::FFT fft (tableOrder);
        std::vector<float> work ((size_t) tableSize * 2);

        // Échelle de la transformée inverse, mesurée sur une sinusoïde d'amplitude 1.
        std::fill (work.begin(), work.end(), 0.0f);
        work[3] = -1.0f;                                  // bin 1, partie imaginaire
        fft.performRealOnlyInverseTransform (work.data());
        const float scale = 1.0f / juce::jmax (1.0e-9f, work[(size_t) tableSize / 4]);

        const float limitHz = 0.4f * oversampledRate;

        for (int wave = 0; wave < 2; ++wave)
        {
            for (int t = 0; t < numTables; ++t)
            {
                const float topHz = lowestHz * std::exp2 ((float) (t + 1));
                const int harmonics = juce::jlimit (1, tableSize / 2 - 1, (int) (limitHz / topHz));

                std::fill (work.begin(), work.end(), 0.0f);
                for (int n = 1; n <= harmonics; ++n)
                {
                    // Dents de scie montante : -2/pi * somme sin (2 pi n p) / n ;
                    // carré : 4/pi * somme des impaires sin (2 pi n p) / n.
                    float amplitude;
                    if (wave == 0)
                        amplitude = -2.0f / (juce::MathConstants<float>::pi * (float) n);
                    else
                        amplitude = (n % 2) != 0 ? 4.0f / (juce::MathConstants<float>::pi * (float) n) : 0.0f;

                    work[(size_t) (2 * n + 1)] = -amplitude;     // sinus = partie imaginaire négative
                }
                fft.performRealOnlyInverseTransform (work.data());

                auto& table = tables[(size_t) wave][(size_t) t];
                table.resize ((size_t) tableSize + 1);
                for (int i = 0; i < tableSize; ++i)
                    table[(size_t) i] = work[(size_t) i] * scale;
                table[(size_t) tableSize] = table[0];
            }
        }
    }

    // Lecture avec interpolation linéaire. phase de 0 à 1, hz = fréquence jouée.
    float read (int wave, float phase, float hz) const noexcept
    {
        const int t = juce::jlimit (0, numTables - 1, (int) std::floor (std::log2 (juce::jmax (lowestHz, hz) / lowestHz)));
        const auto& table = tables[(size_t) juce::jlimit (0, 1, wave)][(size_t) t];
        const float position = phase * (float) tableSize;
        const int i = juce::jlimit (0, tableSize - 1, (int) position);
        const float frac = position - (float) i;
        return table[(size_t) i] + frac * (table[(size_t) i + 1] - table[(size_t) i]);
    }

private:
    std::array<std::array<std::vector<float>, numTables>, 2> tables;
    float builtFor = 0.0f;
};

//==============================================================================
// Filtre passe-bas à diodes, 4 pôles. Contrairement à l'échelle à transistors,
// les condensateurs de l'échelle à diodes sont couplés dans les deux sens :
// chaque étage charge le précédent. Les pôles ne sont donc pas confondus mais
// étalés, et la pente juste au-dessus de la coupure ressemble à celle d'un
// 3 pôles (18 dB/oct), ce qui donne le caractère « caoutchouc » du son acide.
//
//     dy1/dt = wc * ( tanh (x  - y1) - tanh (y1 - y2) )
//     dy2/dt = wc * ( tanh (y1 - y2) - tanh (y2 - y3) )
//     dy3/dt = wc * ( tanh (y2 - y3) - tanh (y3 - y4) )
//     dy4/dt = wc *   tanh (y3 - y4)
//     x = entrée - k * y4                    (boucle de résonance)
//
// Intégration trapézoïdale à retard nul (TPT / ZDF). Les tanh sont remplacées
// par leur pente moyenne autour de l'échantillon précédent : le système devient
// linéaire à chaque échantillon et se résout exactement (élimination de Gauss
// sur la matrice tridiagonale, plus la boucle de résonance). La saturation
// borne l'amplitude : le filtre reste stable même en auto-oscillation.
struct DiodeLadder
{
    // Gain de boucle où le modèle linéaire se met à osciller, et rapport entre la
    // fréquence de résonance et wc (calculés sur la fonction de transfert).
    static constexpr float criticalK = 18.4f;
    static constexpr float peakRatio = 1.1956f;

    std::array<float, 4> s {};   // états des intégrateurs
    std::array<float, 4> y {};   // sorties des étages à l'échantillon précédent
    float x = 0.0f;              // entrée de l'échelle (après la boucle) à l'échantillon précédent

    void reset() noexcept
    {
        s.fill (0.0f);
        y.fill (0.0f);
        x = 0.0f;
    }

    // g = tan (pi * fc / fs), k = gain de résonance (0 à criticalK).
    float process (float in, float g, float k) noexcept
    {
        using detail::tanhSlope;

        const float c1 = tanhSlope (x - y[0]);
        const float c2 = tanhSlope (y[0] - y[1]);
        const float c3 = tanhSlope (y[1] - y[2]);
        const float c4 = tanhSlope (y[2] - y[3]);

        const float gc1 = g * c1, gc2 = g * c2, gc3 = g * c3, gc4 = g * c4;

        // Élimination vers le bas : y1 = P1 + Q1 y2 + R1 y4, etc.
        const float d1 = 1.0f + gc1 + gc2;
        const float p1 = (s[0] + gc1 * in) / d1;
        const float q1 = gc2 / d1;
        const float r1 = -gc1 * k / d1;

        const float e2 = 1.0f + gc2 + gc3 - gc2 * q1;
        const float p2 = (s[1] + gc2 * p1) / e2;
        const float q2 = gc3 / e2;
        const float r2 = gc2 * r1 / e2;

        const float e3 = 1.0f + gc3 + gc4 - gc3 * q2;
        const float p3 = (s[2] + gc3 * p2) / e3;
        const float t3 = (gc4 + gc3 * r2) / e3;

        const float y4 = (s[3] + gc4 * p3) / (1.0f + gc4 - gc4 * t3);
        const float y3 = p3 + t3 * y4;
        const float y2 = p2 + q2 * y3 + r2 * y4;
        const float y1 = p1 + q1 * y2 + r1 * y4;

        y = { y1, y2, y3, y4 };
        x = in - k * y4;

        for (size_t i = 0; i < 4; ++i)
            s[i] = 2.0f * y[i] - s[i];

        // Garde-fou : jamais de NaN ni de valeur folle, quoi qu'il arrive.
        if (! std::isfinite (y4) || std::abs (y4) > 50.0f)
        {
            reset();
            return 0.0f;
        }
        return y4;
    }
};

//==============================================================================
class AcidEngine
{
public:
    static constexpr int oversamplingStages = 2;    // 2 étages de x2 = x4
    static constexpr float accentVelocity = 100.0f / 127.0f;

    void prepare (double sampleRate, int maximumBlockSize)
    {
        baseRate = (float) sampleRate;
        osRate = baseRate * (float) (1 << oversamplingStages);
        maxBlock = juce::jmax (1, maximumBlockSize);

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            1, (size_t) oversamplingStages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampler->initProcessing ((size_t) maxBlock);
        waveTables.build (osRate);

        smoothCoef = detail::rcCoef (0.004f, osRate);
        attackCoef = detail::rcCoef (0.0012f, osRate);
        releaseCoef = detail::rcCoef (0.004f, osRate);
        vcaDecayCoef = detail::rcCoef (0.9f, osRate);
        accentVcaCoef = detail::rcCoef (0.002f, osRate);
        dcCoef = 1.0f - juce::MathConstants<float>::twoPi * 12.0f / baseRate;
        squareHpCoef = detail::rcCoef (1.0f / (juce::MathConstants<float>::twoPi * 70.0f), osRate);

        reset();
        updateTargets();
        smoothed = targets;
    }

    void reset() noexcept
    {
        filter.reset();
        if (oversampler != nullptr)
            oversampler->reset();
        phase = 0.0f;
        squareLow = 0.0f;
        vca = 0.0f;
        vcaStage = VcaStage::idle;
        meg = 0.0f;
        accentSweep = 0.0f;
        accentVca = 0.0f;
        accented = false;
        megAccented = false;
        dcX = dcY = 0.0f;
        numHeld = 0;
    }

    int getLatencySamples() const
    {
        return oversampler != nullptr ? (int) std::round (oversampler->getLatencyInSamples()) : 0;
    }

    void setParams (const Params& p) noexcept
    {
        params = p;
        updateTargets();
    }

    void setPitchBend (float minusOneToOne) noexcept { bend = minusOneToOne; }

    //==========================================================================
    // Clavier monophonique, dernière note prioritaire.
    void noteOn (int note, float velocity) noexcept
    {
        const bool accent = velocity >= accentVelocity - 1.0e-4f;
        removeHeld (note);              // la même touche rejouée redéclenche la note
        const bool legato = numHeld > 0;

        if (numHeld < (int) held.size())
            held[(size_t) numHeld++] = note;

        targetPitch = (float) note;
        accented = accent;

        if (legato)
        {
            gliding = true;        // slide : la hauteur glisse, rien ne redémarre
        }
        else
        {
            gliding = false;
            pitch = targetPitch;
            trigger (accent);
        }
    }

    void noteOff (int note) noexcept
    {
        const bool wasTop = numHeld > 0 && held[(size_t) numHeld - 1] == note;
        removeHeld (note);

        if (numHeld == 0)
        {
            if (vcaStage != VcaStage::idle)
                vcaStage = VcaStage::release;
        }
        else if (wasTop)
        {
            // Retour legato à la note encore tenue.
            targetPitch = (float) held[(size_t) numHeld - 1];
            gliding = true;
        }
    }

    void allNotesOff() noexcept
    {
        numHeld = 0;
        if (vcaStage != VcaStage::idle)
            vcaStage = VcaStage::release;
    }

    //==========================================================================
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

    // Pour les tests et l'affichage.
    float getPitch() const noexcept        { return pitch; }
    float getFilterEnvelope() const noexcept { return meg; }
    float getCutoffHz() const noexcept     { return lastCutoff; }
    float getAmplitude() const noexcept    { return vca; }
    bool isGateOn() const noexcept         { return numHeld > 0; }

private:
    enum class VcaStage { idle, attack, decay, release };

    struct Targets
    {
        float cutoffOct = 0.0f, envOct = 0.0f, k = 0.0f, accent = 0.0f, drive = 0.0f, gain = 0.0f;
    };

    void updateTargets() noexcept
    {
        const float env = params.envMod / 10.0f;

        // CUTOFF : de 60 Hz à 7,7 kHz. ENV MOD ajoute jusqu'à 4,5 octaves au
        // sommet de l'enveloppe et abaisse un peu le point de départ, comme sur
        // la machine d'origine.
        targets.cutoffOct = std::log2 (60.0f) + params.cutoff * 0.7f - 0.8f * env;
        targets.envOct = 4.5f * env;

        const float r = juce::jlimit (0.0f, 1.0f, params.resonance / 10.0f);
        // La résonance maximale s'arrête à 90 % du seuil d'auto-oscillation : le
        // filtre chante fort mais ne siffle pas tout seul, comme l'original.
        targets.k = 0.9f * DiodeLadder::criticalK * std::pow (r, 1.3f);
        targets.accent = juce::jlimit (0.0f, 1.0f, params.accent / 10.0f);
        targets.drive = juce::jlimit (0.0f, 1.0f, params.drive / 10.0f);

        const float v = juce::jlimit (0.0f, 1.0f, params.volume / 10.0f);
        targets.gain = v * v;

        // DECAY : de 0,2 s à 2,5 s (temps pour tomber à -40 dB).
        const float decaySeconds = 0.2f * std::pow (12.5f, juce::jlimit (0.0f, 10.0f, params.decay) / 10.0f);
        megCoef = std::exp (-4.6f / (decaySeconds * osRate));
        megAccentCoef = std::exp (-4.6f / (0.2f * osRate));

        // Le condensateur du circuit d'accent est plus lent quand la résonance monte.
        accentSweepCoef = detail::rcCoef (0.005f + 0.04f * r, osRate);
        glideCoef = detail::rcCoef (juce::jmax (0.001f, params.slide) / 3.0f, osRate);
    }

    void trigger (bool accent) noexcept
    {
        vcaStage = VcaStage::attack;
        meg = 1.0f;
        megAccented = accent;
    }

    void removeHeld (int note) noexcept
    {
        int w = 0;
        for (int r = 0; r < numHeld; ++r)
            if (held[(size_t) r] != note)
                held[(size_t) w++] = held[(size_t) r];
        numHeld = w;
    }

    float tick() noexcept
    {
        // Réglages lissés : pas de « zipper » en tournant les boutons.
        smoothed.cutoffOct += (targets.cutoffOct - smoothed.cutoffOct) * smoothCoef;
        smoothed.envOct += (targets.envOct - smoothed.envOct) * smoothCoef;
        smoothed.k += (targets.k - smoothed.k) * smoothCoef;
        smoothed.accent += (targets.accent - smoothed.accent) * smoothCoef;
        smoothed.drive += (targets.drive - smoothed.drive) * smoothCoef;
        smoothed.gain += (targets.gain - smoothed.gain) * smoothCoef;

        // Hauteur, avec glissement exponentiel (RC) sur les notes liées.
        if (gliding)
            pitch += (targetPitch - pitch) * glideCoef;

        const float note = pitch + params.tune + bend * params.bendRange;
        const float inc = juce::jlimit (0.0f, 0.25f, detail::noteToHz (note) / osRate);

        // Oscillateur à tables d'onde (sans repliement).
        float osc = waveTables.read (params.wave, phase, inc * osRate);
        if (params.wave != 0)
        {
            const float sq = osc;
            // Le carré de la machine d'origine sort d'un transistor : ses
            // paliers penchent légèrement (passe-haut doux vers 70 Hz).
            squareLow += (sq - squareLow) * squareHpCoef;
            osc = sq - 0.85f * squareLow;
        }
        phase += inc;
        if (phase >= 1.0f)
            phase -= 1.0f;

        // Enveloppe du filtre (non soumise à la porte, comme sur l'original).
        meg *= megAccented ? megAccentCoef : megCoef;

        // Accent : le niveau d'accent suit la note, et le coup de cutoff passe
        // par un condensateur qui s'accumule d'une note accentuée à l'autre.
        const float accentAmount = accented ? smoothed.accent : 0.0f;
        accentVca += (accentAmount - accentVca) * accentVcaCoef;
        accentSweep += ((accented ? meg * smoothed.accent : 0.0f) - accentSweep) * accentSweepCoef;

        // Ampli : attaque rapide, longue décroissance tant que la note est
        // tenue, relâchement presque immédiat.
        switch (vcaStage)
        {
            case VcaStage::attack:
                vca += (1.02f - vca) * attackCoef;
                if (vca >= 1.0f) { vca = 1.0f; vcaStage = VcaStage::decay; }
                break;
            case VcaStage::decay:
                vca -= vca * vcaDecayCoef;
                break;
            case VcaStage::release:
                vca -= vca * releaseCoef;
                if (vca < 1.0e-5f) { vca = 0.0f; vcaStage = VcaStage::idle; }
                break;
            case VcaStage::idle:
                break;
        }

        // Coupure du filtre.
        const float logF = smoothed.cutoffOct + smoothed.envOct * meg + 6.0f * accentSweep;
        const float fc = juce::jlimit (15.0f, juce::jmin (20000.0f, 0.2f * osRate), std::exp2 (logF));
        lastCutoff = fc;
        const float g = std::tan (juce::MathConstants<float>::pi * (fc / DiodeLadder::peakRatio) / osRate);

        // Le filtre reçoit plus de signal sur un accent (le « mordant ») et une
        // compensation partielle des graves que la résonance creuse.
        const float k = smoothed.k;
        // Niveau d'entrée choisi pour que la saturation reste douce : poussé plus
        // fort, ce modèle part en sous-harmoniques à résonance maximale.
        const float in = osc * 0.07f * (1.0f + 0.6f * accentVca) * (1.0f + 0.3f * k);
        const float filtered = filter.process (in, g, k) * 12.5f;

        // Étage de sortie : l'ampli arrondit les crêtes des accents résonants
        // au lieu de les laisser dépasser franchement.
        float outSample = 1.25f * detail::fastTanh (filtered * vca * (1.0f + accentVca) * 0.8f);

        // Saturation optionnelle : écrêtage doux asymétrique, niveau compensé.
        if (smoothed.drive > 0.001f)
        {
            const float d = smoothed.drive;
            const float pre = 1.0f + 30.0f * d * d;
            const float bias = 0.25f * d;
            const float shaped = (detail::fastTanh (pre * outSample + bias) - detail::fastTanh (bias))
                               / (1.0f + 0.03f * (pre - 1.0f));
            const float mix = juce::jmin (1.0f, d * 4.0f);
            outSample += (shaped - outSample) * mix;
        }

        return outSample * smoothed.gain;
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

        // Retire la composante continue (saturation asymétrique).
        for (int i = 0; i < numSamples; ++i)
        {
            const float xIn = out[i];
            dcY = xIn - dcX + dcCoef * dcY;
            dcX = xIn;
            out[i] = dcY;
        }
    }

    Params params;
    Targets targets, smoothed;
    DiodeLadder filter;
    WaveTables waveTables;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    float baseRate = 44100.0f, osRate = 176400.0f;
    int maxBlock = 512;

    float smoothCoef = 0.0f, attackCoef = 0.0f, releaseCoef = 0.0f, vcaDecayCoef = 0.0f;
    float accentVcaCoef = 0.0f, accentSweepCoef = 0.0f, glideCoef = 0.0f, squareHpCoef = 0.0f;
    float megCoef = 0.0f, megAccentCoef = 0.0f, dcCoef = 0.0f;

    std::array<int, 16> held {};
    int numHeld = 0;

    float pitch = 36.0f, targetPitch = 36.0f, bend = 0.0f;
    bool gliding = false;
    float phase = 0.0f, squareLow = 0.0f;

    VcaStage vcaStage = VcaStage::idle;
    float vca = 0.0f, meg = 0.0f, accentSweep = 0.0f, accentVca = 0.0f;
    bool accented = false, megAccented = false;
    float lastCutoff = 0.0f;
    float dcX = 0.0f, dcY = 0.0f;
};

} // namespace lp303
