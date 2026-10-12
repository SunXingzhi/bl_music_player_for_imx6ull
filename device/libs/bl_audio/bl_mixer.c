/**
 * @file bl_mixer.c
 * @brief 声卡混音器控制封装(snd_mixer_* Simple Mixer API)
 *
 * 全部接口线程安全(单全局句柄 + 互斥锁), 单次操作为一次 ioctl,
 * 可从 LVGL UI 线程直接调用。
 */

#include <alsa/asoundlib.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "bl_audio.h"

#define DEFAULT_CARD	"hw:1"

static snd_mixer_t	*g_mixer;
static pthread_mutex_t	g_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 打开某张声卡的混音器
 * @param card 控制设备名
 * @return 0 成功; 负值 失败
 */
int bl_mixer_open(const char *card)
{
	snd_mixer_t *m = NULL;
	int ret;

	if (card == NULL)
		card = DEFAULT_CARD;

	pthread_mutex_lock(&g_lock);

	if (g_mixer != NULL) {
		/* 已打开: 同卡幂等, 异卡先关旧的 */
		if (snd_mixer_attach(g_mixer, card) == 0) {
			pthread_mutex_unlock(&g_lock);
			return 0;
		}
		snd_mixer_close(g_mixer);
		g_mixer = NULL;
	}

	ret = snd_mixer_open(&m, 0);
	if (ret < 0)
		goto fail;
	ret = snd_mixer_attach(m, card);
	if (ret < 0)
		goto fail;
	ret = snd_mixer_selem_register(m, NULL, NULL);
	if (ret < 0)
		goto fail;
	ret = snd_mixer_load(m);	/* 从内核枚举全部 kcontrol */
	if (ret < 0)
		goto fail;

	g_mixer = m;
	pthread_mutex_unlock(&g_lock);
	return 0;
fail:
	if (m != NULL)
		snd_mixer_close(m);
	pthread_mutex_unlock(&g_lock);
	return ret;
}

/**
 * @brief 关闭混音器
 */
void bl_mixer_close(void)
{
	pthread_mutex_lock(&g_lock);
	if (g_mixer != NULL) {
		snd_mixer_close(g_mixer);
		g_mixer = NULL;
	}
	pthread_mutex_unlock(&g_lock);
}

/**
 * @brief 按名找控件(须持锁调用)
 * @return 控件; NULL 未找到
 */
static snd_mixer_elem_t *lookup(const char *ctl)
{
	snd_mixer_selem_id_t *sid;

	if (g_mixer == NULL)
		return NULL;

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_name(sid, ctl);
	snd_mixer_selem_id_set_index(sid, 0);
	return snd_mixer_find_selem(g_mixer, sid);
}

/**
 * @brief 设置回放音量(原始刻度)
 * @return 0 成功; 负值 失败
 */
int bl_mixer_set_volume(const char *ctl, long value)
{
	snd_mixer_elem_t *e;
	int ret;

	pthread_mutex_lock(&g_lock);
	e = lookup(ctl);
	if (e == NULL || !snd_mixer_selem_has_playback_volume(e)) {
		ret = -ENOENT;
		goto out;
	}
	ret = snd_mixer_selem_set_playback_volume_all(e, value);
out:
	pthread_mutex_unlock(&g_lock);
	return ret;
}

/**
 * @brief 读取回放音量当前值(取 FRONT_LEFT, 单声道控件取 MONO)
 * @return 0 成功; 负值 失败
 */
int bl_mixer_get_volume(const char *ctl, long *value)
{
	snd_mixer_elem_t *e;
	snd_mixer_selem_channel_id_t ch;
	int ret;

	if (value == NULL)
		return -EINVAL;

	pthread_mutex_lock(&g_lock);
	e = lookup(ctl);
	if (e == NULL || !snd_mixer_selem_has_playback_volume(e)) {
		ret = -ENOENT;
		goto out;
	}
	if (snd_mixer_selem_has_playback_channel(e, SND_MIXER_SCHN_FRONT_LEFT))
		ch = SND_MIXER_SCHN_FRONT_LEFT;
	else
		ch = SND_MIXER_SCHN_MONO;
	ret = snd_mixer_selem_get_playback_volume(e, ch, value);
out:
	pthread_mutex_unlock(&g_lock);
	return ret;
}

/**
 * @brief 查询音量刻度范围
 * @return 0 成功; 负值 失败
 */
int bl_mixer_get_volume_range(const char *ctl, long *vmin, long *vmax)
{
	snd_mixer_elem_t *e;
	int ret;

	if (vmin == NULL || vmax == NULL)
		return -EINVAL;

	pthread_mutex_lock(&g_lock);
	e = lookup(ctl);
	if (e == NULL || !snd_mixer_selem_has_playback_volume(e)) {
		ret = -ENOENT;
		goto out;
	}
	ret = snd_mixer_selem_get_playback_volume_range(e, vmin, vmax);
out:
	pthread_mutex_unlock(&g_lock);
	return ret;
}

/**
 * @brief 设置回放开关
 * @return 0 成功; 负值 失败
 */
int bl_mixer_set_switch(const char *ctl, int on)
{
	snd_mixer_elem_t *e;
	int ret;

	pthread_mutex_lock(&g_lock);
	e = lookup(ctl);
	if (e == NULL || !snd_mixer_selem_has_playback_switch(e)) {
		ret = -ENOENT;
		goto out;
	}
	ret = snd_mixer_selem_set_playback_switch_all(e, on ? 1 : 0);
out:
	pthread_mutex_unlock(&g_lock);
	return ret;
}

/**
 * @brief 读取回放开关状态
 * @return 0 成功; 负值 失败
 */
int bl_mixer_get_switch(const char *ctl, int *on)
{
	snd_mixer_elem_t *e;
	int ret;

	if (on == NULL)
		return -EINVAL;

	pthread_mutex_lock(&g_lock);
	e = lookup(ctl);
	if (e == NULL || !snd_mixer_selem_has_playback_switch(e)) {
		ret = -ENOENT;
		goto out;
	}
	if (snd_mixer_selem_has_playback_channel(e, SND_MIXER_SCHN_FRONT_LEFT))
		ret = snd_mixer_selem_get_playback_switch(e,
			SND_MIXER_SCHN_FRONT_LEFT, on);
	else
		ret = snd_mixer_selem_get_playback_switch(e,
			SND_MIXER_SCHN_MONO, on);
out:
	pthread_mutex_unlock(&g_lock);
	return ret;
}

/**
 * @brief 一键拉起 wm8960 默认播放路由
 * @return 0 全部成功; 负值 首个失败项
 */
int bl_mixer_route_default(void)
{
	struct route_item {
		const char	*ctl;
		long		value;
		int		is_switch;
	};
	static const struct route_item items[] = {
		{ "Playback Volume",		255,	0 },
		{ "Speaker",			127,	0 },	/* 127=0dB, 100 仅 -21dB */
		{ "Headphone",			127,	0 },
		{ "Left Output Mixer PCM",	1,	1 },
		{ "Right Output Mixer PCM",	1,	1 },
	};
	int ret = 0;
	unsigned int i;

	for (i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
		int r;

		if (items[i].is_switch)
			r = bl_mixer_set_switch(items[i].ctl, (int)items[i].value);
		else
			r = bl_mixer_set_volume(items[i].ctl, items[i].value);
		if (r < 0 && ret == 0)
			ret = r;	/* 记录首个失败, 其余照常执行 */
	}
	return ret;
}
