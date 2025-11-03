/**
 * @file example.c
 * @brief Basic example for MicroUDS usage.
 */

#include "MicroUDS.h"
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                       Example Transmit Function                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief User-defined transmit function (replace with your CAN TX API)
 */
static int MyCAN_Transmit(uint8_t *data, size_t size)
{
    printf("[CAN TX] ");
    for (size_t i = 0; i < size; i++)
        printf("%02X ", data[i]);
    printf("\n");
    return 0; // 0 = success
}

/* -------------------------------------------------------------------------- */
/*                       Example Service Callback                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Example handler for service 0x10 (Diagnostic Session Control)
 */
static MicroUDS_Sta_t Example_Service_0x10(uint8_t *req, size_t len, void *param)
{
    (void)param;
    printf("[Service 0x10] Request len=%zu\n", len);

    // Example: just return a positive response
    return MicroUDS_PositiveResponse();
}

/* -------------------------------------------------------------------------- */
/*                               Service Table                                */
/* -------------------------------------------------------------------------- */

static MicroUDS_ServiceTable_t serviceTable[] = {
    {UDS_DIAGNOSTIC_SESSION_CONTROL, Example_Service_0x10, NULL},
};

/* -------------------------------------------------------------------------- */
/*                                Main Example                                */
/* -------------------------------------------------------------------------- */

int main(void)
{
    printf("=== MicroUDS Example Start ===\n");

    /* 1. Assign user transmit callback */

    /* 2. Initialize MicroUDS */
    if (MicroUDS_Init() != MICROUDS_OK)
    {
        printf("MicroUDS Init failed!\n");
        return -1;
    }

    /* 3. Register UDS Services */
    MicroUDS_RegisterService(serviceTable, sizeof(serviceTable) / sizeof(serviceTable[0]));

    /* 4. Simulate receiving a UDS frame */
    uint8_t testFrame[8] = {0x02, 0x10, 0x01, 0x00, 0, 0, 0, 0}; // SID = 0x10
    MicroUDS_ReceiveCallback(testFrame);

    /* 5. Main loop */
    for (;;)
    {
        MicroUDS_TimerHandler(); // periodic service logic
    }

    return 0;
}
