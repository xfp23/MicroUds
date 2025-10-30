
[EN](./readme.md)

# 队列库（基于内存池）

本库提供一个轻量级队列实现，专为嵌入式系统设计。  
使用预分配的内存池（空闲链表 + 活跃链表）来避免运行时动态分配 (`malloc/free`)。

## 特性
- 固定内存池，无内存碎片
- FIFO 队列语义
- 支持可变长度消息（每次入队携带长度）
- 如果外部加锁（互斥或中断保护），可线程安全

## 初始化

```c
#include "Queue.h"

Queue_handle_t q;
Queue_conf_t conf = {
    .size = 16,       // 节点数量
    .item_size = 64   // 每条消息的最大字节数
};

if (Queue_Init(&q, &conf) != QUEUE_OK) {
    // 处理错误
}
```

## API 参考

### `Queue_Init`

```c
Queue_Sta_t Queue_Init(Queue_handle_t *handle, const Queue_conf_t *conf);
```

* 使用给定配置初始化队列。
* 参数：

  * `handle`：队列句柄指针（输出）
  * `conf`：队列配置指针（节点数量 + 最大消息长度）
* 返回值：

  * `QUEUE_OK`    – 成功
  * `QUEUE_ERR`   – 参数无效或分配失败

### `Queue_Push`

```c
Queue_Sta_t Queue_Push(Queue_handle_t handle, const void *data, uint16_t len);
```

* 将消息入队。
* 参数：

  * `handle`：队列句柄
  * `data`：消息缓冲区指针
  * `len`：消息字节长度（≤ 最大消息长度）
* 返回值：

  * `QUEUE_OK`    – 成功
  * `QUEUE_FULL`  – 没有空闲节点
  * `QUEUE_ERR`   – 参数无效

### `Queue_Pop`

```c
Queue_Sta_t Queue_Pop(Queue_handle_t handle, void *out_buf, uint16_t *out_len);
```

* 弹出队列中的一条消息（FIFO）。
* 参数：

  * `handle`：队列句柄
  * `out_buf`：消息拷贝缓冲区
  * `out_len`：实际消息长度输出指针
* 返回值：

  * `QUEUE_OK`     – 成功
  * `QUEUE_EMPTY`  – 队列为空
  * `QUEUE_ERR`    – 参数无效

### `Queue_Count`

```c
size_t Queue_Count(Queue_handle_t handle);
```

* 获取当前活跃消息数量。

### `Queue_FreeCount`

```c
size_t Queue_FreeCount(Queue_handle_t handle);
```

* 获取当前空闲节点数量。

### `Queue_Deinit`

```c
void Queue_Deinit(Queue_handle_t *handle);
```

* 释放内存池并使句柄失效。

## 示例

```c
uint8_t msg[] = "Hello";
Queue_Push(q, msg, sizeof(msg));

uint8_t buf[64];
uint16_t len;
if (Queue_Pop(q, buf, &len) == QUEUE_OK) {
    // buf 中包含 "Hello"，len = 6
}
```

## 返回码

| 返回码           | 含义           |
| ------------- | ------------ |
| `QUEUE_OK`    | 操作成功         |
| `QUEUE_ERR`   | 参数无效或操作失败    |
| `QUEUE_FULL`  | 队列已满，没有空闲节点  |
| `QUEUE_EMPTY` | 队列为空，没有消息可弹出 |

```
