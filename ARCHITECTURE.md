# Audio Sampling + DSP Architecture

## Overview
This project converts a 16 kHz, mono, PCM16 raw input stream into three WAV outputs for each fixed-duration window:

1. **Stage 1:** `preprocessed_16k_pcm_XXXX.wav`
   - 16 kHz
   - PCM16
   - **Length:** 6.14 seconds => `SAMPLES_16K = 16000 * 6.14 = 98240` samples

2. **Stage 2:** `preprocessed_8k_pcm_XXXX.wav`
   - 8 kHz
   - PCM16
   - **Length:** `SAMPLES_8K = 8000 * 6.14 = 49120` samples

3. **Stage 4/5:** `postprocessed_16k_pcm_XXXX.wav`
   - 16 kHz
   - PCM16
   - **Length:** 6.14 seconds => `SAMPLES_16K = 98240` samples

Windows are processed back-to-back. The number of windows is computed from the raw input size and `SAMPLES_16K`.

## Build/Run
### Build
```bash
clang -O2 audioSample.c -o audioSample
```

### Run (interactive mode selection)
```bash
./build_and_run.sh
```

### Run (explicit mode)
```bash
./build_and_run.sh 0
./build_and_run.sh 1
./build_and_run.sh 2
```

## Modes
The executable supports a single CLI argument:

- `mode` (implemented as positional arg `argv[1]`):
  - `0` = naive
  - `1` = FIR + group-delay compensation
  - `2` = FIR without group-delay compensation



### Mode 0 — Baseline / Naive
- **Downsample 16k -> 8k:** pick every other sample (decimation)
- **Reconstruction 8k -> 16k:** sample-hold (repeat each 8k sample twice)

No filtering is applied.

### Mode 1 — FIR-filtered with Group-Delay Compensation
- **Downsample:**
  - FIR lowpass (anti-alias) + decimation by 2
  - group-delay compensated so the FIR’s centered response aligns with the fixed output-length requirement
- **Reconstruction (8k -> 16k):**
  - Upample by 2 via **zero insertion**
  - Apply reconstruction **LPF with cutoff fc = 4 kHz** (at 16 kHz output sampling; normalized cutoff = 0.5)
  - Apply **gain = 2** to compensate zero-insertion amplitude
  - FIR lowpass reconstruction
  - group-delay compensated


### Mode 2 — FIR-filtered without Group-Delay Compensation
Same FIR downsample/reconstruction as mode 1, but:
- group-delay compensation is disabled
- outputs still have the required fixed sample lengths (the algorithm computes exactly the target number of output samples)

## Core Pipeline (per window)
For each chunk `XXXX` (1-based index):

### Stage 0 — Read
- Read `SAMPLES_16K` int16 samples from `counting_1_to_50_16k.raw`.

### Stage 1 — Write preprocessed 16k
- The input chunk is written directly to `preprocessed_16k_pcm_XXXX.wav`.

### Stage 2 — Downsample 16k -> 8k
- **Mode 0:**
  - `pre8[i] = pre16[i*2]`
- **Mode 1/2:**
  - FIR lowpass coefficients are generated (windowed-sinc + Hamming)
  - FIR convolution is computed at fixed output indices while decimating by 2
  - `pre8` is filled with exactly `SAMPLES_8K` samples

### Stage 3 — Write preprocessed 8k
- `pre8` is written as WAV at 8 kHz.

### Stage 4 — Reconstruction 8k -> 16k
- **Mode 0:** sample-hold
  - `post16[2*i] = post16[2*i+1] = pre8[i]`
- **Mode 1/2:** FIR reconstruction
  - compute exactly `SAMPLES_16K` samples
  - conceptually upsample by 2, then lowpass reconstruct using the same FIR

### Stage 5 — Write postprocessed 16k
- `post16` is written as WAV at 16 kHz.

## Data Structures
All processing is window-buffer based:
- `int16_t pre16[SAMPLES_16K]`
- `int16_t pre8[SAMPLES_8K]`
- `int16_t post16[SAMPLES_16K]`

The FIR taps are stored in a small `float h[FIR_TAPS]` array.

## FIR/LPF Design
### Coefficient Generation
FIR coefficients are generated via:
- windowed sinc lowpass
- Hamming window
- cutoff uses normalized frequency `LPF_FC_NORM = 0.45` (interpreted relative to Nyquist)
- coefficients are normalized for unity DC gain.

### Implementation Notes
- Boundary handling: out-of-range input samples are treated as zero (zero padding).
- The functions compute output samples by summing over all FIR taps.

## Output WAV Format
WAV writing is minimal and self-contained:
- PCM16, mono
- writes RIFF header, fmt chunk, and data chunk

## Files
- `audioSample.c`
  - entire pipeline, DSP modes, FIR functions, WAV writer
- `build_and_run.sh`
  - convenience build/run script with interactive mode selection
- `TODO.md`
  - tracks remaining work (now marked done)
- `ARCHITECTURE.md`
  - this document

