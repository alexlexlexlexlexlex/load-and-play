// L'égaliseur de voie, sur le modèle de celui de Cubase : 4 bandes (1 : grave en
// plateau, cloche ou coupe-bas ; 2 et 3 : cloches ; 4 : aigu en plateau, cloche ou
// coupe-haut). Filtres biquad (formules de R. Bristow-Johnson) ; les coupes sont à
// 24 dB par octave (deux cellules de Butterworth). Sans état partagé : testable.
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <complex>

namespace gridplay
{
enum class EqType { peak, lowShelf, highShelf, lowCut, highCut };

struct EqBand
{
    bool on = true;
    EqType type = EqType::peak;
    float frequency = 1000.0f;   // Hz
    float gain = 0.0f;           // dB (sans effet sur les coupes)
    float q = 0.7f;

    bool operator== (const EqBand& o) const
    {
        return on == o.on && type == o.type && juce::exactlyEqual (frequency, o.frequency)
            && juce::exactlyEqual (gain, o.gain) && juce::exactlyEqual (q, o.q);
    }
    bool operator!= (const EqBand& o) const { return ! (*this == o); }

    // Une cloche ou un plateau à 0 dB ne change rien : on ne le calcule pas.
    bool isNeutral() const
    {
        return ! on || ((type == EqType::peak || type == EqType::lowShelf || type == EqType::highShelf) && std::abs (gain) < 0.01f);
    }
};

struct ChannelEq
{
    bool bypass = false;
    std::array<EqBand, 4> bands {{ { true, EqType::lowShelf, 100.0f, 0.0f, 0.7f },
                                   { true, EqType::peak, 400.0f, 0.0f, 1.0f },
                                   { true, EqType::peak, 2500.0f, 0.0f, 1.0f },
                                   { true, EqType::highShelf, 8000.0f, 0.0f, 0.7f } }};

    bool operator== (const ChannelEq& o) const { return bypass == o.bypass && bands == o.bands; }
    bool operator!= (const ChannelEq& o) const { return ! (*this == o); }

    bool isNeutral() const
    {
        if (bypass)
            return true;
        for (const auto& b : bands)
            if (! b.isNeutral())
                return false;
        return true;
    }
};

inline const char* eqTypeName (EqType t)
{
    switch (t)
    {
        case EqType::peak:      return "Cloche";
        case EqType::lowShelf:  return "Plateau grave";
        case EqType::highShelf: return "Plateau aigu";
        case EqType::lowCut:    return "Coupe-bas";
        case EqType::highCut:   return "Coupe-haut";
    }
    return "";
}

//==============================================================================
struct BiquadCoefficients
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    std::complex<double> response (double frequency, double sampleRate) const
    {
        const auto z = std::polar (1.0, -juce::MathConstants<double>::twoPi * frequency / sampleRate);
        const auto z2 = z * z;
        return (b0 + b1 * z + b2 * z2) / (1.0 + a1 * z + a2 * z2);
    }
};

// Une ou deux cellules par bande (deux pour les coupes à 24 dB).
inline std::array<BiquadCoefficients, 2> designBand (const EqBand& band, double sampleRate, int& stages)
{
    std::array<BiquadCoefficients, 2> out {};
    stages = 0;
    if (band.isNeutral())
        return out;

    const double f = juce::jlimit (10.0, sampleRate * 0.49, (double) band.frequency);
    const double w0 = juce::MathConstants<double>::twoPi * f / sampleRate;
    const double cosw = std::cos (w0), sinw = std::sin (w0);

    auto normalise = [] (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        return BiquadCoefficients { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    };

    switch (band.type)
    {
        case EqType::peak:
        {
            const double A = std::pow (10.0, band.gain / 40.0);
            const double alpha = sinw / (2.0 * juce::jmax (0.1, (double) band.q));
            out[0] = normalise (1.0 + alpha * A, -2.0 * cosw, 1.0 - alpha * A, 1.0 + alpha / A, -2.0 * cosw, 1.0 - alpha / A);
            stages = 1;
            break;
        }
        case EqType::lowShelf:
        case EqType::highShelf:
        {
            const double A = std::pow (10.0, band.gain / 40.0);
            const double alpha = sinw / (2.0 * juce::jmax (0.1, (double) band.q));
            const double s = 2.0 * std::sqrt (A) * alpha;
            if (band.type == EqType::lowShelf)
                out[0] = normalise (A * ((A + 1) - (A - 1) * cosw + s), 2 * A * ((A - 1) - (A + 1) * cosw), A * ((A + 1) - (A - 1) * cosw - s),
                                    (A + 1) + (A - 1) * cosw + s, -2 * ((A - 1) + (A + 1) * cosw), (A + 1) + (A - 1) * cosw - s);
            else
                out[0] = normalise (A * ((A + 1) + (A - 1) * cosw + s), -2 * A * ((A - 1) + (A + 1) * cosw), A * ((A + 1) + (A - 1) * cosw - s),
                                    (A + 1) - (A - 1) * cosw + s, 2 * ((A - 1) - (A + 1) * cosw), (A + 1) - (A - 1) * cosw - s);
            stages = 1;
            break;
        }
        case EqType::lowCut:
        case EqType::highCut:
        {
            // Butterworth d'ordre 4 : deux cellules de Q 0,541 et 1,307.
            const double qs[2] = { 0.5411961, 1.3065630 };
            for (int i = 0; i < 2; ++i)
            {
                const double alpha = sinw / (2.0 * qs[i]);
                if (band.type == EqType::lowCut)
                    out[(size_t) i] = normalise ((1 + cosw) / 2, -(1 + cosw), (1 + cosw) / 2, 1 + alpha, -2 * cosw, 1 - alpha);
                else
                    out[(size_t) i] = normalise ((1 - cosw) / 2, 1 - cosw, (1 - cosw) / 2, 1 + alpha, -2 * cosw, 1 - alpha);
            }
            stages = 2;
            break;
        }
    }
    return out;
}

// Gain de l'égaliseur entier à une fréquence, en dB (pour la courbe affichée).
inline float eqResponseDb (const ChannelEq& eq, double frequency, double sampleRate)
{
    if (eq.bypass)
        return 0.0f;
    double magnitude = 1.0;
    for (const auto& band : eq.bands)
    {
        int stages = 0;
        const auto coefficients = designBand (band, sampleRate, stages);
        for (int s = 0; s < stages; ++s)
            magnitude *= std::abs (coefficients[(size_t) s].response (frequency, sampleRate));
    }
    return (float) juce::Decibels::gainToDecibels (magnitude, -120.0);
}

//==============================================================================
// Le traitement, en stéréo. Les coefficients sont recalculés quand les réglages
// changent ; les états des filtres sont gardés, pour ne pas faire de clic.
class EqProcessor
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
        dirty = true;
    }

    void reset()
    {
        for (auto& band : state)
            for (auto& stage : band)
                for (auto& ch : stage)
                    ch = {};
    }

    void process (juce::AudioBuffer<float>& buffer, int numSamples, const ChannelEq& eq)
    {
        if (dirty || eq != current)
        {
            current = eq;
            dirty = false;
            for (size_t b = 0; b < 4; ++b)
                coefficients[b] = designBand (eq.bands[b], sampleRate, stageCount[b]);
        }
        if (eq.bypass)
            return;

        const int channels = juce::jmin (2, buffer.getNumChannels());
        for (size_t b = 0; b < 4; ++b)
            for (int s = 0; s < stageCount[b]; ++s)
            {
                const auto& c = coefficients[b][(size_t) s];
                for (int ch = 0; ch < channels; ++ch)
                {
                    auto& z = state[b][(size_t) s][(size_t) ch];
                    float* data = buffer.getWritePointer (ch);
                    for (int i = 0; i < numSamples; ++i)
                    {
                        // Forme directe transposée II.
                        const double x = data[i];
                        const double y = c.b0 * x + z.z1;
                        z.z1 = c.b1 * x - c.a1 * y + z.z2;
                        z.z2 = c.b2 * x - c.a2 * y;
                        data[i] = (float) y;
                    }
                }
            }
    }

private:
    struct State { double z1 = 0.0, z2 = 0.0; };
    double sampleRate = 44100.0;
    bool dirty = true;
    ChannelEq current;
    std::array<std::array<BiquadCoefficients, 2>, 4> coefficients {};
    std::array<int, 4> stageCount {};
    std::array<std::array<std::array<State, 2>, 2>, 4> state {};
};
} // namespace gridplay
