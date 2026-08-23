#ifndef ANALYSER_H
#define ANALYSER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/* -------------------------------------------------------------------------
 * Version
 * ------------------------------------------------------------------------- */

#define ANALYSER_VERSION_MAJOR 0
#define ANALYSER_VERSION_MINOR 1
#define ANALYSER_VERSION_PATCH 0


/* -------------------------------------------------------------------------
 * Opaque types
 * ------------------------------------------------------------------------- */

typedef struct analyser analyser_t;


/* -------------------------------------------------------------------------
 * Basic types
 * ------------------------------------------------------------------------- */

/*
 * Position in the original PCM stream.
 *
 * A frame contains one sample for every channel.
 */
typedef uint64_t analyser_frame_t;

/*
 * Maximum representable position in the original PCM stream.
 */
#define ANALYSER_FRAME_MAX UINT64_MAX

/* -------------------------------------------------------------------------
 * Abyss policies
 * ------------------------------------------------------------------------- */

/*
 * Policy used to complete the final incomplete input hop.
 *
 * The analyser always supplies complete hop-sized buffers to detectors.
 */
typedef enum {
    ANALYSER_ABYSS_FADEOUT = 0
} analyser_abyss_policy_t;

/* -------------------------------------------------------------------------
 * Errors
 * ------------------------------------------------------------------------- */

typedef enum {
    ANALYSER_OK = 0,

    ANALYSER_ERROR_INVALID_ARGUMENT,
    ANALYSER_ERROR_OUT_OF_MEMORY,
    ANALYSER_ERROR_OPEN_FILE,
    ANALYSER_ERROR_READ_FILE,
    ANALYSER_ERROR_UNSUPPORTED_FORMAT,
    ANALYSER_ERROR_ANALYSIS,
    ANALYSER_ERROR_CANCELLED,
    ANALYSER_ERROR_INTERNAL

} analyser_error_t;


/* -------------------------------------------------------------------------
 * Event types
 * ------------------------------------------------------------------------- */

typedef enum {
    ANALYSER_EVENT_TRANSIENT = 1

} analyser_event_type_t;


/* -------------------------------------------------------------------------
 * Events
 * ------------------------------------------------------------------------- */

/*
 * A musical event resulting from analysis and channel aggregation.
 */
typedef struct {
    /*
     * Type of event.
     */
    analyser_event_type_t type;

    /*
     * Position in the original PCM stream.
     *
     * This is the canonical time representation used by the
     * analyser.
     */
    analyser_frame_t frame;

    /*
     * Event strength.
     *
     * The exact semantics of this value are determined by the
     * analysis implementation.
     */
    float strength;

    /*
     * Approximate stereo position.
     *
     *     -1.0 = left
     *      0.0 = center
     *     +1.0 = right
     *
     * For mono sources this will normally be 0.0.
     */
    float pan;

    /*
     * Bit mask of channels contributing to this event.
     *
     * Bit N corresponds to channel N.
     *
     * Up to 64 channels are supported.
     */
    uint64_t channel_mask;

} analyser_event_t;


/* -------------------------------------------------------------------------
 * Analysis result
 * ------------------------------------------------------------------------- */

typedef struct analyser_result {
    /*
     * Source sample rate in Hz.
     */
    uint32_t samplerate;

    /*
     * Number of channels in the source file.
     */
    uint32_t channels;

    /*
     * Number of PCM frames in the source file.
     */
    analyser_frame_t frame_count;

    /*
     * Number of aggregated events.
     */
    size_t event_count;

    /*
     * Array of events.
     *
     * Owned by the result and freed by
     * analyser_result_destroy().
     */
    analyser_event_t *events;

} analyser_result_t;


/* -------------------------------------------------------------------------
 * Progress callback
 * ------------------------------------------------------------------------- */

/*
 * Called periodically during analysis.
 *
 * Return 0 to continue.
 * Return non-zero to request cancellation.
 */
typedef int (*analyser_progress_callback)(
    analyser_frame_t current_frame,
    analyser_frame_t total_frames,
    void *userdata
);


/* -------------------------------------------------------------------------
 * Analysis options
 * ------------------------------------------------------------------------- */

typedef struct {
    /*
     * Enable transient/onset detection.
     */
    int detect_transients;

    /*
     * Maximum distance, in PCM frames, between annotations on
     * different channels for them to be considered the same event.
     */
    uint32_t event_merge_window;

    /*
     * Onset detection method.
     *
     * NULL means use the analyser's default method.
     *
     * The resolved default method is explicitly represented internally
     * by the string "default". Aubio is an implementation detail and
     * its method names are not part of this public API.
     */
    const char *onset_method;

    /*
     * Aubio analysis window size.
     *
     * 0 means use the analyser default.
     */
    uint32_t buffer_size;

    /*
     * Aubio hop size.
     *
     * 0 means use the analyser default.
     */
    uint32_t hop_size;

    /*
     * Policy used to complete the final incomplete input hop.
     */
    analyser_abyss_policy_t abyss_policy;

    /*
     * Optional progress callback.
     *
     * NULL disables progress reporting.
     */
    analyser_progress_callback progress;

    /*
     * User-defined data passed to progress().
     */
    void *progress_userdata;

} analyser_options_t;


/* -------------------------------------------------------------------------
 * Analyzer lifecycle
 * ------------------------------------------------------------------------- */

/*
 * Create an analyser.
 *
 * Passing NULL uses the default options.
 *
 * Returns NULL if creation fails.
 */
analyser_t *
analyser_create(const analyser_options_t *options);


/*
 * Destroy an analyser.
 *
 * Passing NULL is allowed.
 */
void
analyser_destroy(analyser_t *analyser);


/* -------------------------------------------------------------------------
 * Analysis
 * ------------------------------------------------------------------------- */

/*
 * Analyze an audio file.
 *
 * The file is opened and decoded internally.
 *
 * Channels are analyzed independently and their annotations are
 * subsequently aggregated into analyser_event_t events.
 *
 * Returns NULL on failure.
 */
analyser_result_t *
analyser_analyse_file(
    analyser_t *analyser,
    const char *filename
);


/*
 * Destroy an analysis result.
 *
 * Passing NULL is allowed.
 */
void
analyser_result_destroy(analyser_result_t *result);


/* -------------------------------------------------------------------------
 * Error reporting
 * ------------------------------------------------------------------------- */

/*
 * Return the error produced by the most recent operation.
 */
analyser_error_t
analyser_get_error(const analyser_t *analyser);


/*
 * Return a human-readable description of the most recent error.
 *
 * The returned string is owned by the analyser and must not be freed
 * by the caller.
 */
const char *
analyser_get_error_message(const analyser_t *analyser);


/* -------------------------------------------------------------------------
 * Version
 * ------------------------------------------------------------------------- */

/*
 * Return the library version as a string.
 *
 * Example: "0.1.0"
 */
const char *
analyser_version(void);


#ifdef __cplusplus
}
#endif

#endif /* ANALYSER_H */
