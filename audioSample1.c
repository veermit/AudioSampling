#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define PI 3.14159265358979323846
#define SAMPLE_RATE_16K 16000
#define SAMPLE_RATE_8K  8000
#define DURATION 6.14
#define WINDOW_SIZE_16K (int)(SAMPLE_RATE_16K * DURATION) // 98240
#define WINDOW_SIZE_8K  (WINDOW_SIZE_16K / 2)             // 49120
#define CIRCULAR_LIMIT (WINDOW_SIZE_16K * 2)              // Buffer for 2 windows

typedef struct {
    int16_t ring_buffer[CIRCULAR_LIMIT];
    int write_ptr;
    int windows_completed;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} AudioEngine;

void save_wav(const char* name, int16_t* data, int samples, int rate) {
    FILE* f = fopen(name, "wb");
    if (!f) return;
    uint32_t dataSize = samples * sizeof(int16_t);
    uint32_t fileSize = 36 + dataSize;
    int32_t fmtLen = 16, br = rate * 2;
    int16_t one = 1, sixteen = 16, ba = 2;

    fwrite("RIFF", 1, 4, f); fwrite(&fileSize, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmtLen, 4, 1, f);
    fwrite(&one, 2, 1, f); fwrite(&one, 2, 1, f);
    fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f); fwrite(&sixteen, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dataSize, 4, 1, f);
    fwrite(data, sizeof(int16_t), samples, f);
    fclose(f);
}

// Thread 1: Continuous Producer (Simulating 16k stream into Circular Buffer)
void* producer_routine(void* arg) {
    AudioEngine* engine = (AudioEngine*)arg;
    int sample_count = 0;

    while (engine->windows_completed < 1) { // Running for one full 6.14s event
        pthread_mutex_lock(&engine->lock);
        
        // Simulate writing data into the circular buffer
        for (int i = 0; i < WINDOW_SIZE_16K; i++) {
            float t = (float)sample_count / SAMPLE_RATE_16K;
            engine->ring_buffer[engine->write_ptr] = (int16_t)(20000 * sin(2 * PI * 440.0 * t));
            engine->write_ptr = (engine->write_ptr + 1) % CIRCULAR_LIMIT;
            sample_count++;
        }

        printf("[LOG] Pre-processing: Circular buffer reached 6.14s mark.\n");
        pthread_cond_signal(&engine->cond);
        engine->windows_completed++;
        pthread_mutex_unlock(&engine->lock);
        
        usleep(1000); // Small sleep to simulate real-time stream
    }
    return NULL;
}

// Thread 2: Consumer (Downsample 8k -> Upsample 16k -> Save)
void* consumer_routine(void* arg) {
    AudioEngine* engine = (AudioEngine*)arg;

    pthread_mutex_lock(&engine->lock);
    while (engine->windows_completed < 1) {
        pthread_cond_wait(&engine->cond, &engine->lock);
    }

    // 1. Processing: Downsample to 8k (from the first window)
    int16_t* buf8k = malloc(WINDOW_SIZE_8K * sizeof(int16_t));
    for (int i = 0; i < WINDOW_SIZE_8K; i++) {
        buf8k[i] = engine->ring_buffer[i * 2];
    }
    printf("[LOG] Processing: 8k Downsampled buffer ready.\n");

    // 2. Post-processing: Upsample back to 16k
    int16_t* buf16k_final = malloc(WINDOW_SIZE_16K * sizeof(int16_t));
    for (int i = 0; i < WINDOW_SIZE_8K; i++) {
        buf16k_final[i * 2] = buf8k[i];
        buf16k_final[i * 2 + 1] = buf8k[i];
    }
    printf("[LOG] Post-processing: 16k Reconstruction complete.\n");

    save_wav("continuous_output.wav", buf16k_final, WINDOW_SIZE_16K, SAMPLE_RATE_16K);
    printf("[LOG] Save Event: Final file stored.\n");

    free(buf8k); free(buf16k_final);
    pthread_mutex_unlock(&engine->lock);
    return NULL;
}

int main() {
    AudioEngine engine = {.write_ptr = 0, .windows_completed = 0};
    pthread_mutex_init(&engine.lock, NULL);
    pthread_cond_init(&engine.cond, NULL);

    pthread_t p, c;
    pthread_create(&p, NULL, producer_routine, &engine);
    pthread_create(&c, NULL, consumer_routine, &engine);

    pthread_join(p, NULL);
    pthread_join(c, NULL);

    pthread_mutex_destroy(&engine.lock);
    pthread_cond_destroy(&engine.cond);
    return 0;
}
