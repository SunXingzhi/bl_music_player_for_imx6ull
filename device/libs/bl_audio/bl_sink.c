/**
 * @file bl_sink.c
 * @brief PCM 输出流封装(snd_pcm_* writei 阻塞循环 + 暂停/停止状态机)
 *
 * 模型: 调用者线程(数据源)直接调 bl_sink_write(), 阻塞直至写完(自然背压);
 *       pause/stop 通过状态位 + 条件变量协作式生效。
 */

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bl_audio.h"

#define BL_SINK_RUN	0	/**< 正常写入 */
#define BL_SINK_PAUSE	1	/**< 后续 write 挂起 */
#define BL_SINK_STOP	2	/**< 唤醒并让 write 返回负值 */

/** PCM 输出流实例 */
struct bl_sink {
	snd_pcm_t		*pcm;
	pthread_mutex_t		lock;
	pthread_cond_t		cond;
	int			state;
};

/**
 * @brief 打开 PCM 输出流并就近协商参数
 * @return 句柄; NULL 失败
 */
bl_sink_t *bl_sink_open(const char *device, unsigned int rate,
	unsigned int channels, snd_pcm_format_t format)
{
	bl_sink_t *s;
	snd_pcm_hw_params_t *hw;
	snd_pcm_sw_params_t *sw;
	snd_pcm_uframes_t bufsize;
	unsigned int rrate = rate;
	int err;

	s = calloc(1, sizeof(*s));
	if (s == NULL)
		return NULL;

	err = snd_pcm_open(&s->pcm, device, SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0)
		goto fail;

	snd_pcm_hw_params_alloca(&hw);
	err = snd_pcm_hw_params_any(s->pcm, hw);
	if (err < 0)
		goto fail;
	err = snd_pcm_hw_params_set_access(s->pcm, hw,
		SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0)
		goto fail;
	err = snd_pcm_hw_params_set_format(s->pcm, hw, format);
	if (err < 0)
		goto fail;
	err = snd_pcm_hw_params_set_channels(s->pcm, hw, channels);
	if (err < 0)
		goto fail;
	err = snd_pcm_hw_params_set_rate_near(s->pcm, hw, &rrate, NULL);
	if (err < 0)
		goto fail;
	if (rrate != rate) {
		/* 走 ASRC 前端(plughw:1,1)时由插件转换, 直连时此为硬性差异 */
		snd_pcm_hw_params_set_rate_resample(s->pcm, hw, 1);
		err = snd_pcm_hw_params_set_rate(s->pcm, hw, rate, 0);
		if (err < 0)
			goto fail;
	}
	err = snd_pcm_hw_params(s->pcm, hw);
	if (err < 0)
		goto fail;

	/* sw: 攒满 buffer 再启动, 避免起步即欠载 */
	snd_pcm_sw_params_alloca(&sw);
	err = snd_pcm_sw_params_current(s->pcm, sw);
	if (err < 0)
		goto fail;
	err = snd_pcm_hw_params_get_buffer_size(hw, &bufsize);
	if (err < 0)
		goto fail;
	err = snd_pcm_sw_params_set_start_threshold(s->pcm, sw, bufsize);
	if (err < 0)
		goto fail;
	err = snd_pcm_sw_params_set_avail_min(s->pcm, sw, bufsize / 4);
	if (err < 0)
		goto fail;
	err = snd_pcm_sw_params(s->pcm, sw);
	if (err < 0)
		goto fail;

	pthread_mutex_init(&s->lock, NULL);
	pthread_cond_init(&s->cond, NULL);
	s->state = BL_SINK_RUN;
	return s;
fail:
	if (s->pcm != NULL)
		snd_pcm_close(s->pcm);
	free(s);
	return NULL;
}

/**
 * @brief 阻塞式写入一批帧(交错)
 * @return 0 全部写入; -1 失败或被 stop 打断
 */
int bl_sink_write(bl_sink_t *s, const void *buf, uint32_t frames)
{
	const uint8_t *p = buf;
	int frame_bytes;
	int rc = 0;

	if (s == NULL || buf == NULL)
		return -EINVAL;

	frame_bytes = snd_pcm_frames_to_bytes(s->pcm, 1);

	pthread_mutex_lock(&s->lock);
	while (frames > 0) {
		snd_pcm_sframes_t n;

		/* 协作式暂停/停止 */
		while (s->state == BL_SINK_PAUSE)
			pthread_cond_wait(&s->cond, &s->lock);
		if (s->state == BL_SINK_STOP) {
			rc = -1;
			break;
		}

		/* writei 可能阻塞, 期间放锁, 允许 pause/stop 并发生效 */
		pthread_mutex_unlock(&s->lock);
		n = snd_pcm_writei(s->pcm, p, frames);
		pthread_mutex_lock(&s->lock);

		if (n < 0) {
			n = snd_pcm_recover(s->pcm, (int)n, 0);
			if (n < 0) {
				rc = -1;
				break;
			}
			continue;	/* 恢复后重写同一块 */
		}
		p += (long)n * frame_bytes;
		frames -= (uint32_t)n;
	}
	pthread_mutex_unlock(&s->lock);
	return rc;
}

/**
 * @brief 暂停(当前阻塞中的 write 最多再写完一块)
 * @return 0 成功
 */
int bl_sink_pause(bl_sink_t *s)
{
	if (s == NULL)
		return -EINVAL;
	pthread_mutex_lock(&s->lock);
	s->state = BL_SINK_PAUSE;
	pthread_mutex_unlock(&s->lock);
	return 0;
}

/**
 * @brief 恢复
 * @return 0 成功
 */
int bl_sink_resume(bl_sink_t *s)
{
	if (s == NULL)
		return -EINVAL;
	pthread_mutex_lock(&s->lock);
	s->state = BL_SINK_RUN;
	pthread_cond_broadcast(&s->cond);
	pthread_mutex_unlock(&s->lock);
	return 0;
}

/**
 * @brief 请求停止(唤醒挂起的 write, 使其返回 -1)
 * @return 0 成功
 */
int bl_sink_stop(bl_sink_t *s)
{
	if (s == NULL)
		return -EINVAL;
	pthread_mutex_lock(&s->lock);
	s->state = BL_SINK_STOP;
	pthread_cond_broadcast(&s->cond);
	pthread_mutex_unlock(&s->lock);
	return 0;
}

/**
 * @brief 等待已写入数据播完
 * @return 0 成功; 负值 失败
 */
int bl_sink_drain(bl_sink_t *s)
{
	int ret;

	if (s == NULL)
		return -EINVAL;
	ret = snd_pcm_drain(s->pcm);
	return ret < 0 ? ret : 0;
}

/**
 * @brief 关闭并释放(未 drain 则丢弃剩余)
 */
void bl_sink_close(bl_sink_t *s)
{
	if (s == NULL)
		return;
	snd_pcm_close(s->pcm);
	pthread_mutex_destroy(&s->lock);
	pthread_cond_destroy(&s->cond);
	free(s);
}
