/* Worldseed Engine — tiny C17 test harness.
 *
 * Deliberately minimal: no external test framework. A test file defines a
 * main() that calls ws_test_run. Each case returns 1 on pass and 0 on fail and
 * may use the WS_CHECK macros. The process exits non-zero if any case fails so
 * CI fails the job.
 */
#ifndef WS_TEST_H
#define WS_TEST_H

#include <stdio.h>

typedef int (*ws_test_case_fn)(void);

typedef struct ws_test_case {
    const char *name;
    ws_test_case_fn fn;
} ws_test_case;

#define WS_CHECK(cond)                                                          \
    do {                                                                        \
        if (!(cond)) {                                                          \
            printf("    FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__);        \
            return 0;                                                           \
        }                                                                       \
    } while (0)

#define WS_CHECK_MSG(cond, msg)                                                 \
    do {                                                                        \
        if (!(cond)) {                                                          \
            printf("    FAIL: %s (%s:%d) %s\n", #cond, __FILE__, __LINE__, msg);\
            return 0;                                                           \
        }                                                                       \
    } while (0)

static inline int ws_test_run(const char *suite, const ws_test_case *cases, int count)
{
    int failed = 0;
    printf("== suite: %s ==\n", suite);
    for (int i = 0; i < count; ++i) {
        int ok = cases[i].fn();
        printf("[%s] %s\n", ok ? "PASS" : "FAIL", cases[i].name);
        if (!ok) {
            failed += 1;
        }
    }
    printf("== suite %s: %d/%d passed ==\n", suite, count - failed, count);
    return failed == 0 ? 0 : 1;
}

#endif /* WS_TEST_H */
