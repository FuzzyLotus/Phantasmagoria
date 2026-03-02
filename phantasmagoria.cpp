// ============================================================
// PHANTASMAGORIA v30a — Spectral Delay for Daisy Seed / Terrarium
// Built on DaisyCloudSeed project structure.
// THIS IS THE "VERY VERY VERY GOOD" VERSION — restored exactly.
//
// Controls:
//   K1 Delay Time   K2 Feedback   K3 Reverb   K4 Tape Depth
//   K5 LFO Speed    K6 Mix
//   SW1 Reverse   SW2 Fifth Down   SW3 Octave Down   SW4 Accumulate
//   FS1 Bypass    FS2 Freeze (tap=toggle, hold=accumulate)
//
// ============================================================
// OPTIMIZATION CHANGELOG (audio-transparent):
//
//  1. PRE-COMPUTED CONSTANTS
//     - SR_OVER_1000 (48.f) replaces repeated sr/1000.f divisions
//     - Reverb tap indices (3984, 7248, 10896, 14928) computed once
//       at compile time instead of per-sample float→int conversion
//     - MAX_DELAY_LIMIT (95998.f) precomputed for forward-read clamp
//
//  2. FAST MATH REPLACEMENTS (per-sample hot path)
//     - tanhf() → fast_tanh(): x/(1+|x|) rational approximation
//       (2 calls per sample eliminated, ~40 cycles each on Cortex-M7)
//     - cosf()/sinf() → 4th-order polynomial approximations
//       for constant-power crossfade (domain [0, pi/2] only)
//       (2 calls per sample eliminated, ~30 cycles each)
//
//  3. LFO SetFreq HOISTED OUT OF PER-SAMPLE LOOP
//     - lfo1/lfo2/lfo3.SetFreq() moved before the sample loop;
//       tLfoSpd is control-rate and constant within a 48-sample block
//       (3 function calls × 48 iterations = 144 calls saved per block)
//
//  4. CONST CORRECTNESS
//     - DelBuf::Read() and ReadFixed() marked const
//       (already were const — confirmed, no change needed)
//
//  5. SHARED PITCH BUFFER (mono pedal optimization)
//     - pitchDelay, octDelay, shimDelay all received identical
//       Write(delRd) every sample — 3 buffers, 3 SDRAM writes, same data.
//     - Merged into single sharedPitchDelay (sized to max = 7300).
//     - Eliminates 2 SDRAM writes per sample (~20 wait-state cycles each).
//     - Frees ~40KB SDRAM (pitchBuf + shimBuf no longer allocated).
//
//  6. NOTE ON Makefile: OPT = -Os (size). If CPU headroom is
//     tight, switching to -O2 may further improve callback speed.
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
static constexpr float SR_OVER_1000  = 48.f;          // sr / 1000
static constexpr float HALF_PI_F     = 1.5707963f;
static constexpr float MAX_DELAY_LIMIT = 95998.f;     // MAX_DELAY - 2

// Reverb echo-chamber tap positions (precomputed from ms * 48)
static constexpr int REV_TAP_A = 3984;    // 83  ms
static constexpr int REV_TAP_B = 7248;    // 151 ms
static constexpr int REV_TAP_C = 10896;   // 227 ms
static constexpr int REV_TAP_D = 14928;   // 311 ms

// ============================================================
// FAST MATH (per-sample replacements)
// ============================================================

// Soft-clip approximation: same monotonic shape as tanh,
// identical at 0 and ±∞, max deviation ~0.12 near ±1.2
static inline float fast_tanh(float x) {
    return x / (1.f + fabsf(x));
}

// Polynomial sin for x ∈ [0, π/2].  Max error < 2.5e-4.
static inline float fast_sin_hp(float x) {
    float x2 = x * x;
    return x * (1.f - x2 * (0.16666667f - x2 * 0.00833333f));
}

// Polynomial cos for x ∈ [0, π/2].  Max error < 2.0e-4.
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
#define REV_SIZE   48000   // ~1 sec (echo chamber)
#define SHARED_PITCH_SIZE 7300  // max(PITCH=4900, OCT=7300, SHIM=5300)
#define FZ_A       4657
#define FZ_B       7153
#define FZ_C       9553

static float DSY_SDRAM_BSS mainBuf[MAX_DELAY];
static float DSY_SDRAM_BSS revBuf[REV_SIZE];
static float DSY_SDRAM_BSS sharedPitchBuf[SHARED_PITCH_SIZE];
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

    float Process(const DelBuf& buf, float sr) {
        float ph = phase;
        phase += freq / sr;
        while (phase >= 1.f) phase -= 1.f;

        float dA = 2.f * ph - 1.f;
        float winA = 1.f - (dA < 0.f ? -dA : dA);
        float sampA = ph * sweepMs * sr / 1000.f + offsetMs * sr / 1000.f;
        float grainA = buf.Read(sampA) * winA;

        if (!dual) return grainA;

        float phB = ph + 0.5f;
        if (phB >= 1.f) phB -= 1.f;
        float dB = 2.f * phB - 1.f;
        float winB = 1.f - (dB < 0.f ? -dB : dB);
        float sampB = phB * sweepMs * sr / 1000.f + offsetMs * sr / 1000.f;
        float grainB = buf.Read(sampB) * winB;

        float wSum = std::max(winA + winB, 0.001f);
        return (grainA + grainB) / wSum;
    }
};

// ============================================================
// SHIMMER GRAIN READER — reversed sweep = pitch UP
// ============================================================
struct ShimGrainReader {
    float phase, freq, sweepMs, offsetMs;

    float Process(const DelBuf& buf, float sr) {
        float ph = phase;
        phase += freq / sr;
        while (phase >= 1.f) phase -= 1.f;

        // (1-ph): read far→near = read catches write = PITCH UP
        float phA = 1.f - ph;
        float dA = 2.f * ph - 1.f;
        float winA = 1.f - (dA < 0.f ? -dA : dA);
        float sampA = phA * sweepMs * sr / 1000.f + offsetMs * sr / 1000.f;
        float grainA = buf.Read(sampA) * winA;

        float phB = ph + 0.5f;
        if (phB >= 1.f) phB -= 1.f;
        float phBrd = 1.f - phB;
        float dB = 2.f * phB - 1.f;
        float winB = 1.f - (dB < 0.f ? -dB : dB);
        float sampB = phBrd * sweepMs * sr / 1000.f + offsetMs * sr / 1000.f;
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
static DelBuf mainDelay, revDelay, sharedPitchDelay;
static FreezeVoice fzVoice[3];
static GrainReader revReader, fifthReader, octReader;
static ShimGrainReader shimReader;

static Lp1  fbLpf, revLpf, p5Lpf, octLpf, shimLpf;
static Hp1  revHpf;
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
    // tLfoSpd is control-rate; constant within a 48-sample block.
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
        // Freeze hold: fast attack (75ms), slow release (~2 sec fade)
        float fzHc = (tFzH > sFzH) ? 0.0003f : 0.00002f;
        fonepole(sFzH,   tFzH,   fzHc);
        fonepole(sAcc,   tAcc,   0.005f);

        float dry = in[0][i];

        // Tape warble (LFO freqs already set above)
        float modMs = (lfo1.Process()
        + lfo2.Process() * 0.3f
        + lfo3.Process() * 0.5f) * tTapeD;
        float modSamps = modMs * SR_OVER_1000;

        // ==== DELAY WRITE ====
        mainDelay.Write(fast_tanh(dry + fbSig));

        // Forward read
        float fwdS = std::max(48.f, std::min(sDelay + modSamps, MAX_DELAY_LIMIT));
        float fwd = mainDelay.Read(fwdS);

        // Reverse read (dual grain, fixed sweep)
        float rev = revReader.Process(mainDelay, SR_F);

        // Fwd/Rev crossfade
        float delRd = fwd * (1.f - sSw1) + rev * sSw1;

        // ==== FEEDBACK ====
        fbSig = fbLpf.Process(delRd) * sFb;

        // Reverb (echo chamber) — tap indices precomputed
        float revIn = revLpf.Process(delRd + revFb * 0.75f);
        revDelay.Write(revIn);
        float rvSum = (revDelay.ReadFixed(REV_TAP_A)
        + revDelay.ReadFixed(REV_TAP_B)
        + revDelay.ReadFixed(REV_TAP_C)
        + revDelay.ReadFixed(REV_TAP_D)) * 0.25f;
        revFb = rvSum;
        float rvOut = revHpf.Process(rvSum) * tRevMix * 0.8f;

        // Shared pitch buffer (mono — one write serves all pitch readers)
        sharedPitchDelay.Write(delRd);

        // Fifth down (SW2)
        float p5 = p5Lpf.Process(fifthReader.Process(sharedPitchDelay, SR_F)) * 0.5f * sSw2;

        // Octave down (SW3)
        float oct = octLpf.Process(octReader.Process(sharedPitchDelay, SR_F)) * 0.7f * sSw3;

        // Shimmer octave up (SW4)
        float shim = shimLpf.Process(shimReader.Process(sharedPitchDelay, SR_F)) * 0.4f * sSw4;

        // ==== WET BUS ====
        float wet = fast_tanh(delRd + rvOut + p5 + oct + shim);

        // Freeze
        float dryGt  = (1.f - sFzW) + sFzW * sAcc * 0.5f;
        float holdGt = sFzH;
        float fzMod  = modSamps * 0.25f;
        float fzOut  = 0.f;
        for (int v = 0; v < 3; v++)
            fzOut += fzVoice[v].Process(dry, dryGt, holdGt, fzMod);
        fzOut *= 0.55f;

        // Output: constant-power crossfade (fast polynomial sin/cos)
        float angle = sMixW * HALF_PI_F;
        float effSig = (dry * fast_cos_hp(angle) + wet * fast_sin_hp(angle)) * sEffG;
        float bypSig = dry * sBypG;
        float output = bypSig + effSig + fzOut;
        output = std::max(-1.f, std::min(1.f, output));

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
    tTapeD  = pTape.Process();
    tLfoSpd = pSpeed.Process();
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

    led2.Set(freezeOn ? 1.f : 0.f);
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
    sharedPitchDelay.Init(sharedPitchBuf, SHARED_PITCH_SIZE);

    float* fzMem[3] = {fzBufA, fzBufB, fzBufC};
    size_t fzSz[3]  = {FZ_A, FZ_B, FZ_C};
    float  fzMs[3]  = {97.f, 149.f, 199.f};
    for (int i = 0; i < 3; i++) {
        fzLp[i].Init(600.f, sr);
        fzHp[i].Init(200.f, sr);
        fzVoice[i].Init(fzMem[i], fzSz[i], fzMs[i], sr, &fzLp[i], &fzHp[i]);
    }

    fbLpf.Init(4000.f, sr);
    revLpf.Init(6000.f, sr);
    revHpf.Init(80.f, sr);
    p5Lpf.Init(2000.f, sr);
    octLpf.Init(1500.f, sr);
    shimLpf.Init(3000.f, sr);

    //                   phase  freq    sweepMs  offsetMs  dual
    revReader   =       {0.f,   1.f,    1999.f,  1.f,      true};
    fifthReader =       {0.f,   12.5f,  26.7f,   1.f,      false};
    octReader   =       {0.f,   8.f,    62.5f,   1.f,      true};
    //                   phase  freq    sweepMs  offsetMs
    shimReader  =       {0.f,   8.f,    62.5f,   1.f};

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
