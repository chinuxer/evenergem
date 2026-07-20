/**
 ******************************************************************************
 Copyright(c) Infy Power 2026-2026
  @file    pau_broker.c
  @author  YBA40320
  @version V1.0
  @date    2026-04-27
  @brief   数据结构在内存中的代理,实现对节点,充电桩,接触器等数据的访问和修改
  @history 2026-04-27 YBA40320 创建;2026-05-15 YBA40320 从模拟机移植到A2605线环1500kW工程
  @details

 ************************************************************************************************************************************************************************/

#define __IMPORT_GLOBALVAR__
#define __IMPORT_PAU_DBFUNC__
#include "pau_broker.h"
/*
 * @brief Create a common constructor method for flexible arrays.
 */
#define CREATE_FLEXSTRUCT_ARRAY(type, count) \
    (type *)create_FlexStruct_Array(sizeof(type), sizeof(((type *)0)->obj_array[0]), (count), type##_Init)

#define IS_FRONT_CANARY_INTACT(ptr, type) \
    (NULL != (ptr) ? (((const type *)(ptr))->front_canary == (FRONT_MAGICWORD)) : false)
#define GET_REAR_CANARY_PTR(ptr, type) \
    ((uint32_t *)((char *)(ptr) + sizeof(type) + ((ptr)->length + 1) * sizeof(((type *)0)->obj_array[0])))
#define IS_REAR_CANARY_INTACT(ptr, type) \
    (NULL != (ptr) ? (*GET_REAR_CANARY_PTR(ptr, type) == (REAR_MAGICWORD)) : false)

int oprt_ratedpwr_per_module(int rated_pwr)
{
    static int g_rated_pwr_per_module IN_PAU_RAM_SECTION = 0;
    if (rated_pwr > 0)
    {
        g_rated_pwr_per_module = rated_pwr;
    }

    return g_rated_pwr_per_module;
}
static void availablePwr_Init(struct Alloc_nodeObj *pnode, int rated_pwr)
{
    pnode->power_available = rated_pwr * (pnode->moudle_box.size);
}
static void modulesPerNode_Init(struct Alloc_nodeObj *pnode)
{
#include "module_config.h"
    if (pnode->id < 1 || pnode->id > (sizeof(module_nbr_map) / sizeof(module_nbr_map[0])))
    {
        pnode->moudle_box.size = 0;
        return;
    }
    pnode->moudle_box.size = module_nbr_map[pnode->id - 1];
}
ID_TYPE calc_plug_connectednode(ID_TYPE plugid, ID_TYPE nodes_total, ID_TYPE plugs_total)
{
    ID_TYPE nodeid = 1 + (plugid - 1) * (nodes_total / plugs_total);
    return nodeid > nodes_total ? nodes_total : nodeid;
}
static size_t factorial(ID_TYPE n)
{
    size_t res = 0;

    for (int i = 1; i <= n; i++)
    {
        res += i;
    }
    return res;
}
static void Alloc_NodesArray_Init(void *const ptr, size_t n)
{
    if (!(bool)ptr)
    {
        return;
    }
    // cstat #CERT-EXP36-C_a #CERT-EXP36-C_b #CERT-EXP39-C_d
    Alloc_NodesArray *p = (Alloc_NodesArray *)ptr;
    p->length = n;
    p->unitpower = oprt_ratedpwr_per_module(0);
    p->circulo = n;
    p->front_canary = FRONT_MAGICWORD;
    for (ID_TYPE i = 1; i <= n; i++)
    {
        p->obj_array[i].id = i;
        p->obj_array[i].state = NODE_IDLE;
        p->obj_array[i].priority = PRIOR_VAIN;
        p->obj_array[i].plug_id = ID_VAIN;
        modulesPerNode_Init(&p->obj_array[i]);
        availablePwr_Init(&p->obj_array[i], p->unitpower);
    }
    *GET_REAR_CANARY_PTR(p, Alloc_NodesArray) = REAR_MAGICWORD;
}
static void Alloc_PlugsArray_Init(void *const ptr, size_t n)
{
    if (!(bool)ptr)
    {
        return;
    }
    // cstat #CERT-EXP36-C_a #CERT-EXP36-C_b #CERT-EXP39-C_d
    Alloc_PlugsArray *p = (Alloc_PlugsArray *)ptr;
    p->length = n;
    p->front_canary = FRONT_MAGICWORD;
    for (ID_TYPE i = 1; i <= n; i++)
    {
        p->obj_array[i].priority = PRIOR_VAIN;
        p->obj_array[i].state = PLUG_IDLE;
        p->obj_array[i].id = i;
        if (ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX)
        {
            p->obj_array[i].connectedNode = calc_plug_connectednode(i, NODES_MAX_ENCIRCLE, n);
        }
        else
        {
            p->obj_array[i].connectedNode = calc_plug_connectednode(i, NODE_MAX, n);
        }
        p->obj_array[i].requiredPower = 0;
        p->obj_array[i].hysteresisCnt = 0;
        p->obj_array[i].refresh = false;
        p->obj_array[i].allocatedNodes = (PAU_Vector *)pau_calloc(sizeof(PAU_Vector) + (PAU_VECTOR_DEFAULT_CAPACITY + 1) * sizeof(size_t), __func__);
        if (NULL != p->obj_array[i].allocatedNodes)
        {
            p->obj_array[i].allocatedNodes->data[0] = PAU_VECTOR_DEFAULT_CAPACITY;
            pau_vector_clear(p->obj_array[i].allocatedNodes);
        }
        p->obj_array[i].disabledNodes = (PAU_Vector *)pau_calloc(sizeof(PAU_Vector) + (PAU_VECTOR_DEFAULT_CAPACITY + 1) * sizeof(size_t), __func__);
        if (NULL != p->obj_array[i].disabledNodes)
        {
            p->obj_array[i].disabledNodes->data[0] = PAU_VECTOR_DEFAULT_CAPACITY;
            pau_vector_clear(p->obj_array[i].disabledNodes);
        }
    }

    *GET_REAR_CANARY_PTR(p, Alloc_PlugsArray) = REAR_MAGICWORD;
}
static void Alloc_ContactorsArray_Init(void *const ptr, size_t n)
{

    if (!(bool)ptr)
    {
        return;
    }
    // cstat #CERT-EXP36-C_a #CERT-EXP36-C_b #CERT-EXP39-C_d
    Alloc_ContactorsArray *p = (Alloc_ContactorsArray *)ptr;
    p->length = n;
    p->front_canary = FRONT_MAGICWORD;
    // 创建环形接触器链路
    size_t initcnt = n;

    if (initcnt != 2 * NODE_MAX || 0 != NODE_MAX % 2)
    {
        NODES_MAX_ENCIRCLE = 2 * NODE_MAX / 3;
    }
    n = NODES_MAX_ENCIRCLE;
    for (ID_TYPE i = 1; i <= n; i++)
    {
        p->obj_array[i].id = i;
        p->obj_array[i].isClosed = false;
        p->obj_array[i].node1 = i;
        p->obj_array[i].node2 = i + 1 > n ? 1 : i + 1;
    }
    // 创建对角线接触器链路
    for (ID_TYPE i = 1; i <= n; i++)
    {
        p->obj_array[n + i].id = n + i;
        p->obj_array[n + i].isClosed = false;
        p->obj_array[n + i].node1 = i;
        p->obj_array[n + i].node2 = i + n / 2 > n ? i + n / 2 - n : i + n / 2;
    }
    // 创建半矩阵节点的接触器链路
    if (initcnt != 2 * NODE_MAX || 0 != NODE_MAX % 2) // nodes 12 + matrix 6 = 18  4*18/3 = 2*12
    {
        size_t contactorIdx = 2 * NODES_MAX_ENCIRCLE + 1; // 从25开始
        n = NODES_MAX_ENCIRCLE / 2;
        for (ID_TYPE node1 = 1; node1 <= n && contactorIdx <= initcnt; node1++)
        {
            for (ID_TYPE node2 = node1 + 1; node2 <= n && contactorIdx <= initcnt; node2++)
            {
                p->obj_array[contactorIdx].id = contactorIdx;
                p->obj_array[contactorIdx].isClosed = false;
                p->obj_array[contactorIdx].node1 = node1 + NODES_MAX_ENCIRCLE;
                p->obj_array[contactorIdx].node2 = node2 + NODES_MAX_ENCIRCLE;

                contactorIdx++;
            }
        }
        for (ID_TYPE node1 = 1; node1 <= n && contactorIdx <= initcnt; node1++)
        {
            p->obj_array[contactorIdx].id = contactorIdx;
            p->obj_array[contactorIdx].isClosed = false;
            p->obj_array[contactorIdx].node1 = node1 + NODES_MAX_ENCIRCLE;
            p->obj_array[contactorIdx].node2 = node1 * CONTACTOR_SPLICE_MULTIPLE + node1 + n;
            contactorIdx++;
        }
        // for (ID_TYPE i = 1; i < contactorIdx; i++)
        // {
        //     pau_printf("contactor %d: node1=%d, node2=%d\r\n", p->obj_array[i].id, p->obj_array[i].node1, p->obj_array[i].node2);
        // }
    }
    // 打印所有接触器的连接点

    *GET_REAR_CANARY_PTR(p, Alloc_ContactorsArray) = REAR_MAGICWORD;
}

static void Alloc_ReqSettlerArray_Init(void *const ptr, size_t n)
{
    if (!(bool)ptr)
    {
        return;
    }
    Alloc_ReqSettlerArray *p = (Alloc_ReqSettlerArray *)ptr;
    p->length = n;
    p->front_canary = FRONT_MAGICWORD;
    *GET_REAR_CANARY_PTR(p, Alloc_ReqSettlerArray) = REAR_MAGICWORD;
    memset((void *)(p->obj_array), 0, p->length * sizeof(p->obj_array[0]));
}
static void *create_FlexStruct_Array(size_t header_size, size_t element_size, size_t count, void (*init_func)(void *, size_t))
{
    // Validate parameters
    if (count == 0u || 0u == header_size || 0u == element_size || NULL == init_func)
    {
        return NULL;
    }

    // Calculate total required memory
    size_t total_size = sizeof(uint32_t) + header_size + (count + 1) * element_size;

    // Allocate and zero-initialize memory
    void *ptr = pau_calloc(total_size, __func__);
    if (ptr == NULL)
    {
        pau_printf("!!!flexible array mem allocation failed\r\n");
        return NULL;
    }
    // 元编程模版告知IAR编译器需要调用的函数地址，优化时尽量最近,跳转调用
#define CONTEXT_EXPANDER_PROJ_GLOBAL(x) Alloc_##x##Array_Init,
#pragma calls = VARIABLE_LIST_PENDING_EXPANDED
    init_func(ptr, count);
    return ptr;
#undef CONTEXT_EXPANDER_PROJ_GLOBAL
}

bool database_building(TOPOTYPE topology, size_t nodes_num, size_t plugs_num)
{
#define CONTEXT_EXPANDER_PROJ_GLOBAL(x)                                                      \
    do                                                                                       \
    {                                                                                        \
        VAR_NAME(x) = CREATE_FLEXSTRUCT_ARRAY(Alloc_##x##Array, x##_varonstack);             \
        if (NULL == VAR_NAME(x))                                                             \
        {                                                                                    \
            pau_printf("!!!flexible array mem allocation of Alloc_" #x "Array failed.\r\n"); \
            return false;                                                                    \
        }                                                                                    \
    } while (0);

    size_t Nodes_varonstack = nodes_num;
    size_t Plugs_varonstack = plugs_num;
    size_t Contactors_varonstack = 2 * nodes_num;
    size_t ReqSettler_varonstack = Plugs_varonstack;
    (void)pau_calloc(0, __func__);
    directedConfig_Init(Nodes_varonstack, Plugs_varonstack);
    if (SemiHybrid == topology)
    {
        Nodes_varonstack += nodes_num / 2;
        Contactors_varonstack += factorial(nodes_num / 2);
    }

    VARIABLE_LIST_PENDING_EXPANDED
#undef CONTEXT_EXPANDER_PROJ_GLOBAL
    TOPOLOGY_TYPE = topology;
    pau_alloc(0, RATE_SPAN);
    return true;
}
bool hear_Canaries_Twittering(void)
{
#define CONTEXT_EXPANDER_PROJ_GLOBAL(x)                                            \
    do                                                                             \
    {                                                                              \
        if (!IS_FRONT_CANARY_INTACT(VAR_NAME(x), VAR_TYPE(x)))                     \
        {                                                                          \
            pau_printf("allocated object of " #x ": Front canary corrupted!\r\n"); \
            return false;                                                          \
        }                                                                          \
        if (!IS_REAR_CANARY_INTACT(VAR_NAME(x), VAR_TYPE(x)))                      \
        {                                                                          \
            pau_printf("allocated object of " #x ": Rear canary corrupted!\r\n");  \
            return false;                                                          \
        }                                                                          \
    } while (0);
    VARIABLE_LIST_PENDING_EXPANDED
    bool graphconfig_Canaries_Twittering(void);
    if (!graphconfig_Canaries_Twittering())
    {
        return false;
    }
    return true;
#undef CONTEXT_EXPANDER_PROJ_GLOBAL
}
NodeState get_node_state(ID_TYPE nodeid)
{
    if (!ASSERT_NODE_ID(nodeid))
    {
        return NODE_DISABLED;
    }
    return refer_Node_Extracted(nodeid)->state;
}
PlugState get_plug_state(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return PLUG_IDLE;
    }
    return refer_Plug_Extracted(plugid)->state;
}
size_t get_plug_requiredPower(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return 0;
    }
    return refer_Plug_Extracted(plugid)->requiredPower;
}

int get_node_available_power(ID_TYPE nodeid)
{
    if (!ASSERT_NODE_ID(nodeid))
    {
        return 0;
    }
    return refer_Node_Extracted(nodeid)->power_available;
}
PRIOR
get_node_priority(ID_TYPE nodeid)
{
    if (!ASSERT_NODE_ID(nodeid))
    {
        return PRIOR_VAIN;
    }
    return refer_Node_Extracted(nodeid)->priority;
}
PRIOR get_plug_priority(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return PRIOR_VAIN;
    }
    return refer_Plug_Extracted(plugid)->priority;
}
void set_plug_priority(ID_TYPE plugid, PRIOR priority)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    pplug->priority = priority;
}
size_t get_plug_chargingnodes_cnt(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return -1;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    size_t num = pau_vector_size(pplug->allocatedNodes) - pau_vector_size(pplug->disabledNodes);
    num = num > 0 ? num : 0;
    return num;
}
size_t get_node_module_cnt(ID_TYPE nodeid)
{
    if (!ASSERT_NODE_ID(nodeid))
    {
        return -1;
    }
    return refer_Node_Extracted(nodeid)->moudle_box.size;
}
size_t get_plug_allocated_cnt(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return 0;
    }
    return refer_Plug_Extracted(plugid)->allocatedNodes->size;
}
size_t get_plug_allocated_cnt_excircle(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return 0;
    }
    size_t cnt = 0;
    PAU_VECTOR_FOREACH(nodeid, refer_Plug_Extracted(plugid)->allocatedNodes)
    {

        if (nodeid > NODES_MAX_ENCIRCLE)
        {
            cnt++;
        }
    }
    return cnt;
}
void clr_plug_allocated_cnt(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    pau_vector_clear(pplug->allocatedNodes);
}
size_t get_plug_chargingmodules_cnt(ID_TYPE plugid)
{
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    size_t cnt = 0;
    PAU_VECTOR_FOREACH(nodeid, pplug->allocatedNodes)
    {
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (pnode->state == NODE_IDLE || pnode->state == NODE_DISABLED)
        {
            continue;
        }
        cnt += pnode->moudle_box.size;
    }
    return cnt;
}

int get_plug_shortage(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return NODE_MAX;
    }
    return refer_Plug_Extracted(plugid)->shortage;
}
size_t *get_plug_hysteresisCnt(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return NULL;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    return &(pplug->hysteresisCnt);
}
int get_plug_charging_power(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return 0;
    }
    int power_total = 0;
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    PAU_VECTOR_FOREACH(nodeid, pplug->allocatedNodes)
    {
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (pnode->state != NODE_OCCUPIED || pnode->plug_id != plugid)
        {
            continue;
        }
        power_total += pnode->power_available;
    }
    return power_total;
}
void update_plug_shortage_power(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return;
    }
    int power_total = get_plug_charging_power(plugid);
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    int shortage_power = (int)(pplug->requiredPower) - power_total;
    pplug->shortage = (shortage_power >= 0) ? (int)(shortage_power + UNITPWR_MAX - SIZING_TOLERANCE) / UNITPWR_MAX : -1 * (-1 * shortage_power / UNITPWR_MAX);
}
ID_TYPE get_node_chargingplugid(ID_TYPE node)
{
    if (!ASSERT_NODE_ID(node))
    {
        return ID_VAIN;
    }
    return refer_Node_Extracted(node)->plug_id;
}
bool get_plug_refresh_flag(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    return refer_Plug_Extracted(plugid)->refresh;
}
void set_plug_refresh_flag(ID_TYPE plugid, bool val)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return;
    }
    refer_Plug_Extracted(plugid)->refresh = val;
}

ID_TYPE get_plug_connectednode(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return NODE_MAX;
    }
    return refer_Plug_Extracted(plugid)->connectedNode;
}

bool plug_allocated_contain_node(ID_TYPE plugid, ID_TYPE nodeid)
{
    if (!ASSERT_PLUG_ID(plugid) || !ASSERT_NODE_ID(nodeid))
    {
        return false;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    return pau_vector_contains(pplug->allocatedNodes, nodeid);
}
