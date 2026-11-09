/* Worldseed Engine — application entry point. */
#include "ws_app.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    ws_app_options opts;
    ws_result r = ws_app_parse_options(argc, argv, &opts);
    if (r != WS_OK) {
        fprintf(stderr, "invalid arguments (use --list, --selftest, --capture <file>, "
                        "--scene N, --seed N, --size WxH, --headless)\n");
        return 2;
    }
    return ws_app_run(&opts);
}
