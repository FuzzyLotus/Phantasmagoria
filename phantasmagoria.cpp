// ============================================================
// PHANTASMAGORIA v31b — Spectral Delay for Daisy Seed / Terrarium
//
//   K1 Delay Time   K2 Feedback   K3 Reverb   K4 Tape Depth
//   K5 LFO Speed    K6 Mix
//   SW1 Time Direction (fwd/rev)
//   SW2 Smear (multi-tap diffusion of repeats)
//   SW3 Erosion (repeats age and darken over time)
//   SW4 Freeze Evolution (stable → living texture)
//   FS1 Bypass    FS2 Freeze (tap=toggle, hold=accumulate)
//
// ============================================================
// SWITCH FIX CHANGELOG (v31 → v31b):
//
//  SW2 — SMEAR (was inaudible)
//    Problem: tap offsets of 46 and 112 samples (~1ms, 2.3ms) read
//    nearly identical content from the delay line.  Blending 3 copies
//    of the same signal produces no audible change.
//    Fix: offsets widened to 500 and 1200 samples (~10ms, 25ms).
//    At these distances the taps read meaningfully different content,
//    creating true temporal smear / diffusion.  Smear blend applied
//    to the wet-bus signal path so it's audible at any feedback level.
//
//  SW3 — EROSION (was inaudible)
//    Problem: erosion only shaped the feedback signal, not the audible
//    wet bus.  First repeat was untouched, and you'd need 4-5 feedback
//    passes before the darkening/level reduction became audible.
//    Fix: erosion filter now processes delRd directly.  What you hear
//    in the wet bus is the eroded signal.  First repeat is lightly
//    affected; subsequent repeats compound the erosion dramatically
//    since the feedback also carries the eroded signal.
//
// ============================================================
// Retains all v30c dynamics / hi-fi improvements and v31 SW4 freeze
// evolution, K5 curve shaping, LED accumulate dimming.
// ============================================================

#include "daisy_petal.h"
#include "daisysp.h"
#include "terrarium.h"
#include <cmath>
#include <cstring>
#include <algorithm>

using namespace daisy;
using namespace daisysp;
using namespace terrarium;

static DaisyPetal petal;

// ============================================================
// COMPILE-TIME CONSTANTS
// ============================================================
static constexpr float TWO_PI_F      = 6.283185307f;
static constexpr float SR_F          = 48000.f;
static constexpr float SR_OVER_1000  = 48.f;
static constexpr float HALF_PI_F     = 1.5707963f;
static constexpr float MAX_DELAY_LIMIT = 95998.f;

// Reverb echo-chamber tap positions
static constexpr int REV_TAP_A = 3984;    // 83  ms
static constexpr int REV_TAP_B = 7248;    // 151 ms
static constexpr int REV_TAP_C = 10896;   // 227 ms
static constexpr int REV_TAP_D = 14928;   // 311 ms

// ============================================================
// FAST MATH
// ============================================================
static inline float gentle_saturate(float x) {
    float ax = fabsf(x);
    if (ax < 0.8f) return x;
    float sign = (x >= 0.f) ? 1.f : -1.f;
    return sign * (0.8f + (ax - 0.8f) / (1.f + (ax - 0.8f) * 0.5f));
}

static inline float fast_tanh(float x) {
    return x / (1.f + fabsf(x));
}

static inline float fast_sin_hp(float x) {
    float x2 = x * x;
    return x * (1.f - x2 * (0.16666667f - x2 * 0.00833333f));
}

static inline float fast_cos_hp(float x) {
    float x2 = x * x;
    return 1.f - x2 * (0.5f - x2 * 0.04166667f);
}

// ============================================================
// INLINE FILTERS
// ============================================================
struct Lp1 {
    float y, c;
    void Init(float freq, float sr) { y = 0.f; c = 1.f - expf(-TWO_PI_F * freq / sr); }
    void SetFreq(float freq, float sr) { c = 1.f - expf(-TWO_PI_F * freq / sr); }
    float Process(float x) { y += c * (x - y); return y; }
};

struct Hp1 {
    float y, c;
    void Init(float freq, float sr) { y = 0.f; c = 1.f - expf(-TWO_PI_F * freq / sr); }
    float Process(float x) { y += c * (x - y); return x - y; }
};

// ============================================================
// SDRAM
// ============================================================
#define MAX_DELAY  96000
#define REV_SIZE   48000
#define FZ_A       4657
#define FZ_B       7153
#define FZ_C       9553

static float DSY_SDRAM_BSS mainBuf[MAX_DELAY];
static float DSY_SDRAM_BSS revBuf[REV_SIZE];
static float DSY_SDRAM_BSS fzBufA[FZ_A];
static float DSY_SDRAM_BSS fzBufB[FZ_B];
static float DSY_SDRAM_BSS fzBufC[FZ_C];

// ============================================================
// DELAY BUFFER
// ============================================================
struct DelBuf {
    float* buf;
    size_t len, wp;

    void Init(float* mem, size_t n) {
        buf = mem; len = n; wp = 0;
        memset(buf, 0, n * sizeof(float));
    }
    void Write(float s) {
        buf[wp] = s;
        wp = (wp + 1) % len;
    }
    float Read(float samps) const {
        samps = std::max(1.f, std::min(samps, (float)(len - 2)));
        float r = (float)wp - samps;
        if (r < 0.f) r += (float)len;
        int i0 = (int)r;
        float fr = r - (float)i0;
        return buf[i0 % len] * (1.f - fr) + buf[(i0 + 1) % len] * fr;
    }
    float ReadFixed(int samps) const {
        int p = (int)wp - samps;
        if (p < 0) p += (int)len;
        return buf[p % len];
    }
};

// ============================================================
// GRAIN READER
// ============================================================
struct GrainReader {
    float phase, freq, sweepMs, offsetMs;
    bool  dual;

    float Process(const DelBuf& buf, float sr, float modSamps = 0.f) {
        float ph = phase;
        phase += freq / sr;
        while (phase >= 1.f) phase -= 1.f;

        float dA = 2.f * ph - 1.f;
        float winA = 1.f - (dA < 0.f ? -dA : dA);
        float sampA = ph * sweepMs * sr / 1000.f + offsetMs * sr / 1000.f + modSamps;
        float grainA = buf.Read(sampA) * winA;

        if (!dual) return grainA;

        float phB = ph + 0.5f;
        if (phB >= 1.f) phB -= 1.f;
        float dB = 2.f * phB - 1.f;
        float winB = 1.f - (dB < 0.f ? -dB : dB);
        float sampB = phB * sweepMs * sr / 1000.f + offsetMs * sr / 1000.f + modSamps;
        float grainB = buf.Read(sampB) * winB;

        float wSum = std::max(winA + winB, 0.001f);
        return (grainA + grainB) / wSum;
    }
};

// ============================================================
// FREEZE VOICE
// ============================================================
struct FreezeVoice {
    DelBuf buf;
    float  bufSamps, fb;
    Lp1*   lp;
    Hp1*   hp;

    void Init(float* mem, size_t sz, float ms, float sr, Lp1* lp_, Hp1* hp_) {
        buf.Init(mem, sz);
        bufSamps = ms * sr / 1000.f;
        fb = 0.f;
        lp = lp_;
        hp = hp_;
    }

    float Process(float input, float dryGt, float holdGt, float modSamps) {
        buf.Write(input * dryGt + fb * holdGt);
        float rd = buf.Read(bufSamps + modSamps);
        fb = rd;
        float sig = rd * holdGt;
        sig = lp->Process(sig);
        sig = hp->Process(sig);
        return sig;
    }
};

// ============================================================
// DSP OBJECTS
// ============================================================
static DelBuf mainDelay, revDelay;
static FreezeVoice fzVoice[3];
static GrainReader revReader;

static Lp1  fbLpf, revLpf;
static Hp1  revHpf;
static Lp1  erosionLpf;     // SW3: dynamic feedback darkening
static Lp1  fzLp[3];
static Hp1  fzHp[3];

static Oscillator lfo1, lfo2, lfo3;

// ============================================================
// SMOOTHED PARAMS
// ============================================================
static float tDelay = 2400.f, sDelay = 2400.f;
static float tFb = 0.f,       sFb = 0.f;
static float tRevMix = 0.f;
static float tTapeD = 0.f, tLfoSpd = 1.f;
static float tMixW = 0.f, sMixW = 0.f;
static float tSw1 = 0.f, sSw1 = 0.f;
static float tSw2 = 0.f, sSw2 = 0.f;
static float tSw3 = 0.f, sSw3 = 0.f;
static float tSw4 = 0.f, sSw4 = 0.f;
static float tEffG = 0.f, sEffG = 0.f;
static float tBypG = 1.f, sBypG = 1.f;
static float tFzW = 0.f, sFzW = 0.f;
static float tFzH = 0.f, sFzH = 0.f;
static float tAcc = 0.f, sAcc = 0.f;

static float fbSig = 0.f;
static float revFb = 0.f;

static bool effectOn = false, freezeOn = false, accumOn = false;
static bool fs2Prev = false, fs2Hold = false;
static uint32_t fs2Time = 0;

static Led led1, led2;
static Parameter pDelay, pFeedback, pReverb, pTape, pSpeed, pMix;

// ============================================================
// AUDIO CALLBACK
// ============================================================
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t size)
{
    // --- Block-rate: set LFO frequencies once per block ---
    lfo1.SetFreq(tLfoSpd * 0.7f);
    lfo2.SetFreq(tLfoSpd * 3.1f);
    lfo3.SetFreq(tLfoSpd * 0.13f);

    for (size_t i = 0; i < size; i++) {

        fonepole(sDelay, tDelay, 0.0002f);
        fonepole(sFb,    tFb,    0.005f);
        fonepole(sMixW,  tMixW,  0.005f);
        fonepole(sSw1,   tSw1,   0.002f);
        fonepole(sSw2,   tSw2,   0.002f);
        fonepole(sSw3,   tSw3,   0.002f);
        fonepole(sSw4,   tSw4,   0.002f);
        fonepole(sEffG,  tEffG,  0.002f);
        fonepole(sBypG,  tBypG,  0.002f);
        fonepole(sFzW,   tFzW,   0.0003f);
        float fzHc = (tFzH > sFzH) ? 0.0003f : 0.00002f;
        fonepole(sFzH,   tFzH,   fzHc);
        fonepole(sAcc,   tAcc,   0.005f);

        float dry = in[0][i];

        // Tape warble
        float modMs = (lfo1.Process()
        + lfo2.Process() * 0.3f
        + lfo3.Process() * 0.5f) * tTapeD;
        float modSamps = modMs * SR_OVER_1000;

        // ==== DELAY WRITE ====
        float delIn = dry + fbSig;
        if (sFb > 0.3f) delIn = fast_tanh(delIn * 0.5f) * 2.f;
        mainDelay.Write(delIn);

        // Forward read
        float fwdS = std::max(48.f, std::min(sDelay + modSamps, MAX_DELAY_LIMIT));
        float fwd = mainDelay.Read(fwdS);

        // Reverse read (dual grain, fixed sweep, now with tape warble)
        float rev = revReader.Process(mainDelay, SR_F, modSamps);

        // Fwd/Rev crossfade (SW1)
        float delRd = fwd * (1.f - sSw1) + rev * sSw1;

        // ==== SW2 — SMEAR ====
        // Multi-tap diffusion: reads at +10ms and +25ms offsets from
        // the main read position.  At these distances the taps contain
        // genuinely different signal content, creating real temporal
        // smear.  Grows with feedback so first repeat is mostly clean.
        //
        // Applied to delRd directly so it affects what you hear AND
        // what feeds back, compounding the smear across repeats.
        if (sSw2 > 0.01f) {
            float smearAmt = sSw2 * (0.25f + sFb * 0.75f);

            // Read at +10ms and +25ms from the main position
            float tapA = mainDelay.Read(
                std::max(48.f, std::min(fwdS + 500.f, MAX_DELAY_LIMIT)));
            float tapB = mainDelay.Read(
                std::max(48.f, std::min(fwdS + 1200.f, MAX_DELAY_LIMIT)));

            // In reverse mode, also offset the taps from the reverse
            // reader's region so smear works in both directions
            if (sSw1 > 0.5f) {
                tapA = tapA * (1.f - sSw1) + rev * sSw1;
                tapB = tapB * (1.f - sSw1) + rev * sSw1;
            }

            // Blend: main signal loses some level, taps fill in
            delRd = delRd * (1.f - smearAmt * 0.45f)
            + tapA * (smearAmt * 0.28f)
            + tapB * (smearAmt * 0.22f);
        }

        // ==== SW3 — EROSION ====
        // Dynamic low-pass that darkens the signal progressively.
        // Applied to delRd so you hear the erosion directly in the
        // wet bus, AND it feeds back into the delay, compounding
        // each pass.  First repeat is lightly darkened; later repeats
        // lose progressively more high-frequency content.
        //
        // Also applies gentle level attenuation so repeats fade
        // faster, like a decaying memory.
        if (sSw3 > 0.01f) {
            float erosionAmt = sSw3 * (0.3f + sFb * 0.7f);

            // Sweep the LPF: 7500 Hz (off) → 1200 Hz (full erosion)
            float erosionHz = 7500.f - erosionAmt * 6300.f;
            erosionLpf.SetFreq(erosionHz, SR_F);
            delRd = erosionLpf.Process(delRd);

            // Level attenuation: up to -6dB at full erosion
            delRd *= (1.f - erosionAmt * 0.5f);
        }

        // ==== FEEDBACK ====
        // fbLpf always runs at 7500 Hz as the baseline tone shaper.
        // SW3 erosion has already darkened delRd further if active.
        fbSig = fbLpf.Process(delRd) * sFb;

        // Reverb (echo chamber)
        float revIn = revLpf.Process(delRd + revFb * 0.75f);
        revDelay.Write(revIn);
        float rvSum = (revDelay.ReadFixed(REV_TAP_A)
        + revDelay.ReadFixed(REV_TAP_B)
        + revDelay.ReadFixed(REV_TAP_C)
        + revDelay.ReadFixed(REV_TAP_D)) * 0.25f;
        revFb = rvSum;
        float rvOut = revHpf.Process(rvSum) * tRevMix * 0.8f;

        // ==== WET BUS ====
        float wet = gentle_saturate(delRd + rvOut);

        // ==== FREEZE ====
        float dryGt  = (1.f - sFzW) + sFzW * sAcc * 0.5f;
        float holdGt = sFzH;

        // SW4 — FREEZE EVOLUTION
        // Ultra-slow independent drift inside freeze voices.
        // Not tied to tape depth or LFO speed.
        float fzMod = modSamps * 0.05f;
        static float evolvePhase = 0.f;
        if (sSw4 > 0.01f) {
            float evolveAmt = sSw4 * sFzH;
            evolvePhase += 0.00003f;
            if (evolvePhase > 1.f) evolvePhase -= 1.f;
            float evolveDrift = sinf(evolvePhase * TWO_PI_F) * 6.0f * evolveAmt;
            fzMod += evolveDrift;
        }

        float fzOut = 0.f;
        for (int v = 0; v < 3; v++)
            fzOut += fzVoice[v].Process(dry, dryGt, holdGt, fzMod);
        fzOut *= 0.45f;

        // Output: constant-power crossfade
        float angle = sMixW * HALF_PI_F;
        float effSig = (dry * fast_cos_hp(angle) + wet * fast_sin_hp(angle)) + fzOut;

        float output;
        if (sBypG > 0.999f) {
            output = dry;
        } else {
            output = gentle_saturate(effSig * sEffG);
        }

        out[0][i] = output;
        out[1][i] = output;
    }
}

// ============================================================
// CONTROL UPDATE
// ============================================================
static void UpdateControls()
{
    petal.ProcessAnalogControls();
    petal.ProcessDigitalControls();
    led1.Update();
    led2.Update();

    tDelay  = (pDelay.Process() * 1780.f + 20.f) * SR_OVER_1000;
    tFb     = pFeedback.Process();
    tRevMix = pReverb.Process();
    float k4 = pTape.Process();
    float k4Norm = std::max(0.f, std::min((k4 - 0.2f) / (8.0f - 0.2f), 1.f));
    k4Norm = powf(k4Norm, 2.1f);
    tTapeD = 0.2f + k4Norm * (8.0f - 0.2f);

    // K5 LFO Speed: piecewise curve for wider sweet spot
    float k5 = pSpeed.Process();
    float k5Norm = std::max(0.f, std::min((k5 - 0.1f) / (6.0f - 0.1f), 1.f));
    float k5Shaped;
    if (k5Norm < 0.60f) {
        float x = k5Norm / 0.60f;
        k5Shaped = 0.38f * powf(x, 1.85f);
    } else {
        float x = (k5Norm - 0.60f) / 0.40f;
        k5Shaped = 0.38f + 0.62f * powf(x, 0.90f);
    }
    tLfoSpd = 0.1f + k5Shaped * (6.0f - 0.1f);

    tMixW   = pMix.Process();

    tSw1 = petal.switches[Terrarium::SWITCH_1].Pressed() ? 1.f : 0.f;
    tSw2 = petal.switches[Terrarium::SWITCH_2].Pressed() ? 1.f : 0.f;
    tSw3 = petal.switches[Terrarium::SWITCH_3].Pressed() ? 1.f : 0.f;
    tSw4 = petal.switches[Terrarium::SWITCH_4].Pressed() ? 1.f : 0.f;

    if (petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge()) {
        effectOn = !effectOn;
    }
    tEffG = effectOn ? 1.f : 0.f;
    tBypG = effectOn ? 0.f : 1.f;
    led1.Set(effectOn ? 1.f : 0.f);

    bool fs2Now = petal.switches[Terrarium::FOOTSWITCH_2].Pressed();
    if (fs2Now && !fs2Prev)  { fs2Time = System::GetNow(); fs2Hold = false; }
    if (fs2Now && !fs2Hold && freezeOn && (System::GetNow() - fs2Time > 300))
    { accumOn = true; fs2Hold = true; }
    if (!fs2Now && fs2Prev) {
        if (fs2Hold) accumOn = false;
        else { freezeOn = !freezeOn; if (!freezeOn) accumOn = false; }
    }
    fs2Prev = fs2Now;

    led2.Set(freezeOn ? (accumOn ? 0.75f : 1.f) : 0.f);
    tFzH = freezeOn ? 1.f : 0.f;
    tFzW = freezeOn ? 1.f : 0.f;
    tAcc = accumOn  ? 1.f : 0.f;
}

// ============================================================
// MAIN
// ============================================================
int main()
{
    petal.Init();
    petal.SetAudioBlockSize(1);
    petal.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    float sr = petal.AudioSampleRate();

    mainDelay.Init(mainBuf, MAX_DELAY);
    revDelay.Init(revBuf, REV_SIZE);

    float* fzMem[3] = {fzBufA, fzBufB, fzBufC};
    size_t fzSz[3]  = {FZ_A, FZ_B, FZ_C};
    float  fzMs[3]  = {97.f, 149.f, 199.f};
    for (int i = 0; i < 3; i++) {
        fzLp[i].Init(5000.f, sr);
        fzHp[i].Init(80.f, sr);
        fzVoice[i].Init(fzMem[i], fzSz[i], fzMs[i], sr, &fzLp[i], &fzHp[i]);
    }

    fbLpf.Init(7500.f, sr);
    revLpf.Init(9000.f, sr);
    revHpf.Init(55.f, sr);
    erosionLpf.Init(7500.f, sr);  // SW3: starts at full bandwidth

    //                   phase  freq    sweepMs  offsetMs  dual
    revReader   =       {0.f,   1.f,    1999.f,  1.f,      true};

    lfo1.Init(sr); lfo1.SetWaveform(Oscillator::WAVE_SIN);
    lfo1.SetAmp(1.f); lfo1.SetFreq(0.7f);
    lfo2.Init(sr); lfo2.SetWaveform(Oscillator::WAVE_SIN);
    lfo2.SetAmp(1.f); lfo2.SetFreq(3.1f);
    lfo3.Init(sr); lfo3.SetWaveform(Oscillator::WAVE_SIN);
    lfo3.SetAmp(1.f); lfo3.SetFreq(0.13f);

    pDelay.Init(petal.knob[Terrarium::KNOB_1],    0.f, 1.f,  Parameter::LINEAR);
    pFeedback.Init(petal.knob[Terrarium::KNOB_2], 0.f, 0.95f,Parameter::LINEAR);
    pReverb.Init(petal.knob[Terrarium::KNOB_3],   0.f, 1.f,  Parameter::LINEAR);
    pTape.Init(petal.knob[Terrarium::KNOB_4],     0.2f,8.f,  Parameter::LINEAR);
    pSpeed.Init(petal.knob[Terrarium::KNOB_5],    0.1f,6.f,  Parameter::LINEAR);
    pMix.Init(petal.knob[Terrarium::KNOB_6],      0.f, 1.f,  Parameter::LINEAR);

    led1.Init(petal.seed.GetPin(Terrarium::LED_1), false);
    led2.Init(petal.seed.GetPin(Terrarium::LED_2), false);

    effectOn = false;
    petal.StartAdc();
    petal.StartAudio(AudioCallback);

    while (true) {
        UpdateControls();
        System::Delay(1);
    }
}
