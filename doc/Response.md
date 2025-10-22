
# 正负响应规则文档

版本：v1.0
作者：xfp23

## 1. 适用范围

本规则适用于根据 ISO 14229-1 规约实现的 UDS 服务中，通信双方（Tester ↔ ECU）请求／响应的正响应（Positive Response）与负响应（Negative Response）格式与行为。

## 2. 响应基本定义

* **正响应 (Positive Response)**：服务请求被 ECU 成功接收并执行，返回 SID + 0x40 作为响应服务号。
* **负响应 (Negative Response)**：服务请求被 ECU 拒绝或未能执行，返回固定字节 0x7F 开头，接着原请求 SID，再接一个 NRC（Negative Response Code）表示拒绝原因。

## 3. 响应格式

### 3.1 正响应

```
[ 响应数据第一字节 = 请求 SID + 0x40 ] [ 可选响应参数… ]
```

例子：请求 `0x10 01` → 正响应 `0x50 01`。

### 3.2 负响应

```
0x7F [ 请求 SID ] [ NRC ]
```

例子：请求 `0x10 03` → 负响应 `0x7F 10 12`（表示子功能不支持）。

## 4. 蜂拥与延迟响应

* 若 ECU 收到请求但因为内部条件（如flash写入、资源占用）暂时无法完成执行，应先返回 NRC = 0x78（Request Correctly Received – Response Pending）表示“已收到，处理中”，稍后再返回正响应或最终负响应。
* ECU 必须在超时前至少发该 0x78，以避免请求方无限等待。

## 5. 常见 NRC（负响应码）

| NRC  | 含义                                            |
| ---- | --------------------------------------------- |
| 0x11 | Service Not Supported                         |
| 0x12 | SubFunction Not Supported                     |
| 0x13 | Incorrect Message Length or Invalid Format    |
| 0x22 | Conditions Not Correct                        |
| 0x31 | Request Out Of Range                          |
| 0x33 | Security Access Denied                        |
| 0x35 | Invalid Key                                   |
| 0x72 | General Programming Failure                   |
| 0x7E | SubFunction Not Supported in Active Session   |
| 0x7F | Service Not Supported in Active Session       |
| 0x78 | Request Correctly Received – Response Pending |

## 6. 实现建议

* 在服务处理入口，首先校验会话、子功能、参数长度、状态条件。
* 若校验失败，马上用负响应返回相应的 NRC。
* 若校验通过但执行时间较长，先发 0x78，然后继续执行操作，完成后再发正响应。
* 若校验通过且执行快，直接发正响应。
* 在正响应中，服务处理数据应紧跟响应 SID，无需再重复请求 SID。

## 7. 注意事项

* 正响应 SID = 请求 SID + 0x40，仅当服务成功执行。
* 负响应始终以 0x7F 开头，接请求 SID，再接 NRC。
* 流控、分帧、Session转换等机制为其他层协议控制，不改变本响应规则。
* 如在某些服务中设置 “Suppress Positive Response Message Indication Bit（子功能字节最高位）”，可抑制正响应发送，但**负响应仍必须发送**。
