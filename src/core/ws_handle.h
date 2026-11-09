/* Worldseed Engine — generation-checked handles.
 *
 * Long-lived references to world objects use an identity/generation pair, not
 * a raw pointer. A handle whose generation no longer matches the stored slot
 * is stale and must be rejected; this prevents use-after-free and identity
 * reuse across object destruction.
 */
#ifndef WS_CORE_HANDLE_H
#define WS_CORE_HANDLE_H

#include <stdint.h>

#include "ws_result.h"

#define WS_HANDLE_INVALID_INDEX UINT32_MAX
#define WS_HANDLE_INVALID_GENERATION 0u

typedef struct ws_handle {
    uint32_t index;
    uint32_t generation;
} ws_handle;

/* A handle that never resolves to a live object. */
ws_handle ws_handle_invalid(void);

/* True when the handle is not the canonical invalid handle. */
int ws_handle_is_valid(ws_handle handle);

int ws_handle_equal(ws_handle a, ws_handle b);

#endif /* WS_CORE_HANDLE_H */
