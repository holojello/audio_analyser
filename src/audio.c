#include "audio.h"

#include <sndfile.h>

#include <stdint.h>
#include <stdlib.h>

pcm_audio_t *
pcm_audio_load(const char *filename)
{
    SNDFILE *file = NULL;
    SF_INFO info = {0};
    pcm_audio_t *audio = NULL;
    float *interleaved = NULL;

    size_t frame_count;
    size_t channels;
    size_t channel_bytes;
    size_t sample_count;

    if (filename == NULL) {
        return NULL;
    }

    file = sf_open(filename, SFM_READ, &info);
    if (file == NULL) {
        return NULL;
    }

    if (info.frames < 0 ||
        info.channels <= 0 ||
        info.samplerate <= 0) {
        goto failure;
    }

    /* Note: the first two checks are kept deliberately separate;
     * - First check checks that the frame count can be converted to size_t
     * - Second check checks that the frame count can be converted into
     *   analyser_frame_t 
     * The big idea here is to avoid using analyser_error_t in audio,
     * so this layer's failure contract remains simple (null on failure) */
    if ((uintmax_t)info.frames > SIZE_MAX ||
        (uintmax_t)info.frames > UINT64_MAX ||
        (uintmax_t)info.channels > SIZE_MAX ||
        (uintmax_t)info.samplerate > UINT32_MAX ||
        (uintmax_t)info.channels > UINT32_MAX) {
        goto failure;
    }

    frame_count = (size_t)info.frames;
    channels = (size_t)info.channels;

    if (frame_count > SIZE_MAX / sizeof(float)) {
        goto failure;
    }

    channel_bytes = frame_count * sizeof(float);

    if (frame_count != 0 && channels > SIZE_MAX / frame_count) {
        goto failure;
    }

    sample_count = frame_count * channels;

    if (sample_count > SIZE_MAX / sizeof(float)) {
        goto failure;
    }

    audio = calloc(1, sizeof(*audio));
    if (audio == NULL) {
        goto failure;
    }

    audio->samplerate = (uint32_t)info.samplerate;
    audio->channels = (uint32_t)info.channels;
    audio->frame_count = (analyser_frame_t)frame_count;

    audio->channels_data = calloc(
        channels,
        sizeof(*audio->channels_data)
    );
    if (audio->channels_data == NULL) {
        goto failure;
    }

    for (size_t c = 0; c < channels; ++c) {
        audio->channels_data[c] = malloc(channel_bytes);

        if (audio->channels_data[c] == NULL && frame_count != 0) {
            goto failure;
        }
    }

    if (sample_count != 0) {
        interleaved = malloc(sample_count * sizeof(float));
        if (interleaved == NULL) {
            goto failure;
        }

        if (sf_readf_float(file, interleaved, info.frames) != info.frames) {
            goto failure;
        }

        for (size_t f = 0; f < frame_count; ++f) {
            for (size_t c = 0; c < channels; ++c) {
                audio->channels_data[c][f] =
                    interleaved[f * channels + c];
            }
        }
    }

    free(interleaved);
    sf_close(file);

    return audio;

failure:
    free(interleaved);

    if (file != NULL) {
        sf_close(file);
    }

    pcm_audio_destroy(audio);

    return NULL;
}

void
pcm_audio_destroy(pcm_audio_t *audio)
{
    if (audio == NULL) {
        return;
    }

    if (audio->channels_data != NULL) {
        for (uint32_t c = 0; c < audio->channels; ++c) {
            free(audio->channels_data[c]);
        }

        free(audio->channels_data);
    }

    free(audio);
}
