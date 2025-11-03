非常好 👍 你这份 README 已经接近正式文档的水平了，结构清晰、条理分明、注释专业。
我帮你**整体优化成可直接发布在 GitHub 的正式版本**，主要是：

* 保留你原本的技术内容不动；
* 调整排版、标题层级、空行与代码块格式，使渲染更美观；
* 统一中英文格式（如反引号、单位说明、⚠️提醒等）；
* 加一点点视觉优化（让别人一眼知道这是个干净的嵌入式库）。

---

# 🧩 MicroUDS 快速开始

> 轻量级 UDS（ISO 14229）协议栈，适用于 MCU 环境
> 作者：[https://github.com/xfp23](https://github.com/xfp23)

---

## ⚙️ 1. 配置

```c
#define MICROUDS_HASH_SIZE            64
#define MICROUDS_TICK_FREQ_HZ         1000
#define MICROUDS_TIMEOUT_N_CS_MS      150
#define MICROUDS_SERVICE_TIMEOUT_MS   5000
#define MICROUDS_TRANSMIT_CB          NULL
#define MICROUDS_SERVICE_RECORDS      64
```

1. `MICROUDS_HASH_SIZE`
   哈希桶的大小。每个服务会占用一个桶空间，所以这里的值应对应你计划注册的服务数量。

2. `MICROUDS_TICK_FREQ_HZ`
   时间基准频率，即你定时器回调 `MicroUDS_TickHandler()` 的频率。推荐 1 ms（即 1000 Hz）。

3. `MICROUDS_TIMEOUT_N_CS_MS`
   多帧间隔超时（`N_Cs`）。超过该时间未接收到下一帧则中止传输。
   （至于 N_Cs 的定义，请参考 ISO 14229-2 标准。）

4. `MICROUDS_TRANSMIT_CB`
   用户提供的发送函数。类型为：

   ```c
   /**
    * @brief 发送函数指针类型
    * @param data 发送的数据
    * @param size 数据长度
    * @return 1：发送失败，0：发送成功
    */
   typedef int (*MicroUDS_TransmitFunc_t)(uint8_t *data, size_t size);
   ```

   示例：

   ```c
   #define MICROUDS_TRANSMIT_CB   MyCan_Transmit
   ```

5. `MICROUDS_SERVICE_RECORDS`
   记录区大小，应与注册服务的数量一致。
   此区域用于支持 `MicroUDS_Delete()` 的清理逻辑。

---

## 🧭 2. API 使用

### 初始化

```c
MicroUDS_Sta_t MicroUDS_Init(void);
```

### 时基回调（定时器中断调用）

```c
void MicroUDS_TickHandler(void);
```

### 主任务循环中调用

建议频率不低于 `MICROUDS_SERVICE_TIMEOUT_MS` 与 `MICROUDS_TIMEOUT_N_CS_MS` 中较小者。

```c
void MicroUDS_TimerHandler(void);
```

### 接收回调（输入 8 字节 CAN 帧）

```c
void MicroUDS_ReceiveCallback(uint8_t *data);
```

### 注册服务

```c
MicroUDS_Sta_t MicroUDS_RegisterService(MicroUDS_ServiceTable_t *table, size_t table_len);
```

* `table`：服务表（数组）
* `table_len`：数组元素数量

### 向服务注册会话

```c
MicroUDS_Sta_t MicroUDS_RegisterSession(MicroUDS_Sid_t sid, MicroUDS_SessionTable_t *table, size_t table_len);
```

* `sid`：服务 ID
* `table`：会话表（数组）
* `table_len`：数组元素数量

---

## 🧰 3. 辅助 API

| 函数                              | 功能描述           |
| ------------------------------- | -------------- |
| `MicroUDS_ReadMultiframeInfo()` | 读取当前多帧信息       |
| `MicroUDS_Delete()`             | 释放所有内存资源       |
| `MicroUDS_NegativeResponse()`   | 发送负响应          |
| `MicroUDS_PositiveResponse()`   | 发送正响应          |
| `MicroUDS_ResetTimer()`         | 重置超时边界，保持当前会话  |
| `MicroUDS_GetTickCount()`       | 获取当前 Tick 计数器值 |

示例：

```c
MicroUDS_MultiInfo_t info;
MicroUDS_ReadMultiframeInfo(&info);
printf("SID: %02X, len: %d\n", info.sid, info.data_len);
```

---

## 📦 4. 类型定义

### 状态码

```c
typedef enum
{
    MICROUDS_OK,
    MICROUDS_ERR,
    MICROUDS_ERR_TIMEOUT,
    MICROUDS_ERR_MEMORY,
    MICROUDS_ERR_HASH,
    MICROUDS_ERR_PARAM,
    MICROUDS_ERR_TRANS,
} MicroUDS_Sta_t;
```

---

### 负响应码（NRC）

```c
typedef enum
{
    UDS_NRC_NO = -1,
    UDS_NRC_SUCCESS = 0x00,
    UDS_NRC_GENERAL_REJECT = 0x10,
    UDS_NRC_SERVICE_NOT_SUPPORTED = 0x11,
    UDS_NRC_SUBFUNCTION_NOT_SUPPORTED = 0x12,
    UDS_NRC_INVALID_FORMAT = 0x13,
    UDS_NRC_RESPONSE_TOO_LONG = 0x14,
    UDS_NRC_BUSY_REPEAT_REQUEST = 0x21,
    UDS_NRC_CONDITION_NOT_CORRECT = 0x22,
    UDS_NRC_REQUEST_SEQ_ERROR = 0x24,
    UDS_NRC_NO_RESPONSE_FROM_SUBNET = 0x25,
    UDS_NRC_FAILURE_PREVENT_EXECUTION = 0x26,
    UDS_NRC_REQUEST_OUT_OF_RANGE = 0x31,
    UDS_NRC_SECURITY_ACCESS_DENIED = 0x33,
    UDS_NRC_INVALID_KEY = 0x35,
    UDS_NRC_EXCEED_ATTEMPTS = 0x36,
    UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED = 0x37,
    UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED = 0x70,
    UDS_NRC_TRANSFER_DATA_SUSPENDED = 0x71,
    UDS_NRC_GENERAL_PROGRAMMING_FAILURE = 0x72,
    UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER = 0x73,
    UDS_NRC_REQUEST_CORRECTLY_RECEIVED_RSP_PENDING = 0x78,
    UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_ACTIVE_SESSION = 0x7E,
    UDS_NRC_SERVICE_NOT_SUPPORTED_ACTIVE_SESSION = 0x7F,
    UDS_NRC_RPM_TOO_HIGH = 0x81,
    UDS_NRC_RPM_TOO_LOW = 0x82,
    UDS_NRC_ENGINE_RUNNING = 0x83,
    UDS_NRC_ENGINE_NOT_RUNNING = 0x84,
    UDS_NRC_ENGINE_RUN_TIMEOUT = 0x85,
    UDS_NRC_TEMP_TOO_HIGH = 0x86,
    UDS_NRC_TEMP_TOO_LOW = 0x87,
    UDS_NRC_VOLTAGE_TOO_HIGH = 0x88,
    UDS_NRC_VOLTAGE_TOO_LOW = 0x89,
} MicroUDS_NRC_t;
```

---

### 服务 ID

```c
typedef enum
{
    UDS_DIAGNOSTIC_SESSION_CONTROL = 0x10,
    UDS_ECU_RESET = 0x11,
    UDS_CLEAR_DIAGNOSTIC_INFORMATION = 0x14,
    UDS_READ_DTC_INFORMATION = 0x19,
    UDS_READ_DATA_BY_IDENTIFIER = 0x22,
    UDS_READ_MEMORY_BY_ADDRESS = 0x23,
    UDS_SECURITY_ACCESS = 0x27,
    UDS_COMMUNICATION_CONTROL = 0x28,
    UDS_READ_DATA_BY_PERIODIC_ID = 0x2A,
    UDS_DYNAMICALLY_DEFINE_DATA_ID = 0x2C,
    UDS_WRITE_DATA_BY_IDENTIFIER = 0x2E,
    UDS_INPUT_OUTPUT_CONTROL_BY_ID = 0x2F,
    UDS_ROUTINE_CONTROL = 0x31,
    UDS_REQUEST_DOWNLOAD = 0x34,
    UDS_REQUEST_UPLOAD = 0x35,
    UDS_TRANSFER_DATA = 0x36,
    UDS_REQUEST_TRANSFER_EXIT = 0x37,
    UDS_TESTER_PRESENT = 0x3E,
    UDS_LINK_CONTROL = 0x87,
} MicroUDS_Sid_t;
```

---

### 服务与会话表

```c
typedef struct
{
    MicroUDS_Sid_t sid;
    MicroUDS_GeneralFunc_t func;
    void *param;
} MicroUDS_ServiceTable_t;

typedef struct
{
    uint8_t ssid;
    MicroUDS_GeneralFunc_t func;
    void *param;
} MicroUDS_SessionTable_t;
```

---

### 多帧信息结构体

```c
typedef struct
{
    uint8_t sid;
    uint16_t data_len;
    uint8_t *data;
} MicroUDS_MultiInfo_t;
```
---
