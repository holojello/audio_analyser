#ifndef ANALYSER_AUDIO_H
#define ANALYSER_AUDIO_H

#include <stdint.h>

#include "analyser.h"


/*
 * Channel-oriented floating-point PCM data.
 *
 * channels_data[c][f] contains the sample for channel c at
 * frame f.
 */
typedef struct {
    uint32_t samplerate;
    uint32_t channels;
    analyser_frame_t frame_count;

    float **channels_data;

} pcm_audio_t;


/*
 * Load and decode an audio file using libsndfile.
 *
 * The resulting samples are floating-point and channel-oriented.
 */
pcm_audio_t *
pcm_audio_load(const char *filename);


/*
 * Free decoded PCM data.
 */
void
pcm_audio_destroy(pcm_audio_t *audio);


#endif /* ANALYSER_AUDIO_H */
