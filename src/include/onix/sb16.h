#ifndef ONIX_SB16_H
#define ONIX_SB16_H

typedef enum sb16_cmd_t
{
    SB16_CMD_ON = 1,   // 声卡开启
    SB16_CMD_OFF,      // 声卡关闭
    SB16_CMD_MONO8,    // 八位单声道模式
    SB16_CMD_STEREO16, // 16位立体声模式
    SB16_CMD_VOLUME,   // 设置音量
} sb16_cmd_t;

#endif