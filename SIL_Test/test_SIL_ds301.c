#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define MAX_MESSAGES 10
#define MSG_LENGTH 8
typedef struct 
{
    uint32_t can_id;
    uint64_t can_data;
} can_message_t;

can_message_t test_can_messages[MAX_MESSAGES];

void send_can_message(int index) 
{
    char cmd[256];
    char data_str[32] = "";
    uint32_t can_id = test_can_messages[index].can_id & 0x7FF;  
    uint64_t data = test_can_messages[index].can_data;          

    printf("Extracted CAN ID: %03X\n", can_id);
    printf("Extracted Data: %016llX\n", data);

    for (int j = MSG_LENGTH-1; j >= 0 ; j--) 
    {
        char temp[4];
        uint8_t byte = (data >> (j * 8)) & 0xFF; 
        snprintf(temp, sizeof(temp), "%02X", byte);
        strcat(data_str, temp);
    }

    snprintf(cmd, sizeof(cmd), "cansend can0 %03X#%s", can_id, data_str);
    printf("Formatted CAN Message: %s\n", cmd);
}

int main()
{
    test_can_messages[0].can_id = 0x610;
    test_can_messages[0].can_data = 0x2F011a0000000000;

    printf("Running standalone CAN message test...\n");

    send_can_message(0);

    return 0;
}
