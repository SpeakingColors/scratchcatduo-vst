# Roadmap: Scratch Cat Duo

## Done (v0.1 test build)
- [x] Deck engine (Hermite varispeed, scratch, cue, hot cues, loops, key lock, brake, backspin, roll, censor, stutter, transform), mixer, FX chain, analysis, tests
- [x] Plugin shell on cat-ui with the shared PurrOne wood

## Next
- [ ] Listen and tune: scratch feel, key lock quality (switch to a WSOLA style stretch), brake and backspin times, FX
- [ ] Beat phase alignment button (sync the beat position, not only the tempo)
- [ ] Controller maps for real devices, from the vendor MIDI charts (FLX4, DDJ-400, and others from the mockup list)
- [ ] Waveform colours by frequency, overview strip, track list
- [ ] Manual with screenshots from the real editor

## Every cat plugin
- Draw through `catui::RenderGate` so meters and animations stop when the window is not in front (audio, automation and MIDI keep running)
- Settings page with MIDI filters on by default, selectable preset/backup folders, UI size reset to 100%
- The PurrOne wood side cheeks from cat-ui (`catui::drawWoodCheeks`), never another wood
