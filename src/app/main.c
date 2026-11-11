/* Worldseed Engine — application entry point. */
#include "ws_app.h"
#include "ws_live.h"

#include "../core/ws_version.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("Worldseed Engine %s\n", WS_VERSION_STRING);
            return 0;
        }
    }

    /* The live interactive application (P11) takes its own flags. When none is
     * present the program falls through to the showcase/capture application. */
    ws_live_options live;
    ws_result lr = ws_live_parse_options(argc, (const char *const *)argv, &live);
    if (lr != WS_OK) {
        fprintf(stderr, "invalid live arguments (use --live, --fullscreen, "
                        "--live-no-select, --live-headless <s>, --live-tour <dir>, "
                        "--live-replay <dir>, --live-select <dir>, "
                        "--live-export <dir>, --live-capture <file>)\n");
        return 2;
    }
    if (live.enabled) {
        return ws_live_run(&live);
    }

    ws_app_options opts;
    ws_result r = ws_app_parse_options(argc, argv, &opts);
    if (r != WS_OK) {
        fprintf(stderr, "invalid arguments (use --list, --selftest, --capture <file>, "
                        "--scene N, --seed N, --size WxH, --headless, or --live*)\n");
        return 2;
    }
    return ws_app_run(&opts);
}
