#ifndef PAUBROKER_H
#define PAUBROKER_H
#ifdef __cplusplus
extern "C"
{
#endif
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <stdarg.h>
#include <ctype.h>
#include "inttypes.h"
#include "math.h"
#include <float.h>
#include <limits.h>
#include "pau_vector.h"

#define MAXNODES_MEM_LMT PAU_VECTOR_DEFAULT_CAPACITY
#define MAX_MODULES_PER_NODE 3
#define RATED_PWR_PER_MODULE 625

#define SIZING_TOLERANCE 1 * 10 // 2kW  匹配模块个数的容差 如果模块RATED_PWR_PER_MODULE是40kW 则79kW匹配2模块 80kW匹配3模块
    typedef enum
    {
        FullMatrix,
        CakraWheel,
        SemiHybrid,
    } TOPOTYPE;
    typedef enum
    {
        PRIOR_VAIN = 0, // 没有置优先级=枪没有在工作
        PRIOR_BASE,     // 会被PRIOR_SVIP褫夺功率配额
        PRIOR_VIP,      // 不会被PRIOR_SVIP抢褫夺功率配额,会分摊PRIOR_BASE的功率配额
        PRIOR_SVIP,     // 会褫夺PRIOR_BASE的功率配合
        PRIOR_EXTREME,  // 会褫夺所有非PRIO_EXTREME功率配合
    } PRIOR;
    typedef enum
    {
        NODE_IDLEFREE = 0, // plugid ×  available √
        NODE_OCCUPIED,     // plugid √  available √
        NODE_DISABLED,     // plugid ×  available ×
        NODE_OUTORDER,     // plugid √  available ×
    } NodeState;

    // 充电桩状态
    typedef enum
    {
        PLUG_IDLE = 0,
        PLUG_CHARGING
    } PlugState;

    /*
    ******************************************************************************
    * Definitely Needed Macros
    ******************************************************************************/
    // Declare the related symbols exported from ICF
    extern const unsigned char *const __PAU_CORE_RAM_start__;
    extern const unsigned char *const __PAU_CORE_RAM_end__;
    extern const unsigned char *const __PAU_CORE_HEAP_start__;
    extern const unsigned char *const __PAU_CORE_HEAP_end__;
    extern const unsigned int __PAU_CORE_RAM_size__;
    extern const unsigned int __PAU_CORE_HEAP_size__;
#if defined(__IAR_SYSTEMS_ICC__)
#define IN_PAU_RAM_SECTION __attribute__((section(".pau_ram_section"), aligned(4)))
#define IN_PAU_HEAP_SECTION __attribute__((section(".pau_heap_section")))
#define pau_printf(fmt, ...) d_printf("\033[33m" fmt "\033[0m", ##__VA_ARGS__)
#else
#define IN_PAU_RAM_SECTION
#define IN_PAU_HEAP_SECTION
void pau_log_printf(const char *fmt, ...);
#define pau_printf(fmt, ...) pau_log_printf("\033[33m" fmt "\033[0m", ##__VA_ARGS__)
#endif
#define IN_PCU_RAM_SECTION

#define RAM_CAPACITY (__PAU_CORE_RAM_size__ / sizeof(ptrdiff_t))
    typedef enum
    {
        SRAM_SPAN = 0,
        HEAP_SPAN,
        INIT_SPAN,
        RATE_SPAN
    } PAU_MALLOC_CURSE;
// magic word,The canary value is set at the buffer edge; put simply, it detects memory overflow in advance
#define FRONT_MAGICWORD 0xDEADCAFEu
#define REAR_MAGICWORD 0xBABEFACEu
#define ID_VAIN 0
    typedef union
    {
        ID_TYPE modules[MAX_MODULES_PER_NODE + 1]; // size + 节点包含的模块编号
        struct
        {
            size_t size;
            ID_TYPE _pad[MAX_MODULES_PER_NODE];
        };
    } ModuleBox;
    struct Alloc_nodeObj
    {
        ID_TYPE id;
        ID_TYPE plug_id;
        PRIOR priority;
        int power_available;
        ModuleBox moudle_box;
        NodeState state;
    };

    struct Alloc_plugObj
    {
        ID_TYPE id;
        PRIOR priority;        // 优先级 (1-4)
        PlugState state;       // 充电桩状态
        ID_TYPE connectedNode; // 直接的节点ID
        size_t requiredPower;  // 需求功率 (kW)
        int shortage;          // 需求欠额节点数
        bool refresh;
        bool sequent;
        size_t hysteresisCnt;       // 功率回差迟滞计数器
        PAU_Vector *allocatedNodes; // 已分配的节点
    };

    struct Alloc_contactorObj
    {
        ID_TYPE id;    // 接触器ID
        ID_TYPE node1; // 连接的节点1
        ID_TYPE node2; // 连接的节点2
        bool isClosed; // 是否闭合
    };
    struct Tactic_ReqCurrentObj
    {
#define DEMAND_CURRENT_SAMPLENUM 6
        size_t index;
        int counter;
        size_t func_reentry_cntr;
        float sample_current_pool[DEMAND_CURRENT_SAMPLENUM];
    };
    typedef struct
    {
        uint32_t front_canary; // absolutely required to be top element
        size_t length;
        size_t unitpower;
        size_t circulo;
        TOPOTYPE topology;
        struct Alloc_nodeObj obj_array[]; // __attribute__((counted_by(length)));
    } Alloc_NodesArray;

    typedef struct
    {
        uint32_t front_canary; // absolutely required to be top element
        size_t length;
        struct Alloc_plugObj obj_array[]; // __attribute__((counted_by(length)));
    } Alloc_PlugsArray;

    typedef struct
    {
        uint32_t front_canary; // absolutely required to be top element
        size_t length;
        struct Alloc_contactorObj obj_array[]; // __attribute__((counted_by(length)));
    } Alloc_ContactorsArray;
    typedef struct
    {
        uint32_t front_canary; // absolutely required to be top element
        size_t length;
        struct Tactic_ReqCurrentObj obj_array[]; // __attribute__((counted_by(length)));
    } Alloc_ReqSettlerArray;
#define VARIABLE_LIST_PENDING_EXPANDED       \
    CONTEXT_EXPANDER_PROJ_GLOBAL(Nodes)      \
    CONTEXT_EXPANDER_PROJ_GLOBAL(Contactors) \
    CONTEXT_EXPANDER_PROJ_GLOBAL(ReqSettler) \
    CONTEXT_EXPANDER_PROJ_GLOBAL(Plugs)

// cstat -DEFINE-hash-multiple
#define VAR_NAME(x) gp##x##Array
#define VAR_TYPE(x) Alloc_##x##Array
// cstat +DEFINE-hash-multiple
#ifdef __IMPORT_GLOBALVAR__
#define CONTEXT_EXPANDER_PROJ_GLOBAL(x) VAR_TYPE(x) * VAR_NAME(x) IN_PAU_RAM_SECTION = NULL;
    // here，Defined PAU's only 7 global variables (gpxxxArray) via metaprogramming templates
    VARIABLE_LIST_PENDING_EXPANDED
#undef CONTEXT_EXPANDER_PROJ_GLOBAL
#else
#define CONTEXT_EXPANDER_PROJ_GLOBAL(x) extern VAR_TYPE(x) * VAR_NAME(x);
VARIABLE_LIST_PENDING_EXPANDED
#undef CONTEXT_EXPANDER_PROJ_GLOBAL
#endif // __IMPORT_GLOBALVAR__

#define PLUG_MAX (gpPlugsArray->length)
#define NODE_MAX (gpNodesArray->length)
#define CONTACTOR_MAX (gpContactorsArray->length)
#define UNITPWR_MAX (gpNodesArray->unitpower)
#define TOPOLOGY_TYPE (gpNodesArray->topology)
#define NODES_MAX_ENCIRCLE (gpNodesArray->circulo)
#define ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX (NODES_MAX_ENCIRCLE != NODE_MAX)
#define ASSERT_TOPOTYPE_WHEEL_UNMIXED_SIMPLEX (NODES_MAX_ENCIRCLE == NODE_MAX)
#define CONTACTOR_SPLICE_MULTIPLE 100
#define ASSERT_NODE_ID(id) ((id) <= NODE_MAX && (id) > ID_VAIN)
#define ASSERT_NODE_ID_ENCIRCLE(id) ((id) <= NODES_MAX_ENCIRCLE && (id) > ID_VAIN)
#define ASSERT_PLUG_ID(id) ((id) <= PLUG_MAX && (id) > ID_VAIN)
#define ASSERT_CONTACTOR_ID(id) ((id) <= CONTACTOR_MAX && (id) > ID_VAIN)
#define FORCE_INLINE __attribute__(always_inline)

#ifdef __IMPORT_PAU_DBFUNC__
    void *
    pau_alloc(size_t size, PAU_MALLOC_CURSE span)
    {
#define ALIGNMENT_SIZE sizeof(size_t)
#define ALIGN_SIZE(s) (((s) + ALIGNMENT_SIZE - 1) & ~(ALIGNMENT_SIZE - 1))
        static uint8_t custom_mem_pool[CUSTOM_HEAP_SIZE] IN_PAU_HEAP_SECTION = {sizeof(size_t)};
        static size_t *custom_mem_offset = (size_t *)custom_mem_pool;
        size = ALIGN_SIZE(size);
        if (*custom_mem_offset + size > CUSTOM_HEAP_SIZE)
        {
            return NULL;
        }
        if (SRAM_SPAN == span)
        {
            void *new_ptr = (void *)(custom_mem_pool + *custom_mem_offset);
            *custom_mem_offset += size;
            memset(new_ptr, 0, size);
            // pau_printf("%s: %p\r\n", "SRAM_SPAN", new_ptr);
            return new_ptr;
        }
        else if (HEAP_SPAN == span)
        {
            // 在分区中逐个找到PAU_Vector大小的空闲块，检查vec->data[0]是否为VACANT魔数或0，如果是则分配成功并返回指针，否则继续寻找，直到找到或遍历完整个分区
            void *new_ptr = NULL;
            ptrdiff_t step = sizeof(PAU_Vector) + sizeof(size_t) * (1 + PAU_VECTOR_DEFAULT_CAPACITY);
            for (ptrdiff_t offset = *custom_mem_offset; offset + step <= CUSTOM_HEAP_SIZE; offset += step)
            {
                if (((PAU_Vector *)(custom_mem_pool + offset))->data[0] == 0)
                {
                    new_ptr = (void *)(custom_mem_pool + offset);
                    memset(new_ptr, 0, size);
                    break;
                }
            }
            // pau_printf("%s: %p \r\n", "HEAP_SPAN", new_ptr);
            return new_ptr;
        }
        else if (INIT_SPAN == span)
        {
            *custom_mem_offset = sizeof(size_t);
        }
        else if (RATE_SPAN == span)
        {
            pau_printf("PAU_HEAP_USAGE: %.2f%% %p/%p\r\n", (float)(*custom_mem_offset) / CUSTOM_HEAP_SIZE * 100, *custom_mem_offset, CUSTOM_HEAP_SIZE);
        }
        return NULL;
    }
    void *pau_calloc(size_t size, const char *func_name)
    {
        if (0 == size)
        {
            return pau_alloc(size, INIT_SPAN);
        }
        if (0 == strncmp(func_name, "pau_vector_create", 17))
        {
            return pau_alloc(size, HEAP_SPAN);
        }
        else
        {
            return pau_alloc(size, SRAM_SPAN);
        }
    }

    struct Alloc_nodeObj *refer_Node_Extracted(ID_TYPE node)
    {
        if (!ASSERT_NODE_ID(node))
        {
            return NULL;
        }
        else
        {
            return (gpNodesArray->obj_array + node);
        }
    }

    struct Alloc_plugObj *refer_Plug_Extracted(ID_TYPE plug)
    {
        if (!ASSERT_PLUG_ID(plug))
        {
            return NULL;
        }
        else
        {
            return (gpPlugsArray->obj_array + plug);
        }
    }

    struct Alloc_contactorObj *refer_Contactor_Extracted(ID_TYPE contactor)
    {
        if (!ASSERT_CONTACTOR_ID(contactor))
        {
            return NULL;
        }
        else
        {
            return (gpContactorsArray->obj_array + contactor);
        }
    }
    struct Tactic_ReqCurrentObj *refer_ReqSettler_Extracted(ID_TYPE plug)
    {
        if (!ASSERT_PLUG_ID(plug))
        {
            return NULL;
        }
        else
        {
            return (gpReqSettlerArray->obj_array + plug);
        }
    }
#else
void *
pau_calloc(size_t size, const char *func_name);
struct Alloc_nodeObj *refer_Node_Extracted(ID_TYPE node);
struct Alloc_plugObj *refer_Plug_Extracted(ID_TYPE plug);
struct Alloc_contactorObj *refer_Contactor_Extracted(ID_TYPE contactor);
struct Tactic_ReqCurrentObj *refer_ReqSettler_Extracted(ID_TYPE plug);
#endif // __IMPORT_PAU_DBFUNC__
    void directedConfig_Init(ID_TYPE nodes, ID_TYPE plugs);
#ifdef __cplusplus
}
#endif // __cplusplus
#endif // PAUBROKER_H
