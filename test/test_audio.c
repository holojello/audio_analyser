#include "audio.h"

#include <assert.h>
#include <math.h>

int
main(void)
{
    const char *filename = "test/fixtures/mono_5_samples.wav";
    const float expected[] = {
        0.8f,
        -0.2f,
        0.0f,
        0.5f,
        -0.9f
    };

    const float tolerance = 0.0001f;

    pcm_audio_t *audio = pcm_audio_load(filename);

    assert(audio != NULL);

    assert(audio->samplerate == 44100);
    assert(audio->channels == 1);
    assert(audio->frame_count == 5);

    assert(audio->channels_data != NULL);
    assert(audio->channels_data[0] != NULL);

    for (size_t frame = 0; frame < audio->frame_count; ++frame) {
        assert(
            fabsf(audio->channels_data[0][frame] - expected[frame])
            < tolerance
        );
    }

    pcm_audio_destroy(audio);

    return 0;
}
