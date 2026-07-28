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
            write_idx++;
        }
    }

    return write_idx;
}

static int map_outlier_truncated(ID_TYPE plugid, FlowMap *map, St_PolicyTargetResult *outcome)
{
    // 比较outcome和map的内容,如果节点和接触器的匹配发生了变化,则该节点和该条支路上hops大于该节点hops的节点匹配关系都需被截断,仅保留不变化的节点-接触器匹配关系
    //  在map[m]中找到direction与outcome->PolicyTargetdPowerNode[n]相同的节点,如果map[m]的contactorid与outcome->PolicyTarget_RelayNo[n][0]不同
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    if (NULL == pplug)
    {
        return 0;
    }
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
        if (ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX && outcome->PolicyTargetdPowerNode[n] > NODES_MAX_ENCIRCLE)
        {
            memset(outcome->PolicyTargetdPowerNode + n, 0, MAXNODES_MEM_LMT - n);
            break;
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
            ID_TYPE c = outcome->PolicyTarget_RelayNo[m][0];
            if (!ASSERT_CONTACTOR_ID(c))
            {
                continue;
            }
            struct Alloc_contactorObj *pcontactor = refer_Contactor_Extracted(c);
            if (NULL == pcontactor)
            {
                continue;
            }
            ID_TYPE checknodeid = (pcontactor->node2 == outcome->PolicyTargetdPowerNode[m]) ? pcontactor->node1 : pcontactor->node2;
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
static void fillout_Outcomes(ID_TYPE chargeeID, FlowMap *map, St_PolicyTargetResult *outcome, int size)
{

    int n;
    for (n = 0; n < size; n++)
    {
        if (ID_VAIN == map[n].direction || ID_VAIN == map[n].contactorid)
        {
            break;
        }
        outcome->PolicyTargetdPowerNode[n] = (unsigned char)map[n].direction;
        outcome->PolicyTarget_RelayNo[n][0] = (unsigned char)map[n].contactorid;
        outcome->PolicyTarget_RelayNo[n][1] = (unsigned char)map[n].appendix;
        if (ASSERT_TOPOTYPE_WHEEL_UNMIXED_SIMPLEX)
        {
            outcome->PolicyTarget_RelayNo[n][1] = 255;
        }
        if (ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX && map[n].contactorid > 2 * NODES_MAX_ENCIRCLE && map[n].appendix > ID_VAIN)
        {
            ID_TYPE appendix_contactor = map[n].appendix;
            appendix_contactor = (ID_TYPE)(appendix_contactor + NODES_MAX_ENCIRCLE);
            outcome->PolicyTarget_RelayNo[n][1] = (unsigned char)appendix_contactor;
        }
    }
}
static int get_encirclenodes_num_outcomes(St_PolicyTargetResult *outcome)
{
    int cnt = 0;
    for (int i = 0; i < MAXNODES_MEM_LMT; i++)
    {
        if (ID_VAIN < outcome->PolicyTargetdPowerNode[i] && outcome->PolicyTargetdPowerNode[i] <= NODES_MAX_ENCIRCLE)
        {
            cnt++;
        }
    }
    return cnt;
}
/**
 * @brief Perform serviceable patrol on devices connected to a plug
 * Checks for faulty nodes or contactors and handles them by deordering
 * @param plug_id ID of the plug to patrol
 * @param patrol_type Type of patrol to perform (NODE_PATROLLING or CONTACTOR_PATROLLING)
 * @return void
 * @sideeffect May deorder faulty nodes and update plug allocation
 * @errorcond Returns early if plug has invalid priority or no chargers
 */

void publish_Outcomes(ID_TYPE chargeeID, St_PolicyTargetResult *outcome)
{
    if (!ASSERT_PLUG_ID(chargeeID))
    {
        return;
    }
    print_outcomes(chargeeID);
    if (0 == get_plug_allocated_cnt(chargeeID))
    {
        memset(outcome->PolicyTargetdPowerNode, 0, MAXNODES_MEM_LMT);
        memset(outcome->PolicyTarget_RelayNo, 0, MAXNODES_MEM_LMT * 2);
        return;
    }
    FlowMap map[MAXNODES_MEM_LMT] = {{ID_VAIN, ID_VAIN, ID_VAIN, ID_VAIN}};
    FlowMap *nexttag = encircle_flowDirectioned(chargeeID, map);
    int encirclenodes_num = get_encirclenodes_num_outcomes(outcome);
    int offset = map_outlier_truncated(chargeeID, map, outcome);
    if (offset == encirclenodes_num)
    {
        pau_printf("[PAU] %d ==%d\r\n", offset, encirclenodes_num);
        outcome->u8PolicyTargetPowerNodeNum = get_plug_allocated_cnt(chargeeID);
        (void)excircle_flowDirectioned(chargeeID, nexttag, map + MAXNODES_MEM_LMT - 1);
        fillout_Outcomes(chargeeID, map, outcome, outcome->u8PolicyTargetPowerNodeNum);
    }
    else
    {
        pau_printf("[PAU] %d !=%d\r\n", offset, encirclenodes_num);
        outcome->u8PolicyTargetPowerNodeNum = offset;
        set_plug_sequent_flag(chargeeID, true);
        pau_printf("[PAU] plug%d shift power route...shrink to minimal collection with %d node(s)\r\n", chargeeID, offset);
    }

    for (int n = outcome->u8PolicyTargetPowerNodeNum; n < MAXNODES_MEM_LMT; n++)
    {
        outcome->PolicyTargetdPowerNode[n] = 0;
        outcome->PolicyTarget_RelayNo[n][0] = 0;
        outcome->PolicyTarget_RelayNo[n][1] = 0;
    }
    pau_printf("[PAU] plug%d:Outcomes %d\r\n", chargeeID, outcome->u8PolicyTargetPowerNodeNum);
    for (int n = 0; n < outcome->u8PolicyTargetPowerNodeNum; n++)
    {
        pau_printf("[%d] = %02d %02d %02d\r\n", n, outcome->PolicyTargetdPowerNode[n], outcome->PolicyTarget_RelayNo[n][0], outcome->PolicyTarget_RelayNo[n][1]);
    }
}

bool route_authentichanged(ID_TYPE plugid, St_PolicyTargetResult *outcome, bool (*check_freenode_func)(ID_TYPE))
{
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    PAU_Vector *vec_copy = pau_vector_copy(pplug->allocatedNodes);
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
    PAU_VECTOR_FOREACH(nodeid, vec_copy)
    {
        if (!check_freenode_func(nodeid))
        {
            ret = false;
            break;
        }
    }

    pau_vector_destroy(vec_copy);
    return ret;
}