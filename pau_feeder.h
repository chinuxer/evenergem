#include "pwralloc/pau_broker.h"
#include "pwralloc/pau_tactic.h"

#ifdef __cplusplus
#include <QList>
#endif
extern "C"
{
    bool publish_Outcomes(ID_TYPE chargeeID, St_PolicyTargetResult *outcome);
    bool database_building(TOPOTYPE, size_t, size_t);
    int oprt_ratedpwr_per_module(int);
    bool oprt_node_module_count_set(ID_TYPE nodeid, size_t module_count);
    bool requestPower(ID_TYPE, int);
    bool releasePower(ID_TYPE, int);
    bool hear_Canaries_Twittering(void);
    bool set_node_availability(ID_TYPE node_id);
    void push_NodetoPlug_pseudocyclose(ID_TYPE nodeid, ID_TYPE plugid);
    void pull_NodefromPlug_pseudocyclose(ID_TYPE nodeid, ID_TYPE plugid);
}
#ifdef __IMPORT_DATAFEEDER__
St_PolicyTargetResult gtarget_result[MAXNODES_MEM_LMT] = {0};
#else
extern St_PolicyTargetResult gtarget_result[MAXNODES_MEM_LMT];

int get_contactor_pwrflow_dest(int contactorId)
{
    if (contactorId < 1 || contactorId > CONTACTOR_MAX)
    {
        return -1;
    }
    if (ASSERT_TOPOTYPE_WHEEL_UNMIXED_SIMPLEX && contactorId <= 2 * NODES_MAX_ENCIRCLE && contactorId > 3 * NODES_MAX_ENCIRCLE / 2)
    {
        contactorId -= NODES_MAX_ENCIRCLE / 2;
    }
    for (int i = 0; i < MAXNODES_MEM_LMT; i++)
    {
        if (gtarget_result[i].u8PolicyTargetPowerNodeNum == 0)
        {
            continue;
        }
        for (int j = 0; j < gtarget_result[i].u8PolicyTargetPowerNodeNum; j++)
        {
            if (gtarget_result[i].PolicyTarget_RelayNo[j][0] == contactorId)
            {
                return i + 1;
            }
            if (gtarget_result[i].PolicyTarget_RelayNo[j][1] == contactorId)
            {
                return i + 1;
            }
        }
    }
    return -1;
}

#ifdef __cplusplus
// 将对应充电桩id的节点分配结果PolicyTargetdPowerNode转成QList<int>，用于Qt界面显示
QList<int> get_plug_allocated_nodes(int plugId)
{
    QList<int> allocatedNodes;
    allocatedNodes.clear();
    if (plugId < 1 || plugId > MAXNODES_MEM_LMT)
    {
        return allocatedNodes; // 返回空列表
    }

    St_PolicyTargetResult *result = &gtarget_result[plugId - 1];
    for (int i = 0; i < result->u8PolicyTargetPowerNodeNum; ++i)
    {
        allocatedNodes.append(result->PolicyTargetdPowerNode[i]);
    }
    return allocatedNodes;
}

void clear_publish_outcomes(int plugId)
{
    if (plugId < 1 || plugId > MAXNODES_MEM_LMT)
    {
        return;
    }
    memset(&gtarget_result[plugId - 1], 0, sizeof(St_PolicyTargetResult));
}

#endif
#endif
