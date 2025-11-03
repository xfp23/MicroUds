
---

[中文](./readme.zh.md)

# 🚀 Quick Start Guide — MicroUDS

## 1. Configuration

In your `MicroUDS_conf.h`, adjust the following macros as needed:

```c
#define MICROUDS_HASH_SIZE            64
#define MICROUDS_TICK_FREQ_HZ         1000
#define MICROUDS_TIMEOUT_N_CS_MS      150
#define MICROUDS_SERVICE_TIMEOUT_MS   5000
#define MICROUDS_TRANSMIT_CB          NULL
#define MICROUDS_SERVICE_RECORDS      64
```

### Parameter Description

1. **`MICROUDS_HASH_SIZE`**
   Size of the internal hash table.
   Each registered service occupies one bucket, so set this according to the number of services you intend to register.

2. **`MICROUDS_TICK_FREQ_HZ`**
   System tick frequency (in Hz).
   This must match the call rate of your periodic `MicroUDS_TickHandler()`.
   Recommended: `1000` (1 ms period).

3. **`MICROUDS_TIMEOUT_N_CS_MS`**
   Timeout for inter-frame transmission (N_Cs).
   If exceeded, the multi-frame transfer is aborted.
   (Refer to ISO 14229 for details on N_Cs timing.)

4. **`MICROUDS_TRANSMIT_CB`**
   User-defined transmit function for sending data (e.g., via CAN).
   Must conform to the following prototype:

   ```c
   /**
    * @brief Transmit function pointer type.
    * @param data Pointer to transmit buffer.
    * @param size Number of bytes to transmit.
    * @return 0 = success, non-zero = failure.
    */
   typedef int (*MicroUDS_TransmitFunc_t)(uint8_t *data, size_t size);
   ```

   Example:

   ```c
   #define MICROUDS_TRANSMIT_CB   MyCAN_Transmit
   ```

5. **`MICROUDS_SERVICE_RECORDS`**
   Number of service record slots.
   This should match (or slightly exceed) the total number of UDS services you plan to register.
   It is also used internally by `MicroUDS_Delete()`.

---

## 2. API Usage

### 1. Initialization

```c
MicroUDS_Sta_t MicroUDS_Init(void);
```

Initializes internal structures and prepares the system for operation.

---

### 2. Periodic Tick Handler

```c
void MicroUDS_TickHandler(void);
```

Should be called periodically at the rate defined by `MICROUDS_TICK_FREQ_HZ`.

---

### 3. Main Loop Task

```c
void MicroUDS_TimerHandler(void);
```

Call this function frequently within your main loop (recommended rate ≤ min(`MICROUDS_SERVICE_TIMEOUT_MS`, `MICROUDS_TIMEOUT_N_CS_MS`)).

---

### 4. Receive Callback

```c
void MicroUDS_ReceiveCallback(uint8_t *data);
```

Pass one complete 8-byte CAN frame to this function whenever new data is received.

---

### 5. Register UDS Services

```c
MicroUDS_Sta_t MicroUDS_RegisterService(MicroUDS_ServiceTable_t *table, size_t table_len);
```

Registers an array of service entries.

* `table`: Pointer to a service table (array of `MicroUDS_ServiceTable_t`)
* `table_len`: Number of elements in the table

---

### 6. Register Service Sessions

```c
MicroUDS_Sta_t MicroUDS_RegisterSession(MicroUDS_Sid_t sid, MicroUDS_SessionTable_t *table, size_t table_len);
```

Adds session entries for a specific service ID.

---

## 3. Auxiliary APIs

| Function                        | Description                                      |
| ------------------------------- | ------------------------------------------------ |
| `MicroUDS_ReadMultiframeInfo()` | Read current multi-frame transfer information.   |
| `MicroUDS_Delete()`             | Release all allocated resources.                 |
| `MicroUDS_NegativeResponse()`   | Send a negative response with NRC code.          |
| `MicroUDS_PositiveResponse()`   | Send a positive response.                        |
| `MicroUDS_ResetTimer()`         | Reset timeout counter (stay in current session). |
| `MicroUDS_GetTickCount()`       | Get the current tick counter value.              |

---

## 4. Basic Types

### Status Codes

```c
typedef enum {
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

### Negative Response Codes (NRC)

```c
typedef enum {
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

### Service and Session Structures

```c
typedef struct {
    MicroUDS_Sid_t sid;
    MicroUDS_GeneralFunc_t func;
    void *param;
} MicroUDS_ServiceTable_t;
```

```c
typedef struct {
    uint8_t ssid;
    MicroUDS_GeneralFunc_t func;
    void *param;
} MicroUDS_SessionTable_t;
```

---

### Multi-frame Info

```c
typedef struct {
    uint8_t sid;
    uint16_t data_len;
    uint8_t *data;
} MicroUDS_MultiInfo_t;
```

---

✅ **That’s it!**
Once your transmit function and periodic handlers are correctly set up, your MicroUDS stack should be ready to handle standard UDS services seamlessly.

---
