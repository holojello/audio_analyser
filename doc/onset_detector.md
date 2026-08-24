# Onset detector

## General overview

The onset detector implemented in `onset.c` detects transient musical events
from a single mono PCM channel.

The detector operates in several stages:

1. **PCM framing**
2. **Spectral-flux novelty extraction**
3. **Background and variability estimation**
4. **Local-maximum detection**
5. **Adaptive thresholding**
6. **Onset-strength calculation**
7. **Minimum-distance filtering**
8. **Raw annotation production**

The detector deliberately uses aubio only as a source of an onset-related
descriptor. Aubio's own onset decisions and timestamps are not used. This keeps
the semantics of the `audio_analyser` detector independent from the particular
aubio primitive used internally.

For the current implementation, the aubio descriptor is `specflux`.

### 1. PCM framing

`onset_analyse_channel()` receives a mono PCM buffer together with its frame
count, sample rate, analysis buffer size, hop size, abyss policy, and channel
number.

The PCM samples are consumed by the common hop reader. Each iteration supplies
one hop of samples to aubio.

The hop reader is also responsible for handling the end of the input according
to the selected abyss policy. The current proof of concept uses
`ANALYSER_ABYSS_FADEOUT`.

The detector does not emit an annotation for the final descriptor unless that
descriptor has a right-hand neighbour. This prevents the end of the audio
stream, including any samples introduced by the abyss policy, from being
interpreted as an artificial onset.

### 2. Spectral-flux novelty extraction

Each PCM hop is passed to aubio's `aubio_onset_do()`.

The output of `aubio_onset_do()` is deliberately ignored. Instead, the detector
retrieves the current value of aubio's onset descriptor with
`aubio_onset_get_descriptor()`.

The descriptor method used by the current implementation is `specflux`.

The resulting value is called the **novelty**.

The novelty signal represents changes in the spectral content of the input. A
transient generally produces a rise in spectral flux and therefore a peak in the
novelty signal.

Aubio exposes the onset detection function separately from its final onset
decision, which makes this separation possible.

### 3. Background and variability estimation

The detector maintains a rolling history of the most recent novelty values.

The history contains up to 31 observations. This specific number is an arbitrary
choice; the history size just needs to be an odd number so its median can be
computed unambiguously. The actual duration represented by the history is
`history size * hop size / sample rate`, so at 44.1kHz sample rate and with a
hop size of 512 data points, the 31 observations history contains ~360ms of
audio.

The background level is estimated using the median:

```text
background = median(recent novelty values)
```

The median is used rather than the arithmetic mean because isolated transient
peaks should have limited influence on the estimated background.

The detector also calculates the median absolute deviation (MAD):

```text
MAD = median(
    abs(novelty - background)
)
```

The MAD is converted to a robust standard-deviation estimate using:

```text
robust_deviation = 1.4826 * MAD
```

The conversion factor is appropriate when the observations are approximately
normally distributed. It is used here as a robust measure of the scale of
ordinary novelty fluctuations rather than as a claim that the novelty signal
itself is Gaussian.

The detector's normalization scale is:

```text
scale = robust_deviation * ONSET_STRENGTH_SIGMA
```

where `ONSET_STRENGTH_SIGMA` is 6 in the current implementation.

Thus `scale` corresponds to six robust deviation units above the background.

Importantly, the current novelty observation is **not** added to the background
history until its background and scale have been calculated. Consequently, an
observation is evaluated against the background that existed immediately before
it.

During the initial warm-up period, the detector accumulates background
observations and does not emit onset candidates.

### 4. Local-maximum detection

An onset candidate must first be a local maximum in the novelty signal.

Three consecutive observations are considered:

```text
previous    current    next
    \          |         /
     \         |        /
      \        |       /
```

`current` is a local maximum when:

```text
current.novelty > previous.novelty
```

and:

```text
current.novelty >= next.novelty
```

The first condition requires a strict rise into the candidate.

The second condition permits a plateau after the maximum. When two adjacent
observations have the same maximum value, the first one is selected.

This operation is implemented independently from thresholding by
`is_local_maximum()`.

A local maximum alone is **not** an onset. Random fluctuations in the novelty
signal can also create local maxima.

### 5. Adaptive thresholding

After a local maximum has been identified, its significance relative to the
recent background is evaluated.

The current detector uses:

```text
ONSET_THRESHOLD_SIGMA = 6
```

and requires:

```text
novelty >
    background +
    ONSET_THRESHOLD_SIGMA * robust_deviation
```

Since `scale` is defined as:

```text
scale =
    ONSET_STRENGTH_SIGMA * robust_deviation
```

the implementation expresses this threshold as:

```text
threshold =
    background +
    (ONSET_THRESHOLD_SIGMA / ONSET_STRENGTH_SIGMA) * scale
```

With both constants currently equal to 6, this simplifies to:

```text
threshold = background + scale
```

Therefore an observation must be approximately six robust deviation units above the recent background to become an onset candidate.

The local-maximum test and adaptive-threshold test are deliberately separate:

```text
local maximum
      +
sufficiently high relative to background
      =
onset candidate
```

This separation allows the peak-picking and significance criteria to evolve
independently.

It is particularly important for stationary signals such as white noise. White
noise naturally produces random fluctuations in spectral flux, and therefore
necessarily produces local maxima. The detector must additionally determine that
those maxima are sufficiently unusual before reporting them as musical events.

The conservative threshold used for now is intended to prevent such stationary
fluctuations from becoming annotations.

### 6. Onset strength

Once a local maximum passes the adaptive threshold, its strength is calculated.

For a normal, non-zero background scale:

```text
strength =
    clamp(
        (novelty - background) / scale,
        0,
        1
    )
```

Therefore:

* a novelty at the background level has strength `0`;
* a novelty halfway between the background and the normalization scale has
  strength `0.5`;
* a novelty at or above the normalization scale has strength `1`.

The strength is a measure of how strongly the novelty stands out from the recent
background. It is not a direct measurement of the physical amplitude of the
audio signal.

If the background has zero measurable variation, `scale` is zero and cannot be
used as a divisor. In this case, a novelty strictly greater than the background
is treated as having strength `1`.

This special case allows an event such as a click following silence to remain detectable.

### 7. Temporal position

The detector associates each aubio descriptor with the centre of its
corresponding analysis window.

For a hop beginning at `hop_start` and an analysis buffer size of `buffer_size`,
the associated frame is:

```text
frame = hop_start + buffer_size / 2
```

The detector therefore reports an analysis position rather than necessarily the
exact PCM sample at which a transient begins.

If the calculated position reaches the end of the source, it is clamped to the final source frame.

Aubio's own onset timestamp and delay are not used.

### 8. Minimum distance between detections

After a candidate has passed the local-maximum and adaptive-threshold tests, the
detector applies a minimum temporal distance between consecutive annotations.

The current value is 50 ms.

The value is converted to frames using the input sample rate.

If a candidate occurs less than 50 ms after the preceding accepted onset, it is discarded.

This is currently an internal parameter. It is expected to become configurable
if the detector API is expanded in the future.

### 9. Raw annotation production

An accepted candidate is appended to the `raw_annotations_t` collection.

Each raw annotation contains:

* the frame at which the event was detected;
* the calculated onset strength;
* the channel on which the event was detected.

At this stage the annotation is still associated with a single input channel.

Channel-independent event interpretation is deliberately outside the onset
detector. In particular, if the same physical event produces detections on
several channels, `onset.c` emits one raw annotation per channel. The later
aggregation stage is responsible for deciding whether those annotations
represent one event or several distinct events.

## Detection method summary

The complete current method can therefore be summarized as:

```text
mono PCM
   │
   ▼
hop reader
   │
   ▼
aubio specflux
   │
   ▼
novelty sequence
   │
   ├──────────────► rolling median ──► background
   │                    │
   │                    └────────────► MAD ──► robust deviation
   │
   ▼
three-point local maximum
   │
   ▼
adaptive threshold
   │
   │   novelty > background + 6 × robust deviation
   │
   ▼
onset candidate
   │
   ▼
strength calculation
   │
   ▼
50 ms minimum-distance filter
   │
   ▼
raw annotation
```

The essential distinction is between **novelty peaks** and **musical events**. A
peak in the spectral-flux signal is only an intermediate observation. The
detector reports an event only when that peak is sufficiently prominent relative
to the recent background.

This design intentionally leaves room for future detectors and future
aggregation policies without exposing aubio-specific concepts through the public
`audio_analyser` API.
