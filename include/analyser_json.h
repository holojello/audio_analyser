#ifndef ANALYSER_JSON_H
#define ANALYSER_JSON_H

#include "analyser.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Version of the JSON data format.
 *
 * This is independent of ANALYSER_VERSION_*.
 */
#define ANALYSER_JSON_VERSION 1


/* -------------------------------------------------------------------------
 * JSON serialization
 * ------------------------------------------------------------------------- */

/*
 * Write an analysis result to a JSON file.
 *
 * Returns ANALYSER_OK on success.
 */
analyser_error_t
analyser_result_write_json(
    const analyser_result_t *result,
    const char *filename
);


/*
 * Read an analysis result from a JSON file.
 *
 * On success, returns a newly allocated result which must be freed
 * with analyser_result_destroy().
 *
 * On failure, returns NULL and writes the error code to *error if
 * error is not NULL.
 */
analyser_result_t *
analyser_result_read_json(
    const char *filename,
    analyser_error_t *error
);


#ifdef __cplusplus
}
#endif

#endif /* ANALYSER_JSON_H */
