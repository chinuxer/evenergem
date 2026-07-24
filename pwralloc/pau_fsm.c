/**
 ******************************************************************************
 * Copyright(c) Infy Power 2025-2025
 * @file    pau_fsm.c
 * @author  YBA40320
 * @version V1.0
 * @date    2025-11-07
 * @brief   1. 来车->分配一个模块（不管空闲还是抢来的）做绝缘 2. 预充->分配尽可能满足BCP电流的空闲节点,有空闲节点就分配空闲节点，没有就维持绝缘检测节点不变 3.充电->按照需求功率折算变化来实际分配节点，BCP电流不再作为参考
 * @history 2025-11-07 YBA40320 对接PCU状态机接口
 ******************************************************************************
 */
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
static enum METABOLIN metabole_alethes(unsigned char nodeid, unsigned char relayid, FlowMap *pflow_map)
{
    for (int m = 0; m < MAXNODES_MEM_LMT; m++)
    {
        if (ID_VAIN == pflow_map[m].direction)
        {
            return METABOLIN_VANISH;
        }
        if (pflow_map[m].direction != nodeid)
        {
            continue;
        }
        return pflow_map[m].contactorid == relayid ? METABOLIN_INTACT : METABOLIN_CHANGE;
    }
    return METABOLIN_VANISH;
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
    int n;
    for (n = 0; n < outcome->u8PolicyTargetPowerNodeNum; n++)
    {
        if (ID_VAIN == outcome->PolicyTargetdPowerNode[n])
        {
            break;
        }
        if (ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX && outcome->PolicyTargetdPowerNode[n] > NODES_MAX_ENCIRCLE)
        {
            break;
        }
        if (METABOLIN_INTACT != metabole_alethes(outcome->PolicyTargetdPowerNode[n], outcome->PolicyTarget_RelayNo[n][0], map))
        {
            memset(outcome->PolicyTargetdPowerNode + n, 0, MAXNODES_MEM_LMT - n);
            memset(outcome->PolicyTarget_RelayNo + n, 0, 2 * (MAXNODES_MEM_LMT - n));
            break;
        }
    }
    return n;
}
static void fillout_Outcomes(ID_TYPE chargeeID, FlowMap *map, St_PolicyTargetResult *outcome)
{

    int n;
    for (n = 0; n < outcome->u8PolicyTargetPowerNodeNum; n++)
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
    for (n = n; n < MAXNODES_MEM_LMT; n++)
    {
        outcome->PolicyTargetdPowerNode[n] = 0;
        outcome->PolicyTarget_RelayNo[n][0] = 0;
        outcome->PolicyTarget_RelayNo[n][1] = 0;
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
    FlowMap map[MAXNODES_MEM_LMT] = {{ID_VAIN, ID_VAIN, ID_VAIN}};
    FlowMap *nexttag = encircle_flowDirectioned(chargeeID, map);
    int offset = map_outlier_truncated(chargeeID, map, outcome);
    if (offset == get_encirclenodes_num_outcomes(outcome))
    {
        outcome->u8PolicyTargetPowerNodeNum = get_plug_allocated_cnt(chargeeID);
        excircle_flowDirectioned(chargeeID, nexttag, map + MAXNODES_MEM_LMT);
        fillout_Outcomes(chargeeID, map, outcome);
    }
    else
    {
        outcome->u8PolicyTargetPowerNodeNum = offset + excircle_flowDirectioned(chargeeID, map + offset, map + MAXNODES_MEM_LMT);
        set_plug_sequent_flag(chargeeID, true);
    }
    pau_printf("[PAU] plug%d:Outcomes %d\r\n", chargeeID, outcome->u8PolicyTargetPowerNodeNum);
    for (int n = 0; n < outcome->u8PolicyTargetPowerNodeNum; n++)
    {
        pau_printf("[%d] = %02d %02d %02d\r\n", n, outcome->PolicyTargetdPowerNode[n], outcome->PolicyTarget_RelayNo[n][0], outcome->PolicyTarget_RelayNo[n][1]);
    }
}

static void handle_init_cmd(va_list *args)
{
    bool database_building(TOPOTYPE, size_t, size_t);
    int oprt_ratedpwr_per_module(int);
    (void)oprt_ratedpwr_per_module(625);
    database_building(CakraWheel, 8, 8);
    return;
}

static void handle_plugin_cmd(va_list *args)
{
    va_list copy;
    va_copy(copy, *args);
    St_PolicyTargetResult *target_result = va_arg(copy, St_PolicyTargetResult *);
    ID_TYPE CCU_placed_id = (ID_TYPE)va_arg(copy, ID_TYPE);
    PRIOR priority = (PRIOR)va_arg(copy, int);
    va_end(copy);
    if (CCU_placed_id > PLUG_MAX || 0 == CCU_placed_id)
    {
        return;
    }
    if (PRIOR_VAIN != get_plug_priority(CCU_placed_id) || 0 < get_plug_allocated_cnt(CCU_placed_id))
    {
        return;
    }
    pau_printf("[PAU] Plug %d is placed\r\n", CCU_placed_id);
    clr_plug_allocated_cnt(CCU_placed_id);
    bool requestPower(ID_TYPE, int);
    set_plug_priority(CCU_placed_id, priority);
    requestPower(CCU_placed_id, UNITPWR_MAX);
    publish_Outcomes(CCU_placed_id, target_result);
    return;
}

static void handle_charging_cmd(va_list *args)
{

    va_list copy;
    va_copy(copy, *args);
    St_PolicyTargetResult *target_result = va_arg(copy, St_PolicyTargetResult *);
    ID_TYPE CCU_ordering_id = (ID_TYPE)va_arg(copy, ID_TYPE);
    float longfor_current = va_arg(copy, double);
    float voltage = va_arg(copy, double);
    va_end(copy);
    if (CCU_ordering_id > PLUG_MAX)
    {
        return;
    }

    int requiredPower = (int)(longfor_current * voltage) / 100;
    bool requestPower(ID_TYPE, int);
    // void available_power_update(void);
    // available_power_update();
    requestPower(CCU_ordering_id, requiredPower);
    pau_printf("[PAU] Charging %d is ordered %fA %fV %dkW\r\n", CCU_ordering_id, longfor_current, voltage, requiredPower);
    publish_Outcomes(CCU_ordering_id, target_result);
    return;
}

static void handle_plugout_cmd(va_list *args)
{
    va_list copy;
    va_copy(copy, *args);
    St_PolicyTargetResult *target_result = va_arg(copy, St_PolicyTargetResult *);
    ID_TYPE CCU_deorder_id = (ID_TYPE)va_arg(copy, ID_TYPE);
    va_end(copy);
    if (CCU_deorder_id > PLUG_MAX)
    {
        return;
    }
    if (PRIOR_VAIN == get_plug_priority(CCU_deorder_id) || 0 == get_plug_allocated_cnt(CCU_deorder_id))
    {
        return;
    }
    pau_printf("[PAU] Plug %d is deordered\r\n", CCU_deorder_id);
    bool releasePower(ID_TYPE, int);
    releasePower(CCU_deorder_id, 0);
    publish_Outcomes(CCU_deorder_id, target_result);
    return;
}
