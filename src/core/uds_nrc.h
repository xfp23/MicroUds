/**
 * @file    uds_nrc.h
 * @author  https://github.com/xfp23
 * @version 0.1
 * @date    2026/04/02
 */

#ifndef UDS_NRC_H_
#define UDS_NRC_H_

/* Includes ------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
typedef enum {
    /* 0x00 不是负响应，但在代码逻辑中通常用来表示 Positive Response */
    UDS_NRC_PR                                     = 0x00, 

    /* 标准 NRC */
    UDS_NRC_GENERAL_REJECT                         = 0x10, // 一般拒绝
    UDS_NRC_SERVICE_NOT_SUPPORTED                  = 0x11, // 不支持该服务
    UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED             = 0x12, // 不支持该子功能
    UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT = 0x13, // 报文长度不正确或格式无效
    UDS_NRC_RESPONSE_TOO_LONG                      = 0x14, // 响应过长（无法装入传输协议）

    UDS_NRC_BUSY_REPEAT_REQUEST                    = 0x21, // 忙，请重复请求
    UDS_NRC_CONDITIONS_NOT_CORRECT                 = 0x22, // 条件不正确（如车速不为0）
    UDS_NRC_REQUEST_SEQUENCE_ERROR                 = 0x24, // 请求顺序错误
    UDS_NRC_NO_RESPONSE_FROM_SUBNET_COMPONENT      = 0x25, // 子网节点无响应
    UDS_NRC_FAILURE_PREVENTS_EXECUTION_OF_REQUESTED_ACTION = 0x26, // 故障导致无法执行请求任务

    UDS_NRC_REQUEST_OUT_OF_RANGE                   = 0x31, // 请求超出范围（DID无效等）
    UDS_NRC_SECURITY_ACCESS_DENIED                 = 0x33, // 安全访问拒绝（未解锁）
    UDS_NRC_INVALID_KEY                            = 0x35, // 密钥无效
    UDS_NRC_EXCEED_NUMBER_OF_ATTEMPTS              = 0x36, // 尝试次数超限
    UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED        = 0x37, // 延时时间未到（尝试锁定中）

    UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED           = 0x70, // 上传下载不接受
    UDS_NRC_TRANSFER_DATA_SUSPENDED                = 0x71, // 数据传输挂起
    UDS_NRC_GENERAL_PROGRAMMING_FAILURE            = 0x72, // 一般编程失败
    UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER           = 0x73, // 错误的数据块序列计数器
    
    /* 关键 NRC：RCRRP */
    UDS_NRC_RESPONSE_PENDING                       = 0x78, // 请求已正确接收，但响应挂起（常用于等待 Flash 擦除）

    UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION = 0x7E, // 当前会话不支持该子功能
    UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION     = 0x7F, // 当前会话不支持该服务

    /* 车辆特定限制 (ISO 14229-1:2013+) */
    UDS_NRC_RPM_TOO_HIGH                           = 0x81, // 转速过高
    UDS_NRC_RPM_TOO_LOW                            = 0x82, // 转速过低
    UDS_NRC_ENGINE_IS_RUNNING                      = 0x83, // 引擎正在运行
    UDS_NRC_ENGINE_IS_NOT_RUNNING                  = 0x84, // 引擎未运行
    UDS_NRC_ENGINE_RUN_TIME_TOO_LOW                = 0x85, // 引擎运行时间过短
    UDS_NRC_TEMPERATURE_TOO_HIGH                   = 0x86, // 温度过高
    UDS_NRC_TEMPERATURE_TOO_LOW                    = 0x87, // 温度过低
    UDS_NRC_VEHICLE_SPEED_TOO_HIGH                 = 0x88, // 车速过高
    UDS_NRC_VEHICLE_SPEED_TOO_LOW                  = 0x89, // 车速过低
    UDS_NRC_PEDAL_TOO_HIGH                         = 0x8A, // 踏板位置过高
    UDS_NRC_PEDAL_TOO_LOW                          = 0x8B, // 踏板位置过低
    UDS_NRC_TRANSMISSION_RANGE_NOT_IN_NEUTRAL      = 0x8C, // 档位不在空档
    UDS_NRC_TRANSMISSION_RANGE_NOT_IN_GEAR         = 0x8D, // 档位不在行驶档
    UDS_NRC_BRAKE_SWITCH_NOT_CLOSED                = 0x8F, // 制动开关未闭合
    UDS_NRC_SHIFTER_LEVER_NOT_IN_PARK              = 0x90, // 档杆不在驻车档
    UDS_NRC_TORQUE_CONVERTER_CLUTCH_LOCKED         = 0x91, // 变矩器离合器锁定
    UDS_NRC_VOLTAGE_TOO_HIGH                       = 0x92, // 电压过高
    UDS_NRC_VOLTAGE_TOO_LOW                        = 0x93  // 电压过低
} Uds_NrcCode_t;
/* Exported constants --------------------------------------------------------*/

/* Exported macros -----------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
extern MicroUds_Status_t Uds_Nrc_NegativeResponse(uint8_t sid,Uds_NrcCode_t code);
#ifdef __cplusplus
}
#endif

#endif /* UDS_NRC_H_ */
