# Roadmap: Scratch Cat Duo

## Done (v0.1 test build)
- [x] Deck engine (Hermite varispeed, scratch, cue, hot cues, loops, key lock, brake, backspin, roll, censor, stutter, transform), mixer, FX chain, analysis, tests
- [x] Plugin shell on cat-ui with the shared PurrOne wood

## Next
- [x] Key lock: WSOLA style grain alignment (unit test: tone purity 0.013 before, 0.996 after at +-8 %, +50 %, -40 %)
- [ ] Listen and tune by ear: scratch feel, key lock on real music, brake and backspin times, FX
- [x] Align button (moves a deck by up to half a beat onto the other deck's beat; unit tested; not yet tried by ear)
- [ ] Controller maps for real devices, from the vendor MIDI charts (FLX4, DDJ-400, and others from the mockup list)
- [ ] Waveform colours by frequency, overview strip, track list
- [ ] Manual with screenshots from the real editor

## Every cat plugin
- Draw through `catui::RenderGate` so meters and animations stop when the window is not in front (audio, automation and MIDI keep running)
- Settings page with MIDI filters on by default, selectable preset/backup folders, UI size reset to 100%
- The PurrOne wood side cheeks from cat-ui (`catui::drawWoodCheeks`), never another wood
