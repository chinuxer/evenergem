#include "pau_broker.h"
#include "pau_tactic.h"

static int compress_outcomes(St_PolicyTargetResult *outcome, int size)
{
    int write_idx = 0;

    for (int read_idx = 0; read_idx < size; read_idx++)
    {
        if (outcome->PolicyTargetdPowerNode[read_idx] != 0)
        {
            outcome->PolicyTargetdPowerNode[write_idx] = outcome->PolicyTargetdPowerNode[read_idx];
            outcome->PolicyTarget_RelayNo[write_idx][0] = outcome->PolicyTarget_RelayNo[read_idx][0];
            outcome->PolicyTarget_RelayNo[write_idx][1] = outcome->PolicyTarget_RelayNo[read_idx][1];
            outcome->u8Hops[write_idx] = outcome->u8Hops[read_idx];
            outcome->u8ParentNodeNo[write_idx] = outcome->u8ParentNodeNo[read_idx];
            write_idx++;
        }
    }

    return write_idx;
}

static int map_outlier_truncated(FlowMap *map, St_PolicyTargetResult *outcome)
{
    /*
     * 比较上一次的 outcome 与本次生成的 map：
     * 若某节点的潮流接触器（RelayNo[0]）发生了改变，则该节点及其下游（hops 更大的）节点
     * 的匹配关系都要被截断，仅保留到该节点为止仍保持原样的节点-接触器匹配关系。
     * outcome 与 map 均已按 hops 从小到大排列，因此按顺序遍历即可。
     */
    PAU_Vector *vec_unvaried = pau_vector_create(MAXNODES_MEM_LMT);
    if (NULL == vec_unvaried)
    {
        return 0;
    }

    for (int n = 0; n < outcome->u8PolicyTargetPowerNodeNum; n++)
    {
        if (0 == outcome->PolicyTargetdPowerNode[n])
        {
            continue;
        }
        if (METABOLIN_INTACT == metabole_alethes(outcome->PolicyTargetdPowerNode[n], outcome->PolicyTarget_RelayNo[n][0], map))
        {
            if (!pau_vector_contains(vec_unvaried, outcome->PolicyTargetdPowerNode[n]))
            {
                pau_vector_append(vec_unvaried, outcome->PolicyTargetdPowerNode[n]);
            }
            continue;
        }
        outcome->PolicyTargetdPowerNode[n] = 0;
        outcome->PolicyTarget_RelayNo[n][0] = 0;
        outcome->PolicyTarget_RelayNo[n][1] = 0;
        for (int m = n + 1; m < outcome->u8PolicyTargetPowerNodeNum; m++)
        {
            /* 下游节点：其父节点（u8ParentNodeNo）若已不在保持集合中，则一并截断 */
            ID_TYPE checknodeid = outcome->u8ParentNodeNo[m];
            if (!pau_vector_contains(vec_unvaried, checknodeid))
            {
                outcome->PolicyTargetdPowerNode[m] = 0;
                outcome->PolicyTarget_RelayNo[m][0] = 0;
                outcome->PolicyTarget_RelayNo[m][1] = 0;
            }
            else if (METABOLIN_INTACT == metabole_alethes(outcome->PolicyTargetdPowerNode[m], outcome->PolicyTarget_RelayNo[m][0], map))
            {
                pau_vector_append(vec_unvaried, outcome->PolicyTargetdPowerNode[m]);
            }
        }
    }

    pau_vector_destroy(vec_unvaried);
    return compress_outcomes(outcome, MAXNODES_MEM_LMT);
}
static void fillout_Outcomes(FlowMap *map, St_PolicyTargetResult *outcome, int size)
{
    for (int n = 0; n < size; n++)
    {
        if (ID_VAIN == map[n].direction || ID_VAIN == map[n].contactorid)
        {
            break;
        }
        outcome->PolicyTargetdPowerNode[n] = (unsigned char)map[n].direction;
        outcome->PolicyTarget_RelayNo[n][0] = (unsigned char)map[n].contactorid;
        outcome->PolicyTarget_RelayNo[n][1] = (unsigned char)map[n].appendix;
        outcome->u8Hops[n] = (unsigned char)map[n].hops;
        outcome->u8ParentNodeNo[n] = (unsigned char)map[n].genogram;
        if (ASSERT_TOPOTYPE_WHEEL_UNMIXED_SIMPLEX)
        {
            outcome->PolicyTarget_RelayNo[n][1] = 255;
        }
    }
}

bool publish_Outcomes(ID_TYPE chargeeID, St_PolicyTargetResult *outcome)
{
    if (!ASSERT_PLUG_ID(chargeeID))
    {
        return false;
    }
    print_outcomes(chargeeID);
    if (0 == get_plug_allocated_cnt(chargeeID))
    {
        outcome->u8PolicyTargetPowerNodeNum = 0;
        memset(outcome->PolicyTargetdPowerNode, 0, MAXNODES_MEM_LMT);
        memset(outcome->PolicyTarget_RelayNo, 0, MAXNODES_MEM_LMT * 2);
        memset(outcome->u8Hops, 0, MAXNODES_MEM_LMT);
        memset(outcome->u8ParentNodeNo, 0, MAXNODES_MEM_LMT);
        pau_printf("[PAU] plug%d:Outcomes %d\r\n", chargeeID, outcome->u8PolicyTargetPowerNodeNum);
        return false;
    }

    FlowMap map[MAXNODES_MEM_LMT] = {{ID_VAIN, ID_VAIN, ID_VAIN, ID_VAIN, ID_VAIN}};
    FlowMap *end = flow_directioned(chargeeID, map);
    int map_size = (NULL != end) ? (int)(end - map) : 0;
    int old_num = outcome->u8PolicyTargetPowerNodeNum;
    int offset = map_outlier_truncated(map, outcome);
    bool is_outlier = false;
    if (offset == old_num)
    {
        outcome->u8PolicyTargetPowerNodeNum = (unsigned char)map_size;
        fillout_Outcomes(map, outcome, map_size);
    }
    else
    {
        outcome->u8PolicyTargetPowerNodeNum = (unsigned char)offset;
        set_plug_sequent_flag(chargeeID, true);
        pau_printf("[PAU] plug%d shift power route...shrink to minimal collection with %d node(s)\r\n", chargeeID, offset);
        is_outlier = true;
    }

    for (int n = outcome->u8PolicyTargetPowerNodeNum; n < MAXNODES_MEM_LMT; n++)
    {
        outcome->PolicyTargetdPowerNode[n] = 0;
        outcome->PolicyTarget_RelayNo[n][0] = 0;
        outcome->PolicyTarget_RelayNo[n][1] = 0;
        outcome->u8Hops[n] = 0;
        outcome->u8ParentNodeNo[n] = 0;
    }
    pau_printf("[PAU] plug%d:Outcomes %d\r\n", chargeeID, outcome->u8PolicyTargetPowerNodeNum);
    for (int n = 0; n < outcome->u8PolicyTargetPowerNodeNum; n++)
    {
        pau_printf("[%d] = %02d %02d %02d %02d %02d\r\n", n, outcome->PolicyTargetdPowerNode[n], outcome->PolicyTarget_RelayNo[n][0], outcome->PolicyTarget_RelayNo[n][1], outcome->u8Hops[n], outcome->u8ParentNodeNo[n]);
    }
    return is_outlier;
}

bool route_authentichanged(ID_TYPE plugid, St_PolicyTargetResult *outcome, bool (*check_freenode_func)(ID_TYPE))
{
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    PAU_Vector *vec_copy = pau_vector_clone(pplug->allocatedNodes);
    // 不需要检测接触器，只需要检测节点是否关机
    for (int n = 0; n < outcome->u8PolicyTargetPowerNodeNum; n++)
    {
        if (pau_vector_contains(vec_copy, outcome->PolicyTargetdPowerNode[n]))
        {
            pau_vector_remove(vec_copy, outcome->PolicyTargetdPowerNode[n]);
        }
    }
    // 逐个检测vec_copy中剩余节点是否关机
    bool ret = true;
    bool stop = false;
    PAU_VECTOR_FOREACH_BREAK(nodeid, vec_copy, stop)
    {
        if (!check_freenode_func(nodeid))
        {
            ret = false;
            stop = true;
            break;
        }
    }

    pau_vector_destroy(vec_copy);
    return ret;
}
