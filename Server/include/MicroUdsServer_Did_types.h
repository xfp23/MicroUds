/**
 * @file MicroUdsServer_Did_types.h
 * @author https://xfp23.github.io
 * @brief 
 * @version 0.1
 * @date 2026-10-24
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef MICROUDS_DID_TYPES_H
#define MICROUDS_DID_TYPES_H

#include "MicroUdsServer_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
    MICROUDSDID_DYNA_ID,
    MICROUDSDID_DYNA_ADDRESS,
} MicroUdsServerDid_DynaType_t;

/**
 * @brief DID 回调的上下文
 * [REVIEW] 回调的调用时机统一为 "内核在拷贝数据之前调用":
 *   读: 内核校验通过后调用 cb，让用户把最新数据刷新到 data 指向的缓冲，cb 返回后内核再把 data 拷进响应。
 *   写: 内核校验(DID/会话/安全/长度)通过后调用 cb，传入 in_data/in_len 让用户做"数据层校验+提交"。
 *       cb 返回 nrc==0 才表示接受，此时若 data 非空，内核再把 in_data 拷进 data；nrc!=0 则 data 保持不变。
 *       原来的写入是 "先 memcpy 进用户变量，再调 cb"，校验不通过时变量已经被改坏了。
 */
typedef struct
{
    // < 由内核填写，用户只读回调
    uint16_t Did;      // 标识符
    uint16_t Did_len;  // did数量 [REVIEW] 本次请求里一共有几个DID(0x22可能带多个)，0x2E 恒为1。原来写成固定的 1，现在按实际填
    uint8_t *data;     // 数据
    uint16_t data_len; // 数据长度
    void *Userdata;    // 用户数据

    // [REVIEW] 新增的输入
    bool is_write;           // 是写请求还是读请求
    const uint8_t *in_data;  // 写请求: 诊断仪发来的数据 (只读!)
    uint16_t in_len;         // 写请求: 数据长度
    uint8_t session;         // 当前会话编号
    uint8_t sec_level;       // 当前已解锁的安全等级
    uint32_t call_cnt;       // 写请求返回过 pending 之后的重入次数，0=首次

    // < 由用户填写,内核负责处理
    uint8_t nrc; // 用户输出
    bool pending; // [REVIEW] 写请求专用: 置true表示"还没写完"(例如要等NVM)，内核回 0x78 并在后续周期再次调用 cb。读请求不支持pending(会按 GR 处理)

} MicroUdsServerDid_Ctx_t;

typedef void (*MicroUdsServerDid_Cb_t)(MicroUdsServerDid_Ctx_t *);

typedef struct
{
    uint16_t Identifier; // 标识符

    // uint8_t flags; // 标志 似乎不需要标志
    // [REVIEW] 目前确实不需要: 读写分成两张表，"只读"就是不在写表里；功能寻址目前由服务级(0x22/0x2E 都不允许功能寻址)统一拒绝。
    //          以后如果要让个别DID(如F190)支持功能寻址读，再加 flags。

    uint8_t permis;  // 安全权限 0 直接放行
    // [REVIEW] 原来是 uint8_t，只能表示8个会话，且被当成"会话编号"和掩码做与运算(编号3 & 掩码)，结果是错的。
    //          现在是会话位掩码(用 MICROUDS_SET_SESSION(x) 组合)，0 仍然表示"所有会话放行"。
    //          注意: "0=放行"意味着漏配置会悄悄放开权限；想要更保守可以改成 0=任何会话都不允许。
    uint32_t session; // 会话 0 直接放行

    uint8_t *data;     // 数据钩子，配置时指向，用于直接写入和读取用户的数据
    uint16_t data_len; // 数据长度

    volatile bool is_valid; // 有效 [REVIEW] 原名 is_vaild

    void *Userdata; // 用户数据
    MicroUdsServerDid_Cb_t cb;
} MicroUdsServerDidItem_t;

typedef struct
{
    uint16_t Identifier; // 标识符

    // uint8_t flags; // 标志

    uint8_t permis;  // 安全权限 0 直接放行
    uint32_t session; // 会话 0 直接放行  [REVIEW] 位掩码，见 MicroUdsServerDidItem_t

    uint8_t *data;     // 数据，配置时指向
    uint16_t data_len; // 数据长度

    void *Userdata; // 用户数据
    MicroUdsServerDid_Cb_t cb;
} MicroUdsServerDidItem_Config_t; // 配置列表

/**
 * [REVIEW] 动态DID的一个"片段"。
 * 回答你原来的疑问 "因为动态did是组合的静态did，所以我应该保留回调？":
 * 不要拷贝整个 MicroUdsServerDidItem_t (原来 Item[i] = *static_item，一个Item近30字节，一个动态DID 5 个，一共 8 个就是1KB多)，
 * 也不能只存数据指针: 那样读取动态DID时就绕过了源DID的会话/安全检查和刷新回调，还丢掉了"起始位置/长度"。
 * 只记录"来源 + 位置 + 长度"，读取时再回到源DID(查表→权限检查→cb刷新→按位置/长度取数据)。
 */
typedef struct
{
    uint16_t src_did; // 源DID (type == MICROUDSDID_DYNA_ID)
    uint16_t pos;     // 在源数据记录中的起始位置，从 1 开始
    uint16_t size;    // 取多少字节
    uint32_t addr;    // 内存起始地址 (type == MICROUDSDID_DYNA_ADDRESS)
} MicroUdsServerDid_DynaItem_t;

typedef struct {
    uint16_t dync_did; // 动态标识符
    // uint32_t addr; // 原标识符

    MicroUdsServerDid_DynaType_t type;
    // uint8_t *data;   // [REVIEW] 动态DID自己不持有数据，数据在读取时从来源现取，所以这个字段去掉了
    uint16_t data_len;  // [REVIEW] 改为: 各片段长度之和(定义时算好，0x22 读取前用来预估响应长度)

    MicroUdsServerDid_DynaItem_t Item[MICROUDS_DID_DYNAMICALLY_ITEM_LEN]; 

    uint16_t item_num; 

    volatile bool is_active; // 这里用激活标识符而不是有效
}MicroUdsServerDid_Dynamically_t;

typedef struct
{
    MicroUdsServerDidItem_t Read[MICROUDS_DID_READ_SIZE];
    MicroUdsServerDidItem_t Write[MICROUDS_DID_WRITE_SIZE];
    MicroUdsServerDid_Dynamically_t Dynamically[MICROUDS_DID_DYNAMICALLY_SIZE];
    uint16_t read_num;
    uint16_t write_num;
    uint16_t dync_num; 

} MicroUdsServerDid_Obj_t;

#ifdef __cplusplus
}
#endif

#endif
