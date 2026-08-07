#ifndef PAUDISTINCT_H
#define PAUDISTINCT_H
#include "pau_broker.h"

#define MAXNODES_MEM_LMT PAU_VECTOR_DEFAULT_CAPACITY
#if defined(STM32F407xx)

#include "infy_debug.h"
#include "app_dc_port.h"
#include "app_gun.h"
#include "app_converter_type.h"
#include "app_device_do.h"
typedef enum
{
    ENUM_PCUDATA_OF_PLUG = 0x00100u,
    ENUM_PCUDATA_OF_PLUG_DEMAND_CURRENT,
    ENUM_PCUDATA_OF_PLUG_MAXIMUM_CURRENT,
    ENUM_PCUDATA_OF_PLUG_STATUS,
    ENUM_PCUDATA_OF_PLUG_DURATION,
    ENUM_PCUDATA_OF_PLUG_VOLTAGE,
    ENUM_PCUDATA_OF_PLUG_SOC,
    ENUM_PCUDATA_OF_PLUG_LIMIT_CURRENT,
    ENUM_PCUDATA_OF_NODE = 0x10000u,
    ENUM_PCUDATA_OF_NODE_WORKHOURS,
    ENUM_PCUDATA_OF_NODE_CURRENT,
    ENUM_PCUDATA_OF_NODE_VOLTAGE,
    ENUM_PCUDATA_OF_NODE_STATUS,
    ENUM_PCUDATA_OF_NODE_AVAILABLEPWR,
    ENUM_PCUDATA_OF_CONTACTOR = 0x1000000u,
    ENUM_PCUDATA_OF_CONTACTOR_ONOFF,
    ENUM_PCUDATA_OF_CONTACTOR_STATUS,
    ENUM_PCUDATA_OF_CONTACTOR_CLOSETIMES,
    ENUM_PCUDATA_OF_CONTACTOR_LIMIT_CURRENT,

} PCURawData;

typedef enum
{
    ENUM_NODE_FAULT = 0,  // 模块故障
    ENUM_NODE_IDLE,       // 模块空闲
    ENUM_NODE_CHARGING,   // 模块充电
    ENUM_NODE_MAINTENANCE // 模块维护
} NodeModuleStatus;
union PCU_RawData
{
    int32_t int32_data;
    uint32_t uint32_data;
    float float_data;
};

#define GET_PCU_RAWDATA(id, data, type) (retriver_PCU_RawData(id, data).type##_data)
union PCU_RawData retriver_PCU_RawData(ID_TYPE id, PCURawData data_type);
#else
#define GET_PCU_RAWDATA(id, data, type) 0
typedef struct
{
    unsigned char u8PolicyTargetPowerNodeNum;
    unsigned char PolicyTargetdPowerNode[MAXNODES_MEM_LMT];
    unsigned char PolicyTarget_RelayNo[MAXNODES_MEM_LMT][2];
} St_PolicyTargetResult;

#endif
#endif