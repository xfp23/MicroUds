#ifndef MICROUDS_TYPES_H
#define MICROUDS_TYPES_H

/**
 * @file Microuds_types.h
 * @author https://github.com/xfp23
 * @brief MicroUDS类型
 * @version 0.1
 * @date 2025-10-21
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "Microuds_conf.h"
#include "stdint.h"
#include "string.h"
#include "stdlib.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    MICROUDS_OK,
    MICROUDS_ERR,
    MICROUDS_TIMEOUT,
} MicroUDS_Sta_t;

//====================================================
// 0x10 - Diagnostic Session Control (诊断会话控制)
//====================================================
typedef enum
{
    UDS_SESSION_DEFAULT = 0x01,     // 默认会话 (Default Session)
    UDS_SESSION_PROGRAMMING = 0x02, // 编程会话 (Programming Session)
    UDS_SESSION_EXTENDED = 0x03,    // 扩展会话 (Extended Session)
    UDS_SESSION_SAFETY = 0x04,      // 安全会话 (OEM自定义)
} MICROUDS_SessionControl_t;

//====================================================
// 0x11 - ECU Reset (ECU复位)
//====================================================
typedef enum
{
    UDS_RESET_HARD = 0x01,        // 硬件复位
    UDS_RESET_KEY_OFF_ON = 0x02,  // 模拟断电重启
    UDS_RESET_SOFT = 0x03,        // 软件复位
    UDS_RESET_ENABLE_RPD = 0x04,  // 启用快速掉电 (Rapid Power Down)
    UDS_RESET_DISABLE_RPD = 0x05, // 禁用快速掉电
} MICROUDS_EcuReset_t;

//====================================================
// 0x28 - Communication Control (通信控制)
//====================================================
typedef enum
{
    UDS_COMM_ENABLE_TX_RX = 0x00,  // 启用收发
    UDS_COMM_DISABLE_TX = 0x01,    // 禁止发送
    UDS_COMM_DISABLE_RX = 0x02,    // 禁止接收
    UDS_COMM_DISABLE_TX_RX = 0x03, // 禁止收发
} MICROUDS_CommControl_t;

//====================================================
// 0x31 - Routine Control (例行任务控制)
//====================================================
typedef enum
{
    UDS_ROUTINE_START = 0x01,          // 启动例行任务
    UDS_ROUTINE_STOP = 0x02,           // 停止例行任务
    UDS_ROUTINE_REQUEST_RESULT = 0x03, // 请求例行任务结果
} MICROUDS_RoutineControl_t;

//====================================================
// 0x3E - Tester Present (测试仪心跳)
//====================================================
typedef enum
{
    UDS_TESTER_PRESENT_ALIVE = 0x00, // 心跳保持会话
} MICROUDS_TesterPresent_t;

//====================================================
// 0x87 - Link Control (链路控制)
//====================================================
typedef enum
{
    UDS_LINK_SETUP = 0x01,    // 设置通信链路
    UDS_LINK_VERIFY = 0x02,   // 验证链路
    UDS_LINK_ACTIVATE = 0x03, // 激活链路
} MICROUDS_LinkControl_t;

//====================================================
// 0x27 - Security Access (安全访问)
//====================================================
typedef enum
{
    UDS_SEC_REQ_SEED_LVL1 = 0x01, // 请求种子 Level 1
    UDS_SEC_SEND_KEY_LVL1 = 0x02, // 发送密钥 Level 1
    UDS_SEC_REQ_SEED_LVL2 = 0x03, // 请求种子 Level 2
    UDS_SEC_SEND_KEY_LVL2 = 0x04, // 发送密钥 Level 2
    UDS_SEC_REQ_SEED_LVL3 = 0x05, // 请求种子 Level 3
    UDS_SEC_SEND_KEY_LVL3 = 0x06, // 发送密钥 Level 3
} MICROUDS_SecurityAccess_t;

/**
 * @brief UDS 负响应码（Negative Response Code, NRC）
 * @note 适用于所有服务，格式为：7F <RequestSID> <NRC>
 */
typedef enum
{
    UDS_NRC_GENERAL_REJECT = 0x10,            // 通用拒绝（General Reject）
    UDS_NRC_SERVICE_NOT_SUPPORTED = 0x11,     // 服务不支持（Service Not Supported）
    UDS_NRC_SUBFUNCTION_NOT_SUPPORTED = 0x12, // 子功能不支持（SubFunction Not Supported）
    UDS_NRC_INVALID_FORMAT = 0x13,            // 数据格式或长度错误（Incorrect Message Length or Invalid Format）
    UDS_NRC_RESPONSE_TOO_LONG = 0x14,         // 响应消息太长（Response Too Long）
    // 0x15 ~ 0x20 保留
    UDS_NRC_BUSY_REPEAT_REQUEST = 0x21,                      // 服务器繁忙，请重试（Busy Repeat Request）
    UDS_NRC_CONDITION_NOT_CORRECT = 0x22,                    // 条件不满足（Conditions Not Correct）
    UDS_NRC_REQUEST_SEQ_ERROR = 0x24,                        // 请求序列错误（Request Sequence Error）
    UDS_NRC_NO_RESPONSE_FROM_SUBNET = 0x25,                  // 从节点无响应（No Response From Subnet Component）
    UDS_NRC_FAILURE_PREVENT_EXECUTION = 0x26,                // 故障导致无法执行请求（Failure Prevents Execution）
    UDS_NRC_REQUEST_OUT_OF_RANGE = 0x31,                     // 请求超出范围（Request Out of Range）
    UDS_NRC_SECURITY_ACCESS_DENIED = 0x33,                   // 安全访问被拒绝（Security Access Denied）
    UDS_NRC_INVALID_KEY = 0x35,                              // 安全密钥错误（Invalid Key）
    UDS_NRC_EXCEED_ATTEMPTS = 0x36,                          // 超出最大尝试次数（Exceeded Number of Attempts）
    UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED = 0x37,          // 等待时间未到（Required Time Delay Not Expired）
    UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED = 0x70,             // ECU 拒绝刷写请求（Upload/Download Not Accepted）
    UDS_NRC_TRANSFER_DATA_SUSPENDED = 0x71,                  // 数据传输挂起（Transfer Data Suspended）
    UDS_NRC_GENERAL_PROGRAMMING_FAILURE = 0x72,              // 通用编程失败（General Programming Failure）
    UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER = 0x73,             // 数据块顺序错误（Wrong Block Sequence Counter）
    UDS_NRC_REQUEST_CORRECTLY_RECEIVED_RSP_PENDING = 0x78,   // 响应延迟（Response Pending）
    UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_ACTIVE_SESSION = 0x7E, // 当前会话下不支持子功能（SubFunction Not Supported in Active Session）
    UDS_NRC_SERVICE_NOT_SUPPORTED_ACTIVE_SESSION = 0x7F,     // 当前会话下不支持服务（Service Not Supported in Active Session）
    UDS_NRC_RPM_TOO_HIGH = 0x81,                             // 转速过高（RPM Too High）
    UDS_NRC_RPM_TOO_LOW = 0x82,                              // 转速过低（RPM Too Low）
    UDS_NRC_ENGINE_RUNNING = 0x83,                           // 发动机正在运行（Engine is Running）
    UDS_NRC_ENGINE_NOT_RUNNING = 0x84,                       // 发动机未运行（Engine not Running）
    UDS_NRC_ENGINE_RUN_TIMEOUT = 0x85,                       // 发动机运行超时（Engine Run Timeout）
    UDS_NRC_TEMP_TOO_HIGH = 0x86,                            // 温度过高（Temperature Too High）
    UDS_NRC_TEMP_TOO_LOW = 0x87,                             // 温度过低（Temperature Too Low）
    UDS_NRC_VOLTAGE_TOO_HIGH = 0x88,                         // 电压过高（Voltage Too High）
    UDS_NRC_VOLTAGE_TOO_LOW = 0x89,                          // 电压过低（Voltage Too Low）
} MicroUds_NegativeResponseCode_t;

typedef enum
{
    UDS_DIAGNOSTIC_SESSION_CONTROL = 0x10,   // 诊断会话控制
    UDS_ECU_RESET = 0x11,                    // ECU复位
    UDS_CLEAR_DIAGNOSTIC_INFORMATION = 0x14, // 清除故障码
    UDS_READ_DTC_INFORMATION = 0x19,         // 读取故障码信息
    UDS_READ_DATA_BY_IDENTIFIER = 0x22,      // 按DID读取数据
    UDS_READ_MEMORY_BY_ADDRESS = 0x23,       // 按内存地址读取
    UDS_SECURITY_ACCESS = 0x27,              // 安全访问
    UDS_COMMUNICATION_CONTROL = 0x28,        // 通信控制
    UDS_READ_DATA_BY_PERIODIC_ID = 0x2A,     // 周期性读取数据
    UDS_DYNAMICALLY_DEFINE_DATA_ID = 0x2C,   // 动态定义DID
    UDS_WRITE_DATA_BY_IDENTIFIER = 0x2E,     // 按DID写数据
    UDS_INPUT_OUTPUT_CONTROL_BY_ID = 0x2F,   // 输入输出控制
    UDS_ROUTINE_CONTROL = 0x31,              // 例行任务控制
    UDS_REQUEST_DOWNLOAD = 0x34,             // 请求下载
    UDS_REQUEST_UPLOAD = 0x35,               // 请求上传
    UDS_TRANSFER_DATA = 0x36,                // 传输数据块
    UDS_REQUEST_TRANSFER_EXIT = 0x37,        // 结束传输
    UDS_TESTER_PRESENT = 0x3E,               // 测试仪在线（心跳）
    UDS_LINK_CONTROL = 0x87,                 // 链路控制
} MicroUDS_Service_t;                        // 服务枚举

//====================================================
// 函数
//====================================================

/**
 * @brief 发送函数指针类型
 * @param data 发送的数据
 * @param size 发送的大小
 * @return 1 : 发送失败 0 : 发送成功
 */
typedef int (*MicroUDS_TransmitFunc_t)(uint8_t *data, size_t size);

/**
 * @brief 接收回调函数指针
 * @param buffer 接收buffer
 * @param size 字节大小
 * 
 * @return 1:失败 0：成功
 * 
 */
typedef int (*MicroUDS_ReveiveFunc_t)(uint8_t *buffer,size_t size);

/**
 * @brief 通用功能函数
 *
 */
typedef void (*MicroUDS_UniversalFunc_t)(void);

//====================================================
// 表
//===================================================
typedef struct
{
    MicroUDS_Service_t id;         // 服务id
    MicroUDS_UniversalFunc_t func; // 功能函数
} MicroUDS_ServiceTable_t;

typedef struct
{
    uint8_t id;                    // 会话id
    MicroUDS_UniversalFunc_t func; // 功能函数
} MicroUDS_SessionTable_t;         // 会话表

//=====================================================
// 链表
//======================================================
typedef struct MicroUDS_SessionNode_t
{
    uint8_t id; // 会话id
    MicroUDS_UniversalFunc_t func;
    MicroUDS_SessionNode_t *next;
} MicroUDS_SessionNode_t; // 会话节点

typedef struct MicroUDS_ServiceNode_t
{
    MicroUDS_Service_t id; // 服务id
    MicroUDS_SessionNode_t *Session;
    MicroUDS_ServiceNode_t *next;
} MicroUDS_ServiceNode_t; // 服务节点

typedef struct
{
    struct
    {
        MicroUDS_TransmitFunc_t Transmit; // 函数指针
        MicroUDS_ReveiveFunc_t ReceiveCb; // 接收回调
    } func;                               // 函数
} MicroUDS_Conf_t;                        // 配置

typedef struct
{
    uint32_t Tick; // 时基
    MicroUDS_TransmitFunc_t Transmit; // 发送
    MicroUDS_ReveiveFunc_t ReceiveCb; // 接收回调
    MicroUDS_ServiceNode_t *Service; // 服务使用节点树查找会话链表，由用户通过API启用服务，并向服务添加会话
} MicroUDS_Obj;

typedef MicroUDS_Obj *MicroUDS_Handle_t;

#ifdef __cplusplus
}
#endif
#endif