
[EN](./readme.md)

---

# 🚀 快速入门指南 — MicroUDS

## 1. 配置

在 `MicroUDS_conf.h` 文件中，根据需要调整以下宏定义：

```c
#define MICROUDS_TICK_FREQ_HZ         1000
#define MICROUDS_TIMEOUT_N_CS_MS      150
#define MICROUDS_SERVICE_TIMEOUT_MS   5000
#define MICROUDS_SERVICE_RECORDS      64

```

### 参数说明

1. **`MICROUDS_HASH_SIZE`**
内部哈希表的大小。
每个注册的服务都会占用一个桶（bucket），请根据你计划注册的服务数量进行设置。
2. **`MICROUDS_TICK_FREQ_HZ`**
系统滴答频率（单位：Hz）。
该值必须与周期性调用 `MicroUDS_TickHandler()` 的速率一致。
推荐值：`1000`（即 1ms 周期）。
3. **`MICROUDS_TIMEOUT_N_CS_MS`**
多帧传输的帧间超时时间（N_Cs）。
如果超过此时间，多帧传输将被终止。
（详情请参考 ISO 14229 关于 N_Cs 时序的规定。）
4. **`MICROUDS_SERVICE_RECORDS`**
服务记录槽位的数量。
该值应等于（或略大于）你计划注册的 UDS 服务总数。
此参数也用于 `MicroUDS_Delete()` 内部的资源释放。

---

## 2. API 使用方法

### 1. 初始化

```c
MicroUDS_Sta_t MicroUDS_Init(void);

```

初始化内部数据结构，并为系统运行做好准备。

---

### 2. 周期性滴答处理程序 (Tick Handler)

```c
void MicroUDS_TickHandler(void);

```

应根据 `MICROUDS_TICK_FREQ_HZ` 定义的频率进行周期性调用。

---

### 3. 主循环任务

```c
void MicroUDS_TimerHandler(void);

```

在主循环中频繁调用此函数（建议调用频率 ≤ `MICROUDS_SERVICE_TIMEOUT_MS` 和 `MICROUDS_TIMEOUT_N_CS_MS` 的最小值）。

---

### 4. 接收回调

```c
void MicroUDS_ReceiveCallback(uint8_t *data);

```

每当接收到新的数据时，将一个完整的 8 字节 CAN 帧传递给此函数。

---

### 5. 注册 UDS 服务

```c
MicroUDS_Sta_t MicroUDS_RegisterService(MicroUDS_ServiceTable_t *table, size_t table_len);

```

注册一个服务条目数组。

* `table`: 指向服务表（`MicroUDS_ServiceTable_t` 数组）的指针。
* `table_len`: 表中元素的数量。

---

### 6. 注册服务会话 (Sessions)

```c
MicroUDS_Sta_t MicroUDS_RegisterSession(MicroUDS_Sid_t sid, MicroUDS_SessionTable_t *table, size_t table_len);

```

---

### 7. 实现发送函数

`MicroUDS_Transmit()` 作为一个弱函数（weak function）提供。
用户**必须重写此函数**来实现实际的数据发送机制（例如 CAN、LIN、DoIP）。

```c
MicroUDS_Sta_t MicroUDS_Transmit(uint8_t *data, size_t len)
{
    // 在此处实现你的发送逻辑
    return MICROUDS_OK;
}

```

---

## 3. 辅助 API

| 函数 | 说明 |
| --- | --- |
| `MicroUDS_ReadMultiframeInfo()` | 读取当前多帧传输的信息。 |
| `MicroUDS_Delete()` | 释放所有分配的资源。 |
| `MicroUDS_NegativeResponse()` | 发送带有 NRC 代码的否定响应。 |
| `MicroUDS_PositiveResponse()` | 发送肯定响应。 |
| `MicroUDS_ResetTimer()` | 重置超时计数器（保持在当前会话）。 |
| `MicroUDS_GetTickCount()` | 获取当前的滴答计数值。 |

---

## 4. 基本类型

### 状态码

```c
typedef enum {
    MICROUDS_OK,             // 成功
    MICROUDS_ERR,            // 错误
    MICROUDS_ERR_TIMEOUT,    // 超时错误
    MICROUDS_ERR_MEMORY,     // 内存错误
    MICROUDS_ERR_HASH,       // 哈希冲突或错误
    MICROUDS_ERR_PARAM,      // 参数错误
    MICROUDS_ERR_TRANS,      // 传输错误
} MicroUDS_Sta_t;

```

---

### 否定响应码 (NRC)

```c
typedef enum {
    UDS_NRC_NO = -1,
    UDS_NRC_SUCCESS = 0x00,
    UDS_NRC_GENERAL_REJECT = 0x10,                             // 一般拒绝
    UDS_NRC_SERVICE_NOT_SUPPORTED = 0x11,                      // 服务不支持
    UDS_NRC_SUBFUNCTION_NOT_SUPPORTED = 0x12,                  // 子功能不支持
    UDS_NRC_INVALID_FORMAT = 0x13,                             // 格式无效
    UDS_NRC_RESPONSE_TOO_LONG = 0x14,                          // 响应过长
    UDS_NRC_BUSY_REPEAT_REQUEST = 0x21,                        // 忙碌，请重复请求
    UDS_NRC_CONDITION_NOT_CORRECT = 0x22,                      // 条件不正确
    UDS_NRC_REQUEST_SEQ_ERROR = 0x24,                          // 请求序列错误
    UDS_NRC_NO_RESPONSE_FROM_SUBNET = 0x25,                   // 子网无响应
    UDS_NRC_FAILURE_PREVENT_EXECUTION = 0x26,                  // 故障防止执行
    UDS_NRC_REQUEST_OUT_OF_RANGE = 0x31,                       // 请求超出范围
    UDS_NRC_SECURITY_ACCESS_DENIED = 0x33,                     // 安全访问被拒绝
    UDS_NRC_INVALID_KEY = 0x35,                                // 密钥无效
    UDS_NRC_EXCEED_ATTEMPTS = 0x36,                            // 尝试次数超限
    UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED = 0x37,            // 延时未到
    UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED = 0x70,               // 上传下载不被接受
    UDS_NRC_TRANSFER_DATA_SUSPENDED = 0x71,                    // 数据传输挂起
    UDS_NRC_GENERAL_PROGRAMMING_FAILURE = 0x72,                // 一般编程错误
    UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER = 0x73,               // 错误的块序列计数器
    UDS_NRC_REQUEST_CORRECTLY_RECEIVED_RSP_PENDING = 0x78,     // 正确接收请求，响应挂起中
    UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_ACTIVE_SESSION = 0x7E,  // 当前会话不支持该子功能
    UDS_NRC_SERVICE_NOT_SUPPORTED_ACTIVE_SESSION = 0x7F,      // 当前会话不支持该服务
    UDS_NRC_RPM_TOO_HIGH = 0x81,                               // 转速过高
    UDS_NRC_RPM_TOO_LOW = 0x82,                                // 转速过低
    UDS_NRC_ENGINE_RUNNING = 0x83,                             // 引擎运行中
    UDS_NRC_ENGINE_NOT_RUNNING = 0x84,                         // 引擎未运行
    UDS_NRC_ENGINE_RUN_TIMEOUT = 0x85,                         // 引擎运行超时
    UDS_NRC_TEMP_TOO_HIGH = 0x86,                              // 温度过高
    UDS_NRC_TEMP_TOO_LOW = 0x87,                               // 温度过低
    UDS_NRC_VOLTAGE_TOO_HIGH = 0x88,                           // 电压过高
    UDS_NRC_VOLTAGE_TOO_LOW = 0x89,                            // 电压过低
} MicroUDS_NRC_t;

```

---

### 服务与会话结构体

```c
typedef struct {
    MicroUDS_Sid_t sid;           // 服务 ID
    MicroUDS_GeneralFunc_t func;  // 处理函数
    void *param;                  // 参数指针
} MicroUDS_ServiceTable_t;

```

```c
typedef struct {
    uint8_t ssid;                 // 子会话 ID
    MicroUDS_GeneralFunc_t func;  // 处理函数
    void *param;                  // 参数指针
} MicroUDS_SessionTable_t;

```

---

### 多帧信息

```c
typedef struct {
    uint8_t sid;         // 服务 ID
    uint16_t data_len;   // 数据长度
    uint8_t *data;       // 数据指针
} MicroUDS_MultiInfo_t;

```

---

✅ **大功告成！**
一旦正确设置了发送函数和周期性处理程序，你的 MicroUDS 协议栈就可以开始无缝处理标准 UDS 服务了。
