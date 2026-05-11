#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

// Reads 16kHz PCM int16 samples from counting_1_to_50_16k.raw,
// DSP modes:
//   --mode=0 : current logic (no filtering)
//   --mode=1 : FIR downsample (LPF) + reconstruction LPF, with group-delay compensated to keep output lengths fixed
//   --mode=2 : FIR downsample + reconstruction LPF, but keep the overall timing like mode=0 (i.e., no group-delay compensation; outputs are trimmed to required length)

#define FIR_HALF_TAPS 8
#define FIR_TAPS (2 * FIR_HALF_TAPS + 1)
#define LPF_FC_NORM 0.45f // default normalized to Fs/2 (used for modes other than mode-1 reconstruction)
#define MODE1_UP_FC_HZ 4000.0f
#define MODE1_UP_GAIN 2.0f


#define SAMPLE_RATE_16K 16000
#define SAMPLE_RATE_8K  8000
#define DURATION_SEC 6.14

#define SAMPLES_16K ((int)(SAMPLE_RATE_16K * DURATION_SEC)) // 98240
#define SAMPLES_8K  ((int)(SAMPLE_RATE_8K  * DURATION_SEC)) // 49120

#define INPUT_RAW "counting_1_to_50_16k.raw"

// Basenames; chunk index suffix is added: _0001.wav, _0002.wav, ...
#define OUT_PRE_16_BASE  "preprocessed_16k_pcm"
#define OUT_PRE_8_BASE   "preprocessed_8k_pcm"
#define OUT_POST_16_BASE "postprocessed_16k_pcm"

static size_t read_exact_pcm16_chunk(FILE* f, int16_t* out, size_t samples) {
    size_t items = fread(out, sizeof(int16_t), samples, f);
    return items == samples;
}

static void build_lpf_coeffs(float* h, int taps) {
    // windowed-sinc lowpass, cutoff given by LPF_FC_NORM normalized to Fs/2
    int M = taps - 1;
    float fc = LPF_FC_NORM;

    float sum = 0.0f;
    for (int n = 0; n < taps; n++) {
        int k = n - M / 2;
        float x = (float)k;
        float w = 0.54f - 0.46f * cosf(2.0f * (float)M_PI * (float)n / (float)M); // Hamming

        float hd;
        if (k == 0) {
            hd = 2.0f * fc;
        } else {
            hd = sinf(2.0f * (float)M_PI * fc * x) / ((float)M_PI * x);
        }

        h[n] = hd * w;
        sum += h[n];
    }

    if (sum != 0.0f) {
        for (int n = 0; n < taps; n++) h[n] /= sum;
    }
}

static inline int16_t sat_int16(int32_t x) {
    if (x > 32767) return 32767;
    if (x < -32768) return -32768;
    return (int16_t)x;
}

static void fir_convolve_decimate_2(const int16_t* x, int x_len, int decim,
                                      const float* h, int taps, int16_t* y, int y_len,
                                      int group_delay_compensate) {
    int half = taps / 2;
    for (int i = 0; i < y_len; i++) {
        int in_center = i * decim;
        if (group_delay_compensate) in_center += half;

        double acc = 0.0;
        for (int k = 0; k < taps; k++) {
            int xi = in_center + (k - half);
            float xv = 0.0f;
            if (xi >= 0 && xi < x_len) xv = (float)x[xi];
            acc += (double)xv * (double)h[k];
        }
        y[i] = sat_int16((int32_t)lrint(acc));
    }
}

static void fir_convolve_upsample_2_reconstruct(const int16_t* x8, int x8_len,
                                                 const float* h, int taps,
                                                 int16_t* y16, int y16_len,
                                                 int group_delay_compensate) {
    int half = taps / 2;
    for (int n = 0; n < y16_len; n++) {
        int in_index_center = n;
        if (group_delay_compensate) in_index_center -= half;

        double acc = 0.0;
        for (int k = 0; k < taps; k++) {
            int m = in_index_center - (k - half);
            float xv = 0.0f;
            if ((m & 1) == 0) {
                int idx8 = m / 2;
                if (idx8 >= 0 && idx8 < x8_len) xv = (float)x8[idx8];
            }
            acc += (double)xv * (double)h[k];
        }
        y16[n] = sat_int16((int32_t)lrint(acc));
    }
}

static int write_wav_pcm16_mono(const char* path, const int16_t* data, size_t samples, int rate) {
    FILE* f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[ERR] Cannot open output WAV file: %s\n", path);
        return 0;
    }

    uint32_t dataSize = (uint32_t)(samples * sizeof(int16_t));
    uint32_t fileSize = 36 + dataSize;
    uint16_t audioFormat = 1; // PCM
    uint16_t numChannels = 1;
    uint16_t bitsPerSample = 16;
    uint32_t byteRate = (uint32_t)rate * numChannels * bitsPerSample / 8;
    uint16_t blockAlign = numChannels * bitsPerSample / 8;

    // RIFF header
    fwrite("RIFF", 1, 4, f);
    fwrite(&fileSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    // fmt chunk
    fwrite("fmt ", 1, 4, f);
    uint32_t fmtChunkSize = 16;
    fwrite(&fmtChunkSize, 4, 1, f);
    fwrite(&audioFormat, 2, 1, f);
    fwrite(&numChannels, 2, 1, f);
    fwrite(&rate, 4, 1, f);
    fwrite(&byteRate, 4, 1, f);
    fwrite(&blockAlign, 2, 1, f);
    fwrite(&bitsPerSample, 2, 1, f);

    // data chunk
    fwrite("data", 1, 4, f);
    fwrite(&dataSize, 4, 1, f);
    fwrite(data, sizeof(int16_t), samples, f);

    fclose(f);
    return 1;
}

int main(int argc, char** argv) {
    int mode = 0;
    if (argc >= 2) mode = atoi(argv[1]);

    const char* out_dir = ".";
    if (argc >= 3) out_dir = argv[2];

    char out_dir_slash[512];
    snprintf(out_dir_slash, sizeof(out_dir_slash), "%s/", out_dir);


    // mode=0 : current logic (no filtering)
    // mode=1 : FIR-based down/up with group-delay compensation
    // mode=2 : FIR-based down/up without group-delay compensation (still fixed-length outputs)

    FILE* fin = fopen(INPUT_RAW, "rb");
    if (!fin) {
        fprintf(stderr, "[ERR] Cannot open input raw file: %s\n", INPUT_RAW);
        return 1;
    }

    int16_t* pre16  = (int16_t*)malloc((size_t)SAMPLES_16K * sizeof(int16_t));
    int16_t* pre8   = (int16_t*)malloc((size_t)SAMPLES_8K  * sizeof(int16_t));
    int16_t* post16 = (int16_t*)malloc((size_t)SAMPLES_16K * sizeof(int16_t));

    if (!pre16 || !pre8 || !post16) {
        fprintf(stderr, "[ERR] malloc failed\n");
        free(pre16); free(pre8); free(post16);
        fclose(fin);
        return 1;
    }

    // Determine approximate number of full 6.14s chunks.
    fseek(fin, 0, SEEK_END);
    long totalBytes = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    size_t totalSamples = (size_t)totalBytes / sizeof(int16_t);
    size_t chunkSamples16 = (size_t)SAMPLES_16K;
    size_t fullChunks = totalSamples / chunkSamples16;

    printf("[LOG] Input: %s bytes=%ld (~%zu int16 samples).\n", INPUT_RAW, totalBytes, totalSamples);
    printf("[LOG] Chunk size: %d samples @ %d Hz (%.3f sec). Full chunks found: %zu\n",
           SAMPLES_16K, SAMPLE_RATE_16K, DURATION_SEC, fullChunks);

    if (fullChunks == 0) {
        fprintf(stderr, "[ERR] Input does not contain a full 6.14s chunk.\n");
        free(pre16); free(pre8); free(post16);
        fclose(fin);
        return 1;
    }

    float h[FIR_TAPS];
    float h_recon[FIR_TAPS];

    if (mode != 0) {
        // Default FIR used for downsampling.
        build_lpf_coeffs(h, FIR_TAPS);

        // Reconstruction FIR:
        // Mode 1 spec: Upample by 2 via zero insertion, then LPF with fc=4kHz and gain=2.
        if (mode == 1) {
            // Temporarily override cutoff using normalized fc at 16kHz output.
            // fc_norm = fc / (Fs/2) = 4000 / 8000 = 0.5
            float fc_norm = MODE1_UP_FC_HZ / (SAMPLE_RATE_16K / 2.0f);
            int M = FIR_TAPS - 1;
            float sum = 0.0f;
            for (int n = 0; n < FIR_TAPS; n++) {
                int k = n - M / 2;
                float x = (float)k;
                float w = 0.54f - 0.46f * cosf(2.0f * (float)M_PI * (float)n / (float)M); // Hamming

                float hd;
                if (k == 0) {
                    hd = 2.0f * fc_norm;
                } else {
                    hd = sinf(2.0f * (float)M_PI * fc_norm * x) / ((float)M_PI * x);
                }

                h_recon[n] = hd * w;
                sum += h_recon[n];
            }
            if (sum != 0.0f) {
                for (int n = 0; n < FIR_TAPS; n++) h_recon[n] /= sum;
            }
            // Apply gain=2 for zero-insertion amplitude correction.
            for (int n = 0; n < FIR_TAPS; n++) h_recon[n] *= MODE1_UP_GAIN;
        } else {
            // Mode 2: keep same coefficients as downsampling (existing behavior).
            memcpy(h_recon, h, sizeof(h));
        }
    }


    for (size_t chunk = 0; chunk < fullChunks; chunk++) {
        char out_pre16[256];
        char out_pre8[256];
        char out_post16[256];

        snprintf(out_pre16, sizeof(out_pre16), "%s%s_%04zu.wav", out_dir_slash, OUT_PRE_16_BASE, chunk + 1);
        snprintf(out_pre8, sizeof(out_pre8), "%s%s_%04zu.wav", out_dir_slash, OUT_PRE_8_BASE, chunk + 1);
        snprintf(out_post16, sizeof(out_post16), "%s%s_%04zu.wav", out_dir_slash, OUT_POST_16_BASE, chunk + 1);


        printf("\n[LOG] ===== Chunk %zu/%zu =====\n", chunk + 1, fullChunks);

        if (!read_exact_pcm16_chunk(fin, pre16, (size_t)SAMPLES_16K)) {
            fprintf(stderr, "[ERR] Unexpected EOF while reading chunk %zu\n", chunk + 1);
            break;
        }

        // Stage 1: preprocessed 16k = input (no change)
        printf("[LOG] Stage 1: Write preprocessed 16k WAV: %s\n", out_pre16);
        if (!write_wav_pcm16_mono(out_pre16, pre16, (size_t)SAMPLES_16K, SAMPLE_RATE_16K)) return 1;

        // Stage 2: Downsample 16k -> 8k
        printf("[LOG] Stage 2: Downsample 16k -> 8k\n");
        if (mode == 0) {
            for (int i = 0; i < SAMPLES_8K; i++) pre8[i] = pre16[i * 2];
        } else {
            int group_delay_compensate = (mode == 1) ? 1 : 0;
            fir_convolve_decimate_2(pre16, SAMPLES_16K, 2, h, FIR_TAPS, pre8, SAMPLES_8K,
                                     group_delay_compensate);
        }
        printf("[LOG] Stage 2 done: preprocessed 8k samples=%d\n", SAMPLES_8K);

        // Stage 3: write preprocessed 8k
        printf("[LOG] Stage 3: Write preprocessed 8k WAV: %s\n", out_pre8);
        if (!write_wav_pcm16_mono(out_pre8, pre8, (size_t)SAMPLES_8K, SAMPLE_RATE_8K)) return 1;

        // Stage 4: Upsample 8k -> 16k
        printf("[LOG] Stage 4: Upsample 8k -> 16k\n");
        if (mode == 0) {
            for (int i = 0; i < SAMPLES_8K; i++) {
                post16[i * 2]     = pre8[i];
                post16[i * 2 + 1] = pre8[i];
            }
        } else {
            int group_delay_compensate = (mode == 1) ? 1 : 0;
            // Zero-insertion upsampling is implicit in fir_convolve_upsample_2_reconstruct()
            // (it only injects source samples at even indices).
            fir_convolve_upsample_2_reconstruct(pre8, SAMPLES_8K,
                                                (mode == 1) ? h_recon : h, FIR_TAPS,
                                                post16, SAMPLES_16K,
                                                group_delay_compensate);
        }

        printf("[LOG] Stage 4 done: postprocessed 16k samples=%d\n", SAMPLES_16K);

        // Stage 5: write postprocessed 16k
        printf("[LOG] Stage 5: Write postprocessed 16k WAV: %s\n", out_post16);
        if (!write_wav_pcm16_mono(out_post16, post16, (size_t)SAMPLES_16K, SAMPLE_RATE_16K)) return 1;
    }

    free(pre16); free(pre8); free(post16);
    fclose(fin);

    printf("\n[LOG] DONE. Wrote outputs for %zu full chunks.\n", fullChunks);
    return 0;
}

