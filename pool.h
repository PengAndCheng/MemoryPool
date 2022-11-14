#ifndef __CPMEMP_H__
#define __CPMEMP_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
#define LWIP_MEMPOOL(name,num,size,desc)  MEMP_##name,
#include "pool_std.h"
#undef LWIP_MEMPOOL
  MEMP_MAX
} memp_t;


//声明放这
void pool_init(void);
#if MEMP_OVERFLOW_CHECK
void *pool_malloc_fn(memp_t type, const char* file, const int line);
#define pool_malloc(t) pool_malloc_fn((t), __FILE__, __LINE__)
#else
void *pool_malloc(memp_t type);
#endif
void  pool_free(memp_t type, void *mem);



#ifdef __cplusplus
}
#endif

#endif /* __CPMEMP_H__ */
