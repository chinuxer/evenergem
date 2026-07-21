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
void fillout_Outcomes(ID_TYPE chargeeID, FlowMap *map, St_PolicyTargetResult *outcome)
{
    outcome->PolicyTargetdPowerNode[0] = get_plug_connectednode(chargeeID);

    outcome->PolicyTarget_RelayNo[0][0] = 0xfe;
    outcome->PolicyTarget_RelayNo[0][1] = 0xff;
    for (int n = 1; n < outcome->u8PolicyTargetPowerNodeNum; n++)
    {
        if (ID_VAIN == map[n - 1].direction || ID_VAIN == map[n - 1].contactorid)
        {
            break;
        }
        outcome->PolicyTargetdPowerNode[n] = map[n - 1].direction;
        outcome->PolicyTarget_RelayNo[n][0] = map[n - 1].contactorid;
        outcome->PolicyTarget_RelayNo[n][1] = map[n - 1].appendix;
        if (ASSERT_TOPOTYPE_WHEEL_UNMIXED_SIMPLEX)
        {
            outcome->PolicyTarget_RelayNo[n][1] = 255;
        }
        if (ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX && map[n - 1].contactorid > 2 * NODES_MAX_ENCIRCLE && map[n - 1].appendix > ID_VAIN)
        {
            ID_TYPE appendix_contactor = map[n - 1].appendix;
            appendix_contactor = (ID_TYPE)(appendix_contactor + NODES_MAX_ENCIRCLE);
            outcome->PolicyTarget_RelayNo[n][1] = appendix_contactor;
        }
    }

    pau_printf("[PAU] plug%d:Outcomes %d\r\n", chargeeID, outcome->u8PolicyTargetPowerNodeNum);
    for (int n = 1; n <= outcome->u8PolicyTargetPowerNodeNum; n++)
    {
        pau_printf("[%d] = %02d %02d %02d\r\n", n, outcome->PolicyTargetdPowerNode[n - 1], outcome->PolicyTarget_RelayNo[n - 1][0], outcome->PolicyTarget_RelayNo[n - 1][1]);
    }
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
    memset(outcome->PolicyTargetdPowerNode, 0, MAXNODES_MEM_LMT);
    memset(outcome->PolicyTarget_RelayNo, 0, 2 * MAXNODES_MEM_LMT);

    outcome->u8PolicyTargetPowerNodeNum = get_plug_allocated_cnt(chargeeID);
    FlowMap map[MAXNODES_MEM_LMT] = {{ID_VAIN, ID_VAIN, ID_VAIN}};
    FlowMap *pflow_map = encircle_flowDirectioned(chargeeID, map);
    excircle_flowDirectioned(chargeeID, pflow_map);
    // 打印map
    // for (int n = 0; n < NODES_MAX_ENCIRCLE; n++)
    //{
    //    pau_printf("%d : %02d %02d %02d\r\n", n, map[n].direction, map[n].contactorid, map[n].appendix);
    //}
    fillout_Outcomes(chargeeID, map, outcome);
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
