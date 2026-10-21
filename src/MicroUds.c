#include "MicroUds.h"

// MICROUDS_INSTANCE_NUM 先不做多实例

MicroUds_Obj_t uds_obj = {0};

bool MicroUds_SecurityVerity(void);

/**
 * @brief 构造正响应
 */
static MicroUds_Status_t PositiveResponse(uint8_t sid, uint8_t *payload, uint32_t len)
{
    if (uds_obj.resp.active)
    {
        return MICROUDS_BUSY;
    }

    uds_obj.resp.active = true;
    uds_obj.resp.type = MICROUDS_RESPTYPE_POSITIVE; // 正响应
    uds_obj.resp.data[0] = sid + 0x40;
    uds_obj.resp.len = 1;

    if (len == 0 || payload == NULL)
    {
        return MICROUDS_OK;
    }

    memcpy(&uds_obj.resp.data[1], payload, len);

    uds_obj.resp.len += len;

    return MICROUDS_OK;
}

/**
 * @brief 负响应
 *
 * @param nrc
 */
static MicroUds_Status_t NegativeResponse(uint8_t sid, uint8_t nrc)
{
    if (uds_obj.resp.active)
    {
        return MICROUDS_BUSY;
    }

    uds_obj.resp.active = true;
    uds_obj.resp.type = MICROUDS_RESPTYPE_NEGATIVE; // 负响应
    memset(uds_obj.resp.data, 0, sizeof(uds_obj.resp.data));
    uds_obj.resp.data[0] = 0x7F;
    uds_obj.resp.data[1] = sid;
    uds_obj.resp.data[2] = nrc;

    uds_obj.resp.len = 3;

    return MICROUDS_OK;
}

static void MicroUds_ResponseHandler(void)
{
    if (!uds_obj.resp.active)
    {
        return;
    }

    if (!uds_obj.resp.Inhibit && uds_obj.pdu.addr == MICROUDS_ADDR_PHY)
    {
        MicroUds_Transmit(uds_obj.resp.data, uds_obj.resp.len);
    }

    memset(&uds_obj.resp, 0, sizeof(MicroUds_Response_t));
}

static void MicroUds_SecurityHandler(void)
{
    if (!uds_obj.security.active)
    {
        return; // 不活动
    }

    switch (uds_obj.security.step)
    {
    case 0:

        break;

    case 1:
        // 得到种子并返回
       MicroUds_SecurityReqSeed(uds_obj.security.level, uds_obj.security.seed, &uds_obj.security.seed_len);

        if(uds_obj.security.seed_len >= MICROUDS_SEED_SIZE)
        {
            NegativeResponse(0x27, MICROUDS_NRC_SAD);
            uds_obj.security.step = 0;
            break;
        }

        MicroUds_SecurityGetKey(uds_obj.security.key[0],uds_obj.security.key_len[0]);
        uds_obj.security.step = 2;
        break;

    case 2:
        // 不处理
        break;

    case 3:
        if(!MicroUds_SecurityVerity())
        {
            NegativeResponse(0x27,MICROUDS_NRC_IK);
            break;
        }
        break;

    default:

        break;
    }
}

MicroUds_Status_t MicroUds_Init(void)
{
    memset(&uds_obj, 0, sizeof(MicroUds_Obj_t));
    uds_obj.session.mask = MICROUDS_SET_SESSION(MICROUDS_SESSION_MASK);    // 支持MICROUDS_SESSION_MASK个会话掩码
    uds_obj.session.ssid = MICROUDS_SET_SESSION(MICROUDS_SESSION_DEFAULT); // 开局设置一个默认会话
    uds_obj.security.session = MICROUDS_SESSION_EXTEND;                    // 扩展会话
    return MICROUDS_OK;
}

void MicroUds_TickHandler(void)
{
    uds_obj.tick++;
}

void MicroUds_TimerHandler(void)
{
    uint32_t current_tick = uds_obj.tick; // 取得当前tick

    if (current_tick - uds_obj.session.last_tick >= uds_obj.session.hold_tick)
    {
        uds_obj.session.last_tick += uds_obj.session.hold_tick; // 避免周期漂移

        // 换回默认会话
        uds_obj.session.ssid = MICROUDS_SET_SESSION(1);
        memset(&uds_obj.security, 0, sizeof(MicroUds_Security_t)); // 清空安全
    }

    MicroUds_ResponseHandler(); // 响应管理
}

void MicroUds_RxIndication(const uint8_t *data, size_t len, MicroUds_AddrType_t addrType)
{
    if (len < 2 || data == NULL)
    {
        NegativeResponse(data[0], MICROUDS_NRC_GR);
        return; // 这里应该静默吗?
    }
    // 收到请求后同步会话时间
    uds_obj.session.last_tick = uds_obj.tick;
    uds_obj.pdu.addr = addrType;
    uds_obj.resp.Inhibit = MICROUDS_GET_RESPINHIBIT(data[1]); // 获取响应抑制标志

    if (data[0] == 0x10) // 诊断会话控制作为内核基本服务直接实现？应该是正确的吧
    {
        uint8_t ssid_mask = uds_obj.session.mask & (1 << data[1]);

        if (ssid_mask == 0)
        {
            NegativeResponse(0x10, MICROUDS_NRC_GR);
            return; // 返回一个负响应 MICROUDS_NRC_GR 通用拒绝
        }
        uds_obj.session.ssid = MICROUDS_SET_SESSION(data[1]); // 切换会话
        PositiveResponse(0x10, NULL, 0);                      // 发送正响应
    }
    else if (data[0] == 0x27)
    {

        if(!(uds_obj.session.ssid & uds_obj.security.session))
        {
            uds_obj.security.step = 0;
            NegativeResponse(0x27,MICROUDS_NRC_CNC);
        }

        switch (uds_obj.security.step)
        {
        case 0:
        uds_obj.security.level = data[1];
        uds_obj.security.active = true;
        uds_obj.security.step = 1;
            break;

        case 1:

            break;

        case 2: // 解析诊断仪响应
        memcpy(uds_obj.security.key[1],&data[1],MICROUDS_KEY_SIZE);
        uds_obj.security.step = 3;
            break;

        default:

            break;
        }
    }
    else
    { // 普通服务 ，后续可能还会有DID？RID？
    }
}

/**
 * @brief 弱函数，重写 避免vscode爆红，先不放弱符号
 *
 * @param pud
 * @param len
 * @return MicroUds_Status_t
 */
MicroUds_Status_t /*__attribute__((weak))*/ MicroUds_Transmit(const uint8_t *pdu, uint32_t len)
{
    (void)pdu;
    (void)len;

    return MICROUDS_OK;
}

/**
 * @brief 弱函数，由用户重写
 *
 */
void /*__attribute__((weak))*/ MicroUds_SecurityReqSeed(uint8_t level, uint8_t *out, uint32_t *len) // 请求种子
{
    (void)level;
    (void)out;
    (void)len;
    return 0U;
}

/**
 * @brief 弱函数，用户重写
 * 
 */
void /*__attribute__((weak))*/  MicroUds_SecurityGetKey(uint8_t *key,uint32_t *len)
{
    (void)key;
    (void)len;
}

bool MicroUds_SecurityVerity(void)
{
    for(uint32_t i = 0; i < MICROUDS_KEY_SIZE; i++)
    {
        if(uds_obj.security.key[0][i] != uds_obj.security.key[1][0])
        {
            return false;
        }
    }
    return true;
}