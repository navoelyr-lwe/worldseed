/* Worldseed Engine — result codes.
 *
 * Every fallible operation returns a ws_result. There is no silent default:
 * callers must check the result. Values are stable and serialisable.
 */
#ifndef WS_CORE_RESULT_H
#define WS_CORE_RESULT_H

#ifdef __cplusplus
#error "Worldseed Engine is ISO C17 only; C++ is not permitted."
#endif

typedef enum ws_result {
    WS_OK = 0,
    WS_ERR_INVALID_ARGUMENT = 1,
    WS_ERR_NULL_POINTER = 2,
    WS_ERR_OUT_OF_MEMORY = 3,
    WS_ERR_OVERFLOW = 4,
    WS_ERR_NOT_FOUND = 5,
    WS_ERR_ALREADY_EXISTS = 6,
    WS_ERR_UNSUPPORTED = 7,
    WS_ERR_INVALID_STATE = 8,
    WS_ERR_PARSE = 9,
    WS_ERR_IO = 10,
    WS_ERR_CORRUPT = 11,
    WS_ERR_CAPACITY = 12,
    WS_ERR_NOT_IMPLEMENTED = 13
} ws_result;

/* Human-readable, stable, English identifier for a result code. */
const char *ws_result_name(ws_result result);

/* True when the result represents success. */
int ws_result_is_ok(ws_result result);

#endif /* WS_CORE_RESULT_H */
