#include "Isotp.h"

Isotp_Obj Isotp = {0};

/**
 * @brief 打包单帧 (Single Frame Encode)
 */
Isotp_Sta_t Isotp_PackSingleFrame(uint8_t *frame, const uint8_t *src, size_t size)
{
    if (frame == NULL || src == NULL)
        return ISOTP_ERR_PARAM;

    if (size == 0 || size > 7)
        return ISOTP_ERR_LENGTH;

    memset(frame, 0, 8);
    frame[0] = (uint8_t)(0x00 | (size & 0x0F));  // SF PCI
    memcpy(&frame[1], src, size);

    return ISOTP_OK;
}


Isotp_Sta_t Isotp_UnpackSingleFrame(const uint8_t *frame, uint8_t *dst, size_t *size)
{
    if (frame == NULL || dst == NULL || size == NULL)
        return ISOTP_ERR_PARAM;

    uint8_t len = frame[0] & 0x0F;  // 低4位是长度
    if (len == 0 || len > 7)
        return ISOTP_ERR_LENGTH;

    memcpy(dst, &frame[1], len);
    *size = len;

    return ISOTP_OK;
}


Isotp_Sta_t Isotp_PackFirstFrame(uint8_t *frame, const uint8_t *src, size_t size)
{
    if (frame == NULL || src == NULL)
        return ISOTP_ERR_PARAM;

    if (size <= 7 || size > 4095) // 首帧仅用于 >7 且 <= 4095 字节
        return ISOTP_ERR_LENGTH;

    memset(frame, 0, 8);

    // 第一字节：高4位0x1表示FirstFrame，低4位是总长度高4位
    frame[0] = 0x10 | ((size >> 8) & 0x0F);

    // 第二字节：长度低8位
    frame[1] = (uint8_t)(size & 0xFF);

    // 后6字节：放前6个数据
    memcpy(&frame[2], src, 6);

    return ISOTP_OK;
}

/**
 * @brief 解析首帧 (First Frame Decode)
 * @param dst            输出的数据缓冲区（至少 6 字节）
 * @param frame          输入的CAN帧（8字节）
 * @param total_size_out 输出：完整数据总长度（由帧头解析得出）
 * @return 状态码
 */
Isotp_Sta_t Isotp_UnpackFirstFrame(uint8_t *dst, const uint8_t *frame, size_t *total_size_out)
{
    if (dst == NULL || frame == NULL || total_size_out == NULL)
        return ISOTP_ERR_PARAM;

    // 第一字节：高4位=0x1 → First Frame
    uint8_t pci_type = (frame[0] & 0xF0) >> 4;
    if (pci_type != 0x1)
        return ISOTP_ERR_LENGTH; // 不是首帧

    // 提取总长度（12 bit）
    uint16_t total_len = ((frame[0] & 0x0F) << 8) | frame[1];
    if (total_len <= 7 || total_len > 4095)
        return ISOTP_ERR_LENGTH;

    // 前6个字节数据复制出去
    memcpy(dst, &frame[2], 6);

    *total_size_out = total_len;

    return ISOTP_OK;
}

Isotp_Sta_t Isotp_PackFlowControlFram(uint8_t *dst,const )
{

}
