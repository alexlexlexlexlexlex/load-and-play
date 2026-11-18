#pragma once

// Effets ajoutés après le synthé (absents du Minimoog d'origine) :
// overdrive -> chorus BBD -> autopan -> delay. Tous coupés, le signal passe intact.

#include "SynthEngine.h"

#include <vector>

namespace modeld
{

struct EffectsParams
{
    bool driveOn = false;
    float driveGain = 5.0f;         // 0..10
    float driveLowCut = 20.0f;      // Hz
    float driveHighCut = 20000.0f;  // Hz

    int chorusMode = 0;             // 0 = coupé, 1 = I, 2 = II, 3 = I+II (façon Juno)
    float chorusLevel = 10.0f;      // 0..10, 10 = mélange égal sec / retardé comme le Juno

    bool autopan = false;           // suit la vitesse du LFO

    bool delayOn = false;
    float delayLevel = 3.0f;        // 0..10
    float delayTime = 0.35f;        // secondes, sans synchro
    bool delaySync = false;
    int delayDivision = 5;          // index dans divisionBeats()
    bool delayRightDouble = false;  // canal droit deux fois plus rapide
    float delayFeedback = 4.0f;     // 0..10

    bool anyOn() const noexcept { return driveOn || chorusMode > 0 || autopan || delayOn; }
};

class Effects
{
public:
    void prepare (double sampleRate)
    {
        rate = (float) sampleRate;

        for (auto* line : { &chorusL, &chorusR })
            line->prepare ((int) (rate * 0.05f));
        for (auto* line : { &delayL, &delayR })
            line->prepare ((int) (rate * 4.5f));

        lowCut = highCut = {};
        chorusInput = chorusOutL = chorusOutR = {};
        for (auto* filter : { &chorusInput, &chorusOutL, &chorusOutR })
            filter->setCutoff (9000.0f, rate);
        feedbackL = feedbackR = 0.0f;
        chorusPhase = panPhase = 0.0f;
        delaySamplesL = delaySamplesR = -1.0f;
        smoothCoef = detail::rcCoef (0.05f, rate, 1.0f);
    }

    // panHz : vitesse de l'autopan (celle du LFO).
    void setParams (const EffectsParams& p, double bpm, float panHz) noexcept
    {
        params = p;
        panIncrement = panHz / rate;

        const float beatSeconds = (float) (60.0 / juce::jlimit (20.0, 400.0, bpm));
        const float seconds = p.delaySync ? (float) divisionBeats (p.delayDivision) * beatSeconds : p.delayTime;
        targetL = juce::jlimit (1.0f, rate * 4.0f, seconds * rate);
        targetR = p.delayRightDouble ? targetL * 0.5f : targetL;
        if (delaySamplesL < 0.0f)
        {
            delaySamplesL = targetL;
            delaySamplesR = targetR;
        }

        // Chorus façon Juno. Valeurs mesurées sur le Juno-60, dont le chorus est
        // très proche de celui du 106 : I à 0,513 Hz et II à 0,863 Hz, retard
        // de 1,66 à 5,35 ms ; I+II à 9,75 Hz, de 3,3 à 3,7 ms, en mono.
        switch (p.chorusMode)
        {
            case 1:  setChorus (0.513f, 0.00166f, 0.00535f, true); break;
            case 2:  setChorus (0.863f, 0.00166f, 0.00535f, true); break;
            case 3:  setChorus (9.75f, 0.0033f, 0.0037f, false); break;
            default: break;
        }

        lowCut.setCutoff (p.driveLowCut, rate);
        highCut.setCutoff (p.driveHighCut, rate);
        driveAmount = std::pow (10.0f, p.driveGain / 10.0f * 1.5f);   // 0 à +30 dB
        driveNorm = 0.5f / std::tanh (driveAmount * 0.5f);
    }

    // left contient le signal mono du synthé ; left et right reçoivent la sortie stéréo.
    void process (float* left, float* right, int numSamples) noexcept
    {
        const float chorusMix = params.chorusLevel / 10.0f;
        const float delayWet = params.delayLevel / 10.0f;
        const float feedback = params.delayFeedback / 10.0f * 0.9f;
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        for (int i = 0; i < numSamples; ++i)
        {
            float x = left[i];

            if (params.driveOn)
            {
                x = lowCut.highPass (x);
                x = std::tanh (x * driveAmount) * driveNorm;
                x = highCut.lowPass (x);
            }

            float l = x, r = x;

            if (params.chorusMode > 0)
            {
                // Un LFO triangle module deux lignes à retard (les BBD du Juno),
                // celle de droite en opposition de phase. Les BBD sont précédés et
                // suivis d'un filtre passe-bas et saturent un peu.
                const float tri = detail::Oscillator::triangle (chorusPhase);
                chorusPhase += chorusIncrement;
                if (chorusPhase >= 1.0f)
                    chorusPhase -= std::floor (chorusPhase);

                const float input = chorusInput.process (x);
                chorusL.push (input);
                chorusR.push (input);

                const float spread = chorusStereo ? -1.0f : 1.0f;
                const float wetL = chorusOutL.process (detail::fastTanh (chorusL.read ((chorusCentre + chorusDepth * tri) * rate)));
                const float wetR = chorusOutR.process (detail::fastTanh (chorusR.read ((chorusCentre + spread * chorusDepth * tri) * rate)));

                const float norm = 1.0f / (1.0f + 0.5f * chorusMix);
                l = (x + chorusMix * wetL) * norm;
                r = (x + chorusMix * wetR) * norm;
            }

            if (params.autopan)
            {
                // Panoramique à puissance constante, niveau inchangé au centre.
                const float angle = juce::MathConstants<float>::pi * 0.25f * (1.0f + std::sin (twoPi * panPhase));
                panPhase += panIncrement;
                if (panPhase >= 1.0f)
                    panPhase -= std::floor (panPhase);

                l *= juce::MathConstants<float>::sqrt2 * std::cos (angle);
                r *= juce::MathConstants<float>::sqrt2 * std::sin (angle);
            }

            if (params.delayOn)
            {
                delaySamplesL += (targetL - delaySamplesL) * smoothCoef;
                delaySamplesR += (targetR - delaySamplesR) * smoothCoef;

                const float echoL = delayL.read (delaySamplesL);
                const float echoR = delayR.read (delaySamplesR);

                // Répétitions légèrement assombries, comme un delay analogique.
                feedbackL += (echoL - feedbackL) * 0.35f;
                feedbackR += (echoR - feedbackR) * 0.35f;
                delayL.push (l + feedbackL * feedback);
                delayR.push (r + feedbackR * feedback);

                l += echoL * delayWet;
                r += echoR * delayWet;
            }

            left[i] = l;
            right[i] = r;
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

        // Lecture interpolée, delaySamples échantillons en arrière.
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

    struct OnePole
    {
        float coef = 1.0f, state = 0.0f;

        void setCutoff (float hz, float sampleRate) noexcept
        {
            coef = detail::rcCoef (1.0f / (juce::MathConstants<float>::twoPi * hz), sampleRate, 1.0f);
        }

        float lowPass (float x) noexcept  { state += (x - state) * coef; return state; }
        float highPass (float x) noexcept { state += (x - state) * coef; return x - state; }
    };

    // Passe-bas 12 dB/oct (deux pôles), comme autour des BBD.
    struct TwoPole
    {
        OnePole a, b;

        void setCutoff (float hz, float sampleRate) noexcept
        {
            a.setCutoff (hz, sampleRate);
            b.setCutoff (hz, sampleRate);
        }

        float process (float x) noexcept { return b.lowPass (a.lowPass (x)); }
    };

    void setChorus (float hz, float minDelay, float maxDelay, bool stereo) noexcept
    {
        chorusIncrement = hz / rate;
        chorusCentre = 0.5f * (minDelay + maxDelay);
        chorusDepth = 0.5f * (maxDelay - minDelay);
        chorusStereo = stereo;
    }

    EffectsParams params;
    float rate = 44100.0f;

    OnePole lowCut, highCut;
    float driveAmount = 1.0f, driveNorm = 1.0f;

    DelayLine chorusL, chorusR, delayL, delayR;
    TwoPole chorusInput, chorusOutL, chorusOutR;
    float chorusIncrement = 0.0f, chorusCentre = 0.0035f, chorusDepth = 0.0f;
    bool chorusStereo = true;
    float chorusPhase = 0.0f, panPhase = 0.0f, panIncrement = 0.0f;
    float targetL = 1.0f, targetR = 1.0f, delaySamplesL = -1.0f, delaySamplesR = -1.0f;
    float feedbackL = 0.0f, feedbackR = 0.0f, smoothCoef = 0.001f;
};

} // namespace modeld
