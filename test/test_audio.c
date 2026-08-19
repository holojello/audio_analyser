#include "audio.h"

#include <assert.h>
#include <math.h>

void
test_mono_5_samples()
{
    const char *filename = "test/fixtures/mono_5_samples.wav";
    const float expected[] = {
        0.8f, -0.2f, 0.0f, 0.5f, -0.9f
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
}

void
test_stereo_5_samples()
{
    const char *filename = "test/fixtures/stereo_5_samples.wav";
    const float expected_left[] = {
        0.8f, -0.2f, 0.0f, 0.5f, -0.9f
    };
    const float expected_right[] = {
        0.7f, -0.6f, 0.1f, 0.2f, -0.3f
    };
    
    const float tolerance = 0.0001f;

    pcm_audio_t *audio = pcm_audio_load(filename);

    assert(audio != NULL);

    assert(audio->samplerate == 44100);
    assert(audio->channels == 2);
    assert(audio->frame_count == 5);

    assert(audio->channels_data != NULL);
    assert(audio->channels_data[0] != NULL);
    assert(audio->channels_data[1] != NULL);

    for (size_t frame = 0; frame < audio->frame_count; ++frame) {
        assert(
            fabsf(audio->channels_data[0][frame] - expected_left[frame])
            < tolerance
        );
        assert(
            fabsf(audio->channels_data[1][frame] - expected_right[frame])
            < tolerance
        );
    }

    pcm_audio_destroy(audio);
}

int
main(void)
{
    test_mono_5_samples();
    test_stereo_5_samples();

    return 0;
}
