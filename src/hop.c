#include "hop.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static int
hop_has_remaining(
    const analyser_hop_reader_t *reader
)
{
    return reader->position < reader->frame_count;
}

static void
fill_fadeout(
    float *buffer,
    uint32_t hop_size,
    const float *samples,
    analyser_frame_t position,
    analyser_frame_t frame_count
)
{
    analyser_frame_t remaining =
        frame_count - position;

    uint32_t actual = (uint32_t)remaining;

    if (actual > hop_size) {
        actual = hop_size;
    }

    if (actual == 0) {
        memset(buffer, 0, (size_t)hop_size * sizeof(*buffer));
        return;
    }

    /*
     * A half-cosine fade goes smoothly from 1 to 0:
     *
     *     f(t) = cos(pi * t / 2)
     *
     * where t goes from 0 to 1.
     *
     * For a one-sample partial hop there is no meaningful interval
     * over which to perform a fade. Preserve that sample rather than
     * silently deleting the only remaining input sample.
     */
    if (actual == 1) {
        buffer[0] = samples[position];
    } else {
        for (uint32_t i = 0; i < actual; ++i) {
            double t = (double)i / (double)(actual - 1);
            const double half_pi = 1.57079632679489661923;
            float gain = (float)cos(half_pi * t);

            buffer[i] = samples[position + i] * gain;
        }
    }

    if (actual < hop_size) {
        memset(
            buffer + actual,
            0,
            (size_t)(hop_size - actual) * sizeof(*buffer)
        );
    }
}

int
analyser_hop_reader_init(
    analyser_hop_reader_t *reader,
    const float *samples,
    analyser_frame_t frame_count,
    uint32_t hop_size,
    analyser_abyss_policy_t abyss_policy
)
{
    if (reader == NULL ||
        (samples == NULL && frame_count != 0) ||
        hop_size == 0) {
        return -1;
    }

    if (abyss_policy != ANALYSER_ABYSS_FADEOUT) {
        return -1;
    }

    reader->samples = samples;
    reader->frame_count = frame_count;
    reader->hop_size = hop_size;
    reader->abyss_policy = abyss_policy;
    reader->position = 0;
    reader->finished = 0;

    return 0;
}

int
analyser_hop_next(
    analyser_hop_reader_t *reader,
    float *buffer
)
{
    if (reader == NULL || buffer == NULL) {
        return -1;
    }

    if (reader->finished || !hop_has_remaining(reader)) {
        reader->finished = 1;
        return 0;
    }

    /*
     * The normal case: a complete hop remains.
     */
    if (reader->frame_count - reader->position >= reader->hop_size) {
        memcpy(
            buffer,
            reader->samples + reader->position,
            (size_t)reader->hop_size * sizeof(*buffer)
        );

        reader->position += reader->hop_size;

        return 1;
    }

    /*
     * The remaining data forms the final partial hop.
     */
    switch (reader->abyss_policy) {
    case ANALYSER_ABYSS_FADEOUT:
        fill_fadeout(
            buffer,
            reader->hop_size,
            reader->samples,
            reader->position,
            reader->frame_count
        );
        break;

    default:
        return -1;
    }

    reader->position = reader->frame_count;
    reader->finished = 1;

    return 1;
}
