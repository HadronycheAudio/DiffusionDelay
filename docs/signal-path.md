# DiffusionDelay signal path

**Requirement** marks what the owner specified. **Proposed** marks a choice made during implementation where the brief says nothing, or that needed interpreting. Those are open to change.

```
             ┌──────────────────────────────────────────────────────┐
In (stereo) ─┤                                                      ├→ Dry/wet → Volume → Out
             └→ + → Diffusion network, T long ─┬→ Output gain ──────┘
                ↑                              │
                └─ Limiter ← Feedback ← DC blocker ← Damper
```

## The echo

- **Requirement:** a delay with a time and a feedback, which is an ordinary clean echo when Diffusion is at 0.
- **Requirement:** at 100% feedback, the repeats never end, like a looper.
- **Requirement:** Time goes up to 10 s.
- **Proposed:** Time starts at 10 ms, on a log curve. Feedback is 0 to 100%.
- **Requirement:** tempo sync.
- **Proposed:** Sync is on or off, and Division picks a note value, from 1/32 to 4/1, with straight, dotted and triplet values up to 1/1. They're note values rather than bars, so they mean the same in any time signature. Sync starts off, at a dotted eighth, which at 120 BPM is the same as the default Time.
- **Proposed:** a synced time is held to the range of Time, so 4/1 stops at 10 s below 96 BPM. A host that gives no tempo gets the last one it gave, or 120 BPM until it does. A change of tempo or of Sync glides like any other change of Time.
- **Proposed:** a change of Time glides there with a 100 ms time constant, like moving a tape head, which bends the pitch of what's in the lines. On arrival, the delay is a whole number of samples again, so that the repeats pass through unchanged.

## Diffusion

- **Requirement:** Diffusion adds early reflections before each repeat and a reverb tail after it, scattered across the stereo image, inside the feedback loop. They're built from parameters related to the delay time.
- **Requirement:** at 100% feedback, each pass dissolves the repeats further, until there's only a mass of diffused sound.

### Why it's a network and not an impulse response

- **Proposed:** the reflections come from a lossless network rather than from convolving with a generated impulse response. A filter in a loop at a gain of 1 has to have a gain of exactly 1 at every frequency. If it's above 1 anywhere, the loop runs away there, and if it's below 1, then only the frequencies at its peaks survive, so the mass would ring at those pitches. Random impulse responses have peaks and dips, and a lossless network has neither.

### How it works

Eight channels run in parallel, four fed from the left and four from the right, at half level each.

```
In → Stage 1 → Tail → Rotation → Stage 2 → Rotation → Stage 3 → Rotation → Stage 4 → Out
```

- **Stages** are delay lines. The four on each channel add up to exactly T, so that every straight path takes exactly T. Each is between about 13% and 37% of T, and different on each channel, from a table chosen by a search to make the arrival times land apart.
- **Rotations** move signal between the channels: `cos (a) I + sin (a) / sqrt (7) S`, where `S` is a skew-symmetric conference matrix of order 8. It's exactly a rotation at every angle, and at an angle of 0 it does nothing.
- **Reflections:** a signal that changes channel part way through swaps the rest of one channel's stages for the rest of another's, and so arrives at a different time. They start at 58% of T.
- **Tails** are one all-pass per channel, 3 to 10% of T long, turned by a similar rotation. A lossless filter's first-order reflections are forced to be symmetrical either side of its main path, which is what the rotations give. Only recirculation can reach further after the repeat than before it, so that's what the tails are for. Being before the rotations, they get spread across the channels too.

### What Diffusion sets

- **Proposed:** the rotations turn to 35° and the tails to 45.6° at 100%, in proportion to the knob. That leaves the straight path 8 dB down per pass in the loop, so there's a clear repeat that dissolves over the next few. Going further would lose the repeat altogether.
- **Proposed:** every time in the network is a fixed share of T, so the reflections scale with Time, and moving Diffusion only moves rotations, with no change of pitch.
- **Proposed:** the output has a gain of up to 3 dB at 100%, in proportion to the knob and outside the loop. Each side of the output adds up four channels. A clean repeat is the same on all four, so it adds up to full level, while sound that has scattered is on one at a time, and comes out 6 dB lower although its energy is all still in the loop. This makes up for some of that, leaving the first repeat at a sensible level.

Measured on an impulse at T = 400 ms, relative to the input:

| Diffusion | Repeat | Before it | After it | All of it |
| --- | --- | --- | --- | --- |
| 25% | +0.3 dB | -20.4 dB | -16.9 dB | +0.40 dB |
| 50% | -0.4 dB | -14.4 dB | -10.6 dB | +0.16 dB |
| 100% | -5.1 dB | -8.8 dB | -5.3 dB | -1.37 dB |

At 100%, it reaches 42% of T before the repeat and 138% after it, to 60 dB below its peak.

### At 100% feedback

Measured at T = 250 ms and 100 ms, 100% Diffusion and no damping:

- From 10 s to 39 s, the level changes by 0.00 dB.
- The crest factor goes from 19.4 dB on the first repeat to 15.3 dB on the third, and 12.4 dB by the forty-fifth, which is about the same as noise.
- The mass is 1.2 dB below the first repeat, in energy per repeat.
- The correlation of one repeat with the next stays at about 0.4, which is the straight path's gain per pass. So the mass keeps a faint pulse at T rather than none at all.

## Damper

- **Requirement:** a damper in the feedback loop, which cuts the highs above about 4 kHz, like a shelf EQ.
- **Proposed:** a one-pole shelf with its corner at 4 kHz. The knob is its depth, per pass, from 0 to 18 dB, and it's on each channel. A first-order shelf can't go above a gain of 1, and has exactly 1 at DC. So the lows still go on for ever at 100% feedback.
- **Proposed:** it's only in the path that goes round again, so the first repeat has all its top end.

Measured at 12 dB, per pass: 0.0 dB at 200 Hz, -0.9 dB at 2 kHz, -2.7 dB at 4 kHz, -6.4 dB at 8 kHz, and -10.7 dB at 16 kHz.

## The rest of the feedback path

- **Proposed:** a DC blocker at 1 Hz, with a gain of exactly 1 at the top of the range, so that an offset in the input can't fill up the loop. A repeat goes through it on every pass, which is why it's so low. A 40 Hz note at T = 100 ms fades by 1.6 dB per minute at 100% feedback, and higher notes by far less.
- **Proposed:** a soft limiter on each channel, which holds back what builds up when you play into 100% feedback. It leaves anything below 0.7 per channel exactly as it is. A full-scale input is 0.5, so a single hit isn't touched by it. Its ceiling is 1.0 per channel.

## Output

- **Proposed:** Dry/wet at 50% plays both at full level, as in the other Hadronyche plug-ins, and that's the default. The dry path has no latency, and stays in stereo.
- **Proposed:** Volume goes from -60 dB, which is silent, to +6 dB.

## Timing

- **Proposed:** there's no latency. The early reflections come from taking the repeat late, not from looking ahead.

## Sleep

- **Proposed:** once the input and everything in the loop has been below -140 dB for longer than the longest path through it, the plug-in stops working and sets the silence flags. At 100% feedback, that only happens with nothing to repeat.
- **Proposed:** it reports its tail as infinite, and leaves it to the silence flags.
