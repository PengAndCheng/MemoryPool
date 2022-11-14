
#include <stddef.h>

#include <stdint.h>
typedef uint8_t   u8_t;
typedef int8_t    s8_t;
typedef uint16_t  u16_t;
typedef int16_t   s16_t;
typedef uint32_t  u32_t;
typedef int32_t   s32_t;
typedef uintptr_t mem_ptr_t;

#include <stdio.h>

#include "pool.h"

#ifndef MEMP_STATS
//MEMP_STATS==1: Enable memp.c pool stats.
#define MEMP_STATS 1        //推介1 程序依赖度此宏启用某些功能 不更改程序源码是为了与lwip保持一致性
#endif

//字节对齐 应设置为与CPU对齐
#ifndef MEM_ALIGNMENT
#define MEM_ALIGNMENT 4     //32位cpu推介4
#endif

//lwip中计算x进行内存对齐后的大小
#ifndef LWIP_MEM_ALIGN_SIZE
#define LWIP_MEM_ALIGN_SIZE(size) (((size) + MEM_ALIGNMENT - 1) & ~(MEM_ALIGNMENT-1))
#endif

//将内存指针与MEM_alignment定义的对齐方式对齐 使得ADDR%MEM_ALIGNMENT==0
#ifndef LWIP_MEM_ALIGN
#define LWIP_MEM_ALIGN(addr) ((void *)(((mem_ptr_t)(addr) + MEM_ALIGNMENT - 1) & ~(mem_ptr_t)(MEM_ALIGNMENT-1)))
#endif


//是否为各个内存池提供定义 否则使用单一内存池  推介为0使用一块连续空间因为MEMP_OVERFLOW_CHECK依赖单片内存
#define MEMP_SEPARATE_POOLS 0   //推介0
//使用debug 会存在各类型内存池的描述信息static const char *memp_desc[MEMP_MAX] 推介打开为1初始化后输出内存池信息
#define LWIP_DEBUG 1            //推介1
//进行溢出检查 这里默认MEMP_OVERFLOW_CHECK>=2将对所有内存池进行溢出检查 前提条件使用单一内存池MEMP_SEPARATE_POOLS==0 推介在开发中遇到故障时打开调试
#define MEMP_OVERFLOW_CHECK 0   //推介常为0，出现bug后打开2
//定义memp是否开启环路检测  这个应该是检查memp是不是循环链表 初始化算法中不是循环链表 可以常为0
#define MEMP_SANITY_CHECK 0     //推介0，出现bug后打开1

//debug
#define PRINTF printf
#define LWIP_ASSERT(message, assertion) do { if(!(assertion))PRINTF(message); } while(0)


/*
 MEMP_MEM_MALLOC    定义是否使用内存堆机制来为内存池分配内存
 MEM_USE_POOLS      定义使用内存池来给内存堆分配内存
 MEMP_OVERFLOW_CHECK  溢出检测
 MEMP_SANITY_REGION_BEFORE   内存池下溢检测区域大小
 MEMP_SANITY_REGION_AFTER    内存池溢出检测区域大小
 MEMP_SIZE            对齐后的mem结构的大小(用于管理memp)
 MEMP_ALIGN_SIZE(x)   计算x进行内存对齐后的大小 #define LWIP_MEM_ALIGN_SIZE(size) (((size) + MEM_ALIGNMENT - 1) & ~(MEM_ALIGNMENT-1))
 LWIP_DEBUG           lwip的调试输出
 MEMP_SEPARATE_POOLS  定义memp是否使用分离的内存池
 MEMP_SANITY_CHECK    定义memp是否开启环路检测
 */


//memp的内存管理结构
struct memp {
  struct memp *next;      //下一个可用内存池的首地址
#if MEMP_OVERFLOW_CHECK
  const char *file;
  int line;
#endif /* MEMP_OVERFLOW_CHECK */
};


//由是否溢出检查定义出 MEMP_SIZE 和 MEMP_ALIGN_SIZE(x)
#if MEMP_OVERFLOW_CHECK
/* if MEMP_OVERFLOW_CHECK is turned on, we reserve some bytes at the beginning
 * and at the end of each element, initialize them as 0xcd and check
 * them later. */                                                       //如果MEMP_OVERFLOW_CHECK处于打开状态，我们将在开头保留一些字节在每个元素的末尾，将它们初始化为0xcd并检查以后再给他们。
/* If MEMP_OVERFLOW_CHECK is >= 2, on every call to memp_malloc or memp_free,
 * every single element in each pool is checked!
 * This is VERY SLOW but also very helpful. */                          //如果MEMP_OVERFLOW_CHECK>=2，检查每个池中的每个元素！这很慢，但也很有帮助。
/* MEMP_SANITY_REGION_BEFORE and MEMP_SANITY_REGION_AFTER can be overridden in
 * lwipopts.h to change the amount reserved for checking. */
#ifndef MEMP_SANITY_REGION_BEFORE
#define MEMP_SANITY_REGION_BEFORE  16
#endif /* MEMP_SANITY_REGION_BEFORE*/
#if MEMP_SANITY_REGION_BEFORE > 0
#define MEMP_SANITY_REGION_BEFORE_ALIGNED    LWIP_MEM_ALIGN_SIZE(MEMP_SANITY_REGION_BEFORE) //对齐前的MEMP健全区域
#else
#define MEMP_SANITY_REGION_BEFORE_ALIGNED    0
#endif /* MEMP_SANITY_REGION_BEFORE*/
#ifndef MEMP_SANITY_REGION_AFTER
#define MEMP_SANITY_REGION_AFTER   16
#endif /* MEMP_SANITY_REGION_AFTER*/
#if MEMP_SANITY_REGION_AFTER > 0
#define MEMP_SANITY_REGION_AFTER_ALIGNED     LWIP_MEM_ALIGN_SIZE(MEMP_SANITY_REGION_AFTER) //对齐后的MEMP健全区域
#else
#define MEMP_SANITY_REGION_AFTER_ALIGNED     0
#endif /* MEMP_SANITY_REGION_AFTER*/

/* MEMP_SIZE: save space for struct memp and for sanity check */
#define MEMP_SIZE          (LWIP_MEM_ALIGN_SIZE(sizeof(struct memp)) + MEMP_SANITY_REGION_BEFORE_ALIGNED)
#define MEMP_ALIGN_SIZE(x) (LWIP_MEM_ALIGN_SIZE(x) + MEMP_SANITY_REGION_AFTER_ALIGNED)

#else /* MEMP_OVERFLOW_CHECK */

/* No sanity checks
 * We don't need to preserve the struct memp while not allocated, so we
 * can save a little space and set MEMP_SIZE to 0.
 */
#define MEMP_SIZE           0
#define MEMP_ALIGN_SIZE(x) (LWIP_MEM_ALIGN_SIZE(x))

#endif /* MEMP_OVERFLOW_CHECK */











//记录每一类型的内存池大小 存在字节对齐说明最小存储单元为对齐数 最重要的理解 说明数据空闲时当作为链表节 一段内存既可以存数据也可以是链表
const u16_t memp_sizes[MEMP_MAX] = {
#define LWIP_MEMPOOL(name,num,size,desc)  LWIP_MEM_ALIGN_SIZE(size),
#include "pool_std.h"
};

//记录每一类型的内存池的数量
static const u16_t memp_num[MEMP_MAX] = {
#define LWIP_MEMPOOL(name,num,size,desc)  (num),
#include "pool_std.h"
};

//记录各类型空闲内存池的首地址
static struct memp *memp_tab[MEMP_MAX];





#if MEMP_SEPARATE_POOLS //如果需要为各个内存池提供定义

/** This creates each memory pool. These are named memp_memory_XXX_base (where
 * XXX is the name of the pool defined in memp_std.h).
 * To relocate a pool, declare it as extern in cc.h. Example for GCC:
 *   extern u8_t __attribute__((section(".onchip_mem"))) memp_memory_UDP_PCB_base[];
 */
#define LWIP_MEMPOOL(name,num,size,desc) u8_t memp_memory_ ## name ## _base \
  [((num) * (MEMP_SIZE + MEMP_ALIGN_SIZE(size)))];   
#include "pool_std.h"

/** This array holds the base of each memory pool. */
static u8_t *const memp_bases[] = { 
#define LWIP_MEMPOOL(name,num,size,desc) memp_memory_ ## name ## _base,   
#include "pool_std.h"
};

#else /* MEMP_SEPARATE_POOLS */ //否则使用单一内存池
/** This is the actual memory used by the pools (all pools in one big block). */
static u8_t memp_memory[MEM_ALIGNMENT - 1 
#define LWIP_MEMPOOL(name,num,size,desc) + ( (num) * (MEMP_SIZE + MEMP_ALIGN_SIZE(size) ) )
#include "pool_std.h"
];

static const int memp_memory_size = sizeof(memp_memory);
//拿到尺寸变量便于使用 需要MEM_ALIGNMENT - 1是为了对memp_memory进行对齐后依然不越界

#endif /* MEMP_SEPARATE_POOLS */


//如果使用了debug 各类型内存池的描述信息
#ifdef LWIP_DEBUG
static const char *memp_desc[MEMP_MAX] = {
#define LWIP_MEMPOOL(name,num,size,desc)  (desc),
#include "pool_std.h"
};
#endif /* LWIP_DEBUG */








#if MEMP_SANITY_CHECK
/**
 * Check that memp-lists don't form a circle, using "Floyd's cycle-finding algorithm".
 */
static int
memp_sanity(void)
{
  s16_t i;
  struct memp *t, *h;

  for (i = 0; i < MEMP_MAX; i++) {
    t = memp_tab[i];
    if(t != NULL) {
      for (h = t->next; (t != NULL) && (h != NULL); t = t->next,
        h = (((h->next != NULL) && (h->next->next != NULL)) ? h->next->next : NULL)) {
        if (t == h) {//这个应该是检查是不是循环链表
          return 0;
        }
      }
    }
  }
  return 1;
}
#endif /* MEMP_SANITY_CHECK*/


#if MEMP_OVERFLOW_CHECK
#if defined(LWIP_DEBUG) && MEMP_STATS
static const char * memp_overflow_names[] = {
#define LWIP_MEMPOOL(name,num,size,desc) "/"desc,
#include "pool_std.h"
};
#endif

/**
 * Check if a memp element was victim of an overflow
 * (e.g. the restricted area after it has been altered)
 *
 * @param p the memp element to check
 * @param memp_type the pool p comes from
 */
static void
memp_overflow_check_element_overflow(struct memp *p, u16_t memp_type)
{
  u16_t k;
  u8_t *m;
#if MEMP_SANITY_REGION_AFTER_ALIGNED > 0
  m = (u8_t*)p + MEMP_SIZE + memp_sizes[memp_type];
  for (k = 0; k < MEMP_SANITY_REGION_AFTER_ALIGNED; k++) {
    if (m[k] != 0xcd) {
      char errstr[128] = "detected memp overflow in pool ";
      char digit[] = "0";
      if(memp_type >= 10) {
        digit[0] = '0' + (memp_type/10);
        strcat(errstr, digit);
      }
      digit[0] = '0' + (memp_type%10);
      strcat(errstr, digit);
#if defined(LWIP_DEBUG) && MEMP_STATS
      strcat(errstr, memp_overflow_names[memp_type]);
#endif
      LWIP_ASSERT(errstr, 0);
    }
  }
#endif
}

/**
 * Check if a memp element was victim of an underflow
 * (e.g. the restricted area before it has been altered)
 *
 * @param p the memp element to check
 * @param memp_type the pool p comes from
 */
static void
memp_overflow_check_element_underflow(struct memp *p, u16_t memp_type)
{
  u16_t k;
  u8_t *m;
#if MEMP_SANITY_REGION_BEFORE_ALIGNED > 0
  m = (u8_t*)p + MEMP_SIZE - MEMP_SANITY_REGION_BEFORE_ALIGNED;
  for (k = 0; k < MEMP_SANITY_REGION_BEFORE_ALIGNED; k++) {
    if (m[k] != 0xcd) {
      char errstr[128] = "detected memp underflow in pool ";
      char digit[] = "0";
      if(memp_type >= 10) {
        digit[0] = '0' + (memp_type/10);
        strcat(errstr, digit);
      }
      digit[0] = '0' + (memp_type%10);
      strcat(errstr, digit);
#if defined(LWIP_DEBUG) && MEMP_STATS
      strcat(errstr, memp_overflow_names[memp_type]);
#endif
      LWIP_ASSERT(errstr, 0);
    }
  }
#endif
}




/**
 * Do an overflow check for all elements in every pool.
 *
 * @see memp_overflow_check_element for a description of the check
 */
static void
memp_overflow_check_all(void)
{
  u16_t i, j;
  struct memp *p;

  p = (struct memp *)LWIP_MEM_ALIGN(memp_memory);
  for (i = 0; i < MEMP_MAX; ++i) {
    p = p;
    for (j = 0; j < memp_num[i]; ++j) {
      memp_overflow_check_element_overflow(p, i);
      p = (struct memp*)((u8_t*)p + MEMP_SIZE + memp_sizes[i] + MEMP_SANITY_REGION_AFTER_ALIGNED);
    }
  }
  p = (struct memp *)LWIP_MEM_ALIGN(memp_memory);
  for (i = 0; i < MEMP_MAX; ++i) {
    p = p;
    for (j = 0; j < memp_num[i]; ++j) {
      memp_overflow_check_element_underflow(p, i);
      p = (struct memp*)((u8_t*)p + MEMP_SIZE + memp_sizes[i] + MEMP_SANITY_REGION_AFTER_ALIGNED);
    }
  }
}

/**
 * Initialize the restricted areas of all memp elements in every pool.
 */
static void
memp_overflow_init(void)
{
  u16_t i, j;
  struct memp *p;
  u8_t *m;

  p = (struct memp *)LWIP_MEM_ALIGN(memp_memory);//从此函数看出仅仅在使用单一内存空间时 即 MEMP_SEPARATE_POOLS=0时使用单一内存池
  for (i = 0; i < MEMP_MAX; ++i) {
    p = p;
    for (j = 0; j < memp_num[i]; ++j) {
#if MEMP_SANITY_REGION_BEFORE_ALIGNED > 0
      m = (u8_t*)p + MEMP_SIZE - MEMP_SANITY_REGION_BEFORE_ALIGNED;//头部偏移LWIP_MEM_ALIGN_SIZE(sizeof(struct memp))
      memset(m, 0xcd, MEMP_SANITY_REGION_BEFORE_ALIGNED);
#endif
#if MEMP_SANITY_REGION_AFTER_ALIGNED > 0
      m = (u8_t*)p + MEMP_SIZE + memp_sizes[i];//尾部距离偏移
      memset(m, 0xcd, MEMP_SANITY_REGION_AFTER_ALIGNED);
#endif
      p = (struct memp*)((u8_t*)p + MEMP_SIZE + memp_sizes[i] + MEMP_SANITY_REGION_AFTER_ALIGNED);//每个元素更迭
    }
  }
}
#endif /* MEMP_OVERFLOW_CHECK */
































//声明放这里 因为使用的是三方API
static void memp_init(void);
#if MEMP_OVERFLOW_CHECK
static void *memp_malloc_fn(memp_t type, const char* file, const int line);
#define memp_malloc(t) memp_malloc_fn((t), __FILE__, __LINE__)
#else
static void *memp_malloc(memp_t type);
#endif
static void  memp_free(memp_t type, void *mem);


















/**
 * Initialize this module.
 * 
 * Carves out memp_memory into linked lists for each pool-type.
 */
static void
memp_init(void)
{
  struct memp *memp;
  u16_t i, j;

  /*
  for (i = 0; i < MEMP_MAX; ++i) { LWIP中状态标志 这里不用管
    MEMP_STATS_AVAIL(used, i, 0);
    MEMP_STATS_AVAIL(max, i, 0);
    MEMP_STATS_AVAIL(err, i, 0);
    MEMP_STATS_AVAIL(avail, i, memp_num[i]);
  }
  */
  //有说法是在这里将内存池数据初始化为0 不再这里初始化值可以重复调用memp_init恢复链表能力
  //但是申请后得到的内存一般都自行先初始化 所以也没有必要重新初始化数据



#if !MEMP_SEPARATE_POOLS
  memp = (struct memp *)LWIP_MEM_ALIGN(memp_memory);//这里是使用统一的内存
#endif /* !MEMP_SEPARATE_POOLS */
  /* for every pool: */
  for (i = 0; i < MEMP_MAX; ++i) {
    memp_tab[i] = NULL;                             //初始化各内存的末端 全部指向NULL
#if MEMP_SEPARATE_POOLS
    memp = (struct memp*)memp_bases[i];             //使用独立的内存
#endif /* MEMP_SEPARATE_POOLS */
    /* create a linked list of memp elements */
    for (j = 0; j < memp_num[i]; ++j) {
      memp->next = memp_tab[i];                     //连接同类型内存池为一个空闲链表，并将表头存放在memp_tab中
      memp_tab[i] = memp;                           //看样子初始化的表头在数组的末尾
      memp = (struct memp *)(void *)((u8_t *)memp + MEMP_SIZE + memp_sizes[i]
#if MEMP_OVERFLOW_CHECK
        + MEMP_SANITY_REGION_AFTER_ALIGNED
#endif
      );
    }
  }

#if MEMP_OVERFLOW_CHECK             //加入安全机制 前提使用单一内存池
  memp_overflow_init();             //安全机制的初始化
  /* check everything a first time to see if it worked */
  memp_overflow_check_all();        //检测是否存在溢出(包括上溢和下溢)
#endif /* MEMP_OVERFLOW_CHECK */
}








/**
 * Get an element from a specific pool.
 *
 * @param type the pool to get an element from
 *
 * the debug version has two more parameters:
 * @param file file name calling this function
 * @param line number of line where this function is called
 *
 * @return a pointer to the allocated memory or a NULL pointer on error
 */
static void *
#if !MEMP_OVERFLOW_CHECK
memp_malloc(memp_t type)
#else
memp_malloc_fn(memp_t type, const char* file, const int line)
#endif
{
  struct memp *memp;
  //SYS_ARCH_DECL_PROTECT(old_level);进入临界区
 
  //LWIP_ERROR("memp_malloc: type < MEMP_MAX", (type < MEMP_MAX), return NULL;);不存在此类型内存池 返回null;
  if (type < MEMP_MAX)//等价以上
  {
    //合法
  }else{
      PRINTF("memp_malloc: type >= MEMP_MAX;\r\n");//不存在内存池
      return NULL;
  }


  //SYS_ARCH_PROTECT(old_level);//处于临界区中
#if MEMP_OVERFLOW_CHECK >= 2
  memp_overflow_check_all();            //这里默认MEMP_OVERFLOW_CHECK>=2将对所有内存池进行溢出检查
#endif /* MEMP_OVERFLOW_CHECK >= 2 */

  memp = memp_tab[type];
  
  if (memp != NULL) {
    memp_tab[type] = memp->next;
#if MEMP_OVERFLOW_CHECK
    memp->next = NULL;
    memp->file = file;
    memp->line = line;
#endif /* MEMP_OVERFLOW_CHECK */
    //MEMP_STATS_INC_USED(used, type);
    LWIP_ASSERT("memp_malloc: memp properly aligned",
                ((mem_ptr_t)memp % MEM_ALIGNMENT) == 0);

    memp = (struct memp*)(void *)((u8_t*)memp + MEMP_SIZE);
  } else {
    //LWIP_DEBUGF(MEMP_DEBUG | LWIP_DBG_LEVEL_SERIOUS, ("memp_malloc: out of memory in pool %s\n", memp_desc[type]));
    PRINTF("memp_malloc: out of memory in pool %s\n", memp_desc[type]);
    //MEMP_STATS_INC(err, type);
  }

  //SYS_ARCH_UNPROTECT(old_level);离开临界区

  return memp;
}

/**
 * Put an element back into its pool.
 *
 * @param type the pool where to put mem
 * @param mem the memp element to free
 */
static void
memp_free(memp_t type, void *mem)
{
  struct memp *memp;
  //SYS_ARCH_DECL_PROTECT(old_level);进入临界区

  if (mem == NULL) {
    return;
  }
  LWIP_ASSERT("memp_free: mem properly aligned",
                ((mem_ptr_t)mem % MEM_ALIGNMENT) == 0);

  memp = (struct memp *)(void *)((u8_t*)mem - MEMP_SIZE);

  //SYS_ARCH_PROTECT(old_level);处于临界区中
#if MEMP_OVERFLOW_CHECK
#if MEMP_OVERFLOW_CHECK >= 2
  memp_overflow_check_all();
#else
  memp_overflow_check_element_overflow(memp, type);
  memp_overflow_check_element_underflow(memp, type);
#endif /* MEMP_OVERFLOW_CHECK >= 2 */
#endif /* MEMP_OVERFLOW_CHECK */

  //MEMP_STATS_DEC(used, type);
  
  memp->next = memp_tab[type]; 
  memp_tab[type] = memp;

#if MEMP_SANITY_CHECK //开启环路检查 这个应该是检查memp是不是循环链表 初始化算法中不是循环链表 可以常为0
  LWIP_ASSERT("memp sanity", memp_sanity());
#endif /* MEMP_SANITY_CHECK */

  //SYS_ARCH_UNPROTECT(old_level);离开临界区
}



void cp_memp_memory_reset0(void){
    //有说法是在这memp_init将内存池数据初始化为0 不再memp_init初始化值可以重复调用memp_init恢复链表能力
    //但是申请后得到的内存一般都自行先初始化 所以也没有必要重新初始化数据
    int i;
#if MEMP_SEPARATE_POOLS
  for (i = 0; i < MEMP_MAX; ++i) {
      u8_t * p = memp_bases[i];
      for (j = 0; j < memp_num[i]; ++j){
          p[j]=0;
      }
  }
#else
  for (i = 0; i < memp_memory_size; ++i) {
      memp_memory[i]=0;
  }
#endif
}

void cp_memp_init(void){
    memp_init();
}

void memp_desc_printf(void){
    int i;
    int memp_memory_sizes=MEM_ALIGNMENT - 1;

    PRINTF("MEMP_SEPARATE_POOLS:%d;\r\n",MEMP_SEPARATE_POOLS);//常为0，不为各个内存池提供定义
    PRINTF("MEMP_OVERFLOW_CHECK:%d;\r\n",MEMP_OVERFLOW_CHECK);//开启检查超级浪费内存


    PRINTF("MEMP_MAX:%d, memp=%d, memp_memory_size=%d;\r\n",MEMP_MAX,(int)(struct memp *)LWIP_MEM_ALIGN(memp_memory),memp_memory_size);
    for (i = 0; i < MEMP_MAX; ++i) {
        PRINTF("index:%d, desc:%s, num=%d, size:%d;\r\n",i,memp_desc[i],memp_num[i],memp_sizes[i]);
        memp_memory_sizes = memp_memory_sizes + ( (memp_num[i]) * (MEMP_SIZE + MEMP_ALIGN_SIZE(memp_sizes[i]) ) );
    }
    //应该存在memp_memory_size = memp_memory_sizes
    PRINTF("memp_memory_size=%d, memp_memory_sizes=%d;\r\n",memp_memory_size,memp_memory_sizes);
}


void *cp_memp_malloc(memp_t type){
    return memp_malloc(type);
}

void  cp_memp_free(memp_t type, void *mem){
    return memp_free(type, mem);
}


