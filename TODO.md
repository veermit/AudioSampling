- [x] Modify target C file to read `counting_1_to_50_16k.raw` (16k PCM int16) for exactly 6.14s (98240 samples)
- [x] Write `preprocessed_16k_pcm.raw` (98240 samples)
- [x] Downsample 16k->8k by decimation to 49120 samples and write `preprocessed_8k_pcm.raw`
- [x] Upsample 8k->16k by sample hold to 98240 samples and write `postprocessed_16k_pcm.raw`
- [x] Add logs for bytes read and sample counts per stage
- [x] Compile + run and verify output file sizes
- [ ] Update program to dump the 3 stages as WAV files as requested

