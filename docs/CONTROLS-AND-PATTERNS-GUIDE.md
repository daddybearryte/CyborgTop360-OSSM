# CyborgTop OSSM — How the Controls & Patterns Work Together

A plain-language guide to the M5 remote's controls and all 27 patterns
(10 stroke + 17 rotation), and — the part that isn't obvious from just
looking at the screen — how they interact with each other.

---

## The controls, in plain terms

**Home screen (stroke axis):**

| Control | Range | What it actually does |
|---|---|---|
| Speed | 0–100% (of your configured max) | How fast the stroking motion happens, overall |
| Depth | 0 to your machine's max travel | How far IN the deepest point of a stroke goes |
| Stroke | 0 to your machine's max travel | How LONG each stroke is — the distance traveled per cycle |
| Sensation | -100 to +100 | A pattern-specific "shape" dial — what it changes depends entirely on which pattern is selected (see below) |
| Pattern | 1 of 10 | Which stroke pattern is active |

**Rotation screen:**

| Control | Range | What it actually does |
|---|---|---|
| Speed | 0–100% | How fast the rotation motor spins, overall |
| Reverse | on/off checkbox | Manually sets spin direction — but several rotation patterns ignore it (see below) |
| Pattern | 1 of 17 | Which rotation pattern is active |

---

## What each stroke pattern feels like

*(These describe the base motion. Sensation reshapes each one further —
see the table above for what the dial does on each.)*

**Simple Stroke** — Smooth, even, predictable back-and-forth. No
surprises, no Sensation effect at all — a good baseline or warm-up
pattern.

**Teasing or Pounding** — Same predictable back-and-forth as Simple
Stroke, but with Sensation you can make one direction noticeably faster
than the other, for either a harder pounding push-in or a slower,
teasing pull-out.

**Robo Stroke** — Same shape as Simple Stroke, but Sensation controls how
abrupt or smooth the starts and stops feel — from soft, gradual ramps to
sharp, machine-like snaps.

**Half'n'Half** — Alternates full-depth and half-depth strokes, so every
other stroke is shorter — layers a shallow/deep rhythm on top of the same
in/out speed control as Teasing or Pounding.

**Deeper** — Starts shallow and builds deeper with each stroke until it
reaches full depth, then resets and starts the climb over — a repeating
build-up pattern.

**Stop'n'Go** — Strokes come in bursts that grow and shrink in size (one
stroke, building up to five, back down to one) with pauses between each
burst — an on-again, off-again rhythm.

**Insist** — Short, quick, insistent strokes that drift toward the front
or the back of your range over time rather than staying in one spot.

**Jack Hammer** — A fast vibrating motion on the way in, followed by one
smooth pull all the way back out.

**Stroke Nibbler** — A fast vibrating motion on both the way in and the
way out — vibration in both directions instead of just one.

**Depth Pulse** — Strokes in smoothly all the way to full depth, then
vibrates in place right there for a moment, then pulls all the way back
out smoothly in one go. The opposite of Jack Hammer: that one vibrates
on the way in and leaves; this one arrives cleanly and vibrates once
it's there. Sensation controls how LONG the vibration lasts before it
pulls out — turn it up for a longer hold, down for a quicker one.
(Vibration strength itself is a separate fixed setting, not tied to
Sensation, unlike Jack Hammer/Stroke Nibbler.) Pairs naturally with the
rotation axis's Depth Trigger Spin (below) — see the Suggested Combos
section.

## What each rotation pattern feels like

**Steady** — Constant spin speed. Direction only changes if you flip
Reverse yourself.

**Sweep** — Speed rises and falls smoothly in a repeating wave; direction
stays fixed.

**Escalation** — Speed climbs steadily up to full speed, holds briefly,
then drops back down and climbs again.

**Random Bursts** — Alternates unpredictably between fast bursts and a
slow idle — you can't predict exactly when the next burst hits.

**Screw Sync** — Spins one way while the stroke axis pulls in, the other
way while it pulls out — like screwing in and unscrewing.

**Stutter Sync** — Same screwing-in/unscrewing idea as Screw Sync, but
only flips every couple of stroke reversals instead of every single one
— a slower, more deliberate direction change.

**Bottom Hold Sync** — Same idea as Screw Sync, but pauses briefly at a
dead stop right before each direction change, then spins the other way.

**Depth Surge** — Spins faster the deeper the stroke axis goes, slower as
it pulls back.

**Threshold Burst** — Stays slow through most of the stroke, then
suddenly speeds up once the stroke passes a certain depth.

**Tempo Match** — Spins faster when the stroke axis is moving fast,
slower when it's moving slow.

**Snap Reverse** — Same direction-flipping as Screw Sync, but adds a
quick speed burst right at the moment it changes direction, for a
sharper "snap."

**Alternating Sweep** — Same rising-and-falling wave as Sweep, but also
flips direction every couple of waves.

**Random Direction Bursts** — Same unpredictable bursts as Random
Bursts, but some bursts also switch direction — you won't know which way
it'll spin next either.

**Metronome Flip** — Steady constant speed, flips direction back and
forth on a regular, fixed beat.

**Drunken Walk** — Speed and direction both wander around
unpredictably and independently of each other, with no set rhythm.

**Escalating Reversal** — Starts flipping direction slowly, then flips
faster and faster over time before resetting back to slow again.

**Depth Trigger Spin** — Spins at your normal knob-set speed most of
the time. The instant the stroke axis reaches full depth, it jumps to
the machine's true maximum speed — overriding your Speed knob for this
one moment — and rapidly flips direction back and forth several times
before settling. How long this burst lasts (and how many times it
flips within it) is tied to the same timing Depth Pulse uses for its
vibration hold. Once the burst ends, it drops straight back to your
normal knob-controlled speed until the next time the stroke reaches
full depth. Pairs naturally with the stroke axis's Depth Pulse (above)
— see the Suggested Combos section.

---

## How Depth and Stroke work together

These two always work together, on every stroke pattern, with no
exceptions — **Depth is the deepest point, Stroke is how far back from
that point the motion travels.** So your actual range of motion is:

```
(Depth − Stroke)  <---- travel ---->  Depth
     shallowest                      deepest
```

Turning Stroke down without changing Depth keeps the deepest point fixed
and shrinks the range from the shallow end. Turning Depth down keeps the
stroke length the same but moves the whole range shallower. This is true
for every one of the stroke patterns — patterns reshape the *timing and
speed* within that window, but none of them override where the window
itself sits.

---

## Speed — the one control that means (almost) the same thing everywhere

Speed sets the overall pace. Every stroke pattern uses your Speed setting
as its baseline tempo, then reshapes the timing on top of it — so cranking
Speed up always makes things faster, on every pattern, but *how* that
extra speed shows up (smoother? harder pounding? faster vibration?)
depends on which pattern and Sensation setting you've got dialed in.

Rotation Speed works the same way for the rotation axis, and is
completely separate from stroke Speed — you set your stroking pace and
your spinning pace independently.

---

## Sensation — the control that changes meaning per pattern

This is the one that trips people up: **Sensation doesn't have one fixed
meaning.** It's a shape dial whose effect is entirely defined by whichever
stroke pattern is active. Here's what it does on each:

| Pattern | Sensation controls... |
|---|---|
| Simple Stroke | *(nothing — this pattern ignores Sensation entirely)* |
| Teasing or Pounding | Speed balance between the in-stroke and out-stroke. Push it positive for a harder, faster push in; push it negative for a slower, more lingering pull out. |
| Robo Stroke | How abrupt the motion is. Positive = snappier, more machine-like starts/stops. Negative = softer, more gradual ramps. Centered = same as Simple Stroke. |
| Half'n'Half | Same in/out speed balance as Teasing or Pounding — but every second stroke only goes half as deep, so you get a shallow/deep/shallow/deep rhythm layered on top. |
| Deeper | How many strokes it takes to build from shallow to full depth before it resets and starts over. Push it one way for a quick build, the other way for a long, slow escalation. |
| Stop'n'Go | How long the pauses are between each burst of strokes. The burst itself grows and shrinks (1 stroke, up to 5, back down) — Sensation only changes the length of the rest in between. |
| Insist | Whether the short, rapid-fire strokes drift toward the front (shallow) or the back (deep) of your range. Positive = drifts shallow, negative = drifts deep. |
| Jack Hammer | Vibration intensity on the way in (it pulls out smoothly). |
| Stroke Nibbler | Vibration intensity on both the way in and the way out. |
| Depth Pulse | How long the vibration hold lasts (in seconds) before it pulls out — not vibration strength, which is a separate fixed setting here. **If you're also running Depth Trigger Spin on the rotation axis, this same slider is simultaneously setting how long that pattern's whole speed-burst-and-flipping window lasts too — see the Suggested Combos section.** |

---

## Rotation's Reverse checkbox — when it matters and when it doesn't

Rotation patterns fall into three groups, and the Reverse checkbox
behaves differently for each:

- **Always follows the checkbox:** Steady, Sweep, Escalation, Random
  Bursts, Depth Surge, Threshold Burst, Tempo Match. Flip the checkbox,
  direction flips immediately, no matter what else is happening.
- **Sets the *starting* direction only:** Alternating Sweep, Random
  Direction Bursts, Metronome Flip, Drunken Walk, Escalating Reversal,
  Depth Trigger Spin. These flip direction on their own schedule once
  running — the checkbox just decides which way they start out.
- **Ignores the checkbox completely:** Screw Sync, Stutter Sync, Bottom
  Hold Sync, Snap Reverse. These derive direction entirely from what the
  *stroke* axis is doing (more on this below) — the Reverse checkbox has
  no effect while one of these is selected.

---

## How the stroke and rotation axes affect each other

This is the part that isn't visible on the screen at all: several
rotation patterns quietly watch what the stroke axis is doing and react
to it. Which stroke controls matter depends on which *rotation* pattern
you've picked:

**Rotation patterns that watch stroke direction** — Screw Sync, Stutter
Sync, Bottom Hold Sync, Snap Reverse. These flip rotation direction in
sync with the stroke axis reversing (going from pulling-in to pulling-out
or vice versa). Because of this, your stroke Pattern choice indirectly
controls how *often* rotation flips direction too — a fast, frequent
stroke pattern makes these rotation patterns flip more often; a slow one
makes them flip less often. Your Depth/Stroke/Speed/Sensation settings on
the stroke side all feed into this indirectly, since they're what
determine how often the stroke axis itself reverses.

**Rotation patterns that watch stroke position** — Depth Surge, Threshold
Burst. These tie rotation speed to how deep the stroke axis currently is
within your Depth/Stroke window — so these two are directly shaped by
wherever you've set Depth and Stroke, not just by the rotation controls.
*(Note: these two haven't been calibrated to this specific machine's
travel range yet — until that's done, their behavior will look
inconsistent or barely noticeable rather than cleanly tracking depth.)*

**Rotation pattern that watches stroke speed** — Tempo Match. Ties
rotation speed to how fast the stroke axis is currently moving, so your
stroke Speed setting (and any pattern that varies stroke speed
internally, like Robo Stroke or the vibration patterns) directly drives
rotation speed too. *(Also not yet fully calibrated — will work, but the
fast/slow feel may be over- or under-sensitive until tuned.)*

**Rotation pattern that watches for the stroke reaching full depth** —
Depth Trigger Spin. This one doesn't continuously track position like
Depth Surge/Threshold Burst — it just waits for the single moment the
stroke axis arrives at full depth, then does its burst-and-flip. Also
reads your Sensation slider (see the combo note below).

**Rotation patterns that don't care about the stroke axis at all** —
Steady, Sweep, Escalation, Random Bursts, Alternating Sweep, Random
Direction Bursts, Metronome Flip, Drunken Walk, Escalating Reversal.
These run entirely on their own — whatever the stroke axis is doing has
zero effect on them. Pick one of these if you want the two axes to feel
completely independent of each other.

**One thing that flows the other direction too:** if you're on Jack
Hammer or Stroke Nibbler (the two vibration stroke patterns), the
direction-syncing rotation patterns (Screw Sync, Stutter Sync, Bottom
Hold Sync, Snap Reverse) will stop following the stroke axis and just
hold their last direction instead. This is automatic and isn't a
setting you control — it's there because those two stroke patterns
reverse direction very rapidly, and asking the rotation motor to keep up
with that isn't something it should be doing.

---

## Suggested combos

A few pairings worth starting from, rather than the full 10×17 grid:

**Locked-in and building.** Stroke: Teasing or Pounding or Robo Stroke.
Rotation: Screw Sync or Stutter Sync. The rotation axis mirrors your
stroke direction the whole time, so the two axes feel like one connected
motion. Stutter Sync if you want the direction changes less frequent and
more deliberate; Screw Sync if you want it tracking every single stroke.

**Independent layers.** Stroke: Deeper or Stop'n'Go. Rotation: Sweep or
Drunken Walk. Neither axis reacts to the other, so you get two
unrelated rhythms layered on top of each other rather than one
coordinated motion — good if you want the rotation to feel like its own
separate thing.

**Depth-aware speed.** Stroke: any pattern with a clear deep point
(Simple Stroke, Teasing or Pounding, Deeper). Rotation: Depth Surge or
Threshold Burst. Rotation speed rises and falls (or gates on and off)
with how deep the stroke is — needs the calibration step mentioned
above to feel accurate, but it's a genuinely different sensation from
the direction-syncing combos.

**Tempo-aware speed.** Stroke: Robo Stroke (its acceleration swings give
Tempo Match something to react to) or one of the vibration patterns.
Rotation: Tempo Match. Rotation speed tracks how fast the stroke axis is
currently moving rather than how deep it is.

**Building to a climax.** Stroke: Depth Pulse. Rotation: Depth Trigger
Spin. The purpose-built pairing — every time the stroke reaches full
depth, it vibrates there while rotation simultaneously jumps to true
max speed (ignoring your Speed knob for that moment) and rapidly flips
direction several times, then both settle back to normal together as
the toy pulls out. Turn Sensation up for a longer, more elaborate
moment each time (longer vibration hold, longer burst window, more
flips fit inside it); turn it down for something quicker. This is the
only combo where Sensation deliberately drives two things at once — see
the note in the Sensation table above if that catches you by surprise.

**Chaos.** Stroke: Insist or Stop'n'Go. Rotation: Drunken Walk or Random
Direction Bursts. Nothing about either axis is predictable — good for
not knowing what's coming next.

---

## Quick reference: what to change to get what you want

- **Want a bigger or smaller range of motion?** Depth and Stroke.
- **Want everything faster or slower, without changing the shape?**
  Speed (stroke and rotation are independent).
- **Want the same pattern to feel different?** Sensation — but check the
  table above first, since it does something different on each pattern.
- **Want the two axes to move in sync with each other?** Pick a Screw
  Sync-family rotation pattern (4-6, 10) and note it'll pause syncing
  during Jack Hammer/Stroke Nibbler.
- **Want rotation speed to follow how deep or how fast the stroke is
  going, without direction syncing?** Depth Surge, Threshold Burst, or
  Tempo Match — though these three still need calibration on this build.
- **Want the two axes totally independent?** Any rotation pattern not
  listed as "watches stroke direction/position/speed" above, paired with
  any stroke pattern.
- **Want a distinct moment at full depth — vibration plus a rotation
  burst and flip?** Depth Pulse paired with Depth Trigger Spin — see the
  Suggested Combos section.
