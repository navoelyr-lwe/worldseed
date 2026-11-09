/* Worldseed Engine — generation-checked handles. */
#include "ws_handle.h"

ws_handle ws_handle_invalid(void)
{
    ws_handle h = {WS_HANDLE_INVALID_INDEX, WS_HANDLE_INVALID_GENERATION};
    return h;
}

int ws_handle_is_valid(ws_handle handle)
{
    return handle.index != WS_HANDLE_INVALID_INDEX &&
           handle.generation != WS_HANDLE_INVALID_GENERATION;
}

int ws_handle_equal(ws_handle a, ws_handle b)
{
    return a.index == b.index && a.generation == b.generation;
}
