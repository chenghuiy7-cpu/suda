#ifndef _BLOWFISH_H_
#define _BLOWFISH_H_

#include "stdlib.h"
#include "stdint.h"

#include "spdk/compute.h"

struct blowfish_ctx {
    uint64_t *buf;
    uint32_t start;
    uint32_t end;
	struct spdk_nvmf_mcdma_request *mcdma_req;
};

uint64_t blowfish_decrypt(uint64_t target);

uint64_t blowfish_encrypt(uint64_t target);

void blowfish_encrypt_range(uint64_t *buf, uint32_t start, uint32_t end);

#endif