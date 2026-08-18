#ifndef ANALYSER_AGGREGATE_H
#define ANALYSER_AGGREGATE_H

#include <stddef.h>
#include <stdint.h>

#include "analyser.h"
#include "onset.h"


/*
 * Aggregate annotations from all channels into musical events.
 *
 * channel_annotations[c] contains the raw annotations detected
 * on channel c.
 *
 * Annotations sufficiently close in time may be merged into a
 * single analyser_event_t.
 *
 * The returned events array is owned by the caller and must
 * eventually be freed with free().
 */
int
aggregate_annotations(
    const raw_annotations_t *channel_annotations,
    uint32_t channels,
    uint32_t merge_window,
    analyser_event_t **events,
    size_t *event_count
);


#endif /* ANALYSER_AGGREGATE_H */
