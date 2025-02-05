#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

MODULE_AUTHOR("Emad Shamsaki");
MODULE_DESCRIPTION("HAL DS301 CANopen Interface Layer");
MODULE_LICENSE("Apache");

#define MAX_MESSAGES 50

// HAL pins & parameters
typedef struct 
{
    hal_bit_t *send;
    hal_bit_t *status;

    // Store CAN ID, Data Low, Data High
    hal_u32_t can_commands[MAX_MESSAGES][3]; 

    // Store message length (DLC)
    hal_u32_t can_dlc[MAX_MESSAGES]; 
    hal_s32_t command_count;
} ds301_t;

static ds301_t *ds301_data;
static int comp_id;
static pthread_t can_thread;
static int can_thread_running = 1;
static int can_trigger = 0;

void *can_send_task(void *arg) 
{
    while (can_thread_running) 
    {
        if (can_trigger) 
        {
            rtapi_print_msg(RTAPI_MSG_INFO, "ds301: Sending %d CAN messages...\n", ds301_data->command_count);

            for (int i = 0; i < ds301_data->command_count; i++) 
            {
                char cmd[256];
                char data_str[32] = "";

                // Extract 11-bit CAN ID
                uint32_t can_id = ds301_data->can_commands[i][0] & 0x7FF; 
                uint64_t data = ((uint64_t)ds301_data->can_commands[i][2] << 32) | ds301_data->can_commands[i][1];

                uint8_t dlc = ds301_data->can_dlc[i];
                if (dlc == 0) 
                {
                    dlc = 8;
                }

                // Extract Each Byte in Correct Order
                for (int j = dlc - 1; j >= 0; j--) 
                {
                    char temp[4];
                    uint8_t byte = (data >> (j * 8)) & 0xFF;
                    snprintf(temp, sizeof(temp), "%02X", byte);
                    strcat(data_str, temp);
                }

                // Format and Send CAN Message
                snprintf(cmd, sizeof(cmd), "cansend can0 %03X#%s", can_id, data_str);
           int result = system(cmd);
                *(ds301_data->status) = (result == 0);
            }

            *(ds301_data->send) = 0;
            can_trigger = 0; 
        }
        usleep(1000); 
    }
    return NULL;
}

void write_all(void *arg, long period) 
{
    if (*(ds301_data->send)) 
    {
        can_trigger = 1; 
    }
}

int rtapi_app_main(void) 
{
    comp_id = hal_init("ds301");
    if (comp_id < 0) 
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "ds301: ERROR: hal_init() failed\n");
        return -1;
    }

    ds301_data = hal_malloc(sizeof(ds301_t));
    if (!ds301_data) 
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "ds301: ERROR: hal_malloc() failed\n");
        return -1;
    }

    memset(ds301_data, 0, sizeof(ds301_t));

    if (hal_pin_bit_new("ds301.send", HAL_IN, &(ds301_data->send), comp_id) != 0) 
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "ds301: ERROR: Cannot create send pin\n");
        return -1;
    }

    if (hal_pin_bit_new("ds301.status", HAL_OUT, &(ds301_data->status), comp_id) != 0) 
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "ds301: ERROR: Cannot create status pin\n");
        return -1;
    }

    for (int i = 0; i < MAX_MESSAGES; i++) 
    {
        char param_name[32];

        // Store CAN ID
        snprintf(param_name, sizeof(param_name), "ds301.can_command_%d_id", i);
        if (hal_param_u32_new(param_name, HAL_RW, &(ds301_data->can_commands[i][0]), comp_id) != 0) 
        {
            return -1;
        }

        // Store CAN Data Low
        snprintf(param_name, sizeof(param_name), "ds301.can_command_%d_data_low", i);
        if (hal_param_u32_new(param_name, HAL_RW, &(ds301_data->can_commands[i][1]), comp_id) != 0) 
        {
            return -1;
        }

        //Store CAN Data High
        snprintf(param_name, sizeof(param_name), "ds301.can_command_%d_data_high", i);
        if (hal_param_u32_new(param_name, HAL_RW, &(ds301_data->can_commands[i][2]), comp_id) != 0) 
        {
            return -1;
        }
     // Store DLC (Data Length Code)
        snprintf(param_name, sizeof(param_name), "ds301.can_command_%d_dlc", i);
        if (hal_param_u32_new(param_name, HAL_RW, &(ds301_data->can_dlc[i]), comp_id) != 0) 
        {
            return -1;
        }

        //Set Default DLC to 8
        ds301_data->can_dlc[i] = 8;
    }

    if (hal_param_s32_new("ds301.command_count", HAL_RW, &(ds301_data->command_count), comp_id) != 0) 
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "ds301: ERROR: Cannot create command count parameter\n");
        return -1;
    }

    if (pthread_create(&can_thread, NULL, can_send_task, NULL) != 0) 
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "ds301: ERROR: Cannot create CAN thread\n");
        return -1;
    }

    if (hal_export_funct("ds301.write_all", write_all, ds301_data, 1, 0, comp_id) != 0) 
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "ds301: ERROR: Cannot export function\n");
        return -1;
    }

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) 
{
    can_thread_running = 0;
    pthread_join(can_thread, NULL);
    hal_exit(comp_id);
}

