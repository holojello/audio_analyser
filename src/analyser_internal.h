#ifndef ANALYSER_INTERNAL_H
#define ANALYSER_INTERNAL_H

#include <stdint.h>

#include "analyser.h"
#include "audio.h"
#include "onset.h"


struct analyser {

    /*
     * Effective configuration.
     *
     * These values are copied from analyser_options_t during
     * analyser_create().
     */
    int detect_transients;

    uint32_t event_merge_window;

    uint32_t buffer_size;
    uint32_t hop_size;

    /*
     * Owned copy of the onset method string.
     *
     * NULL means the default method.
     */
    char *onset_method;

    /*
     * Progress reporting.
     */
    analyser_progress_callback progress;
    void *progress_userdata;

    /*
     * Last error.
     */
    analyser_error_t error;

    /*
     * Human-readable description of the last error.
     */
    char error_message[256];
};


/*
 * Set the analyzer's error state.
 */
void
analyser_set_error(
    analyser_t *analyser,
    analyser_error_t error,
    const char *message
);


#endif /* ANALYSER_INTERNAL_H */
