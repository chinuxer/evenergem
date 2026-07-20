#ifndef PAUTACTIC_H
#define PAUTACTIC_H
#include "pau_broker.h"

#define WEIGHT_HIERARCHY 10
#define WEIGHT_1 1
#define WEIGHT_2 WEIGHT_HIERARCHY *WEIGHT_1
#define WEIGHT_3 WEIGHT_HIERARCHY *WEIGHT_2
#define WEIGHT_4 WEIGHT_HIERARCHY *WEIGHT_3
#define WEIGHT_5 WEIGHT_HIERARCHY *WEIGHT_4
#define WEIGHT_6 WEIGHT_HIERARCHY *WEIGHT_5
#define WEIGHT_7 WEIGHT_HIERARCHY *WEIGHT_6
#define WEIGHT_8 WEIGHT_HIERARCHY *WEIGHT_7
#define WEIGHT_9 WEIGHT_HIERARCHY *WEIGHT_8
#define WEIGHT_10 WEIGHT_HIERARCHY *WEIGHT_9
#define GET_PCU_RAWDATA(id, data, type) //(retriver_PCU_RawData(id, data).type##_data)
enum Senario
{
    SENARIO_PREEMPT = 0,
    SENARIO_SHARING,
    SENARIO_INHERIT,
    SENARIO_ACQUIRE,
    SENARIO_RELEASE,
    SENARIO_SUBSIDY,

};
typedef struct
{
    ID_TYPE contactorid, direction, appendix;
} FlowMap;

typedef struct
{
    unsigned char u8PolicyTargetPowerNodeNum;
    unsigned char PolicyTargetdPowerNode[MAXNODES_MEM_LMT];
    unsigned char PolicyTarget_RelayNo[MAXNODES_MEM_LMT][2];
} St_PolicyTargetResult;
// union PCU_RawData retriver_PCU_RawData(ID_TYPE id, PCURawData data_type);
size_t get_plug_chargingnodes_cnt(ID_TYPE plugid);
size_t makeScore(enum Senario senario, int quota, ID_TYPE plugid, ID_TYPE neighbor_plugid, ID_TYPE nodeid, ID_TYPE neighbor_nodeid);
size_t get_plug_requiredPower(ID_TYPE plugid);
PRIOR get_plug_priority(ID_TYPE plugid);
void set_plug_priority(ID_TYPE plugid, PRIOR priority);
size_t get_plug_allocated_cnt(ID_TYPE plugid);
void clr_plug_allocated_cnt(ID_TYPE plugid);
size_t get_plug_chargingmodules_cnt(ID_TYPE plugid);
size_t *get_plug_hysteresisCnt(ID_TYPE plugid);
ID_TYPE get_plug_connectednode(ID_TYPE plugid);

void update_plug_shortage_power(ID_TYPE plugid);
int get_plug_shortage(ID_TYPE plugid);
void print_outcomes(ID_TYPE plugid);
FlowMap *encircle_flowDirectioned(ID_TYPE, FlowMap *);
void excircle_flowDirectioned(ID_TYPE, FlowMap *);
int get_plug_charging_power(ID_TYPE plugid);

bool get_plug_refresh_flag(ID_TYPE plugid);
void set_plug_refresh_flag(ID_TYPE plugid, bool val);
bool hear_Canaries_Twittering(void);

PRIOR get_node_priority(ID_TYPE nodeid);
size_t get_node_module_cnt(ID_TYPE nodeid);
ID_TYPE get_node_chargingplugid(ID_TYPE node);
int get_node_available_power(ID_TYPE nodeid);
NodeState get_node_state(ID_TYPE nodeid);
PlugState get_plug_state(ID_TYPE plugid);
bool plug_allocated_contain_node(ID_TYPE plugid, ID_TYPE nodeid);
size_t get_plug_allocated_cnt_excircle(ID_TYPE plugid);
#endif