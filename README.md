# Reticle

Open-source audio metering plugin. Level meters, loudness (LUFS), spectrum analyzer, spectrogram, and vectorscope in one window.

**Status:** Phase 1 — Level metering (peak + RMS)

## Build

Requires: macOS, Xcode, CMake 3.22+

```bash
git clone --recursive https://github.com/YOUR_USERNAME/Reticle.git
cd Reticle
cmake -B build -G Xcode
cmake --build build --config Release
```

The AU plugin is automatically copied to `~/Library/Audio/Plug-Ins/Components/`.

## Formats

- [x] AU (Audio Unit)
- [ ] VST3 (planned)
- [ ] AAX (planned)

## Roadmap

| Phase | Feature | Status |
|-------|---------|--------|
| 1 | Level meter (peak + RMS) | In progress |
| 2 | LUFS loudness metering | Planned |
| 3 | Spectrum analyzer | Planned |
| 4 | Stereo vectorscope | Planned |
| 5 | 2D spectrogram | Planned |
| 6 | Unified panel layout | Planned |

## License

GPL-3.0 (required by JUCE free license)
