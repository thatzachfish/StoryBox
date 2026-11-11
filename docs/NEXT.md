# StoryBox: notes for the next build

From Zach, playing the first hardware build (2026-11-08):

- [x] Loading screen in the new palette
- [x] HOME goes back to the home screen
- [x] Story and Garden pages feel redundant: merge
- [x] Battery indicator not pink (reads as "running out")
- [x] Remove leftover scale-mode text (Ionian, Dorian ...) from the screen
- [x] Reverse and Stutter misbehave when latched (they hold one moment forever); a latched effect needs a way back to its knobs
- [x] An "add to chord" leftover (the keys still toggle scale notes): drop scale editing
- [x] The page dots by the page name: far too many
- [x] Bells too loud overall; lead's default level too loud
- [~] Strings don't sound like a violin; Piano nice but not piano-like
- [x] Keep Guitar (koto-like, liked) and Flute
- [x] Phaser too weak
- [x] ACTS key map cluttered: black-key labels at the top, cleaner layout
- [x] Stings too quiet: own level, louder, longer, independent of the bells

Decided earlier:

- [x] SEL = Acts mode, EDIT = Lead mode; no scale-editing mode
- [x] Key map while holding HOME
- [x] Every button shows feedback on screen
- [x] Home screen: a lean status view (story, act, chord, tracks, Energy, Tension, effects) with a legend of the buttons
- [x] Show a mockup of the Home screen before building it
- [x] Act knob shows the old story's act name during a story change (cosmetic)

Later (2026-11-08 evening):

- [x] Inside Act 3 still too decoherent: restructure it
- [x] A top-level Structure knob (steadier drums, chords as written, less wandering)
- [x] Pre-show livelier, more of a hip-hop beat (needs a snare / clap)
- [x] UX: one job per screen (Home calm; hold HOME the buttons; SEL the acts map; EDIT the lead; FX the effects),
      not everything on Home (knobs on top, the mode at the bottom right)
- [x] Sleep timer (Setup, knob 3): 15 to 60 minutes, fading over the last three
- [ ] Looper: last, maybe not at all

From Zach, playing 0.2 on the FM-1 (2026-11-08 night):

- [x] Feel second in the pages, next to Home
- [x] Home and Feel: a visualizer distinct from the instrument pages; an instrument page lights its part of the rings
- [x] Energy shown: a meter on the Feel page
- [ ] (Reverse and Stutter sound amazing: keep)
- [x] SELECT on an effect's page (Filter ...) goes back to the Effects page
- [x] Double-tap latches an effect, and double-tap unlatches it
- [x] Swappable sounds for the non-lead tracks (to discuss)
- [x] Page name and dots at the top left above the knobs, the battery top right
- [x] SELECT wraps around: past the last page to the first, and back
- [x] Guitar lead a little louder
- [x] Piano out: a Rhodes, polyphonic
- [x] Strings: a bowed solo violin, polyphonic (a cello is fine; a section if much better). Try a bowed voice
      with body resonances, slow swell, delayed vibrato, bow noise; Violin and Cello registers
- [x] Colours a little brighter
- [x] Drums a little louder, more variation: Zach trusts me (fills at phrase ends, bar-to-bar variation,
      human velocities, more kits). Keep the Structure knob, liked
- [x] A new story: Fly (Zach to describe)

Also in 0.3: Melody (the bells' page) with a Sound per act; Pad Strings and Organ; a Brushes kit; the story's path on
Story; Lead's keyboard; Setup's sleep timer and key; System in words; a picture for each effect; RAM packed (80 KB free).
- [x] Intermission (D5): a structured boom-bap groove in C minor, 90 bpm, swung (Lights Up, Lobby, Snack Bar, Last Call)

From Zach, after 0.3 (2026-11-08 late), for the next build:

- [ ] Call and response (as NACHFAHRT's): play a melody, StoryBox answers it, improvising on it (notes kept as
      events, not audio: its rhythm with notes moved in the scale, a turn, an octave; cheap)
- [x] A Scatter effect (as TONITOR's) in place of the least liked punch-in (the Phaser?): the last moment cut in
      slices, replayed out of order, some reversed or an octave up (from the punch-in ring we have)
- [x] An effect of its own for each instrument (suggested: Pad Shimmer, Melody a ping-pong echo, Bass Drive,
      Drums Crush), or a Space knob each: to discuss

From Zach, playing 0.3 (2026-11-09 morning):

- [x] Feel's tide picture confusing (keep Surf: loved)
- [x] One knob order for every instrument page, and as far as it goes every page: the same place, the same job
- [x] Lead mode: the keys go dark (they should light; the scale's keys at least)
- [x] Lights (System) seems to do nothing: in ACTS the keys ignore it; make it govern everything, say what it does
- [ ] Battery: read 1 of 3 bars after a night's play, and still after hours on a charger. The bars come from the
      battery's voltage, 3 steps; a wall charger isn't seen (the bolt only with a computer). Look into it
- [ ] (Sleep timer: worked well; the lights going dark too: liked)
- [x] Home isn't homey: call it Mixer (it is the four levels)?
- [x] ACTS page: the black keys' row (tracks, breakdown) at the top, as on the keyboard (above the white keys)
- [x] Lead's picture distinct from the instrument pages', at a glance
- [x] Mixer (Home): a picture of its own too (the four tracks as a mixer: their levels and how loud each is now)

Sound notes (2026-11-09):

- [x] Rhodes: held notes sustain while held (then ring out)
- [x] Flute: a slower release
- [x] Guitar still too loud; Slide (loved) a bit loud too
- [x] Violin: not string-like but liked: keep, a slower release
- [x] Cello: more a digital sax, but nice (keep; maybe name it so)
- [x] Vibes too near Marimba: make it a vibraphone (pure, long, the motor's shimmer); Harp: the least liked
- [x] Drums and fills: loved, Intermission's especially: keep developing the drums
- [ ] Fly: Zach to go through carefully
- [x] Post Show: a ninth story like Intermission (boom-bap), musically distinct, triumphant (on E5, key 23)
- [ ] Fades: good as they are
- [x] Stings far too loud (Intermission's especially): bring them down, level them across stories
- [x] Breakdown's label: with the tracks' row (the black keys), not off by the story's name
- [x] Fly's Buzz: a fly buzzing (a buzzy tone wobbling in pitch, swelling, flying across left to right), not bells;
      The web: liked
- [x] Knob layout: keep Drums' Swing and Melody's Octave (Zach). Proposal B: same order everywhere (Level, how busy,
      Sound, range), every control kept; the instruments' own effects (Shimmer, Echo, Drive, Crush) on one new
      page, in the Mixer's order. Awaiting Zach
- [x] Scatter in place of Phaser: yes
- [x] Harp: replace it (with what: propose)
- [x] Mockups first: Mixer's channel strips, Lead's own picture, a clearer Feel (no tide)
- [x] Fly: more waltz (oom-pah-pah: the brushes' rim on 2 and 3), more Structure; Entry not scary yet; Approach's
      melody instrument wrong: a slide guitar running there (a Melody sound "Slide": each note sliding into the
      next, its vibrato); Tension building across the acts (Devour higher); Good-bye liked as it is
- [x] Knob layout B approved: Level, how busy, Sound, range everywhere; a Colours page (Shimmer, Echo, Drive, Crush)
- [x] Colours more saturated (mockups-0.4: yellow 255,212,58; fuchsia 255,62,178; pad 104,232,84; melody 64,214,240;
      bass 255,140,52; drums 194,142,255; frost 120,170,255)
- [x] Mockups sent (2026-11-09): Mixer channel strips, Colours, Lead's melody trace, Feel without the tide
- [x] Feel: "Right now" is called Energy: one Energy meter (ALGORITHM's setting lit; a marker riding on it where the
      swell has the music now)
- [x] Rings redrawn, option B (rings-options mockup): every ring in short segments like the meters: drums one
      segment a step (hits thick, now yellow), bass and pad segmented arcs (lit as loud as they are), the melody's
      notes as segments at their places, fading
- [x] Lighthouse: the sea (Surf) under every act, a stronger pad, a little Tension; Market Day livelier, sparklier
      (faster, brighter, the marimba an octave up), its drums looser (Structure down, more hits)
- [x] Banjo (lead and melody); Riebeck (C4): an original campfire-and-stars piece in Outer Wilds' spirit
- [ ] Battery reading: still to look into (Zach to try charging it switched off)

From Zach, playing 0.4 (2026-11-09):

- [x] ACTS keyboard too full, its banks hard to tell apart, the screen's lists don't help: a clearer layout (stings to
      the black keys?), lighting by bank, the screen echoing the keyboard (proposal sent)
- [x] Stings still too loud
- [x] Scatter doesn't sound good: another draft, more a shimmer or a sparkle
- [ ] Reverse: not loving it yet (Zach to play more)
- [x] The page name's dots jump with the name's length: the dots in one place
- [x] Post Show's Stage Door good; more unusual chords across the acts (a minor, a maj7 now and then); its acts'
      drum patterns more distinct
- [x] Feel: brighter, more colour; Tension dark at 0; Next chord shows nothing (with a progression: en_drift is
      only the free drift's); Energy set to half reads a quarter (the "now" marker? check)
- [x] Post Show: too minor, a little scary at times: jazzy instead, maj7s (replaces the "unusual chords" note)
- [x] ACTS keyboard (approved, Zach): acts F3 G3 A3 B3; the mix F#3 Pad, G#3 Melody, A#3 Bass, C#4 Drums, D#4
      Break; stories from middle C: C4 D4 E4 F4 G4 A4 B4 C5 D5 E5 (reorder soon); stings C#5 D#5 F#5 (up to
      three); F5 G5, F#4 G#4 A#4 spare. Lights by bank
- [x] Story page: four rows of a short keyboard (F3-B3, C4-F4, F#4-C5, C#5-G5), the black keys' names above each
      strip, the white keys' under, every key its letter (black keys wider, the letter in them); where it is filled
      (mockup acts-rows-2)
- [x] Story page rows: F3-B3, C4-F4, F#4-B4, C5-E5 (Intermission starts the last row, the stings over it); the spares
      (F5 F#5 G5) not drawn until used (mockup acts-rows-3)
- [x] Banjo: a melody sound only (no lead banjo; Riebeck's lead another); quieter, less bright/sharp, less like the
      guitar (a banjo's plunk: its head's quick ring)
- [x] Lead Glide does nothing for most sounds (only Flute and Slide, legato): make it work for every lead sound (each
      note gliding from the last)
- [x] Lead: every key plays its own note (no snapping to the scale); only the key's notes lit (the rest dark). Check
      the lit keys match the scale (Zach: some sounded wrong: the snapping moved out-of-key keys to a neighbour)
- [x] Colours page next to Mixer (Mixer, Colours, Feel, Story, ...)
- [x] Pages, left to right: Story (acts), Feel, Mixer, Colours, Effects (?), Pad, Melody, Bass, Drums, Lead, Setup,
      System (replaces "Colours next to Mixer"). Buttons: HOME the acts (ACTS mode and the Story page), SEL Effects,
      FX Colours, EDIT Lead, GLO Setup
- [x] SEL: Effects (right after Colours in the pages)
- [x] Freeze sounds muffled: it shuts the reverb's input (new notes go dry) and holds its tail forever (the plate's
      damping darkening it). Keep some reverb for what plays (~35%), hold the tail long but not forever, less damping
      while frozen
- [ ] Bluetooth audio out (A2DP): wanted. fm1-base's own stack, bt_audio: about +18 KB RAM, +31 KB pool (of ~77 KB
      free), SBC at its best (bitpool 53, ~328 kbit/s, ~3 % CPU); 150-300 ms late (for listening); the FM-1's own
      output muted while streaming; four devices remembered, the last reconnected at power-on. Needs a page
      (scan, pick, forget). Test on hardware. Bluetooth MIDI: later (the same stack)
- [x] A track silent after a mute and unmute (0.4, until a restart): the engine's command queue (64) could drop a word
      when a story change posted many at once: 256 now, and the mutes said again every second

Future (Zach, 2026-11-09):

- [x] Bluetooth audio (0.6): the Bluetooth page after System (Speaker, Sound to, Bluetooth on/off); SAVE there connects
      or lets go, held forgets; the last speaker called again at power-on. To try on hardware
- [ ] Tap tempo: tap the beat to sync with live musicians
- [ ] A chord mode: play chords live, change a story's progressions on the fly (a live-music tool)
- [x] Call and response (0.6): Answer on the System page (0 off .. 100 free); a phrase on the lead, a beat's rest,
      and StoryBox answers it (rhythm kept, notes moved along the scale, ending on the chord), in blue on the Lead page
- [ ] Bluetooth MIDI (later)
- [ ] The instrument pages' pictures as columns (like the Mixer's), not rings: the look the rest has now
- [ ] The punch-in effects' pages as nice as the Colours page

## Fast and Slow (Zach's, in progress)

Two brothers who try to walk across the world; they lose each other and find each other again. Happy and sad.
- On E4 (Riebeck's key now); Riebeck moves to A4; Deep Forest goes.
- A waltz in 3/4, in G, about 90 bpm, brushes, warmer than Fly.
- Acts: 1 Together (G Em D Em Em D D G, two bars each; the story also ends here), 2 Dream (D6, Cmaj7, four bars each),
  3 Flower (Fmaj7, Cmaj7), 4 The Dark (Em, Cmaj7, four bars each).
- Two melodies, one a brother, in counterpoint: one-shots on the sting keys, starting on the next downbeat, in time.
  Sweet, legato instruments that harmonize with each other (two that blend, not two that clash). The melodies should
  also shape the melody / improvisation (the generated melody drawing on their notes and rhythms).
- Waiting for: the melodies (the recorder: thatzachfish.github.io/StoryBox/record/).
- Slow's theme (Zach played it, 90 bpm; as cleaned up, a bar a line, quarters unless held), over Together's chords:
  B A G | B. | B A G | G. | E G A | B A G | B (two bars) || B A G | B. | B A G | G. | E G A | B A G | G (two bars)
  (as played: the notes short, a few early (bar 12 and 14 of the take), an extra G eighth at the end of bar 4)
- Fast's theme (Zach played it, 90 bpm; a bar a line, as read): G4 B4 D5 | G5 G5 G5 | G5 E5(e) D5 | D5(h) D5 |
  E5 E5 E5 | E5 C5 B4 | E5(e) C5(e) B4 B4 — seven bars as recorded (more to come?), above Slow's
- (Zach, 2026-11-09 evening) Act 1 renamed Together; its chords: G Em Cmaj7 G | Em Cmaj7 Cmaj7 G, two bars each (3/4)
  (not G Em D Em ...). Dream: D6 (the 6th, the 5th and the octave brought out), then Cmaj7. Flower: Fmaj7 to Cmaj7,
  brighter and happier. A stronger waltz, a slower tempo. Waiting: is Fast's theme 7 bars; bars 3 and 7's quick notes.
- Next build (queued): Answer on the Lead page in Tone's place (Off / answer / past half: improvises, busier with
  Energy; off at every start); Punch for Crush (drums); a Harmonizer for Reverse; the Energy meter in the bottom bar,
  equal blocks; the Bluetooth page simpler, its first knob free; the little guy as the boot screen, Setup's box like
  him; Drums' Hits named Density. Later: the Story page's first knob (free now PRESETS turns the acts).
- Bluetooth crashes: render_chunk (pc 0x2028878) while streaming: flash reads corrupted under the radio's current
  (fm1-base radio.c's note). Possible fix: the engine's render in RAM (PLAT_RAM_CODE), its own test build.

## After 0.7 (Zach, 2026-11-09)
- Story page: the act's four squares (PRESETS turns them) and the Energy meter side by side, near their controls; then
  Story's knob 1 is free for something new.
- OCTAVE buttons: per page. On Pad, Bells, Bass and Lead pages they move that instrument's octave; on Story, Energy
  down/up. In LEAD key mode they stay the keyboard's octave.
- Bluetooth crash: render_chunk (the engine's audio render) faulting under radio load; it can also reset without a
  crash record. Bluetooth was steady through 0.6.5. Candidates: the engine render in RAM (PLAT_RAM_CODE), or
  comparing what 0.6.6+ added to the audio path (the strips' redraws, Answer, themes) under a long BT stream.

## The crashes (2026-11-10 night)
- 0.7 resets with no crash record, also without a speaker (Sleep, Riebeck, on the FM-1's own speaker). The reports'
  "last watchdog reset in ?" is a stale .noinit value (wdt_stage is only written on a watchdog reset), not evidence.
- 0.6.11's two records: the same pc, render_chunk+0x5208, vec 1 (a CPU exception). The disassembler in CI (pi32v2
  objdump) prints no code, so the instruction is unknown.
- The engine's load is unchanged from 0.6.5 (host bench, every act, 60 s: averages within noise), and 20 minutes of
  each Sleep and Riebeck act under ASan/UBSan (float casts, divides) found nothing.
- 0.7.1 keeps the crash history and debug stats in flash (fm1.toml crash_history, debug_stats; plat_stats_enable): the
  next crash's record survives a power-off, and the stats show the sound's load every 5 minutes.
- Range (the radio's strength: fm1-base's 16 + level in each BR/EDR frame's txpwer/txset) needs a helper in
  fm1-base's BSP: the build refuses FM1_BR_* registers outside it. A request for fm1-base.

## Queued for 0.8 (Zach, 2026-11-10 review) — not built yet
Already in the code (0.7.4, unpublished): Together's chords G Em Cmaj7 G / Em Cmaj7 Cmaj7 G, two bars each; Energy 50 at
power-on.
- Dream: a haunting bell refrain over D6, D D D B A (from his dream.mid: D5 D5 D5 B4 A4).
- Story page: Act off knob 1 (PRESETS does it); Tempo on knob 1.
- HOME held + SELECT: Fade (not Tempo). Fade's default 5 s.
- Instrument pages, one pattern: 1 level, 2 how much, 3 the sound (the only box), 4 its character:
  Pad Level Swell Sound Bright; Melody Level Density Sound Octave (a circle); Bass Level Fifth Sound Glide;
  Drums Level Density Kit Swing; Lead Level Glide Sound(box) Octave.
- OCT-/OCT+: Answer on every page, five steps: Off, Short answer, Long answer, Sparse improv, Full improv
  (OCT+ up, OCT- down); both held: Off. The screen says the step. (Lead's octave: its knob 4, not the buttons.)
- System: CPU as text on the page; knob 4 free.
- Punch-in pages' knobs 3 and 4: Harmony Shimmer, Width; Stutter Length, Pitch; Filter Sweep, Type (low/high/band);
  Sparkle Pitch, Spread.
- Later: one-shot controls (Level, Reverb, Echo for the themes) on their own small page; Bluetooth's knob 1 for Range
  (needs a helper in fm1-base).
- One-shots page (Zach, 2026-11-10): hold a one-shot key (C#5 D#5 F#5) to open it (a tap still plays). Four knobs
  for all the one-shots together: Level, Space (their own reverb), Echo, Tone (dark .. bright). Kept with each story.
  The page shows the three one-shots' names, the one playing lit.

## The middle bank (Zach, 2026-11-10) — standing direction
- The banks: C4 D4 E4 his performance stories (Inside, Fly, Fast and Slow); F4 .. B4 the middle bank (Lighthouse,
  Market Day, Riebeck, Sleep); C5 D5 E5 the house music (Pre-show, Intermission, Post Show).
- He'll decide later what the middle bank becomes (more of his own stories over time). Meanwhile: now and then refine
  its four from what we learn: the performance bank's music, his MIDI, the improvising research (chord tones on the
  beats, motifs), his notes on the house music.
- Keep one of them uptempo, and keep one calm and sleepy, for actually sleeping to (Sleep).

## Queued for the next build (Zach, 2026-11-11)

- Story and Acts pages: move the one-shots and act tiles back to the bottom of
  the screen (undo the 0.8.13 "up top" swap). Keep them looking the same on
  both pages, keep dim yellow / dim pink, and the 0.8.13 black-key layout.
- Fade range: 1 s .. 2 min (was 5 s .. 2 min). Zach uses 1 s, 3 s and 5 s
  often, so make those easy to land on near the bottom of the knob. Keep the
  default at 5 s (bump saved projects' fade so their time doesn't change).
- Harmony's Interval: add thirds and an "All". Settings: Third, Fifth, Octave,
  Fifth + Octave (today's "Both"), All (third + fifth + octave). The third
  follows the chord: major over a major chord, minor over a minor one. Needs a
  third read head; map saved Fifth/Octave/Both to their new places.
- Middle bank genres (Zach, 2026-11-11): vary them. He'd like an acid house
  story (a 303-style squelchy resonant bass) and a hip hop one (boom bap). Sleep
  stays calm. Which stories turn into which is his call.
- Punch-in ideas from JAZZY (GPL-3.0, so ideas only, our own code; StoryBox is
  MPL-2.0): Tape Stop (with the knob as a hand on the record), Freeze (the last
  eighth as slow grains), Throw (the dry sound pushed into reverb/delay, rising
  over two bars), Glitch, Shuffle, Repeat 1/8 1/16 1/32. Waiting on Zach's pick.

Built in 0.8.14: the bottom layout again, Fade 1 s .. 2 min, Harmony's thirds and All, the punch-ins (Repeat, Throw /
Cloud with Tone, Glitch / Shuffle), Warehouse (acid house) and Rooftop (lo-fi hip hop). Ideas for later: vinyl crackle
for Rooftop; a dusty hip hop kit of its own.
