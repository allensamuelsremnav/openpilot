#include "msg_logger.h"
#include <stdio.h>

// Write 10 bytes to a binary log file "test_logger.bin"
// On linux do hexdump -C test_logger.bin to check the contents.

int main() {
    char msg[] = "HelloWorld";
    MsgLogger& log = MsgLogger::get_instance();
    printf(">>Created log file `msg_log.bin' for a messge of size=%d bytes\n",
           (int)strlen(msg));
    log.raw_bytes((uint8_t*) msg, strlen(msg));
    return 0;
}