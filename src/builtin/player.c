#include <onix/types.h>
#include <onix/stdio.h>
#include <onix/syscall.h>
#include <onix/string.h>
#include <onix/fs.h>
#include <onix/sb16.h>

#define BUFLEN 0x4000

static char buf[BUFLEN];

// WAVE file header format
typedef struct wav_header_t
{
    u8 riff[4];       // RIFF string
    u32 chunksize;    // 32 + subchunksize
    u8 wave[4];       // WAVE
    u8 fmt[4];        // ASCII码表示的 fmt (0x666d7420)，最后一个是空格
    u32 format;       // 本块数据的大小，(对于PCM, 值为16)
    u16 type;         // 格式 1-PCM, 3- IEEE float ...
    u16 channels;     // 通道数
    u32 sample_rate;  // 采样率 sampling rate (blocks per second)
    u32 byterate;     // 码率 SampleRate * NumChannels * BitsPerSample/8
    u16 block_align;  // NumChannels * BitsPerSample/8
    u16 bits;         // 位数 8 bits, 16 bits
    u8 subchunk[4];   // DATA 或 FLLR
    u32 subchunksize; // 数据大小
} _packed wav_header_t;

#define TYPE_PCM 1

int main(int argc, char const *argv[])
{
    if (argc < 2)
    {
        return EOF;
    }

    fd_t sb16 = 0;
    fd_t fd = 0;
    int ret = EOF;

    fd = open((char *)argv[1], O_RDONLY, 0);
    if (fd < 0)
    {
        printf("File %s not exists.\n", argv[1]);
        return EOF;
    }

    wav_header_t header;
    ret = read(fd, (char *)&header, sizeof(header));
    if (ret != sizeof(header) || memcmp(header.wave, "WAVE", 4))
    {
        printf("Only wave files are supported\n");
        ret = -1;
        goto rollback;
    }

    if (header.type != TYPE_PCM)
    {
        printf("Only pcm format is supported\n");
        ret = -1;
        goto rollback;
    }

    if (header.sample_rate != 44100)
    {
        printf("support sample != 44100 Hz.\n");
        ret = -1;
        goto rollback;
    }

    u8 mode;
    if (header.bits == 8 && header.channels == 1)
    {
        mode = SB16_CMD_MONO8;
    }
    else if (header.bits == 16 && header.channels == 2)
    {
        mode = SB16_CMD_STEREO16;
    }
    else
    {
        printf("Cannot play channels %d and bits %d\n",
               header.channels,
               header.bits);
        ret = -1;
        goto rollback;
    }

    sb16 = open("/dev/sb16", O_WRONLY, 0);
    if (sb16 < 0)
    {
        printf("Can't open sb16\n");
        goto rollback;
    }

    ioctl(sb16, SB16_CMD_ON, 0);
    ioctl(sb16, mode, 0);
    ioctl(sb16, SB16_CMD_VOLUME, 0xff);

    while (1)
    {
        int len = read(fd, buf, BUFLEN);
        if (len < 0)
        {
            break;
        }
        write(sb16, buf, len);
    }

rollback:
    if (fd > 0)
    {
        close(fd);
    }
    if (sb16 > 0)
    {
        ioctl(sb16, SB16_CMD_OFF, 0);
        close(sb16);
    }
    return 0;
}
