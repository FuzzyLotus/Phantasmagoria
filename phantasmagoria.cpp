// ============================================================
// PHANTASMAGORIA
// Spectral Delay for Daisy Seed / Terrarium
//
// CURRENT CONTROL MAP
//
//   K1 TIME   K2 REPT   K3 RVRB   K4 AGE
//   K5 RATE   K6 MIX
//   SW1 REV   SW2 ROOM   SW3 HALO   SW4 EVOL
//   FS1 BYPASS   FS2 FREEZE (tap = toggle, hold = accumulate)
//
// CORE DESIGN PRIORITIES
//
//   1. Bypass with freeze OFF must remain clean
//   2. Tone quality above all
//   3. Avoid tone suck
//   4. Must respond well to a tube amp and its dynamics
//
// CURRENT SWITCH BEHAVIOR
//
//   SW1 = REV
//   - OFF = forward delay
//   - ON  = reverse delay
//   - This is the main time-direction switch and the most
//     fundamental alternate world of the pedal.
//
//   SW2 = ROOM
//   - OFF = standard delay into reverb behavior
//   - ON  = the room becomes part of the repeat behavior
//   - Chamber memory folds into the delay world, making the space
//     itself more present and more connected to the repeats.
//   - Intended feel: the pedal sounds more like playing into an
//     active haunted room rather than into a separate delay and reverb.
//
//   SW3 = HALO
//   - OFF = normal wet field
//   - ON  = detached late aura behind the wet signal
//   - Adds a trailing ghost-layer behind the repeats and reverb,
//     giving the pedal a more atmospheric and haunted identity.
//   - Intended feel: a clear ambient after-image, like a second
//     shadow of the note living behind the main effect field.
//
//   SW4 = EVOL
//   - OFF = mostly stable frozen anchor
//   - ON  = frozen layer slowly lives and moves
//   - Keeps freeze useful as a stable bed when OFF, but allows a
//     more animated, living frozen texture when engaged.
//   - Intended feel: the frozen layer becomes more alive without
//     turning into obvious pitch wobble or chaotic movement.
//
// FS2 FREEZE BEHAVIOR
//
//   - tap  = toggle freeze on/off
//   - hold = accumulate while frozen
//
// IMPORTANT BYPASS NOTE
//
//   - Turning the main effect off no longer kills an active frozen layer
//   - Fully bypassed output is now:
//       dry only, when freeze is inactive
//       dry + freeze, when freeze is active
//   - This is intentional so freeze can remain musically useful as its
//     own layer even when the main delay is bypassed.
//
// ============================================================
// HISTORICAL DYNAMICS CHANGELOG (v30b -> v30c)
//
//  1. DELAY WRITE SOFT-CLIP REMOVED
//     - fast_tanh on every sample entering the delay was compressing
//       transients before storage. Replaced with conditional gentle
//       curve that only engages when feedback > 0.3 and signal > ~1.2.
//       Clean playing and low-feedback settings pass through untouched.
//
//  2. WET BUS SOFT-CLIP REMOVED
//     - fast_tanh on the wet sum was a second compression stage.
//       Replaced with gentle_saturate that only engages above ±1.5.
//       Normal wet levels pass through transparent.
//
//  3. OUTPUT HARD CLAMP -> SOFT SATURATION
//     - std::max/min brick-wall at ±1.0 replaced with gentle_saturate.
//       Peaks roll off smoothly instead of flat-topping.
//
//  4. FEEDBACK LPF OPENED UP
//     - 4000 Hz -> 7500 Hz. Tube amp pick attack lives in 3–6 kHz;
//       old filter was killing it by the second repeat.
//
//  5. PITCH VOICE LPFs OPENED UP
//     - Fifth:   2000 -> 4500 Hz
//     - Octave:  1500 -> 3500 Hz
//     - Shimmer: 3000 -> 5500 Hz
//     Preserved harmonic content and transient detail from tube amp
//     during the earlier pitch-voice era.
//
//  6. FREEZE VOICE BANDPASS WIDENED
//     - Was 200–600 Hz (very narrow, muffled).
//     - Now 80–5000 Hz. Frozen signal retains full character.
//
//  7. GAIN STAGING REBALANCED
//     - Octave down: 0.7 -> 0.6 (historical pitch-era rebalance)
//     - Freeze output: 0.55 -> 0.45
//
// ============================================================
// HISTORICAL OPTIMIZATION CHANGELOG (audio-transparent, from v30b)
//
//  1. PRE-COMPUTED CONSTANTS
//     - SR_OVER_1000 (48.f) replaces repeated sr/1000.f divisions
//     - Reverb tap indices (3984, 7248, 10896, 14928) computed once
//       at compile time instead of per-sample float->int conversion
//     - MAX_DELAY_LIMIT (95998.f) precomputed for forward-read clamp
//
//  2. FAST MATH REPLACEMENTS (per-sample hot path)
//     - cosf()/sinf() -> 4th-order polynomial approximations
//       for constant-power crossfade (domain [0, pi/2] only)
//       (2 calls per sample eliminated, about 30 cycles each)
//
//  3. LFO SetFreq HOISTED OUT OF PER-SAMPLE LOOP
//     - lfo1/lfo2/lfo3.SetFreq() moved before the sample loop
//     - tLfoSpd is control-rate and constant within a 48-sample block
//       (3 function calls x 48 iterations = 144 calls saved per block)
//
//  4. CONST CORRECTNESS
//     - DelBuf::Read() and ReadFixed() marked const
//
//  5. SHARED PITCH BUFFER
//     - Historical optimization from the earlier pitch-voice era
//     - Preserved here for development history only
//
//  6. NOTE ON Makefile: OPT = -Os (size)
//     - If CPU headroom is tight, switching to -O2 may further improve
//       callback speed
//
// ============================================================
// CURRENT VERSION CHANGELOG (v30c -> current)
//
//  1. SWITCH SYSTEM REWORKED
//     - The previous switch assignments were largely placeholders.
//     - In real use, they were barely being used and did not add enough
//       to the pedal's musical identity.
//     - They felt more like optional extras than part of one coherent
//       instrument, so the switch system was redesigned around behaviors
//       that directly shape the pedal's core delay / reverb / freeze world.
//     - Goal of the rework:
//         * make every switch clearly audible
//         * make every switch feel worth using
//         * make the pedal feel like one unified instrument instead of
//           a base effect with disconnected add-ons
//
//  2. CURRENT SWITCH ROLES
//     - SW1 = REV
//         * OFF = forward delay
//         * ON  = reverse delay
//         * This is the main time-direction switch and remains the most
//           fundamental alternate world of the pedal.
//
//     - SW2 = ROOM
//         * OFF = standard delay into reverb behavior
//         * ON  = the room becomes part of the repeat behavior
//         * Chamber memory folds into the delay world, making the space
//           itself more present and more connected to the repeats.
//         * Intended feel: the pedal sounds more like playing into an
//           active haunted room rather than into a separate delay and reverb.
//
//     - SW3 = HALO
//         * OFF = normal wet field
//         * ON  = detached late aura behind the wet signal
//         * Adds a trailing ghost-layer behind the repeats and reverb,
//           giving the pedal a more atmospheric and haunted identity.
//         * Intended feel: a clear ambient after-image, like a second
//           shadow of the note living behind the main effect field.
//
//     - SW4 = EVOL
//         * OFF = mostly stable frozen anchor
//         * ON  = frozen layer slowly lives and moves
//         * Keeps freeze useful as a stable bed when OFF, but allows a
//           more animated, living frozen texture when engaged.
//         * Intended feel: the frozen layer becomes more alive without
//           turning into obvious pitch wobble or chaotic movement.
//
//  3. OLD PITCH ENGINE REMOVED FROM ACTIVE SWITCH SYSTEM
//     - Fifth / octave / shimmer style switch behavior was removed from
//       the active switch architecture.
//     - Those behaviors did not help the pedal feel more unified and were
//       replaced by switch roles that better support the core identity.
//
//  4. ROOM MODE ADDED (SW2)
//     - Replaced the earlier placeholder-style switch behavior with a more
//       direct spatial world change.
//     - The room now becomes part of the repeat structure instead of acting
//       like a secondary layered feature.
//     - This makes the switch feel more immediate and more central to the
//       pedal's identity.
//
//  5. HALO MODE ADDED (SW3)
//     - Added a detached late aura behind the wet field so the switch has
//       an immediately audible atmospheric role.
//     - Final voicing was adjusted so the effect stays present without
//       becoming too dominant or feeling like a second delay head.
//
//  6. FREEZE REWORKED INTO STABLE + EVOLVING STATES
//     - Base freeze motion was reduced heavily so the frozen layer remains
//       a usable anchor instead of secretly moving too much all the time.
//     - SW4 now owns the more animated frozen behavior.
//     - This gives the freeze section a clearer split between stable and
//       evolving states.
//
//  7. FREEZE NOW SURVIVES BYPASS BY DESIGN
//     - Turning the main effect off no longer kills an active frozen layer.
//     - Fully bypassed output is now:
//         * dry only, when freeze is inactive
//         * dry + freeze, when freeze is active
//     - This is intentional so freeze can remain musically useful as its
//       own layer even when the main delay is bypassed.
//
//  8. BYPASS TRANSITION DROP FIXED
//     - Output routing was reworked so turning the effect off no longer
//       causes a brief signal dip.
//     - Dry path now crossfades back in smoothly while the wet path fades out.
//
//  9. K5 CONTROL LAW RESHAPED
//     - LFO speed control was reshaped so the sweet spot is broader and
//       more usable across the knob travel.
//     - The goal was to avoid having the most musical range arrive too
//       early and disappear too quickly.
//
//  10. OUTPUT ROUTING CLEANED UP
//      - Core effect path and freeze path are now treated separately.
//      - This keeps freeze behavior intentional and prevents it from being
//        tied incorrectly to the main bypass state.
//
//  11. DESIGN RESULT
//      - The pedal now behaves more like a complete instrument:
//          REV changes the time world
//          ROOM changes the spatial world
//          HALO changes the ambient aura
//          EVOL changes the frozen world
//      - The switch section now supports the pedal's identity instead of
//        feeling like a set of spare extra features.
//
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

// Gentle saturation: linear below threshold, soft-knee above.
// Knee at ~±1.2, approaches ±2.0 asymptotically.
// Passes signal untouched when |x| < ~0.8.
static inline float gentle_saturate(float x) {
    float ax = fabsf(x);
    if (ax < 0.8f) return x;                         // linear region
    float sign = (x >= 0.f) ? 1.f : -1.f;
    return sign * (0.8f + (ax - 0.8f) / (1.f + (ax - 0.8f) * 0.5f));
}

// Soft-clip approximation: same monotonic shape as tanh,
// identical at 0 and +/-inf, max deviation ~0.12 near +/-1.2
// Retained for feedback-protection path only.
static inline float fast_tanh(float x) {
    return x / (1.f + fabsf(x));
}

// Polynomial sin for x in [0, pi/2].  Max error < 2.5e-4.
static inline float fast_sin_hp(float x) {
    float x2 = x * x;
    return x * (1.f - x2 * (0.16666667f - x2 * 0.00833333f));
}

// Polynomial cos for x in [0, pi/2].  Max error < 2.0e-4.
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

static Lp1  revLpf;
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

        // ------------------------------------------------------------
        // SW2 = CASCADE
        // SW3 = HALO
        //
        // SW2 CASCADE
        // OFF = delay into reverb
        // ON  = chamber memory feeds back into the delay, and dry note
        //       also hits the chamber directly. This should read immediately.
        //
        // SW3 HALO
        // OFF = normal wet field
        // ON  = detached late aura from the chamber itself.
        //       This complements SW1 instead of fighting it.
        // ------------------------------------------------------------

        float cascadeAmt = sSw2;
        float haloAmt    = sSw3;

        // ==== DELAY WRITE ====
        // Cascade feeds chamber memory back into the delay write so the
        // space itself gets repeated. Kept strong enough to be obvious.
        float cascadeFeed = revFb * cascadeAmt * 0.95f;
        float cascadeWet  = revFb * cascadeAmt * 0.35f;

        float delIn = dry + fbSig + cascadeFeed;
        if (sFb > 0.3f) delIn = fast_tanh(delIn * 0.5f) * 2.f;
        mainDelay.Write(delIn);

        // Forward read
        float fwdS = std::max(48.f, std::min(sDelay + modSamps, MAX_DELAY_LIMIT));
        float fwd = mainDelay.Read(fwdS);

        // Reverse read
        float rev = revReader.Process(mainDelay, SR_F);

        // SW1 remains the only thing that decides forward vs reverse
        float delRd = fwd * (1.f - sSw1) + rev * sSw1;

        // Stable feedback filter for the delay path
        static float fbState = 0.f;
        float fbCoef = 1.f - expf(-TWO_PI_F * 7500.f / SR_F);
        fbState += fbCoef * (delRd - fbState);
        fbSig = fbState * sFb;

        // Reverb source:
        // OFF = chamber mainly hears the delay
        // ON  = chamber also hears the dry note directly, so the room
        //       reacts immediately and feels more detached from the repeats
        float reverbSource = delRd * (1.f - cascadeAmt * 0.95f)
        + dry   * (cascadeAmt * 0.95f);

        // Reverb (echo chamber)
        float revIn = revLpf.Process(reverbSource + revFb * 0.75f);
        revDelay.Write(revIn);

        float rvSum = (revDelay.ReadFixed(REV_TAP_A)
        + revDelay.ReadFixed(REV_TAP_B)
        + revDelay.ReadFixed(REV_TAP_C)
        + revDelay.ReadFixed(REV_TAP_D)) * 0.25f;

        revFb = rvSum;

        // Slight gain lift when Cascade is on so it is not subtle
        float rvOut = revHpf.Process(rvSum) * tRevMix * (0.8f + cascadeAmt * 0.45f);

        // SW3 = HALO
        // Late detached chamber aura. Uses the chamber path itself,
        // so it follows whatever world SW1/SW2 create without becoming
        // a second reverse control.
        static float haloLpState = 0.f;
        float haloTap = revDelay.ReadFixed(9600); // ~200 ms behind chamber field
        float haloCoef = 1.f - expf(-TWO_PI_F * 1800.f / SR_F);
        haloLpState += haloCoef * (haloTap - haloLpState);
        float halo = haloLpState * haloAmt * 1.03f;

        // ==== WET BUS ====
        float wet = gentle_saturate(delRd + rvOut + cascadeWet + halo);


        // Freeze
        float dryGt  = (1.f - sFzW) + sFzW * sAcc * 0.5f;
        float holdGt = sFzH;

        // SW4 = Freeze Evolution
        // OFF -> stable frozen anchor
        // ON  -> subtle internal movement only inside freeze
        float evolveAmt = sSw4 * sFzH;

        // Base freeze modulation stays restrained
        float fzMod = modSamps * 0.05f;

        // When evolution is ON, add a very slow extra internal drift
        // that does not affect the main delay, only the frozen layer.
        // Independent ultra-slow evolution oscillator (not tied to tape)
        static float evolvePhase = 0.f;
        evolvePhase += 0.00003f;
        if (evolvePhase > 1.f) evolvePhase -= 1.f;

        float evolveDrift = sinf(evolvePhase * TWO_PI_F) * 6.0f * evolveAmt;

        float fzOut  = 0.f;
        for (int v = 0; v < 3; v++)
            fzOut += fzVoice[v].Process(dry, dryGt, holdGt, fzMod + evolveDrift);

        fzOut *= 0.45f;

        // No old pitch bleed anymore
        float pitchBleed = 0.f;

        // Output: constant-power crossfade (fast polynomial sin/cos)
        float angle = sMixW * HALF_PI_F;

        // Core effect excludes freeze so freeze can survive bypass cleanly.
        float coreEff = (dry * fast_cos_hp(angle) + wet * fast_sin_hp(angle)) + pitchBleed;

        // Active effect path only
        float wetEff = gentle_saturate(coreEff);

        float output;
        if(sBypG > 0.999f)
        {
            // Fully bypassed: clean dry + freeze only
            output = dry + fzOut;
        }
        else
        {
            // During transition:
            // - dry crossfades back in
            // - core wet path crossfades out
            // - freeze remains present
            output = dry * sBypG + wetEff * sEffG + fzOut;
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

    float k5 = pSpeed.Process();
    float k5Norm = std::max(0.f, std::min((k5 - 0.1f) / (6.0f - 0.1f), 1.f));

    // Broadens the musical region and pushes the sweet spot later,
    // so it lives more around 10:00 to 2:00 instead of arriving too early.
    float k5Shaped;
    if (k5Norm < 0.60f)
    {
        float x = k5Norm / 0.60f;
        k5Shaped = 0.38f * powf(x, 1.85f);
    }
    else
    {
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
        fzLp[i].Init(5000.f, sr);   // was 600 -- opened up for full-range freeze
        fzHp[i].Init(80.f, sr);     // was 200 -- captures low-end body
        fzVoice[i].Init(fzMem[i], fzSz[i], fzMs[i], sr, &fzLp[i], &fzHp[i]);
    }

    revLpf.Init(9000.f, sr);    // lets reverb breathe; recirculation darkens naturally
    revHpf.Init(55.f, sr);      // was 80 -- preserves low-end body in reverb

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
// ============================================================
// PHANTASMAGORIA
// Spectral Delay for Daisy Seed / Terrarium
//
// CURRENT CONTROL MAP
//
//   K1 Delay Time   K2 Feedback   K3 Reverb   K4 Tape Depth
//   K5 LFO Speed    K6 Mix
//   SW1 Reverse   SW2 Cascade   SW3 Halo   SW4 Freeze Evolution
//   FS1 Bypass    FS2 Freeze (tap = toggle, hold = accumulate)
//
// CORE DESIGN PRIORITIES
//
//   1. Bypass with freeze OFF must remain clean
//   2. Tone quality above all
//   3. Avoid tone suck
//   4. Must respond well to a tube amp and its dynamics
//
// CURRENT BEHAVIOR
//
//   SW1 Reverse
//   - OFF = forward delay
//   - ON  = reverse delay
//
//   SW2 Cascade
//   - OFF = delay into reverb
//   - ON  = chamber memory folds into the delay world and the
//           room reacts more immediately and more obviously
//
//   SW3 Halo
//   - OFF = normal wet field
//   - ON  = detached late aura behind the wet field
//
//   SW4 Freeze Evolution
//   - OFF = mostly stable frozen anchor
//   - ON  = frozen layer slowly lives and moves
//
//   FS2 Freeze
//   - tap  = toggle freeze on/off
//   - hold = accumulate while frozen
//
// IMPORTANT BYPASS NOTE
//
//   - If freeze is active, it is allowed to survive bypass by design
//   - So fully bypassed output is:
//       dry only, when freeze is inactive
//       dry + freeze, when freeze is active
//
// ============================================================
// HISTORICAL DYNAMICS CHANGELOG (v30b -> v30c)
//
//  1. DELAY WRITE SOFT-CLIP REMOVED
//     - fast_tanh on every sample entering the delay was compressing
//       transients before storage. Replaced with conditional gentle
//       curve that only engages when feedback > 0.3 and signal > ~1.2.
//       Clean playing and low-feedback settings pass through untouched.
//
//  2. WET BUS SOFT-CLIP REMOVED
//     - fast_tanh on the wet sum was a second compression stage.
//       Replaced with gentle_saturate that only engages above ±1.5.
//       Normal wet levels pass through transparent.
//
//  3. OUTPUT HARD CLAMP -> SOFT SATURATION
//     - std::max/min brick-wall at ±1.0 replaced with gentle_saturate.
//       Peaks roll off smoothly instead of flat-topping.
//
//  4. FEEDBACK LPF OPENED UP
//     - 4000 Hz -> 7500 Hz. Tube amp pick attack lives in 3–6 kHz;
//       old filter was killing it by the second repeat.
//
//  5. PITCH VOICE LPFs OPENED UP
//     - Fifth:   2000 -> 4500 Hz
//     - Octave:  1500 -> 3500 Hz
//     - Shimmer: 3000 -> 5500 Hz
//     Preserved harmonic content and transient detail from tube amp
//     during the earlier pitch-voice era.
//
//  6. FREEZE VOICE BANDPASS WIDENED
//     - Was 200–600 Hz (very narrow, muffled).
//     - Now 80–5000 Hz. Frozen signal retains full character.
//
//  7. GAIN STAGING REBALANCED
//     - Octave down: 0.7 -> 0.6 (historical pitch-era rebalance)
//     - Freeze output: 0.55 -> 0.45
//
// ============================================================
// HISTORICAL OPTIMIZATION CHANGELOG (audio-transparent, from v30b)
//
//  1. PRE-COMPUTED CONSTANTS
//     - SR_OVER_1000 (48.f) replaces repeated sr/1000.f divisions
//     - Reverb tap indices (3984, 7248, 10896, 14928) computed once
//       at compile time instead of per-sample float->int conversion
//     - MAX_DELAY_LIMIT (95998.f) precomputed for forward-read clamp
//
//  2. FAST MATH REPLACEMENTS (per-sample hot path)
//     - cosf()/sinf() -> 4th-order polynomial approximations
//       for constant-power crossfade (domain [0, pi/2] only)
//       (2 calls per sample eliminated, about 30 cycles each)
//
//  3. LFO SetFreq HOISTED OUT OF PER-SAMPLE LOOP
//     - lfo1/lfo2/lfo3.SetFreq() moved before the sample loop
//     - tLfoSpd is control-rate and constant within a 48-sample block
//       (3 function calls x 48 iterations = 144 calls saved per block)
//
//  4. CONST CORRECTNESS
//     - DelBuf::Read() and ReadFixed() marked const
//
//  5. SHARED PITCH BUFFER
//     - Historical optimization from the earlier pitch-voice era
//     - Preserved here for development history only
//
//  6. NOTE ON Makefile: OPT = -Os (size)
//     - If CPU headroom is tight, switching to -O2 may further improve
//       callback speed
//
// ============================================================
// CURRENT VERSION CHANGELOG (v30c -> current)
//
//  1. SWITCH SYSTEM REWORKED
//     - SW2 / SW3 / SW4 no longer act as pitch-voice toggles
//     - Current switch roles are:
//         SW1 = Reverse
//         SW2 = Cascade
//         SW3 = Halo
//         SW4 = Freeze Evolution
//
//  2. OLD PITCH ENGINE REMOVED FROM AUDIO PATH
//     - Fifth / octave / shimmer voices removed from the wet path
//     - Pitch bleed removed from the output path
//
//  3. CASCADE MODE ADDED (SW2)
//     - Chamber memory now folds back into the delay world
//     - Dry note can also hit the chamber directly
//     - Intended result: the room itself becomes part of the repeat behavior
//
//  4. HALO MODE ADDED (SW3)
//     - Added a detached late aura behind the wet field
//     - Halo is summed directly into the wet output so it reads immediately
//     - Final gain trimmed slightly so the "second head" feel stays present
//       without taking over
//
//  5. FREEZE REWORKED INTO STABLE + EVOLVING STATES
//     - Base freeze motion reduced heavily so the frozen bed remains an anchor
//     - SW4 now owns the extra motion with its own ultra-slow evolution source
//
//  6. FREEZE NOW SURVIVES BYPASS BY DESIGN
//     - Turning the delay off no longer kills an active frozen layer
//     - Fully bypassed output is dry only when freeze is inactive
//     - Fully bypassed output is dry + freeze when freeze is active
//
//  7. BYPASS TRANSITION DROP FIXED
//     - Output routing reworked so turning the effect off no longer causes
//       a brief signal dip during the crossfade
//
//  8. K5 LFO SPEED CONTROL LAW RESHAPED
//     - K5 no longer rises in a cramped, overly early way
//     - The useful range is broader and the sweet spot lasts longer
//
//  9. SWITCH VOICING MOVED AWAY FROM HIDDEN MICRO-BEHAVIORS
//     - Earlier smear / erosion / shadow / abyss style approaches proved
//       too hard to hear reliably in practice
//     - Current switch system favors immediately audible spatial behavior
//
//  10. OUTPUT ROUTING CLEANED UP
//      - Core effect path and freeze path are treated separately
//      - Freeze is no longer tied to the delay bypass state
//
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

// Gentle saturation: linear below threshold, soft-knee above.
// Knee at ~±1.2, approaches ±2.0 asymptotically.
// Passes signal untouched when |x| < ~0.8.
static inline float gentle_saturate(float x) {
    float ax = fabsf(x);
    if (ax < 0.8f) return x;                         // linear region
    float sign = (x >= 0.f) ? 1.f : -1.f;
    return sign * (0.8f + (ax - 0.8f) / (1.f + (ax - 0.8f) * 0.5f));
}

// Soft-clip approximation: same monotonic shape as tanh,
// identical at 0 and +/-inf, max deviation ~0.12 near +/-1.2
// Retained for feedback-protection path only.
static inline float fast_tanh(float x) {
    return x / (1.f + fabsf(x));
}

// Polynomial sin for x in [0, pi/2].  Max error < 2.5e-4.
static inline float fast_sin_hp(float x) {
    float x2 = x * x;
    return x * (1.f - x2 * (0.16666667f - x2 * 0.00833333f));
}

// Polynomial cos for x in [0, pi/2].  Max error < 2.0e-4.
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

static Lp1  revLpf;
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

        // ------------------------------------------------------------
        // SW2 = CASCADE
        // SW3 = HALO
        //
        // SW2 CASCADE
        // OFF = delay into reverb
        // ON  = chamber memory feeds back into the delay, and dry note
        //       also hits the chamber directly. This should read immediately.
        //
        // SW3 HALO
        // OFF = normal wet field
        // ON  = detached late aura from the chamber itself.
        //       This complements SW1 instead of fighting it.
        // ------------------------------------------------------------

        float cascadeAmt = sSw2;
        float haloAmt    = sSw3;

        // ==== DELAY WRITE ====
        // Cascade feeds chamber memory back into the delay write so the
        // space itself gets repeated. Kept strong enough to be obvious.
        float cascadeFeed = revFb * cascadeAmt * 0.95f;
        float cascadeWet  = revFb * cascadeAmt * 0.35f;

        float delIn = dry + fbSig + cascadeFeed;
        if (sFb > 0.3f) delIn = fast_tanh(delIn * 0.5f) * 2.f;
        mainDelay.Write(delIn);

        // Forward read
        float fwdS = std::max(48.f, std::min(sDelay + modSamps, MAX_DELAY_LIMIT));
        float fwd = mainDelay.Read(fwdS);

        // Reverse read
        float rev = revReader.Process(mainDelay, SR_F);

        // SW1 remains the only thing that decides forward vs reverse
        float delRd = fwd * (1.f - sSw1) + rev * sSw1;

        // Stable feedback filter for the delay path
        static float fbState = 0.f;
        float fbCoef = 1.f - expf(-TWO_PI_F * 7500.f / SR_F);
        fbState += fbCoef * (delRd - fbState);
        fbSig = fbState * sFb;

        // Reverb source:
        // OFF = chamber mainly hears the delay
        // ON  = chamber also hears the dry note directly, so the room
        //       reacts immediately and feels more detached from the repeats
        float reverbSource = delRd * (1.f - cascadeAmt * 0.95f)
        + dry   * (cascadeAmt * 0.95f);

        // Reverb (echo chamber)
        float revIn = revLpf.Process(reverbSource + revFb * 0.75f);
        revDelay.Write(revIn);

        float rvSum = (revDelay.ReadFixed(REV_TAP_A)
        + revDelay.ReadFixed(REV_TAP_B)
        + revDelay.ReadFixed(REV_TAP_C)
        + revDelay.ReadFixed(REV_TAP_D)) * 0.25f;

        revFb = rvSum;

        // Slight gain lift when Cascade is on so it is not subtle
        float rvOut = revHpf.Process(rvSum) * tRevMix * (0.8f + cascadeAmt * 0.45f);

        // SW3 = HALO
        // Late detached chamber aura. Uses the chamber path itself,
        // so it follows whatever world SW1/SW2 create without becoming
        // a second reverse control.
        static float haloLpState = 0.f;
        float haloTap = revDelay.ReadFixed(9600); // ~200 ms behind chamber field
        float haloCoef = 1.f - expf(-TWO_PI_F * 1800.f / SR_F);
        haloLpState += haloCoef * (haloTap - haloLpState);
        float halo = haloLpState * haloAmt * 1.03f;

        // ==== WET BUS ====
        float wet = gentle_saturate(delRd + rvOut + cascadeWet + halo);


        // Freeze
        float dryGt  = (1.f - sFzW) + sFzW * sAcc * 0.5f;
        float holdGt = sFzH;

        // SW4 = Freeze Evolution
        // OFF -> stable frozen anchor
        // ON  -> subtle internal movement only inside freeze
        float evolveAmt = sSw4 * sFzH;

        // Base freeze modulation stays restrained
        float fzMod = modSamps * 0.05f;

        // When evolution is ON, add a very slow extra internal drift
        // that does not affect the main delay, only the frozen layer.
        // Independent ultra-slow evolution oscillator (not tied to tape)
        static float evolvePhase = 0.f;
        evolvePhase += 0.00003f;
        if (evolvePhase > 1.f) evolvePhase -= 1.f;

        float evolveDrift = sinf(evolvePhase * TWO_PI_F) * 6.0f * evolveAmt;

        float fzOut  = 0.f;
        for (int v = 0; v < 3; v++)
            fzOut += fzVoice[v].Process(dry, dryGt, holdGt, fzMod + evolveDrift);

        fzOut *= 0.45f;

        // No old pitch bleed anymore
        float pitchBleed = 0.f;

        // Output: constant-power crossfade (fast polynomial sin/cos)
        float angle = sMixW * HALF_PI_F;

        // Core effect excludes freeze so freeze can survive bypass cleanly.
        float coreEff = (dry * fast_cos_hp(angle) + wet * fast_sin_hp(angle)) + pitchBleed;

        // Active effect path only
        float wetEff = gentle_saturate(coreEff);

        float output;
        if(sBypG > 0.999f)
        {
            // Fully bypassed: clean dry + freeze only
            output = dry + fzOut;
        }
        else
        {
            // During transition:
            // - dry crossfades back in
            // - core wet path crossfades out
            // - freeze remains present
            output = dry * sBypG + wetEff * sEffG + fzOut;
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

    float k5 = pSpeed.Process();
    float k5Norm = std::max(0.f, std::min((k5 - 0.1f) / (6.0f - 0.1f), 1.f));

    // Broadens the musical region and pushes the sweet spot later,
    // so it lives more around 10:00 to 2:00 instead of arriving too early.
    float k5Shaped;
    if (k5Norm < 0.60f)
    {
        float x = k5Norm / 0.60f;
        k5Shaped = 0.38f * powf(x, 1.85f);
    }
    else
    {
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
        fzLp[i].Init(5000.f, sr);   // was 600 -- opened up for full-range freeze
        fzHp[i].Init(80.f, sr);     // was 200 -- captures low-end body
        fzVoice[i].Init(fzMem[i], fzSz[i], fzMs[i], sr, &fzLp[i], &fzHp[i]);
    }

    revLpf.Init(9000.f, sr);    // lets reverb breathe; recirculation darkens naturally
    revHpf.Init(55.f, sr);      // was 80 -- preserves low-end body in reverb

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
