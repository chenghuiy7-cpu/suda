#include <rte_lcore.h>

#include "spdk/env.h"

uint32_t
spdk_env_get_current_core(void)
{
	return rte_lcore_id();
}