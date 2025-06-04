#ifndef _VIO
#define _VIO

#include "spdk/stdinc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPDK_VIO_MAX_RECURSIVE_DEPTH 4

struct spdk_nvme_cmd;
struct spdk_vio;

typedef void (*spdk_vio_completion_cb)(struct spdk_vio *vio,
		bool success,
		void *cb_arg);

struct spdk_vio {
    struct spdk_nvme_cmd *cmd;
    spdk_vio_completion_cb cb_stack[SPDK_VIO_MAX_RECURSIVE_DEPTH];
    const struct spdk_nvme_cpl *cpl;
    void *cb_arg;
    uint8_t recursive_depth;
};

#ifdef __cplusplus
}
#endif

#endif
