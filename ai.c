#include "../src/80s.h"

int on_load(void *ctx, struct serve_params *params, int reload) {
    printf("on load, worker: %d\n", params->workerid);
}

int on_unload(void *ctx, struct serve_params *params, int reload) {
    printf("on unload, worker: %d\n", params->workerid);
}