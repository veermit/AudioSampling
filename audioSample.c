#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Reads 16kHz PCM int16 samples from counting_1_to_50_16k.raw,
// processes each consecutive 6.14s chunk, and writes three outputs per chunk as WAV:
//  - preprocessed_16k_pcm_XXXX.wav (16k, 98240 samples)
//  - preprocessed_8k_pcm_XXXX.wav  (8k,  49120 samples)
//  - postprocessed_16k_pcm_XXXX.wav (16k, 98240 samples)

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
    // returns 1 if fully read, 0 otherwise
    size_t items = fread(out, sizeof(int16_t), samples, f);
    if (items != samples) return 0;
    return 1;
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

int main(void) {
    FILE* fin = fopen(INPUT_RAW, "rb");
    if (!fin) {
        fprintf(stderr, "[ERR] Cannot open input raw file: %s\n", INPUT_RAW);
        return 1;
    }

    int16_t* pre16 = (int16_t*)malloc((size_t)SAMPLES_16K * sizeof(int16_t));
    int16_t* pre8  = (int16_t*)malloc((size_t)SAMPLES_8K  * sizeof(int16_t));
    int16_t* post16= (int16_t*)malloc((size_t)SAMPLES_16K * sizeof(int16_t));

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

    for (size_t chunk = 0; chunk < fullChunks; chunk++) {
        char out_pre16[256];
        char out_pre8[256];
        char out_post16[256];

        snprintf(out_pre16, sizeof(out_pre16), "%s_%04zu.wav", OUT_PRE_16_BASE, chunk + 1);
        snprintf(out_pre8, sizeof(out_pre8), "%s_%04zu.wav", OUT_PRE_8_BASE, chunk + 1);
        snprintf(out_post16, sizeof(out_post16), "%s_%04zu.wav", OUT_POST_16_BASE, chunk + 1);

        printf("\n[LOG] ===== Chunk %zu/%zu =====\n", chunk + 1, fullChunks);

        // Stage 0: Read 16k PCM chunk
        if (!read_exact_pcm16_chunk(fin, pre16, (size_t)SAMPLES_16K)) {
            fprintf(stderr, "[ERR] Unexpected EOF while reading chunk %zu\n", chunk + 1);
            break;
        }
        printf("[LOG] Stage 0: Read %d samples @ %d Hz\n", SAMPLES_16K, SAMPLE_RATE_16K);

        // Stage 1: preprocessed 16k = input
        printf("[LOG] Stage 1: Write preprocessed 16k WAV: %s\n", out_pre16);
        if (!write_wav_pcm16_mono(out_pre16, pre16, (size_t)SAMPLES_16K, SAMPLE_RATE_16K)) return 1;

        // Stage 2: Downsample 16k -> 8k (decimation)
        printf("[LOG] Stage 2: Downsample 16k -> 8k\n");
        for (int i = 0; i < SAMPLES_8K; i++) {
            pre8[i] = pre16[i * 2];
        }
        printf("[LOG] Stage 2 done: preprocessed 8k samples=%d\n", SAMPLES_8K);

        // Stage 3: write preprocessed 8k
        printf("[LOG] Stage 3: Write preprocessed 8k WAV: %s\n", out_pre8);
        if (!write_wav_pcm16_mono(out_pre8, pre8, (size_t)SAMPLES_8K, SAMPLE_RATE_8K)) return 1;

        // Stage 4: Upsample 8k -> 16k (sample hold)
        printf("[LOG] Stage 4: Upsample 8k -> 16k (sample hold)\n");
        for (int i = 0; i < SAMPLES_8K; i++) {
            post16[i * 2]     = pre8[i];
            post16[i * 2 + 1] = pre8[i];
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

