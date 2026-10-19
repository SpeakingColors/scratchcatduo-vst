# Scratch Cat Duo 🐈

<img src="assets/icon.svg" width="96" align="right" alt="icon">

Two DJ decks and a mixer in one plugin (VST3 and standalone). Free and open source, Linux first.

## What works now (v0.1, test build)
- Two decks: load wav, aiff, flac, ogg or mp3; BPM, beat grid and key (Camelot) analysis; scratch on the platter, cue, 8 hot cues, loops, brake, backspin, roll, censor, stutter, transform, nudge, beat align, key lock (WSOLA aligned grains), tempo fader (8, 16, 50 %), sync to the DAW tempo or the other deck.
- DAW input mode: send a DAW track to a deck and scratch the last minute of it.
- Mixer: trim, 3 band EQ with kill, colour filter, channel faders, crossfader (smooth, sharp, constant power), cue (headphone) output, master.
- FX chain of three (filter, echo, reverb, flanger, crush) on deck A, deck B, A+B or master.
- Separate outputs for each deck, MIDI learn, stacked waveforms on one time axis.
- Not tuned by ear yet. Controller maps for specific devices need verified MIDI charts and are not included.

<p align="center">
  <a href="https://ko-fi.com/studio87"><img src="https://img.shields.io/badge/Support_Studio_87-on_Ko--fi-FF5E5B?style=for-the-badge&logo=kofi&logoColor=white" alt="Support Studio 87 on Ko-fi"></a>
</p>

☕ Free and open source, made by one person. If it helps your music, a coffee on [Ko-fi](https://ko-fi.com/studio87) keeps it coming. 💜

## Docs
- [Interactive mockup](docs/README.md)
- [Roadmap](ROADMAP.md)
- Build: `cmake -S . -B build` (JUCE and cat-ui are fetched). Engine tests without JUCE: `sh tests/run.sh`.

## License
AGPL-3.0-or-later, see [LICENSE](LICENSE).

Not affiliated with or endorsed by any hardware maker named here. All trademarks belong to their owners.
