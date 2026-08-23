#include "analyser.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "aggregate.h"
#include "audio.h"
#include "onset.h"


/*
 * -------------------------------------------------------------------------
 * User-facing defaults
 * -------------------------------------------------------------------------
 *
 * These belong to analyser.c.
 *
 * Detector implementations must receive resolved values and must not
 * independently interpret zero/NULL as "use the default".
 */

#define ANALYSER_DEFAULT_BUFFER_SIZE 1024U
#define ANALYSER_DEFAULT_HOP_SIZE    512U

/*
 * -------------------------------------------------------------------------
 * Internal analyser state
 * -------------------------------------------------------------------------
 */

struct analyser {
    int detect_transients;

    uint32_t event_merge_window;

    const char *onset_method;

    uint32_t buffer_size;
    uint32_t hop_size;

    analyser_abyss_policy_t abyss_policy;

    analyser_progress_callback progress;
    void *progress_userdata;

    analyser_error_t error;

    char error_message[256];
};


/*
 * -------------------------------------------------------------------------
 * Error handling
 * -------------------------------------------------------------------------
 */

static void
analyser_set_error(
    analyser_t *analyser,
    analyser_error_t error,
    const char *message
)
{
    if (analyser == NULL) {
        return;
    }

    analyser->error = error;

    if (message == NULL) {
        analyser->error_message[0] = '\0';
        return;
    }

    strncpy(
        analyser->error_message,
        message,
        sizeof(analyser->error_message) - 1U
    );

    analyser->error_message[
        sizeof(analyser->error_message) - 1U
    ] = '\0';
}


/*
 * -------------------------------------------------------------------------
 * Configuration validation/resolution
 * -------------------------------------------------------------------------
 */

static int
resolve_options(
    analyser_t *analyser,
    const analyser_options_t *options
)
{
    if (analyser == NULL) {
        return -1;
    }

    /*
     * User-facing defaults.
     */
    analyser->detect_transients = 1;

    analyser->event_merge_window = 0;

    analyser->onset_method =
        ANALYSER_ONSET_METHOD_DEFAULT;

    analyser->buffer_size =
        ANALYSER_DEFAULT_BUFFER_SIZE;

    analyser->hop_size =
        ANALYSER_DEFAULT_HOP_SIZE;

    /*
     * Zero is deliberately the first/only currently implemented abyss
     * policy, so a zero-initialized options structure naturally selects
     * fadeout.
     */
    analyser->abyss_policy =
        ANALYSER_ABYSS_FADEOUT;

    analyser->progress = NULL;
    analyser->progress_userdata = NULL;

    if (options == NULL) {
        return 0;
    }

    analyser->detect_transients =
        options->detect_transients;

    analyser->event_merge_window =
        options->event_merge_window;

    /*
     * NULL means "use the analyser's default method".
     *
     * The resolved value is explicitly the string "default".
     */
    if (options->onset_method != NULL) {
        if (strcmp(
                options->onset_method,
                ANALYSER_ONSET_METHOD_DEFAULT
            ) != 0) {

            analyser_set_error(
                analyser,
                ANALYSER_ERROR_INVALID_ARGUMENT,
                "unsupported onset detection method"
            );

            return -1;
        }

        analyser->onset_method =
            ANALYSER_ONSET_METHOD_DEFAULT;
    }

    /*
     * Zero means use the analyser's default.
     */
    if (options->buffer_size != 0) {
        analyser->buffer_size =
            options->buffer_size;
    }

    if (options->hop_size != 0) {
        analyser->hop_size =
            options->hop_size;
    }

    /*
     * Validate the resolved hop/window relationship.
     *
     * Overlap is supported and is expected. A hop larger than the
     * analysis window, however, would make the intended overlapping
     * analysis semantics impossible.
     */
    if (analyser->hop_size >
        analyser->buffer_size) {

        analyser_set_error(
            analyser,
            ANALYSER_ERROR_INVALID_ARGUMENT,
            "hop size must not exceed buffer size"
        );

        return -1;
    }

    analyser->abyss_policy =
        options->abyss_policy;

    switch (analyser->abyss_policy) {
    case ANALYSER_ABYSS_FADEOUT:
        break;

    default:
        analyser_set_error(
            analyser,
            ANALYSER_ERROR_INVALID_ARGUMENT,
            "unsupported abyss policy"
        );

        return -1;
    }

    analyser->progress =
        options->progress;

    analyser->progress_userdata =
        options->progress_userdata;

    return 0;
}


/*
 * -------------------------------------------------------------------------
 * Construction / destruction
 * -------------------------------------------------------------------------
 */

analyser_t *
analyser_create(
    const analyser_options_t *options
)
{
    analyser_t *analyser;

    analyser = calloc(
        1,
        sizeof(*analyser)
    );

    if (analyser == NULL) {
        return NULL;
    }

    analyser->error = ANALYSER_OK;
    analyser->error_message[0] = '\0';

    if (resolve_options(
            analyser,
            options
        ) != 0) {

        free(analyser);
        return NULL;
    }

    return analyser;
}


void
analyser_destroy(
    analyser_t *analyser
)
{
    free(analyser);
}


/*
 * -------------------------------------------------------------------------
 * Error accessors
 * -------------------------------------------------------------------------
 */

analyser_error_t
analyser_get_error(
    const analyser_t *analyser
)
{
    if (analyser == NULL) {
        return ANALYSER_ERROR_INVALID_ARGUMENT;
    }

    return analyser->error;
}


const char *
analyser_get_error_message(
    const analyser_t *analyser
)
{
    if (analyser == NULL) {
        return "invalid analyser";
    }

    return analyser->error_message;
}
