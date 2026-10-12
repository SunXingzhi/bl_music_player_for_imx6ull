/**
 * @file bl_wav.c
 * @brief WAV 播放便利层: 文件解析 + 独立播放线程 + 事件回调
 *
 * 回调运行在库线程上下文, LVGL 侧须用 lv_async_call() 转投 UI 线程。
 */

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bl_audio.h"

#define WAV_PERIOD_FRAMES	4096		/**< 每次读文件的帧数 */
#define WAV_DEFAULT_DEVICE	"plughw:1,1"	/**< ASRC 前端: 任意采样率 */

/* ------------------------------------------------------------------ */
/* WAV 头解析(按 RIFF chunk 遍历, 兼容中间夹 LIST/INFO 块)               */
/* ------------------------------------------------------------------ */

/** WAV 文件描述 */
typedef struct {
	FILE			*fp;
	uint16_t		channels;
	uint32_t		rate;
	uint16_t		block_align;
	snd_pcm_format_t	format;
	long			data_offset;
	uint32_t		total_frames;
} wav_info_t;

/**
 * @brief 小端读 16bit
 * @return 值
 */
static uint16_t rd_u16(const uint8_t *p)
{
	return (uint16_t)(p[0] | (p[1] << 8));
}

/**
 * @brief 小端读 32bit
 * @return 值
 */
static uint32_t rd_u32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief WAV fmt_tag + bits -> ALSA 采样格式
 * @return 格式; UNKNOWN 不支持
 */
static snd_pcm_format_t wav_to_format(uint16_t tag, uint16_t bits)
{
	if (tag == 3)			/* IEEE float */
		return bits == 32 ? SND_PCM_FORMAT_FLOAT_LE :
			SND_PCM_FORMAT_UNKNOWN;
	if (tag != 1)			/* 仅 PCM */
		return SND_PCM_FORMAT_UNKNOWN;
	switch (bits) {
	case 8:		return SND_PCM_FORMAT_U8;
	case 16:	return SND_PCM_FORMAT_S16_LE;
	case 24:	return SND_PCM_FORMAT_S24_3LE;
	case 32:	return SND_PCM_FORMAT_S32_LE;
	default:	return SND_PCM_FORMAT_UNKNOWN;
	}
}

/**
 * @brief 解析 WAV 头, 成功后文件指针停在 data 载荷起始
 * @return 0 成功; 负值 失败
 */
static int wav_parse(FILE *fp, wav_info_t *w)
{
	uint8_t riff[12], chunk[8], fmt[40];
	int got_fmt = 0, got_data = 0;

	memset(w, 0, sizeof(*w));

	if (fread(riff, 1, 12, fp) != 12 ||
	    memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0)
		return -EINVAL;

	while (fread(chunk, 1, 8, fp) == 8) {
		uint32_t size = rd_u32(chunk + 4);
		long next = ftell(fp) + (long)((size + 1) & ~1u);

		if (memcmp(chunk, "fmt ", 4) == 0) {
			uint16_t tag;

			if (size < 16 || size > sizeof(fmt) ||
			    fread(fmt, 1, size, fp) != size)
				return -EINVAL;
			tag = rd_u16(fmt);
			if (tag == 0xFFFE && size >= 26)
				tag = rd_u16(fmt + 24);	/* extensible 子格式 */
			w->channels = rd_u16(fmt + 2);
			w->rate = rd_u32(fmt + 4);
			w->block_align = rd_u16(fmt + 12);
			w->format = wav_to_format(tag, rd_u16(fmt + 14));
			if (w->format == SND_PCM_FORMAT_UNKNOWN ||
			    w->channels == 0 || w->rate == 0 ||
			    w->block_align == 0)
				return -EINVAL;
			got_fmt = 1;
		} else if (memcmp(chunk, "data", 4) == 0) {
			w->data_offset = ftell(fp);
			w->total_frames = size / w->block_align;
			got_data = 1;
		}

		if (got_fmt && got_data)
			break;
		if (fseek(fp, next, SEEK_SET) != 0)
			break;
	}
	if (!got_fmt || !got_data)
		return -EINVAL;
	return 0;
}

/* ------------------------------------------------------------------ */
/* 播放线程(单实例)                                                     */
/* ------------------------------------------------------------------ */

/** 播放任务(单实例) */
static struct {
	pthread_t		tid;
	pthread_mutex_t		lock;
	int			active;
	bl_sink_t		*sink;
	char			device[32];
	void			(*cb)(int evt, int detail, void *user);
	void			*user;
} g_wav = {
	.lock = PTHREAD_MUTEX_INITIALIZER,
	.device = WAV_DEFAULT_DEVICE,
};

/**
 * @brief WAV 播放线程主体: 解析 -> sink 写入 -> drain -> 回调
 * @param arg 文件路径(堆字符串, 线程内释放)
 * @return NULL
 */
static void *wav_thread(void *arg)
{
	char *path = arg;
	wav_info_t w;
	FILE *fp;
	uint8_t *buf;
	uint32_t remaining;
	void (*cb)(int evt, int detail, void *user);
	void *user;
	int rc = 0;
	int detail = 0;
	int evt = BL_WAV_EVT_DONE;

	fp = fopen(path, "r");
	free(path);
	if (fp == NULL) {
		evt = BL_WAV_EVT_ERROR;
		detail = -errno;
		goto out_cb;
	}
	if (wav_parse(fp, &w) < 0) {
		evt = BL_WAV_EVT_ERROR;
		detail = -EINVAL;
		goto out_fp;
	}
	if (fseek(fp, w.data_offset, SEEK_SET) != 0) {
		evt = BL_WAV_EVT_ERROR;
		detail = -errno;
		goto out_fp;
	}

	pthread_mutex_lock(&g_wav.lock);
	g_wav.sink = bl_sink_open(g_wav.device, w.rate, w.channels, w.format);
	pthread_mutex_unlock(&g_wav.lock);
	if (g_wav.sink == NULL) {
		evt = BL_WAV_EVT_ERROR;
		detail = -ENODEV;
		goto out_fp;
	}

	buf = malloc(WAV_PERIOD_FRAMES * w.block_align);
	if (buf == NULL) {
		evt = BL_WAV_EVT_ERROR;
		detail = -ENOMEM;
		goto out_sink;
	}

	remaining = w.total_frames;
	while (remaining > 0) {
		uint32_t want = remaining < WAV_PERIOD_FRAMES ?
			remaining : WAV_PERIOD_FRAMES;
		size_t got = fread(buf, w.block_align, want, fp);

		if (got == 0)
			break;		/* EOF */
		remaining -= got;
		rc = bl_sink_write(g_wav.sink, buf, got);
		if (rc < 0)
			break;		/* 被 stop 或硬件错误 */
	}
	free(buf);

	if (rc == 0)
		bl_sink_drain(g_wav.sink);
	else {
		evt = BL_WAV_EVT_ERROR;
		detail = -EIO;
	}

out_sink:
	pthread_mutex_lock(&g_wav.lock);
	bl_sink_close(g_wav.sink);
	g_wav.sink = NULL;
	pthread_mutex_unlock(&g_wav.lock);
out_fp:
	fclose(fp);
out_cb:
	/* 先摘回调再放锁: 避免回调里再调 bl_wav_play 造成死锁 */
	pthread_mutex_lock(&g_wav.lock);
	cb = g_wav.cb;
	user = g_wav.user;
	g_wav.active = 0;
	pthread_mutex_unlock(&g_wav.lock);
	if (cb != NULL)
		cb(evt, detail, user);
	return NULL;
}

/**
 * @brief 播放一个 WAV 文件(单实例, 新播放先停旧任务)
 * @return 0 已启动; 负值 启动失败
 */
int bl_wav_play(const char *path, const char *device,
	void (*cb)(int evt, int detail, void *user), void *user)
{
	char *path_copy;
	int ret;

	if (path == NULL)
		return -EINVAL;

	path_copy = strdup(path);
	if (path_copy == NULL)
		return -ENOMEM;

	pthread_mutex_lock(&g_wav.lock);

	/* 先停旧任务(若在跑) */
	if (g_wav.active) {
		if (g_wav.sink != NULL)
			bl_sink_stop(g_wav.sink);
		pthread_mutex_unlock(&g_wav.lock);
		pthread_join(g_wav.tid, NULL);
		pthread_mutex_lock(&g_wav.lock);
	}

	g_wav.cb = cb;
	g_wav.user = user;
	snprintf(g_wav.device, sizeof(g_wav.device), "%s",
		device != NULL ? device : WAV_DEFAULT_DEVICE);

	ret = pthread_create(&g_wav.tid, NULL, wav_thread, path_copy);
	if (ret != 0) {
		free(path_copy);
		pthread_mutex_unlock(&g_wav.lock);
		return -ret;
	}
	g_wav.active = 1;
	pthread_mutex_unlock(&g_wav.lock);
	return 0;
}

/**
 * @brief 停止当前 WAV 播放(幂等, 等线程收尾)
 */
void bl_wav_stop(void)
{
	pthread_mutex_lock(&g_wav.lock);
	if (!g_wav.active) {
		pthread_mutex_unlock(&g_wav.lock);
		return;
	}
	if (g_wav.sink != NULL)
		bl_sink_stop(g_wav.sink);
	pthread_mutex_unlock(&g_wav.lock);
	pthread_join(g_wav.tid, NULL);
}
