- [x] Fix compile errors / correctness issues in audioSample.c
- [x] Implement DSP mode=1 (length-compensated FIR pre/post filtering) inside audioSample.c
- [x] Implement DSP mode=2 (match previous timing: 6.14 sec output length) inside audioSample.c
- [x] Verify output WAV sample counts match requirements for each mode
- [x] Add per-mode output directories and cleanup script in build_and_run.sh
- [x] Update mode=1 upsampling reconstruction to: zero insertion + LPF fc=4kHz + gain=2


