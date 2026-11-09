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

- [ ] ACTS keyboard too full, its banks hard to tell apart, the screen's lists don't help: a clearer layout (stings to
      the black keys?), lighting by bank, the screen echoing the keyboard (proposal sent)
- [ ] Stings still too loud
- [ ] Scatter doesn't sound good: another draft, more a shimmer or a sparkle
- [ ] Reverse: not loving it yet (Zach to play more)
- [ ] The page name's dots jump with the name's length: the dots in one place
- [ ] Post Show's Stage Door good; more unusual chords across the acts (a minor, a maj7 now and then); its acts'
      drum patterns more distinct
- [ ] Feel: brighter, more colour; Tension dark at 0; Next chord shows nothing (with a progression: en_drift is
      only the free drift's); Energy set to half reads a quarter (the "now" marker? check)
- [ ] Post Show: too minor, a little scary at times: jazzy instead, maj7s (replaces the "unusual chords" note)
- [ ] ACTS keyboard (approved, Zach): acts F3 G3 A3 B3; the mix F#3 Pad, G#3 Melody, A#3 Bass, C#4 Drums, D#4
      Break; stories from middle C: C4 D4 E4 F4 G4 A4 B4 C5 D5 E5 (reorder soon); stings C#5 D#5 F#5 (up to
      three); F5 G5, F#4 G#4 A#4 spare. Lights by bank
- [ ] Story page: four rows of a short keyboard (F3-B3, C4-F4, F#4-C5, C#5-G5), the black keys' names above each
      strip, the white keys' under, every key its letter (black keys wider, the letter in them); where it is filled
      (mockup acts-rows-2)
- [ ] Story page rows: F3-B3, C4-F4, F#4-B4, C5-E5 (Intermission starts the last row, the stings over it); the spares
      (F5 F#5 G5) not drawn until used (mockup acts-rows-3)
- [ ] Banjo: a melody sound only (no lead banjo; Riebeck's lead another); quieter, less bright/sharp, less like the
      guitar (a banjo's plunk: its head's quick ring)
- [ ] Lead Glide does nothing for most sounds (only Flute and Slide, legato): make it work for every lead sound (each
      note gliding from the last)
- [ ] Lead: every key plays its own note (no snapping to the scale); only the key's notes lit (the rest dark). Check
      the lit keys match the scale (Zach: some sounded wrong: the snapping moved out-of-key keys to a neighbour)
- [ ] Colours page next to Mixer (Mixer, Colours, Feel, Story, ...)
- [ ] Pages, left to right: Story (acts), Feel, Mixer, Colours, Effects (?), Pad, Melody, Bass, Drums, Lead, Setup,
      System (replaces "Colours next to Mixer"). Buttons: HOME the acts (ACTS mode and the Story page), SEL Effects,
      FX Colours, EDIT Lead, GLO Setup
- [x] SEL: Effects (right after Colours in the pages)
- [ ] Freeze sounds muffled: it shuts the reverb's input (new notes go dry) and holds its tail forever (the plate's
      damping darkening it). Keep some reverb for what plays (~35%), hold the tail long but not forever, less damping
      while frozen
