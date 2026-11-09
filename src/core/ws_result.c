/* Worldseed Engine — result code helpers. */
#include "ws_result.h"

const char *ws_result_name(ws_result result)
{
    switch (result) {
    case WS_OK: return "WS_OK";
    case WS_ERR_INVALID_ARGUMENT: return "WS_ERR_INVALID_ARGUMENT";
    case WS_ERR_NULL_POINTER: return "WS_ERR_NULL_POINTER";
    case WS_ERR_OUT_OF_MEMORY: return "WS_ERR_OUT_OF_MEMORY";
    case WS_ERR_OVERFLOW: return "WS_ERR_OVERFLOW";
    case WS_ERR_NOT_FOUND: return "WS_ERR_NOT_FOUND";
    case WS_ERR_ALREADY_EXISTS: return "WS_ERR_ALREADY_EXISTS";
    case WS_ERR_UNSUPPORTED: return "WS_ERR_UNSUPPORTED";
    case WS_ERR_INVALID_STATE: return "WS_ERR_INVALID_STATE";
    case WS_ERR_PARSE: return "WS_ERR_PARSE";
    case WS_ERR_IO: return "WS_ERR_IO";
    case WS_ERR_CORRUPT: return "WS_ERR_CORRUPT";
    case WS_ERR_CAPACITY: return "WS_ERR_CAPACITY";
    case WS_ERR_NOT_IMPLEMENTED: return "WS_ERR_NOT_IMPLEMENTED";
    default: return "WS_ERR_UNKNOWN";
    }
}

int ws_result_is_ok(ws_result result)
{
    return result == WS_OK;
}
