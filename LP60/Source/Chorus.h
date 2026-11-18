#pragma once

// Chorus du Juno : deux lignes à retard BBD modulées par un LFO triangle,
// celle de droite en opposition de phase, dans les trois modes. Mesuré sur
// le JU-06A (tests F du kit) en suivant le retard au fil du temps :
//   I    : 0,552 Hz, excursion de 4,1 ms
//   II   : 0,887 Hz, excursion de 4,1 ms
//   I+II : 7,55 Hz,  excursion de 1,1 ms, stéréo lui aussi
// Le centre du retard (3,5 ms) vient des mesures du Juno-60 de pendragon-andyh.
// Souffle des BBD : un bruit propre à chaque ligne, sombre (passe-bas vers
// 1 kHz), à -76 dBFS par canal sur le JU-06A, présent même sans note.
// Estimé : passe-bas 12 dB/oct à 9 kHz autour des BBD, légère saturation.

#include "LP60Engine.h"

#include <vector>

namespace lp60
{

class Chorus
{
public:
    void prepare (double sampleRate)
    {
        rate = (float) sampleRate;
        lineL.prepare ((int) (rate * 0.02f));
        lineR.prepare ((int) (rate * 0.02f));
        input = outL = outR = {};
        for (auto* f : { &input, &outL, &outR })
            f->setCutoff (9000.0f, rate);
        phase = 0.0f;
        hissStateL = hissStateR = 0.0f;
        hissCoef = detail::rcCoef (1.0f / (juce::MathConstants<float>::twoPi * 1000.0f), rate);
        // Un bruit blanc filtré perd du niveau quand la fréquence d'échantillonnage monte : on compense.
        hissRateScale = std::sqrt (rate / 48000.0f);
    }

    // mode : 0 = coupé, 1 = I, 2 = II, 3 = I+II (les deux boutons enfoncés)
    void setMode (int newMode) noexcept
    {
        mode = newMode;
        switch (mode)
        {
            case 1:  set (0.552f, 0.00145f, 0.00555f, true); break;
            case 2:  set (0.887f, 0.00145f, 0.00555f, true); break;
            case 3:  set (7.55f, 0.00295f, 0.00405f, true); break;
            default: break;
        }
    }

    bool isOn() const noexcept { return mode > 0; }

    // Souffle : 0 = aucun, 5 = niveau du JU-06A (-76 dBFS par canal), 10 = +12 dB.
    void setHiss (float amount0to10) noexcept
    {
        hissGain = amount0to10 <= 0.0f
                     ? 0.0f
                     : hissAtFive * std::pow (2.0f, (juce::jlimit (0.0f, 10.0f, amount0to10) - 5.0f) * 0.4f);
    }

    // left contient le signal mono ; left et right reçoivent la sortie stéréo.
    void process (float* left, float* right, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = left[i];
            const float tri = 1.0f - 4.0f * std::abs (phase - 0.5f);
            phase += increment;
            if (phase >= 1.0f)
                phase -= std::floor (phase);

            const float filtered = input.process (x);
            lineL.push (filtered);
            lineR.push (filtered);

            const float spread = stereo ? -1.0f : 1.0f;
            hissStateL += (white (seedL) - hissStateL) * hissCoef;
            hissStateR += (white (seedR) - hissStateR) * hissCoef;
            const float hissL = hissGain * hissRateScale * hissStateL, hissR = hissGain * hissRateScale * hissStateR;
            const float wetL = outL.process (detail::fastTanh (lineL.read ((centre + depth * tri) * rate)) + hissL);
            const float wetR = outR.process (detail::fastTanh (lineR.read ((centre + spread * depth * tri) * rate)) + hissR);

            left[i] = (x + wetL) * 0.67f;
            right[i] = (x + wetR) * 0.67f;
        }
    }

private:
    struct DelayLine
    {
        std::vector<float> buffer;
        int write = 0;

        void prepare (int size)
        {
            buffer.assign ((size_t) juce::jmax (4, size), 0.0f);
            write = 0;
        }

        void push (float x) noexcept
        {
            buffer[(size_t) write] = x;
            if (++write == (int) buffer.size())
                write = 0;
        }

        float read (float delaySamples) const noexcept
        {
            const int size = (int) buffer.size();
            float pos = (float) write - 1.0f - juce::jlimit (0.0f, (float) (size - 2), delaySamples);
            if (pos < 0.0f)
                pos += (float) size;
            const int i0 = (int) pos;
            const int i1 = i0 + 1 == size ? 0 : i0 + 1;
            const float frac = pos - (float) i0;
            return buffer[(size_t) i0] + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);
        }
    };

    // Passe-bas 12 dB/oct (deux pôles).
    struct TwoPole
    {
        float coef = 1.0f, a = 0.0f, b = 0.0f;

        void setCutoff (float hz, float sampleRate) noexcept
        {
            coef = detail::rcCoef (1.0f / (juce::MathConstants<float>::twoPi * hz), sampleRate);
        }

        float process (float x) noexcept
        {
            a += (x - a) * coef;
            b += (a - b) * coef;
            return b;
        }
    };

    static float white (uint32_t& seed) noexcept
    {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return (float) seed * (2.0f / 4294967296.0f) - 1.0f;
    }

    void set (float hz, float minDelay, float maxDelay, bool isStereo) noexcept
    {
        increment = hz / rate;
        centre = 0.5f * (minDelay + maxDelay);
        depth = 0.5f * (maxDelay - minDelay);
        stereo = isStereo;
    }

    float rate = 44100.0f, phase = 0.0f, increment = 0.0f, centre = 0.0035f, depth = 0.0f;
    bool stereo = true;
    int mode = 0;
    float hissGain = 0.0f, hissCoef = 0.1f, hissRateScale = 1.0f, hissStateL = 0.0f, hissStateR = 0.0f;
    uint32_t seedL = 0x1234567u, seedR = 0x7654321u;

    // Gain du bruit pour -76 dBFS par canal en sortie (bruit blanc -> passe-bas
    // 1 kHz -> filtre de sortie -> mélange), réglé par le test RenderTest.
    static constexpr float hissAtFive = 0.00161f;
    DelayLine lineL, lineR;
    TwoPole input, outL, outR;
};

} // namespace lp60
