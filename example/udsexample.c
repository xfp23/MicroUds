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
static MicroUDS_NRC_t Example_Service_0x10(void *param)
{
    (void)param;
    printf("[Service 0x10] Request\n");

    // Example: just return a positive response
    return UDS_NRC_SUCCESS; // return postitive response
}

static MicroUDS_NRC_t Example_Service_0x01(void *param)
{
    (void)param;
    printf("[Session 0x01] Request\n");

    // Example: just return a positive response
    return UDS_NRC_SUCCESS; // return postitive response
}

/* -------------------------------------------------------------------------- */
/*                               Service Table                                */
/* -------------------------------------------------------------------------- */

static MicroUDS_ServiceTable_t serviceTable[] = {
    {UDS_DIAGNOSTIC_SESSION_CONTROL, Example_Service_0x10, NULL},
};

static MicroUDS_SessionTable_t sessionTable[] = {
    {UDS_SESSION_DEFAULT, Example_Service_0x01, NULL},
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

    MicroUDS_RegisterSession(UDS_DIAGNOSTIC_SESSION_CONTROL, sessionTable, sizeof(sessionTable) / sizeof(sessionTable[0]));
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
