/*
 * playback_test.c — 播放指定的 WAV 音频文件
 *
 * 用法: ./playback_test <file.wav> [pcm设备]
 *       默认设备 plughw:0,0 (plug 插件在格式/采样率与硬件不符时自动转换)
 *
 * 流程: 解析 WAV 头 → 取 rate/channels/format
 *       → snd_pcm_open → hw params(就近协商) → sw params(启动阈值)
 *       → 按 period 循环 fread + snd_pcm_writei(欠载自动恢复) → drain → close
 */
#include <alsa/asoundlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PCM_DEVICE_NAME "plughw:0,0" /* plug 前缀: 参数不匹配硬件时自动转换 */
#define BUFFER_TIME_US 500000u       /* 环形缓冲 500ms */
#define PERIOD_TIME_US 100000u       /* 周期 100ms, 即一次唤醒搬运的量 */

/* ------------------------------------------------------------------ */
/* WAV 文件描述 (从头解析出来)                                          */
/* ------------------------------------------------------------------ */
typedef struct
{
        uint16_t fmt_tag; /* 1=PCM, 3=IEEE float, 0xFFFE=extensible */
        uint16_t channels;
        uint32_t rate;
        uint16_t block_align; /* 一帧字节数 = channels * bits/8 */
        uint16_t bits;
        snd_pcm_format_t format; /* 映射后的 ALSA 格式 */
        long data_offset;        /* data chunk 载荷的文件偏移 */
        uint32_t data_bytes;
        uint32_t total_frames; /* = data_bytes / block_align */
} wav_info_t;

/* ------------------------------------------------------------------ */
/* 硬件参数: 前半是期望值(来自 WAV), 后半是协商后实际生效值              */
/* ------------------------------------------------------------------ */
typedef struct
{
        /* in: 期望值 */
        unsigned int rate;
        unsigned int channels;
        snd_pcm_format_t format;
        snd_pcm_access_t access;
        unsigned int buffer_time; /* us */
        unsigned int period_time; /* us */
        int sw_resampled;         /* 允许 alsa-lib 软件重采样 */

        /* out: 协商后实际值 */
        snd_pcm_uframes_t buffer_size;
        snd_pcm_uframes_t period_size;
        unsigned int frame_bytes;
} hw_config_t;

/* WAV 是小端; 显式按小端读, 不依赖主机字节序 */
static uint16_t rd_u16(const uint8_t * p)
{
        return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t rd_u32(const uint8_t * p)
{
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* WAV fmt_tag + bits → ALSA 格式 */
static snd_pcm_format_t wav_to_alsa_format(uint16_t tag, uint16_t bits)
{
        if(tag == 3) /* IEEE float */
                return bits == 32 ? SND_PCM_FORMAT_FLOAT_LE : SND_PCM_FORMAT_UNKNOWN;
        if(tag != 1) /* 只支持 PCM */
                return SND_PCM_FORMAT_UNKNOWN;

        switch(bits) {
                case 8: return SND_PCM_FORMAT_U8;
                case 16: return SND_PCM_FORMAT_S16_LE;
                case 24: return SND_PCM_FORMAT_S24_3LE; /* WAV 24bit 是 3 字节紧凑存储 */
                case 32: return SND_PCM_FORMAT_S32_LE;
                default: return SND_PCM_FORMAT_UNKNOWN;
        }
}

/*
 * 解析 RIFF/WAVE: 按 chunk 遍历, 找到 "fmt " 与 "data"
 * 注意不能假设 data 紧跟 fmt —— 很多文件中间夹着 LIST/INFO 块
 */
static int wav_parse(const char * path, FILE * fp, wav_info_t * w)
{
        uint8_t riff[12], chunk[8], fmt[40];
        int got_fmt = 0, got_data = 0;

        if(fread(riff, 1, 12, fp) != 12 || memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
                fprintf(stderr, "%s: 不是 RIFF/WAVE 文件\n", path);
                return -EINVAL;
        }

        while(fread(chunk, 1, 8, fp) == 8) {
                uint32_t size = rd_u32(chunk + 4);
                long next     = ftell(fp) + (long)((size + 1) & ~1u); /* chunk 偶数对齐 */

                if(memcmp(chunk, "fmt ", 4) == 0) {
                        if(size < 16 || size > sizeof(fmt)) {
                                fprintf(stderr, "%s: fmt 块长度异常 (%u)\n", path, size);
                                return -EINVAL;
                        }
                        if(fread(fmt, 1, size, fp) != size) return -EIO;

                        w->fmt_tag     = rd_u16(fmt);
                        w->channels    = rd_u16(fmt + 2);
                        w->rate        = rd_u32(fmt + 4);
                        w->block_align = rd_u16(fmt + 12);
                        w->bits        = rd_u16(fmt + 14);

                        /* WAVE_FORMAT_EXTENSIBLE: 真实格式在 SubFormat GUID 头 2 字节 */
                        if(w->fmt_tag == 0xFFFE && size >= 26) w->fmt_tag = rd_u16(fmt + 24);

                        w->format = wav_to_alsa_format(w->fmt_tag, w->bits);
                        if(w->format == SND_PCM_FORMAT_UNKNOWN) {
                                fprintf(stderr, "%s: 不支持的格式 (tag=%u, bits=%u)\n", path, w->fmt_tag, w->bits);
                                return -EINVAL;
                        }
                        if(w->channels == 0 || w->rate == 0 || w->block_align == 0) {
                                fprintf(stderr, "%s: fmt 块字段非法\n", path);
                                return -EINVAL;
                        }
                        got_fmt = 1;
                } else if(memcmp(chunk, "data", 4) == 0) {
                        w->data_offset = ftell(fp);
                        w->data_bytes  = size;
                        got_data       = 1;
                }

                if(got_fmt && got_data) break;
                if(fseek(fp, next, SEEK_SET) != 0) break;
        }

        if(!got_fmt || !got_data) {
                fprintf(stderr, "%s: 缺少 %s 块\n", path, !got_fmt ? "fmt " : "data");
                return -EINVAL;
        }

        w->total_frames = w->data_bytes / w->block_align;
        return 0;
}

/* 用 WAV 参数填充期望值, 其余给安全默认 */
static void init_hw_params(hw_config_t * cfg, const wav_info_t * w)
{
        memset(cfg, 0, sizeof(*cfg));
        cfg->rate         = w->rate;
        cfg->channels     = w->channels;
        cfg->format       = w->format;
        cfg->access       = SND_PCM_ACCESS_RW_INTERLEAVED;
        cfg->buffer_time  = BUFFER_TIME_US;
        cfg->period_time  = PERIOD_TIME_US;
        cfg->sw_resampled = 1;
        cfg->frame_bytes  = w->block_align;
}

/*
 * 以期望值向硬件"就近"协商参数, 并把实际生效值写回 cfg
 * (params 对象用 alloca 栈分配 —— 仅本次设置期间存活, ALSA 惯例)
 */
static int set_near_hwparams(snd_pcm_t * handle, hw_config_t * cfg)
{
        snd_pcm_hw_params_t * params;
        unsigned int rrate = cfg->rate;
        snd_pcm_uframes_t size;
        int err, dir = 0; /* dir: *_near 的取整方向提示, 0 = 就近 */

        snd_pcm_hw_params_alloca(&params);

        err = snd_pcm_hw_params_any(handle, params);
        if(err < 0) {
                fprintf(stderr, "no configurations available: %s\n", snd_strerror(err));
                return err;
        }

        err = snd_pcm_hw_params_set_rate_resample(handle, params, cfg->sw_resampled);
        if(err < 0) {
                fprintf(stderr, "resample setup failed: %s\n", snd_strerror(err));
                return err;
        }

        err = snd_pcm_hw_params_set_access(handle, params, cfg->access);
        if(err < 0) {
                fprintf(stderr, "access type not available: %s\n", snd_strerror(err));
                return err;
        }

        err = snd_pcm_hw_params_set_format(handle, params, cfg->format);
        if(err < 0) {
                fprintf(stderr, "sample format not available: %s\n", snd_strerror(err));
                return err;
        }

        err = snd_pcm_hw_params_set_channels(handle, params, cfg->channels);
        if(err < 0) {
                fprintf(stderr, "channels (%u) not available: %s\n", cfg->channels, snd_strerror(err));
                return err;
        }

        err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, &dir);
        if(err < 0) {
                fprintf(stderr, "rate %uHz not available: %s\n", cfg->rate, snd_strerror(err));
                return err;
        }
        if(rrate != cfg->rate) /* plughw 会重采样, 不算致命错误, 警告后继续 */
                fprintf(stderr, "warning: rate %uHz -> %uHz (由插件转换)\n", cfg->rate, rrate);

        err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &cfg->buffer_time, &dir);
        if(err < 0) {
                fprintf(stderr, "unable to set buffer time: %s\n", snd_strerror(err));
                return err;
        }

        err = snd_pcm_hw_params_set_period_time_near(handle, params, &cfg->period_time, &dir);
        if(err < 0) {
                fprintf(stderr, "unable to set period time: %s\n", snd_strerror(err));
                return err;
        }

        err = snd_pcm_hw_params(handle, params);
        if(err < 0) {
                fprintf(stderr, "unable to set hw params: %s\n", snd_strerror(err));
                return err;
        }

        /* 取回实际生效值, 后面写数据循环要用 */
        err = snd_pcm_hw_params_get_buffer_size(params, &size);
        if(err < 0) return err;
        cfg->buffer_size = size;

        dir = 0;
        err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
        if(err < 0) return err;
        cfg->period_size = size;

        /* 一帧字节数以实际协商的格式为准 */
        cfg->frame_bytes = (unsigned int)(snd_pcm_format_physical_width(cfg->format) / 8) * cfg->channels;
        return 0;
}

/*
 * 软件参数 —— 决定 ALSA 何时自动启动/停止 DMA:
 *   start_threshold: 缓冲填到多少帧才开始播。设为 buffer_size:
 *                    攒满再播, 避免刚启动就 underrun
 *   avail_min:       writei 至少要有这么多空位才返回 (默认 period 即可)
 *   stop_threshold:  播完多少帧算结束 (buffer_size = 自然播完)
 */
static int set_sw_params(snd_pcm_t * handle, const hw_config_t * cfg)
{
        snd_pcm_sw_params_t * sw;
        int err;

        snd_pcm_sw_params_alloca(&sw);

        err = snd_pcm_sw_params_current(handle, sw);
        if(err < 0) {
                fprintf(stderr, "unable to get sw params: %s\n", snd_strerror(err));
                return err;
        }

        err = snd_pcm_sw_params_set_start_threshold(handle, sw, cfg->buffer_size);
        if(err < 0) goto fail;
        err = snd_pcm_sw_params_set_avail_min(handle, sw, cfg->period_size);
        if(err < 0) goto fail;
        err = snd_pcm_sw_params_set_stop_threshold(handle, sw, cfg->buffer_size);
        if(err < 0) goto fail;

        return snd_pcm_sw_params(handle, sw);
fail:
        fprintf(stderr, "unable to set sw params: %s\n", snd_strerror(err));
        return err;
}

/* 播放主循环: 按 period 读文件 → writei, 欠载/挂起自动恢复 */
static int play_wav(snd_pcm_t * pcm, FILE * fp, const wav_info_t * w, const hw_config_t * cfg)
{
        const size_t frame_bytes = cfg->frame_bytes;
        uint8_t * buf;
        uint32_t remaining = w->total_frames;
        uint32_t played    = 0;
        int rc             = 0;

        if(fseek(fp, w->data_offset, SEEK_SET) != 0) {
                perror("seek to data chunk");
                return -errno;
        }

        buf = malloc(cfg->period_size * frame_bytes);
        if(!buf) return -ENOMEM;

        while(remaining > 0) {
                snd_pcm_uframes_t want = remaining < cfg->period_size ? remaining : cfg->period_size;
                size_t got             = fread(buf, frame_bytes, (size_t)want, fp);
                const uint8_t * p      = buf;

                if(got == 0) break; /* EOF */
                remaining -= (uint32_t)got;

                while(got > 0) {
                        snd_pcm_sframes_t n = snd_pcm_writei(pcm, p, got);

                        if(n < 0) {
                                /* -EPIPE(欠载)/-ESTRPIPE(挂起) 一键恢复后重试同一块数据 */
                                n = snd_pcm_recover(pcm, (int)n, 0);
                                if(n < 0) {
                                        fprintf(stderr, "write failed: %s\n", snd_strerror((int)n));
                                        rc = (int)n;
                                        goto out;
                                }
                                continue;
                        }
                        p += (size_t)n * frame_bytes;
                        got -= (snd_pcm_uframes_t)n;
                }
                played += (uint32_t)want - remaining % want; /* 仅用于进度 */
        }

        snd_pcm_drain(pcm); /* 等缓冲区播完再关 */
        printf("played %u/%u frames (%.1fs)\n", played, w->total_frames, (double)w->total_frames / w->rate);
out:
        free(buf);
        return rc;
}

int main(int argc, char ** argv)
{
        const char * dev = PCM_DEVICE_NAME;
        wav_info_t wav;
        hw_config_t cfg;
        FILE * fp       = NULL;
        snd_pcm_t * pcm = NULL;
        int rc;

        if(argc < 2) {
                fprintf(stderr, "usage: %s <file.wav> [pcm device, default %s]\n", argv[0], PCM_DEVICE_NAME);
                return EXIT_FAILURE;
        }
        if(argc > 2) dev = argv[2];

        memset(&wav, 0, sizeof(wav));

        fp = fopen(argv[1], "rb");
        if(!fp) {
                perror(argv[1]);
                return EXIT_FAILURE;
        }

        /* 1. 解析 WAV 头 */
        rc = wav_parse(argv[1], fp, &wav);
        if(rc < 0) goto out;

        printf("WAV: %s\n", argv[1]);
        printf("  rate=%uHz channels=%u bits=%u format=%s frames=%u (%.1fs)\n", wav.rate, wav.channels, wav.bits,
               snd_pcm_format_name(wav.format), wav.total_frames, (double)wav.total_frames / wav.rate);

        /* 2. 打开 PCM 设备 */
        rc = snd_pcm_open(&pcm, dev, SND_PCM_STREAM_PLAYBACK, 0);
        if(rc < 0) {
                fprintf(stderr, "failed to open %s: %s\n", dev, snd_strerror(rc));
                goto out;
        }

        /* 3. 配置硬件/软件参数 */
        init_hw_params(&cfg, &wav);
        rc = set_near_hwparams(pcm, &cfg);
        if(rc < 0) goto out;
        rc = set_sw_params(pcm, &cfg);
        if(rc < 0) goto out;

        printf("PCM: %s buffer=%lu frames period=%lu frames\n", dev, (unsigned long)cfg.buffer_size,
               (unsigned long)cfg.period_size);

        /* 4. 播放 */
        rc = play_wav(pcm, fp, &wav, &cfg);

out:
        if(pcm) snd_pcm_close(pcm);
        if(fp) fclose(fp);
        return rc < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
