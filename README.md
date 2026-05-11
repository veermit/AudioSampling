# AudioSampling

## Build

```bash
clang -O2 audioSample.c -o audioSample
```

## Generate sample counting audio (1..50)

Generate a spoken AIFF using macOS `say`:

```bash
say -v Samantha -o counting_1_to_50.aiff "one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen seventeen eighteen nineteen twenty twenty one twenty two twenty three twenty four twenty five twenty six twenty seven twenty eight twenty nine thirty thirty one thirty two thirty three thirty four thirty five thirty six thirty seven thirty eight thirty nine forty forty one forty two forty three forty four forty five forty six forty seven forty eight forty nine fifty"
```

Convert that AIFF to 16 kHz, mono, 16-bit little-endian PCM raw:

```bash
ffmpeg -i counting_1_to_50.aiff -ar 16000 -ac 1 -f s16le counting_1_to_50_16k.raw
```

(Optional) Also create a WAV for playback:

```bash
ffmpeg -i counting_1_to_50.aiff -ar 16000 -ac 1 counting_1_to_50_16k.wav
afplay counting_1_to_50_16k.wav
```

## Run the sampling conversion

This program reads:
- `counting_1_to_50_16k.raw` (16kHz PCM int16)

and for each full 6.14 second chunk it writes:
- `preprocessed_16k_pcm_XXXX.raw` / `.wav`
- `preprocessed_8k_pcm_XXXX.raw` / `.wav`
- `postprocessed_16k_pcm_XXXX.raw` / `.wav`

(Chunk size = 6.14s; 16k samples = 98240, 8k samples = 49120.)

Run:

```bash
./audioSample
```

