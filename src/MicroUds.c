/**
 * @file MicroUds.c
 * @author xfp23
 * @brief MicroUds实现
 * @version 0.1
 * @date 2026-10-21
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "MicroUds.h"

// MICROUDS_INSTANCE_NUM 先不做多实例

MicroUds_Obj_t uds_obj = {0};

static bool MicroUds_SecurityVerity(void);

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
        MicroUds_Transmit((const uint8_t *)uds_obj.resp.data, uds_obj.resp.len);
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

        if (uds_obj.security.seed_len >= MICROUDS_SEED_SIZE)
        {
            NegativeResponse(0x27, MICROUDS_NRC_SAD);
            uds_obj.security.step = 0;
            break;
        }

        MicroUds_SecurityGetKey(uds_obj.security.key[0], &uds_obj.security.key_len[0]);
        uds_obj.security.step = 2;
        break;

    case 2:
        // 不处理
        break;

    case 3:
        if (!MicroUds_SecurityVerity())
        {
            NegativeResponse(0x27, MICROUDS_NRC_IK);
            break;
        }

        PositiveResponse(0X27, NULL, 0);
        uds_obj.security.active = false;
        uds_obj.security.permis = uds_obj.security.level;
        break;

    default:

        break;
    }
}

static void MicroUds_ServiceHandler(void)
{
    if (!uds_obj.service.active)
    {
        return;
    }
    bool is_exist_serv = false;
    bool is_exist_sub_func = false;
    // bool dedic = false;

    for (uint32_t i = 0; i < uds_obj.service.serv_num; i++)
    {

        if (uds_obj.service.serv[i].sid == uds_obj.pdu.sid)
        {
            is_exist_serv = true;

            if ((1 << uds_obj.service.serv[i].ssid & uds_obj.session.ssid) == 0)
            {
                NegativeResponse(uds_obj.pdu.sid, MICROUDS_NRC_SNSIAS);
                uds_obj.service.active = false;
                return; // 返回一个负响应
            }

            uint8_t dedic = MICROUDS_GET_ASSIG(uds_obj.service.serv[i].permis); // 这里获取最高位第七位，安全等级用数字大小就行了。 最高位是指定位，1表示此服务只有在这个安全等级下才允许访问，0表示 >= 此权限即可访问

            if (dedic)
            {
                if (uds_obj.service.serv[i].permis != uds_obj.security.permis)
                {
                    NegativeResponse(uds_obj.pdu.sid, MICROUDS_NRC_SAD);
                    uds_obj.service.active = false;
                    return; // 访问失败
                }
                else if (uds_obj.service.serv[i].permis > uds_obj.security.permis)
                {
                    NegativeResponse(uds_obj.pdu.sid, MICROUDS_NRC_SAD);
                    uds_obj.service.active = false;
                    return; // 访问失败
                }
            }

            for (uint32_t j = 0; j < uds_obj.service.serv[i].sub_func_num; j++)
            {
                if (uds_obj.service.serv[i].sub_func[j].id == uds_obj.pdu.sub_func)
                {
                    is_exist_sub_func = true;

                    if (uds_obj.service.serv[i].sub_func[j].callback != NULL)
                    {
                        MicroUds_ServiceCtx_t ctx = {0};
                        uds_obj.service.serv[i].sub_func[j].callback(&ctx);
                        uds_obj.service.active = false;

                        if (ctx.resp_type == MICROUDS_RESPTYPE_POSITIVE)
                        {
                            PositiveResponse(uds_obj.service.serv[i].sid, ctx.payload, ctx.len);
                            return;
                        }

                        NegativeResponse(uds_obj.service.serv[i].sid, ctx.nrc);
                        return;
                    }
                }
            }
        }
    }

    if (!is_exist_serv)
    {
        NegativeResponse(uds_obj.pdu.sid, MICROUDS_NRC_SNSIAS);
        uds_obj.service.active = false;
        return;
    }

    if (!is_exist_sub_func)
    {
        NegativeResponse(uds_obj.pdu.sid, MICROUDS_NRC_SFNSIAS);
        uds_obj.service.active = false;
    }
}

MicroUds_Status_t MicroUds_Init(void)
{
    memset(&uds_obj, 0, sizeof(MicroUds_Obj_t));
    uds_obj.session.mask = MICROUDS_SET_SESSION(MICROUDS_SESSION_MASK);    // 支持MICROUDS_SESSION_MASK个会话掩码
    uds_obj.session.ssid = MICROUDS_SET_SESSION(MICROUDS_SESSION_DEFAULT); // 开局设置一个默认会话
    uds_obj.session.is_default = true;
    uds_obj.security.session = MICROUDS_SESSION_EXTEND; // 扩展会话下才能访问
    return MICROUDS_OK;
}

void MicroUds_TickHandler(void)
{
    uds_obj.tick++;
}

void MicroUds_TimerHandler(void)
{
    uint32_t current_tick = uds_obj.tick; // 取得当前tick

    if (!uds_obj.session.is_default && uds_obj.session.ssid != MICROUDS_SESSION_DEFAULT && current_tick - uds_obj.session.last_tick >= uds_obj.session.hold_tick)
    {
        uds_obj.session.last_tick += uds_obj.session.hold_tick; // 避免周期漂移

        // 换回默认会话
        uds_obj.session.is_default = true;
        uds_obj.session.ssid = MICROUDS_SET_SESSION(MICROUDS_SESSION_DEFAULT);
        memset(&uds_obj.security, 0, sizeof(MicroUds_Security_t)); // 清空安全
        uds_obj.security.session = MICROUDS_SESSION_EXTEND;        // 扩展会话下才能访问
    }

    MicroUds_SecurityHandler(); // 安全管理
    MicroUds_ServiceHandler();  // 服务管理
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

        if (!((1 << uds_obj.session.ssid) & uds_obj.security.session))
        {
            uds_obj.security.step = 0;
            NegativeResponse(0x27, MICROUDS_NRC_CNC);
            return;
        }

        switch (uds_obj.security.step)
        {
        case 0:
            uds_obj.security.level = data[1]; // 请求的等级
            uds_obj.security.active = true;   // 既然都用了状态机，这个就是不是可以不要了
            uds_obj.security.step = 1;
            break;

        case 1:

            break;

        case 2: // 解析诊断仪响应
            memcpy(uds_obj.security.key[1], &data[1], MICROUDS_KEY_SIZE);
            uds_obj.security.step = 3;
            break;

        default:

            break;
        }
    }
    else
    { // 普通服务 ，后续可能还会有DID？RID？

        uds_obj.pdu.active = true;
        uds_obj.pdu.sid = data[0];
        uds_obj.pdu.sub_func = data[1];
        uds_obj.pdu.addr = addrType;
        uds_obj.pdu.len = 0;
        uds_obj.pdu.payload = NULL;

        if (len > 2)
        {
            uds_obj.pdu.payload = (uint8_t *)&data[2];
            uds_obj.pdu.len = (uint32_t)(len - 2);
        }

        uds_obj.service.active = true;
    }
}

/**
 * @brief 弱函数，用户重写 重新配置了cmake,弱符号不报错了哈哈
 *
 * @param pud
 * @param len
 * @return MicroUds_Status_t
 */
MicroUds_Status_t __attribute__((weak)) MicroUds_Transmit(const uint8_t *pdu, uint32_t len)
{
    (void)pdu;
    (void)len;

    return MICROUDS_OK;
}

/**
 * @brief 弱函数，由用户重写
 *
 */
void __attribute__((weak)) MicroUds_SecurityReqSeed(uint8_t level, uint8_t *out, uint32_t *len) // 请求种子
{
    (void)level;
    (void)out;
    (void)len;
}

/**
 * @brief 弱函数，用户重写
 *
 */
void __attribute__((weak)) MicroUds_SecurityGetKey(uint8_t *key, uint32_t *len)
{
    (void)key;
    (void)len;
}

bool MicroUds_SecurityVerity(void)
{
    for (uint32_t i = 0; i < MICROUDS_KEY_SIZE; i++)
    {
        if (uds_obj.security.key[0][i] != uds_obj.security.key[1][0])
        {
            return false;
        }
    }
    return true;
}

// 先把整个骨架搭起来，什么查找算法的后续再说吧，先跑起来验证架构再说

MicroUds_Status_t MicroUds_ServiceRegisterServ(MicruUds_ServiceTable_t *table,size_t size)
{
    if(table == NULL || size == 0 || size > MICROUDS_SERVICE_MAX_NUMBER)
    {
        return MICROUDS_PARAM_ERR;
    }

    for(uint32_t i = 0; i < size; i++)
    {
        uds_obj.service.serv[i].sid = table[i].sid;
        uds_obj.service.serv[i].ssid = table[i].session;
        uds_obj.service.serv[i].permis = table[i].permis;
        uds_obj.service.serv[i].is_vaild = true;

        memset(uds_obj.service.serv[i].sub_func,0,MICROUDS_SUBFUNC_MAX_NUMBER * sizeof(MicroUds_SubFunc_t ));
    }

    uds_obj.service.active = false;
    uds_obj.service.serv_num = (uint32_t)size; 

    return MICROUDS_OK;
}

MicroUds_Status_t MicroUds_ServiceRegisterSubFunc(uint8_t sid, MicroUds_SubFuncTable_t *table,size_t size)
{
    if(table == NULL || size == 0 || size > MICROUDS_SUBFUNC_MAX_NUMBER)
    {
        return MICROUDS_PARAM_ERR;
    }

    MicruUds_ServiceSub_t *service = NULL;

    bool is_found = false;

    for(uint32_t i = 0; i < uds_obj.service.serv_num; i++)
    {
        if(sid == uds_obj.service.serv[i].sid)
        {
            service = &uds_obj.service.serv[i];
            is_found = true;
            break;
        }
    }

    if(!is_found)
    {
        return MICROUDS_NOT_FOUND;
    }

    for(uint32_t i = 0; i < size; i++)
    {
        service->sub_func[i].callback = table[i].callback;
        service->sub_func[i].id = table[i].id;
        service->sub_func[i].Userdata = table[i].Userdata;
    }

    service->sub_func_num = (uint32_t)size;
    
    return MICROUDS_OK;
}
