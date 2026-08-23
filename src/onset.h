#ifndef ANALYSER_ONSET_H
#define ANALYSER_ONSET_H

#include <stddef.h>
#include <stdint.h>

#include "analyser.h"


/*
 * A raw onset detected on one channel.
 *
 * This is an intermediate representation and is not exposed by
 * the public API.
 */
typedef struct {
    analyser_frame_t frame;
    float strength;
    uint32_t channel;

} raw_annotation_t;


/*
 * Dynamically sized collection of raw annotations.
 */
typedef struct {
    raw_annotation_t *items;

    size_t count;
    size_t capacity;

} raw_annotations_t;


/*
 * Analyze one mono channel for transients/onsets.
 *
 * The input contains samples belonging to exactly one channel.
 *
 * buffer_size, hop_size, method, and abyss_policy have already been
 * resolved by analyser.c. onset.c does not apply user-facing defaults.
 *
 * Aubio is used internally by the implementation.
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
);

/*
 * Free a raw annotation collection.
 */
void
raw_annotations_destroy(
    raw_annotations_t *annotations
);


#endif /* ANALYSER_ONSET_H */
