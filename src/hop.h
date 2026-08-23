#ifndef ANALYSER_HOP_H
#define ANALYSER_HOP_H

#include <stdint.h>

#include "analyser.h"

/*
 * Iterator over a single mono PCM channel.
 *
 * The iterator guarantees that a successful call to
 * analyser_hop_next() fills exactly hop_size samples.
 *
 * The final incomplete hop is completed according to abyss_policy.
 */
typedef struct {
    const float *samples;
    analyser_frame_t frame_count;

    uint32_t hop_size;
    analyser_abyss_policy_t abyss_policy;

    analyser_frame_t position;
    int finished;
} analyser_hop_reader_t;

/*
 * Initialize a hop reader.
 *
 * samples may be NULL only when frame_count is zero.
 */
int
analyser_hop_reader_init(
    analyser_hop_reader_t *reader,
    const float *samples,
    analyser_frame_t frame_count,
    uint32_t hop_size,
    analyser_abyss_policy_t abyss_policy
);

/*
 * Fill buffer with the next complete hop.
 *
 * Returns:
 *
 *   1  a hop was produced
 *   0  end of input
 *  -1  invalid arguments / unsupported policy
 *
 * On success, exactly hop_size samples are written to buffer.
 */
int
analyser_hop_next(
    analyser_hop_reader_t *reader,
    float *buffer
);

#endif /* ANALYSER_HOP_H */
