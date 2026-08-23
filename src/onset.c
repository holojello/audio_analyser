#include "onset.h"

#include "hop.h"

#include <aubio.h>

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


/*
 * The public detector name belongs to audio_analyser.
 *
 * Aubio remains an implementation detail: changing the aubio primitive
 * used here must not require changing this name in the public API.
 */
#define ONSET_METHOD_DEFAULT "default"

/*
 * Aubio is used only as a descriptor/novelty generator.
 *
 * In particular, aubio's own onset decisions and timestamps are
 * deliberately ignored.
 */
#define ONSET_AUBIO_METHOD "specflux"


/*
 * Number of recent descriptor values used by the background estimator.
 *
 * An odd number makes the median unambiguous.
 */
#define ONSET_BACKGROUND_HISTORY 31U


/*
 * Number of descriptor observations required before onset detection
 * starts.
 */
#define ONSET_WARMUP_SAMPLES 8U


/*
 * Candidate onset threshold:
 *
 *     novelty > background + threshold * scale
 */
#define ONSET_THRESHOLD_SIGMA 3.0f


/*
 * Strength reaches 1.0 at this many robust scale units above background.
 *
 * The actual strength is:
 *
 *     clamp(
 *         (novelty - background) / scale,
 *         0,
 *         1
 *     )
 *
 * where scale is chosen such that this value corresponds to
 * ONSET_STRENGTH_SIGMA robust deviations.
 */
#define ONSET_STRENGTH_SIGMA 6.0f


/*
 * First-version minimum distance between two detections.
 *
 * This is deliberately private for now. It is expected to become a
 * user-facing/domain-specific parameter later.
 */
#define ONSET_MIN_DISTANCE_MS 50.0


/*
 * MAD -> standard-deviation conversion factor for a Gaussian
 * distribution.
 */
#define ONSET_MAD_SCALE 1.4826f


/*
 * -------------------------------------------------------------------------
 * Background estimator
 * -------------------------------------------------------------------------
 *
 * The estimator is deliberately private to onset.c.
 *
 * The background is the median of recent descriptor values.
 * The scale is a robust standard-deviation estimate derived from MAD.
 */

typedef struct {
    float values[ONSET_BACKGROUND_HISTORY];

    size_t count;
    size_t next;
} background_estimator_t;


static int
float_compare(
    const void *a,
    const void *b
)
{
    const float fa = *(const float *)a;
    const float fb = *(const float *)b;

    if (fa < fb) {
        return -1;
    }

    if (fa > fb) {
        return 1;
    }

    return 0;
}


static float
median_of_values(
    float *values,
    size_t count
)
{
    if (count == 0) {
        return 0.0f;
    }

    qsort(
        values,
        count,
        sizeof(*values),
        float_compare
    );

    if ((count & 1U) != 0U) {
        return values[count / 2U];
    }

    return 0.5f * (
        values[count / 2U - 1U] +
        values[count / 2U]
    );
}


static void
background_init(
    background_estimator_t *estimator
)
{
    memset(
        estimator,
        0,
        sizeof(*estimator)
    );
}


static int
background_add(
    background_estimator_t *estimator,
    float value
)
{
    if (estimator == NULL ||
        !isfinite(value)) {
        return -1;
    }

    estimator->values[estimator->next] = value;

    estimator->next =
        (estimator->next + 1U) %
        ONSET_BACKGROUND_HISTORY;

    if (estimator->count <
        ONSET_BACKGROUND_HISTORY) {
        ++estimator->count;
    }

    return 0;
}

/**
 * Estimate the current background level and strength normalization scale.
 *
 * The background estimator maintains a history of recent novelty values.
 * The background level is estimated as the median of that history, making
 * it relatively insensitive to isolated transient events.
 *
 * The variability of the background is estimated using the median absolute
 * deviation (MAD). For a normally distributed signal, MAD is converted to
 * a standard-deviation-like quantity using the factor 1.4826.
 *
 * The returned scale is then multiplied by ONSET_STRENGTH_SIGMA. It is
 * therefore the novelty difference corresponding to a strength of 1.0.
 *
 * With the current constants, the values have the following meaning:
 *
 *     background = median(recent novelty)
 *
 *     robust_deviation =
 *         1.4826 * median(
 *             abs(novelty - background)
 *         )
 *
 *     scale =
 *         robust_deviation * ONSET_STRENGTH_SIGMA
 *
 * The onset strength is subsequently calculated as:
 *
 *     strength =
 *         clamp(
 *             (novelty - background) / scale,
 *             0,
 *             1
 *         )
 *
 * Consequently, an observation at the background level has strength 0,
 * while an observation ONSET_STRENGTH_SIGMA robust deviations above the
 * background has strength 1.0.
 *
 * The current observation must not be added to the estimator before this
 * function is called when the caller intends to measure that observation
 * against the background that existed immediately before it. In particular,
 * this preserves the interpretation of onset strength as the magnitude of
 * the current novelty against the previously observed background.
 *
 * \param[in]  estimator
 *     Background estimator containing the previously observed novelty
 *     values. It must contain at least one value.
 *
 * \param[out] background
 *     Receives the estimated background novelty level.
 *
 * \param[out] scale
 *     Receives the novelty difference corresponding to strength 1.0.
 *     A value of zero means that the observed background has no measurable
 *     variability and therefore cannot provide a meaningful normalization
 *     scale.
 *
 * \return
 *     0 on success, or -1 if an argument is NULL or the estimator is empty.
 */
static int
background_estimate(
    const background_estimator_t *estimator,
    float *background,
    float *scale
)
{
    float sorted[ONSET_BACKGROUND_HISTORY];
    float deviations[ONSET_BACKGROUND_HISTORY];

    if (estimator == NULL ||
        background == NULL ||
        scale == NULL ||
        estimator->count == 0) {
        return -1;
    }

    memcpy(
        sorted,
        estimator->values,
        estimator->count * sizeof(*sorted)
    );

    *background = median_of_values(
        sorted,
        estimator->count
    );

    for (size_t i = 0; i < estimator->count; ++i) {
        deviations[i] = fabsf(
            estimator->values[i] - *background
        );
    }
    /* `scale` is the novelty difference corresponding to a strength of 1
     *
     * scale = 6 * robust standard deviation */
    *scale =
        ONSET_MAD_SCALE *
        median_of_values(
            deviations,
            estimator->count
        ) *
        ONSET_STRENGTH_SIGMA;

    return 0;
}


/*
 * -------------------------------------------------------------------------
 * Descriptor observations
 * -------------------------------------------------------------------------
 */

typedef struct {
    float novelty;
    float background;
    float scale;

    analyser_frame_t frame;
} descriptor_observation_t;


static float
observation_strength(
    const descriptor_observation_t *observation
)
{
    if (observation == NULL) {
        return 0.0f;
    }

    /* Here we handle the special case where the background is perfect silence.
     * In such situation, the MAD is zero; there is then no meaningful
     * statistical scale with which to normalize the novelty. Nevertheless, a
     * novelty strictly above that background is still an event candidate: this
     * is important for cases such as silence followed by a click.
     *
     * Treat such a candidate as having maximum strength.
     */
    if (observation->scale <= 0.0f) {
        return observation->novelty >
               observation->background
            ? 1.0f
            : 0.0f;
    }

    return fminf(
        1.0f,
        fmaxf(
            0.0f,
            (observation->novelty -
             observation->background) /
            observation->scale
        )
    );
}

static int
is_peak(
    const descriptor_observation_t *previous,
    const descriptor_observation_t *current,
    const descriptor_observation_t *next
)
{
    float threshold;

    if (previous == NULL ||
        current == NULL ||
        next == NULL) {
        return 0;
    }

    /*
     * Strictly rising into the candidate.
     */
    if (current->novelty <= previous->novelty) {
        return 0;
    }

    /*
     * Non-rising after the candidate.
     *
     * A flat maximum is therefore resolved to its first descriptor.
     */
    if (current->novelty < next->novelty) {
        return 0;
    }

    threshold =
        current->background +
        (ONSET_THRESHOLD_SIGMA / ONSET_STRENGTH_SIGMA) *
        current->scale;

    return current->novelty > threshold;
}


/*
 * -------------------------------------------------------------------------
 * Frame handling
 * -------------------------------------------------------------------------
 *
 * The detector associates an aubio descriptor with the centre of the
 * corresponding analysis window.
 *
 * No aubio onset timestamp is used here: aubio's peak picker is deliberately
 * outside audio_analyser's onset semantics.
 */

static int
descriptor_frame(
    analyser_frame_t hop_start,
    uint32_t buffer_size,
    analyser_frame_t frame_count,
    analyser_frame_t *frame
)
{
    analyser_frame_t offset;

    if (frame == NULL) {
        return -1;
    }

    offset = (analyser_frame_t)(buffer_size / 2U);

    if (hop_start >
        ANALYSER_FRAME_MAX - offset) {
        return -1;
    }

    *frame = hop_start + offset;

    /*
     * The final partial hop may produce a completed analysis window whose
     * centre lies beyond the actual source. Never expose such a position
     * as an annotation.
     */
    if (frame_count != 0 &&
        *frame >= frame_count) {
        *frame = frame_count - 1U;
    }

    return 0;
}


static int
calculate_min_distance(
    uint32_t samplerate,
    analyser_frame_t *distance
)
{
    double value;

    if (samplerate == 0 ||
        distance == NULL) {
        return -1;
    }

    value =
        ((double)samplerate *
         ONSET_MIN_DISTANCE_MS) /
        1000.0;

    if (value < 1.0) {
        value = 1.0;
    }

    if (value >
        (double)ANALYSER_FRAME_MAX) {
        return -1;
    }

    /*
     * The range has been checked against the destination type.
     */
    *distance = (analyser_frame_t)value;

    return 0;
}


/*
 * -------------------------------------------------------------------------
 * Raw annotation storage
 * -------------------------------------------------------------------------
 *
 * The array grows geometrically rather than reallocating for every event.
 */

static int
raw_annotations_append(
    raw_annotations_t *annotations,
    analyser_frame_t frame,
    float strength,
    uint32_t channel
)
{
    raw_annotation_t *items;
    size_t new_capacity;

    if (annotations == NULL) {
        return -1;
    }

    if (annotations->count ==
        annotations->capacity) {

        if (annotations->capacity == 0) {
            new_capacity = 16U;
        } else {
            if (annotations->capacity >
                SIZE_MAX / 2U) {
                return -1;
            }

            new_capacity =
                annotations->capacity * 2U;
        }

        if (new_capacity >
            SIZE_MAX / sizeof(*annotations->items)) {
            return -1;
        }

        items = realloc(
            annotations->items,
            new_capacity *
            sizeof(*annotations->items)
        );

        if (items == NULL) {
            return -1;
        }

        annotations->items = items;
        annotations->capacity = new_capacity;
    }

    annotations->items[annotations->count].frame =
        frame;

    annotations->items[annotations->count].strength =
        strength;

    annotations->items[annotations->count].channel =
        channel;

    ++annotations->count;

    return 0;
}


void
raw_annotations_destroy(
    raw_annotations_t *annotations
)
{
    if (annotations == NULL) {
        return;
    }

    free(annotations->items);

    annotations->items = NULL;
    annotations->count = 0;
    annotations->capacity = 0;
}


/*
 * -------------------------------------------------------------------------
 * Default onset detector
 * -------------------------------------------------------------------------
 */

int
onset_analyse_channel(
    const float *samples,
    analyser_frame_t frame_count,
    uint32_t samplerate,
    uint32_t buffer_size,
    uint32_t hop_size,
    const char *method,
    analyser_abyss_policy_t abyss_policy,
    uint32_t channel,
    raw_annotations_t *annotations
)
{
    aubio_onset_t *aubio = NULL;
    fvec_t *input = NULL;
    fvec_t *aubio_output = NULL;

    analyser_hop_reader_t reader;

    float *hop_buffer = NULL;

    background_estimator_t background;

    descriptor_observation_t previous;
    descriptor_observation_t current;
    descriptor_observation_t next;

    int have_previous = 0;
    int have_current = 0;

    analyser_frame_t min_distance;
    analyser_frame_t last_onset_frame = 0;
    int have_last_onset = 0;

    analyser_frame_t hop_start = 0;

    int result = -1;

    if (annotations == NULL ||
        samplerate == 0 ||
        buffer_size == 0 ||
        hop_size == 0 ||
        method == NULL ||
        (samples == NULL && frame_count != 0)) {
        return -1;
    }

    /*
     * onset.c receives already-resolved configuration from analyser.c.
     * It does not resolve defaults.
     */
    if (strcmp(
            method,
            ONSET_METHOD_DEFAULT
        ) != 0) {
        return -1;
    }

    /*
     * The public API currently limits channels to 64.
     *
     * channel is nevertheless represented as an integer here because
     * aggregation, not the detector, owns the channel-mask operation.
     */
    if (channel >= 64U) {
        return -1;
    }

    if (calculate_min_distance(
            samplerate,
            &min_distance
        ) != 0) {
        return -1;
    }

    if ((size_t)hop_size >
        SIZE_MAX / sizeof(*hop_buffer)) {
        return -1;
    }

    hop_buffer = malloc(
        (size_t)hop_size *
        sizeof(*hop_buffer)
    );

    if (hop_buffer == NULL) {
        goto cleanup;
    }

    if (analyser_hop_reader_init(
            &reader,
            samples,
            frame_count,
            hop_size,
            abyss_policy
        ) != 0) {
        goto cleanup;
    }

    /*
     * Aubio is an implementation detail.
     *
     * The public audio_analyser method is "default"; it must not be
     * confused with an aubio method name.
     */
    aubio = new_aubio_onset(
        ONSET_AUBIO_METHOD,
        buffer_size,
        hop_size,
        samplerate
    );

    if (aubio == NULL) {
        goto cleanup;
    }

    input = new_fvec(hop_size);

    if (input == NULL) {
        goto cleanup;
    }

    /*
     * aubio_onset_do() expects an output vector even though this detector
     * deliberately ignores aubio's onset decision.
     */
    aubio_output = new_fvec(1);

    if (aubio_output == NULL) {
        goto cleanup;
    }

    background_init(&background);

    memset(
        &previous,
        0,
        sizeof(previous)
    );

    memset(
        &current,
        0,
        sizeof(current)
    );

    memset(
        &next,
        0,
        sizeof(next)
    );

    for (;;) {
        int hop_result;

        hop_result =
            analyser_hop_next(
                &reader,
                hop_buffer
            );

        if (hop_result < 0) {
            goto cleanup;
        }

        if (hop_result == 0) {
            break;
        }

        memcpy(
            input->data,
            hop_buffer,
            (size_t)hop_size *
            sizeof(*hop_buffer)
        );

        /*
         * Advance aubio's internal state.
         *
         * The output is deliberately ignored. We do not use aubio's
         * peak-picking/onset decision.
         */
        aubio_onset_do(
            aubio,
            input,
            aubio_output
        );

        memset(
            &next,
            0,
            sizeof(next)
        );

        /*
         * The descriptor is our detector's novelty signal.
         */
        next.novelty =
            aubio_onset_get_descriptor(aubio);

        if (!isfinite(next.novelty)) {
            next.novelty = 0.0f;
        }

        if (descriptor_frame(
                hop_start,
                buffer_size,
                frame_count,
                &next.frame
            ) != 0) {
            goto cleanup;
        }

        /*
         * ---------------------------------------------------------------
         * Background / candidate sequence
         * ---------------------------------------------------------------
         *
         * The current descriptor must NOT participate in the background
         * against which it is evaluated.
         *
         * Therefore:
         *
         *   1. estimate background from previous observations;
         *   2. evaluate current candidate against that background;
         *   3. add current descriptor to history.
         *
         * This gives "magnitude against observed background" the intended
         * causal interpretation.
         */

        if (background.count > 0) {
            if (background_estimate(
                    &background,
                    &next.background,
                    &next.scale
                ) != 0) {
                goto cleanup;
            }
        } else {
            next.background = 0.0f;
            next.scale = 0.0f;
        }

        /*
         * Add the current descriptor only after its background has been
         * determined.
         */
        if (background_add(
                &background,
                next.novelty
            ) != 0) {
            goto cleanup;
        }

        /*
         * The first observations are used to build the background.
         * No onset can be emitted until sufficient history exists.
         */
        if (!have_current) {
            current = next;
            have_current = 1;
        } else if (!have_previous) {
            previous = current;
            current = next;
            have_previous = 1;
        } else {
            /*
             * At this point:
             *
             *     previous = descriptor before candidate
             *     current  = candidate
             *     next     = descriptor after candidate
             */

            if (background.count >=
                    ONSET_WARMUP_SAMPLES &&
                is_peak(
                    &previous,
                    &current,
                    &next
                )) {

                float strength =
                    observation_strength(
                        &current
                    );

                if (strength > 0.0f) {
                    int sufficiently_far =
                        !have_last_onset;

                    if (!sufficiently_far) {
                        /*
                         * current.frame is monotonic.
                         */
                        sufficiently_far =
                            current.frame >=
                                last_onset_frame &&
                            current.frame -
                                last_onset_frame >=
                                min_distance;
                    }

                    if (sufficiently_far) {
                        if (raw_annotations_append(
                                annotations,
                                current.frame,
                                strength,
                                channel
                            ) != 0) {
                            goto cleanup;
                        }

                        last_onset_frame =
                            current.frame;

                        have_last_onset = 1;
                    }
                }
            }

            previous = current;
            current = next;
        }

        /*
         * Advance the source position by exactly one input hop.
         *
         * analyser_hop_next() has already dealt with completion of the
         * final partial hop according to abyss_policy.
         */
        if (hop_start >
            ANALYSER_FRAME_MAX -
            (analyser_frame_t)hop_size) {
            goto cleanup;
        }

        hop_start +=
            (analyser_frame_t)hop_size;
    }

    /*
     * Do not emit the final descriptor as an onset.
     *
     * A local maximum requires a right-hand neighbour. Treating the end
     * of the source as a synthetic neighbour would make the detector
     * vulnerable to exactly the artificial final onset that the abyss
     * policy is designed to prevent.
     */

    result = 0;

cleanup:
    if (aubio_output != NULL) {
        del_fvec(aubio_output);
    }

    if (input != NULL) {
        del_fvec(input);
    }

    if (aubio != NULL) {
        del_aubio_onset(aubio);
    }

    free(hop_buffer);

    return result;
}
