#include "pau_broker.h"
#include "pau_topolog.h"
#include "pau_tactic.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Helper to convert a large decimal string to hex string
// Assumes input dec_str contains only digits '0'-'9'
static void dec_str_to_hex_str(const char *dec_str, char *hex_str, int size)
{
    long num = strtol(dec_str, NULL, 10);

    // 用传入的大小，不要用 sizeof！
    snprintf(hex_str, size, "%lX", num);
}
static void bin2hex_left_pad(const char *bin_str, char *hex_str)
{
    int len = strlen(bin_str);
    int hex_idx = 0;

    // 先处理补的 0
    int i;
    int val = 0;
    for (i = 0; i < len % 4; i++)
    {
        int bit = bin_str[i] - '0';

        val = (val << 1) | bit;
    }
    if (0 < val)
    {
        hex_str[hex_idx++] = "0123456789ABCDEF"[val];
    }
    // 再处理真实的二进制位
    val = 0;
    for (i = i; i < len; i++)
    {
        int bit = bin_str[i] - '0';

        val = (val << 1) | bit;

        if ((i + 1) % 4 == 0)
        {
            hex_str[hex_idx++] = "0123456789ABCDEF"[val];
            val = 0;
        }
    }

    hex_str[hex_idx] = '\0';
}
void print_outcomes(ID_TYPE plugid)
{
#if !defined(__IAR_SYSTEMS_ICC__)
    return;
#endif
    // The argument plugid is unused for the global status dump, but kept for signature compatibility
    (void)plugid;

    char log_buffer[256 + 128] = {'\0'};
    int offset = 0;

    // 1. Header
    offset += snprintf(log_buffer + offset, sizeof(log_buffer) - offset, "$CYCLUS$");

    // 2. System Counts <Nodes><Plugs>
    offset += snprintf(log_buffer + offset, sizeof(log_buffer) - offset, "<%d><%d>", NODE_MAX, PLUG_MAX);

    // 3. Node Allocation Map (Decimal String -> Hex)
    // Format: Each node gets 2 decimal digits. E.g., Node 1 -> Plug 1 -> "01"
    char string_copie_stock[MAXNODES_MEM_LMT * 2 + 1] = {'\0'}; // Enough space for 2 digits per node + null

    for (int i = 1; i <= NODE_MAX; i++)
    {
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(i);
        ID_TYPE pid = 0;
        if (pnode && pnode->state != NODE_IDLE && pnode->plug_id != ID_VAIN)
        {
            pid = pnode->plug_id;
        }
        // Append 2-digit decimal
        snprintf(string_copie_stock + strlen(string_copie_stock), sizeof(string_copie_stock) - strlen(string_copie_stock), "%02d", pid);
    }

    // Convert Node Decimal String to Hex
    // char node_hex_str[256];
    // dec_str_to_hex_str(string_copie_stock, node_hex_str, sizeof(node_hex_str));
    offset += snprintf(log_buffer + offset, sizeof(log_buffer) - offset, "(%s)", string_copie_stock);

    // 4. Contactor State Map (Binary String -> Hex)
    // Format: "Open=0, Closed=1" based on prompt "分为0合为1"
    // Wait, prompt example: C1-0 C2-1 ... -> 01001101...
    // If C1 is Closed, and bit is 0, then Closed=0.
    // If C2 is Open, and bit is 1, then Open=1.
    // This matches "He (Closed)=0, Fen (Open)=1".
    // BUT prompt text says "分为0合为1" (Fen=0, He=1). This is a contradiction.
    // Let's look at the example result: 4D00.
    // Binary 0100 1101 0000 0000.
    // Bits: C1=0, C2=1, C3=0, C4=0, C5=1, C6=1, C7=0, C8=1...
    // Usually, most contactors are OPEN (0) in default state.
    // If "Open=0", then most bits should be 0.
    // If "Closed=1", then active contacts are 1.
    // Let's assume the standard engineering logic: 0 = Open (False), 1 = Closed (True).
    // And the prompt text "分为0合为1" might have been a typo for "合为1分0" or similar.
    // HOWEVER, I must follow the prompt's explicit mapping if possible.
    // Let's re-read carefully: "各接触器分合状态信息为各个节点的分合状态（分为0合为1）"
    // This explicitly says: Open (Fen) = 0, Closed (He) = 1.
    // So if isClosed is true, bit is 1. If false, bit is 0.

    memset(string_copie_stock, 0, sizeof(string_copie_stock));

    pau_printf("[TACTIC] Contactors cloesed: ");
    for (int i = 1; i <= CONTACTOR_MAX; i++)
    {
        struct Alloc_contactorObj *pcont = refer_Contactor_Extracted(i);
        int bit = 0;
        if (pcont)
        {
            // isClosed is bool. True=1, False=0.
            // Mapping: Closed=1, Open=0.
            bit = pcont->isClosed ? 1 : 0;
            if (1 == bit)
            {
                pau_printf("%d ", i);
            }
        }
        snprintf(string_copie_stock + strlen(string_copie_stock), sizeof(string_copie_stock) - strlen(string_copie_stock), "%d", bit);
    }
    pau_printf("\r\n");

    // Convert Binary String to Hex
    char cont_hex_str[12];
    bin2hex_left_pad(string_copie_stock, cont_hex_str); // Reuse buffer for hex string

    offset += sprintf(log_buffer + offset, "{%s}", cont_hex_str);

    // 5. Charging Plugs Info
    // Format: [Hex(ID + Power(5) + Prior(1))]
    for (int i = 1; i <= PLUG_MAX; i++)
    {
        struct Alloc_plugObj *pplug = refer_Plug_Extracted(i);
        if (!pplug)
            continue;

        // Check if charging (has allocated nodes or state is CHARGING)
        if (pplug->state == PLUG_CHARGING || pau_vector_size(pplug->allocatedNodes) > 0)
        {
            char plug_dec_str[10] = {'\0'};
            // Format: ID (decimal) + Power (5 digits) + Priority (1 digit)
            // Example: P1, 240kW, Prior 1 -> "1002401"
            // Note: Prompt says "P1: 1002401". ID=1, Power=00240, Prior=1.
            sprintf(plug_dec_str, "%d%05d%d", pplug->id, pplug->requiredPower * 10, pplug->priority);

            char plug_hex_str[8] = {'\0'};
            dec_str_to_hex_str(plug_dec_str, plug_hex_str, sizeof(plug_hex_str));

            offset += sprintf(log_buffer + offset, "[%s]", plug_hex_str);
        }
    }

    // 6. Footer
    offset += sprintf(log_buffer + offset, "$SULCYC$");

    // Print the result
    pau_printf("%s\r\n", log_buffer);
}

size_t makeScore(enum Senario senario, int quota, ID_TYPE plugid, ID_TYPE neighbor_plugid, ID_TYPE nodeid, ID_TYPE neighbor_nodeid)
{
    if (!ASSERT_PLUG_ID(plugid) || !ASSERT_PLUG_ID(neighbor_plugid) || !ASSERT_NODE_ID(nodeid) || !ASSERT_NODE_ID(neighbor_nodeid))
    {
        return 0;
    }
    size_t score = 0;

    switch (senario)
    {
    case SENARIO_SUBSIDY:
    {
        bool lower_nodeid_alpha = plug_allocated_contain_node(plugid, nodeid - NODES_MAX_ENCIRCLE);
        bool lower_nodeid_beta = plug_allocated_contain_node(plugid, nodeid - NODES_MAX_ENCIRCLE / 2);
        score += WEIGHT_5 * (get_node_state(nodeid) == NODE_IDLE ? 1 : 0);
        score += WEIGHT_4 * lower_nodeid_alpha;
        score += WEIGHT_4 * lower_nodeid_beta;
        if (WEIGHT_4 + WEIGHT_5 <= score)
        {
            ID_TYPE connected_nodeid = get_plug_connectednode(plugid);

            int hops_dist = 0;
            if (lower_nodeid_alpha)
            {
                hops_dist = get_hops_occupied(connected_nodeid, nodeid - NODES_MAX_ENCIRCLE, plugid);
            }
            if (lower_nodeid_beta)
            {
                hops_dist = get_hops_occupied(connected_nodeid, nodeid - NODES_MAX_ENCIRCLE / 2, plugid);
            }
            score += WEIGHT_2 * (WEIGHT_HIERARCHY * WEIGHT_HIERARCHY - hops_dist);
        }
        else
        {
            score += WEIGHT_2 * (get_plug_allocated_cnt_excircle(plugid));
        }

        return score;
    }
    case SENARIO_INHERIT:
    {
        score += WEIGHT_1 * (WEIGHT_HIERARCHY * WEIGHT_HIERARCHY - 1 - get_plug_chargingnodes_cnt(neighbor_plugid));
        score += WEIGHT_3 * (get_plug_priority(neighbor_nodeid) % WEIGHT_HIERARCHY);
        int shortage = 0;
        shortage = get_plug_shortage(neighbor_plugid);
        shortage = shortage < 0 ? 0 : shortage;
        shortage = shortage >= WEIGHT_HIERARCHY ? WEIGHT_HIERARCHY - 1 : shortage;
        score += WEIGHT_4 * (shortage % WEIGHT_HIERARCHY);
        score = (plugid == neighbor_plugid) ? 0 : score; // 如果邻居节点所属充电桩与当前充电桩相同,则得分为0,不进行继承
        return score;
    }
    case SENARIO_ACQUIRE:
    {
        ID_TYPE neighbors[3];
        void get_neighbors(ID_TYPE nodeid, ID_TYPE * neighbors);
        get_neighbors(nodeid, neighbors);

        int occupied_count_neighbors = 0;
        int hops = -1;
        int module_adaptive = get_node_module_cnt(nodeid);
        module_adaptive = module_adaptive <= quota ? (WEIGHT_HIERARCHY - 1) - quota + module_adaptive : (WEIGHT_HIERARCHY - 1) - module_adaptive;
        module_adaptive = module_adaptive <= 0 ? 0 : module_adaptive;
        module_adaptive = module_adaptive >= (WEIGHT_HIERARCHY - 1) ? (WEIGHT_HIERARCHY - 1) : module_adaptive;
        for (int i = 0; i < 3; i++)
        {
            neighbor_nodeid = neighbors[i];
            if (neighbor_nodeid == ID_VAIN)
            {
                return score;
            }
            if (get_node_chargingplugid(neighbor_nodeid) == plugid) // 如果是邻接点，则比较该点到直连点的距离
            {
                hops = get_dist(nodeid);
            }
            else if (get_node_chargingplugid(neighbor_nodeid) > ID_VAIN)
            {
                occupied_count_neighbors++;
            }
        }
        score = WEIGHT_1 * (WEIGHT_HIERARCHY * WEIGHT_HIERARCHY - 1 - hops);
        score += WEIGHT_3 * (3 - occupied_count_neighbors);
        score += WEIGHT_4 * (module_adaptive);
        score += WEIGHT_5 * (hops != -1 ? 1 : 0);
        score += WEIGHT_5 * (nodeid == get_plug_connectednode(plugid) && 0 == get_dist(nodeid) ? 1 : 0);
        score = ID_VAIN < get_node_chargingplugid(nodeid) ? 0 : score; // 如果是占用的节点,并且优先级大于等于本桩的优先级
        return score;
    }
    case SENARIO_PREEMPT:
    {
        ID_TYPE neighbors[3];
        void get_neighbors(ID_TYPE nodeid, ID_TYPE * neighbors);
        get_neighbors(nodeid, neighbors);
        ID_TYPE occupied_plugid_neighbors = ID_VAIN;
        bool is_midst_node = false;
        int hops = -1;
        int module_adaptive = get_node_module_cnt(nodeid);
        module_adaptive = module_adaptive <= quota ? (WEIGHT_HIERARCHY - 1) - quota + module_adaptive : (WEIGHT_HIERARCHY - 1) - module_adaptive;
        module_adaptive = module_adaptive <= 0 ? 0 : module_adaptive;
        module_adaptive = module_adaptive >= (WEIGHT_HIERARCHY - 1) ? (WEIGHT_HIERARCHY - 1) : module_adaptive;
        for (int i = 0; i < 3; i++)
        {
            neighbor_nodeid = neighbors[i];
            if (neighbor_nodeid == ID_VAIN)
            {
                return score;
            }
            if (get_node_chargingplugid(neighbor_nodeid) == plugid) // 如果是邻接点，则比较该点到直连点的距离
            {
                hops = get_dist(nodeid);
            }
            else if (get_node_chargingplugid(neighbor_nodeid) > ID_VAIN)
            {
                if (occupied_plugid_neighbors == ID_VAIN)
                {
                    occupied_plugid_neighbors = get_node_chargingplugid(neighbor_nodeid);
                }
                else
                {
                    is_midst_node = occupied_plugid_neighbors == get_node_chargingplugid(neighbor_nodeid); // 如果存在多个占用邻居节点,则判断是否为同一充电桩,如果是同一充电桩则认为该节点为中间节点
                }
            }
        }
        score = WEIGHT_1 * (WEIGHT_HIERARCHY * WEIGHT_HIERARCHY - 1 - hops);
        score += WEIGHT_3 * (module_adaptive);
        score += WEIGHT_4 * (is_midst_node ? 0 : 1);
        score += WEIGHT_5 * (hops != -1 ? 1 : 0);
        ID_TYPE node_chargingplugid = get_node_chargingplugid(nodeid);
        score = (nodeid == get_plug_connectednode(node_chargingplugid) ? 0 : score);  // 直连节点不可以抢占
        score = (get_node_priority(nodeid) >= get_plug_priority(plugid) ? 0 : score); // 如果是占用的节点,并且优先级大于等于本桩的优先级不可以抢占
        return score;
    }
    case SENARIO_SHARING:
    {
    }
    default:
        return 0;
    }
}
static bool isHysteresis_Active(float pwr, ID_TYPE plugid, uint32_t *timer)
{
#define HYSTERESIS_IMMEDIATE_DEC (5.0f * 1000.0f)
#define HYSTERESIS_IMMEDIATE_AUG (3.0f * 1000.0f)
#define HYSTERESIS_CONSIDERED_DEC (2.0f * 1000.0f)
#define MAXIM_SUPPLY_OF_NODE (UNITPWR_MAX * 100.0f)
#define HYSTERESIS_THRESHOLD (4u * 30u)
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
    int charging_pwr = get_plug_charging_power(plugid);
    int required_pwr = get_plug_requiredPower(plugid);
    if (pwr < 1.0f * (MIN(charging_pwr, required_pwr) * 100.0f - MAXIM_SUPPLY_OF_NODE - HYSTERESIS_IMMEDIATE_DEC))
    {
        if (1 >= get_plug_allocated_cnt(plugid))
        {
            return false;
        }
        pau_printf(" pwr %f is less than %fw\r\n", (double)pwr, (double)charging_pwr * 100.0f);
        return true;
    }
    if (pwr > 1.0f * (MAX(charging_pwr, required_pwr) * 100.0f + HYSTERESIS_IMMEDIATE_AUG))
    {
        pau_printf(" pwr %f is greater than %fw\r\n", (double)pwr, (double)charging_pwr * 100.0f);
        return true;
    }
    if (pwr < 1.0f * (MIN(charging_pwr, required_pwr) * 100.0f - MAXIM_SUPPLY_OF_NODE - HYSTERESIS_CONSIDERED_DEC))
    {
        pau_printf("*");
        *timer = (*timer > HYSTERESIS_THRESHOLD) ? 0 : *timer + 1;
        if (1 >= get_plug_allocated_cnt(plugid))
        {
            return false;
        }
        pau_printf("    dectected pwr value decay:%3.1fw.............\r\n", (double)pwr);
        pau_printf("pwr %f is less than %fw", (double)pwr, (double)charging_pwr * 100.0f);
        return *timer == 0;
    }
    *timer = 0;
    return false;
}
static float get_diffmean_value(const float *pool, uint8_t length)
{
    float average = 0.0f;
    for (uint8_t i = 1; i < length; i++)
    {
        average = average + (pool[i - 1] - average) / (i * 1.0f); // A trivial trick to calculate average
    }
    return average;
}
static float stable_Required_Current(float current, ID_TYPE plug_id)
{
#define FLUCTION_THRESHOLD 5.0f                    // 电流波动阈值 5A
#define STABLISH_THRESHOLD_FALL 18u                // 下降趋势稳定阈值
#define STABLISH_THRESHOLD_RISE 3u                 // 上升趋势稳定阈值
#define RECOVERY_THRESHOLD 3u                      // 恢复阈值
#define REENTRANT_FILTER 30u                       // 函数重入分频过滤器
#define REENTRANT_ACTIVATION_GATE (20u * 60u * 3u) // 进入充电阶段后允许函数功能启用时间
    size_t var_stablish_threshold = STABLISH_THRESHOLD_RISE;
    if (0 == plug_id || plug_id > PLUG_MAX)
    {
        return 0.0f;
    }
    struct Tactic_ReqCurrentObj *pdata = refer_ReqSettler_Extracted(plug_id);
    pdata->func_reentry_cntr++;
    if (pdata->func_reentry_cntr < REENTRANT_ACTIVATION_GATE)
    {
        return 0.0f;
    }
    if (pdata->func_reentry_cntr % REENTRANT_FILTER != 0) // 分频进入函数
    {
        return 0.0f;
    }
    if (pdata->index > DEMAND_CURRENT_SAMPLENUM) // 趋势计算阶段
    {
        float last_once = pdata->sample_current_pool[(pdata->index - 2) % DEMAND_CURRENT_SAMPLENUM];
        float last_twice = pdata->sample_current_pool[(pdata->index - 4) % DEMAND_CURRENT_SAMPLENUM];
        float last_thrice = pdata->sample_current_pool[(pdata->index - 6) % DEMAND_CURRENT_SAMPLENUM];
        if (current < last_once && last_once < last_twice && last_twice < last_thrice)
        {
            var_stablish_threshold = STABLISH_THRESHOLD_FALL;
        }
    }
    float last_interpolated = get_diffmean_value(pdata->sample_current_pool, DEMAND_CURRENT_SAMPLENUM); // 获取当前节点的电流均值
    if (fabsf(current - last_interpolated) > FLUCTION_THRESHOLD)                                        // 电流变化过大
    {
        pdata->counter -= var_stablish_threshold / RECOVERY_THRESHOLD;
        pdata->counter = (pdata->counter <= 0 ? 0 : pdata->counter);
    }
    else
    {
        pdata->counter = (pdata->counter + 1 >= var_stablish_threshold ? var_stablish_threshold : pdata->counter + 1);
    }
    pdata->sample_current_pool[pdata->index++ % DEMAND_CURRENT_SAMPLENUM] = current;
    return (var_stablish_threshold == pdata->counter ? last_interpolated : 0.0f);
}
