#include "onset.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"


#define TEST_SAMPLERATE 44100U
#define TEST_BUFFER_SIZE 1024U
#define TEST_HOP_SIZE 512U

#define TEST_CHANNEL 0U

/*
 * The click begins at this frame in the fixture.
 */
#define TEST_CLICK_FRAME 44100U


/*
 * We do not require the detector to identify the exact source sample.
 *
 * The detector reports the centre of an analysis window, and the novelty
 * signal itself has a temporal response. The expected annotation should
 * therefore be close to the source click, rather than necessarily equal
 * to it.
 */
#define TEST_CLICK_FRAME_TOLERANCE 1024U


/*
 * Keep the generated signals long enough for the detector to establish
 * its background before the event being tested.
 */
#define TEST_SIGNAL_FRAME_COUNT (TEST_CLICK_FRAME + 8192U)


static void
assert_no_onsets(
    const float *samples,
    analyser_frame_t frame_count
)
{
    raw_annotations_t annotations = {0};

    int result =
        onset_analyse_channel(
            samples,
            frame_count,
            TEST_SAMPLERATE,
            TEST_BUFFER_SIZE,
            TEST_HOP_SIZE,
            ANALYSER_ONSET_METHOD_DEFAULT,
            ANALYSER_ABYSS_FADEOUT,
            TEST_CHANNEL,
            &annotations
        );

    assert(result == 0);
    assert(annotations.count == 0);

    raw_annotations_destroy(&annotations);
}


static void
test_onset_silence(void)
{
    float *samples;

    samples = calloc(
        TEST_SIGNAL_FRAME_COUNT,
        sizeof(*samples)
    );

    assert(samples != NULL);

    assert_no_onsets(
        samples,
        TEST_SIGNAL_FRAME_COUNT
    );

    free(samples);
}


static uint32_t
test_prng_next(
    uint32_t *state
)
{
    /*
     * Deterministic xorshift32.
     *
     * The test deliberately does not use rand(), because the properties
     * of rand() are implementation-dependent.
     */
    uint32_t x = *state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    *state = x;

    return x;
}


static float
test_white_noise_sample(
    uint32_t *state
)
{
    /*
     * Convert the upper 24 bits to approximately [-1, 1].
     */
    uint32_t value =
        test_prng_next(state) >> 8;

    float normalized =
        (float)value / 8388607.5f;

    return normalized - 1.0f;
}


static void
test_onset_white_noise(void)
{
    float *samples;
    uint32_t state = 0x12345678U;

    samples = malloc(
        (size_t)TEST_SIGNAL_FRAME_COUNT *
        sizeof(*samples)
    );

    assert(samples != NULL);

    for (analyser_frame_t frame = 0;
         frame < TEST_SIGNAL_FRAME_COUNT;
         ++frame) {

        samples[frame] =
            0.1f *
            test_white_noise_sample(&state);
    }

    assert_no_onsets(
        samples,
        TEST_SIGNAL_FRAME_COUNT
    );

    free(samples);
}


static void
test_onset_audacity_click(void)
{
    const char *filename =
        "test/fixtures/click_44100.wav";

    pcm_audio_t *audio =
        pcm_audio_load(filename);

    raw_annotations_t annotations = {0};

    assert(audio != NULL);

    assert(audio->samplerate == TEST_SAMPLERATE);
    assert(audio->channels == 1);
    assert(audio->channels_data != NULL);
    assert(audio->channels_data[0] != NULL);

    /*
     * The fixture is intentionally mono so this test exercises only the
     * onset detector, not channel aggregation.
     */
    assert(
        audio->frame_count >
        TEST_CLICK_FRAME
    );

    assert(
        onset_analyse_channel(
            audio->channels_data[0],
            audio->frame_count,
            audio->samplerate,
            TEST_BUFFER_SIZE,
            TEST_HOP_SIZE,
            ANALYSER_ONSET_METHOD_DEFAULT,
            ANALYSER_ABYSS_FADEOUT,
            TEST_CHANNEL,
            &annotations
        ) == 0
    );

    /*
     * The fixture contains exactly one musical event.
     */
    assert(annotations.count == 1);

    /*
     * The detector's annotation refers to an analysis position, not
     * necessarily to the exact sample at which the click waveform starts.
     */
    analyser_frame_t detected =
        annotations.items[0].frame;

    analyser_frame_t distance =
        detected > TEST_CLICK_FRAME
            ? detected - TEST_CLICK_FRAME
            : TEST_CLICK_FRAME - detected;

    assert(
        distance <=
        TEST_CLICK_FRAME_TOLERANCE
    );

    /*
     * Strength is a confidence/magnitude against background, not a
     * physical amplitude measurement.
     */
    assert(annotations.items[0].strength >= 0.0f);
    assert(annotations.items[0].strength <= 1.0f);

    assert(
        annotations.items[0].channel ==
        TEST_CHANNEL
    );

    raw_annotations_destroy(&annotations);
    pcm_audio_destroy(audio);
}


int
main(void)
{
    test_onset_silence();
    test_onset_white_noise();
    test_onset_audacity_click();

    return 0;
}
