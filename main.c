#include "pau_broker.h"
#include "SEGGER_RTT.h"
void rtt_init(void)
{
    // 初始化RTT缓冲区，通道0用于输入输出
    SEGGER_RTT_Init();
    SEGGER_RTT_ConfigUpBuffer(0, NULL, NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_TRIM);
    SEGGER_RTT_ConfigDownBuffer(0, NULL, NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_TRIM);
}

void test_sequence(void);

int main(void)
{
    rtt_init();
    test_sequence();
}