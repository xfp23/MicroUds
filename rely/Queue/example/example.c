#include <stdio.h>
#include <stdint.h>
#include "Queue.h"

int main(void)
{
    // 初始化队列
    Queue_handle_t q = NULL; // must be NULL
    Queue_conf_t conf = {
        .size = 23,      // 节点数量
        .item_size = 8,  // 每条消息最大长度
    };

    if (Queue_Init(&q, &conf) != QUEUE_OK) {
        printf("Queue initialization failed!\n");
        return -1;
    }

    // 示例数据
    uint8_t msg[8];
    for (uint8_t i = 0; i < 8; i++) {
        msg[i] = i + 1;
    }

    // 入队演示
    for (int i = 0; i < 5; i++) {
        msg[0] = i; // 修改首字节表示序号
        Queue_Sta_t status = Queue_Push(q, msg, 8);
        if (status == QUEUE_OK) {
            printf("Push message %d success.\n", i);
        } else if (status == QUEUE_FULL) {
            printf("Queue is full!\n");
            break;
        } else {
            printf("Push failed with error!\n");
        }
    }

    // 出队演示
    uint8_t out[8];
    uint16_t out_len = 0;
    while (Queue_Pop(q, out, &out_len) == QUEUE_OK) {
        printf("Pop message (len=%d):", out_len);
        for (uint16_t i = 0; i < out_len; i++) {
            printf(" %02X", out[i]);
        }
        printf("\n");
    }

    // 释放队列
    Queue_Deinit(&q);

    return 0;
}
