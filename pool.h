#ifndef __CPMEMP_H__
#define __CPMEMP_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
#define LWIP_MEMPOOL(name,num,size,desc)  MEMP_##name,
#include "pool_std.h"
  MEMP_MAX
} memp_t;

//声明
void cp_memp_memory_reset0(void);
void cp_memp_init(void);
void memp_desc_printf(void);

void *cp_memp_malloc(memp_t type);
void  cp_memp_free(memp_t type, void *mem);

#ifdef __cplusplus
}
#endif

#endif /* __CPMEMP_H__ */
