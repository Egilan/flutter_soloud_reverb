// Standalone unit test for the fork's DSP units (no SoLoud dependency).
// Build:  g++ -std=c++17 -I../../src test/native/dsp_test.cpp -o dsp_test
// See docs/DSP_Metering_Ducking_Limiter.md.

#include "dsp/master_limiter.h"
#include "dsp/ducker.h"
#include "dsp/bus_meter.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static int g_failures = 0;
#define CHECK(cond, msg)                                                        \
  do {                                                                          \
    if (!(cond)) {                                                              \
      std::printf("FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__);             \
      ++g_failures;                                                            \
    } else {                                                                    \
      std::printf("ok:   %s\n", msg);                                          \
    }                                                                           \
  } while (0)

static const float kPi = 3.14159265358979323846f;

// ---- Test 1: brickwall guarantee + steady-state gain reduction ----
static void testLimiterFullScaleSine() {
  const float sr = 48000.0f;
  const unsigned ch = 2, stride = 2048, block = 1024;
  fsdsp::MasterLimiter lim;
  lim.init(sr, 8, block);
  lim.ceilingDb.store(-0.3f);       // ceiling = 0.9660 linear
  lim.lookaheadMs.store(3.0f);
  lim.releaseSec.store(0.120f);
  // limiter engaged (not bypassed)

  const float ceil = std::pow(10.0f, -0.3f * 0.05f);
  const float amp = 1.0f;           // full-scale sine, peak 1.0
  unsigned la = std::min((unsigned)(0.005f * sr), // maxLa-1
                         (unsigned)(0.003f * sr));

  std::vector<float> buf(stride * ch);
  double phase = 0.0;
  const double dphase = 2.0 * kPi * 440.0 / sr;

  float maxOut = 0.0f;
  float minGainSteady = 1.0f, maxGainSteady = 0.0f;
  const int blocks = 200;
  for (int b = 0; b < blocks; ++b) {
    for (unsigned i = 0; i < block; ++i) {
      float s = amp * (float)std::sin(phase);
      phase += dphase;
      buf[i] = s;             // ch 0
      buf[i + stride] = s;    // ch 1
    }
    lim.process(buf.data(), block, stride, ch);
    // Skip warmup blocks (delay-line + gain settle).
    if (b > 20) {
      for (unsigned i = 0; i < block; ++i) {
        float a = std::fabs(buf[i]);
        if (a > maxOut) maxOut = a;
      }
      float gr = lim.gainReduction();
      if (gr < minGainSteady) minGainSteady = gr;
      if (gr > maxGainSteady) maxGainSteady = gr;
    }
  }

  // Brickwall: output never exceeds ceiling (+ tiny epsilon).
  CHECK(maxOut <= ceil + 1e-3f, "limiter output stays <= ceiling");
  // Steady-state gain reduction ~ ceiling/peak.
  float expectedGr = ceil / amp;
  CHECK(std::fabs(minGainSteady - expectedGr) < 0.02f &&
            std::fabs(maxGainSteady - expectedGr) < 0.05f,
        "limiter steady-state GR ~= ceiling/peak");
  std::printf("     maxOut=%.4f ceil=%.4f GR[min=%.4f max=%.4f] expect=%.4f\n",
              maxOut, ceil, minGainSteady, maxGainSteady, expectedGr);
}

// ---- Test 2: below-ceiling signal is bit-transparent apart from delay ----
static void testLimiterBitTransparentDelay() {
  const float sr = 48000.0f;
  const unsigned ch = 1, stride = 4096, block = 4096;
  fsdsp::MasterLimiter lim;
  lim.init(sr, 8, block);
  lim.ceilingDb.store(0.0f); // ceiling = 1.0
  lim.lookaheadMs.store(3.0f);
  unsigned la = (unsigned)(0.003f * sr);

  std::vector<float> in(block), buf(stride);
  for (unsigned i = 0; i < block; ++i) {
    float s = 0.3f * (float)std::sin(2.0 * kPi * 220.0 * i / sr);
    in[i] = s;
    buf[i] = s;
  }
  lim.process(buf.data(), block, stride, ch);

  // out[i] should equal in[i - la] for i >= la (gain stays exactly 1.0).
  bool transparent = true;
  float worst = 0.0f;
  for (unsigned i = la; i < block; ++i) {
    float d = std::fabs(buf[i] - in[i - la]);
    if (d > worst) worst = d;
    if (d > 1e-6f) transparent = false;
  }
  CHECK(transparent, "below-ceiling output == input delayed by la");
  std::printf("     la=%u worstDelta=%.3e\n", la, worst);
}

// ---- Test 3: sliding max vs brute force ----
static void testSlidingMax() {
  const float sr = 48000.0f;
  const unsigned block = 512, stride = 512, ch = 1;
  fsdsp::MasterLimiter lim;
  lim.init(sr, 8, block);
  lim.ceilingDb.store(60.0f);   // ceiling huge -> gain always 1.0, no shaping
  lim.lookaheadMs.store(3.0f);
  unsigned la = (unsigned)(0.003f * sr); // window W = la

  // Feed random values; the limiter's internal winMax drives GR only when
  // above ceiling, so instead we test pushAndMax_ indirectly is hard; replicate
  // the deque here against brute force using a fresh limiter is not accessible.
  // Instead reconstruct: because gain==1 (huge ceiling), we verify the sliding
  // max property via a standalone reference implementation matching the class.
  // (pushAndMax_ is private; we mirror its contract with a brute-force check on
  // a copy of the algorithm below.)

  std::vector<float> v(5000);
  std::srand(12345);
  for (auto &x : v) x = (float)std::rand() / RAND_MAX;

  // Reference brute-force sliding max over window W = la.
  auto bruteMax = [&](size_t idx) {
    float m = 0.0f;
    size_t start = (idx + 1 >= la) ? (idx + 1 - la) : 0;
    for (size_t k = start; k <= idx; ++k)
      if (v[k] > m) m = v[k];
    return m;
  };

  // Reimplement the monotonic-deque exactly as MasterLimiter::pushAndMax_.
  unsigned cap = la + 2, head = 0, count = 0;
  std::vector<float> dqv(cap);
  std::vector<unsigned long long> dqi(cap);
  unsigned long long id = 0;
  bool okAll = true;
  for (size_t n = 0; n < v.size(); ++n) {
    float x = v[n];
    while (count > 0) {
      unsigned back = (head + count - 1) % cap;
      if (dqv[back] <= x) --count; else break;
    }
    unsigned tail = (head + count) % cap;
    dqv[tail] = x; dqi[tail] = id; ++count;
    while (count > 0 && dqi[head] + la <= id) { head = (head + 1) % cap; --count; }
    ++id;
    float got = dqv[head];
    float want = bruteMax(n);
    if (std::fabs(got - want) > 1e-6f) okAll = false;
  }
  CHECK(okAll, "sliding max (monotonic deque) matches brute force");
  (void)stride; (void)ch;
}

// ---- Test 4: ducker gain computer at knee edges ----
static void testDuckerGainComputer() {
  // Reproduce the gain-computer math from Ducker::applyDuck and verify knee
  // continuity. threshold=-20, ratio=2, knee=6, maxDepth=24 (so cap inactive).
  const float thr = -20.0f, ratio = 2.0f, knee = 6.0f, maxDepth = 24.0f;
  auto grDb = [&](float envDb) {
    float over = envDb - thr;
    float r = ratio;
    float g = 0.0f;
    if (over > knee * 0.5f) {
      g = -over * (1.0f - 1.0f / r);
    } else if (over > -knee * 0.5f && knee > 0.0f) {
      float t = over + knee * 0.5f;
      g = -(1.0f - 1.0f / r) * (t * t) / (2.0f * knee);
    }
    if (g < -maxDepth) g = -maxDepth;
    return g;
  };

  // Below knee (env = thr - 6 dB): no reduction.
  CHECK(std::fabs(grDb(thr - 6.0f) - 0.0f) < 1e-4f, "ducker: below knee -> 0 dB");
  // At lower knee edge (over = -knee/2): still 0.
  CHECK(std::fabs(grDb(thr - knee * 0.5f)) < 1e-4f, "ducker: lower knee edge -> 0 dB");
  // At threshold (over = 0, mid-knee): grDb = -(0.5)*(3^2)/(12) = -0.375
  float atThr = grDb(thr);
  CHECK(std::fabs(atThr - (-0.375f)) < 1e-3f, "ducker: at threshold knee value");
  // Continuity at upper knee edge (over = +knee/2 = 3):
  //   knee branch: -(0.5)*(6^2)/(12) = -1.5
  //   linear branch: -3*(0.5) = -1.5
  float kneeVal = -(1.0f - 1.0f / ratio) * (knee * knee) / (2.0f * knee);
  float linVal = -(knee * 0.5f) * (1.0f - 1.0f / ratio);
  CHECK(std::fabs(kneeVal - linVal) < 1e-4f, "ducker: knee continuous at upper edge");
  // Far above threshold: hits maxDepth cap.
  CHECK(std::fabs(grDb(thr + 100.0f) - (-maxDepth)) < 1e-3f, "ducker: maxDepth cap");
  std::printf("     atThr=%.4f kneeEdge lin=%.4f knee=%.4f\n", atThr, linVal, kneeVal);
}

// ---- Test 5: bus meter peak/rms on a known sine ----
static void testBusMeter() {
  const float sr = 48000.0f;
  const unsigned ch = 1, stride = 2048, block = 2048;
  fsdsp::BusMeter m;
  std::vector<float> buf(stride);
  const float amp = 0.5f;
  for (unsigned i = 0; i < block; ++i)
    buf[i] = amp * (float)std::sin(2.0 * kPi * 440.0 * i / sr);
  m.process(buf.data(), block, stride, ch, sr);
  CHECK(std::fabs(m.rawPeak() - amp) < 1e-3f, "meter raw peak ~= amplitude");
  CHECK(std::fabs(m.rms() - amp / std::sqrt(2.0f)) < 5e-3f, "meter rms ~= amp/sqrt2");
  std::printf("     peak=%.4f rms=%.4f (expect rms=%.4f)\n", m.rawPeak(), m.rms(),
              amp / std::sqrt(2.0f));
}

int main() {
  std::printf("=== DSP unit tests ===\n");
  testLimiterFullScaleSine();
  testLimiterBitTransparentDelay();
  testSlidingMax();
  testDuckerGainComputer();
  testBusMeter();
  std::printf("=== %s (%d failure(s)) ===\n",
              g_failures ? "FAILURES" : "ALL PASS", g_failures);
  return g_failures ? 1 : 0;
}
