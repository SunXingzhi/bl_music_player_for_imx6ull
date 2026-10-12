/*
 * mixer_set.c — 用 C 代码复刻 amixer (snd_mixer_* Simple Mixer API)
 *
 * 用法:
 *   ./mixer_set                                列出所有控件       ≈ amixer -c1 scontrols
 *   ./mixer_set hw:1                           指定声卡列出
 *   ./mixer_set 'Speaker'                      查看当前值         ≈ amixer sget
 *   ./mixer_set 'Speaker' 127                  设置回放音量       ≈ amixer sset 'Speaker' 127
 *   ./mixer_set 'Left Output Mixer PCM' on     设置回放开关       ≈ amixer sset ... on
 *
 * 默认卡 hw:1 (wm8960-audio); 传 "hw:0" 等可换卡.
 * 带音量的控件给数字, 带开关的控件给 on/off; 音量控件同时给 "on/off" 也按开关处理.
 */
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define DEFAULT_CARD "hw:1"

/* 打开 mixer: open -> attach(control设备) -> register -> load(枚举) */
static snd_mixer_t *mixer_open_card(const char *card)
{
	snd_mixer_t *m = NULL;

	if (snd_mixer_open(&m, 0) < 0) {
		fprintf(stderr, "snd_mixer_open failed\n");
		return NULL;
	}
	if (snd_mixer_attach(m, card) < 0 ||
	    snd_mixer_selem_register(m, NULL, NULL) < 0 ||
	    snd_mixer_load(m) < 0) { /* load: 从内核枚举全部 kcontrol */
		fprintf(stderr, "cannot open mixer on %s: %s\n",
			card, snd_strerror(errno));
		snd_mixer_close(m);
		return NULL;
	}
	return m;
}

/* 按名字找控件 (amixer 的 'name',index) */
static snd_mixer_elem_t *find_elem(snd_mixer_t *m, const char *name)
{
	snd_mixer_selem_id_t *sid;

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_name(sid, name);
	snd_mixer_selem_id_set_index(sid, 0);
	return snd_mixer_find_selem(m, sid);
}

/* ≈ amixer scontrols */
static void list_controls(snd_mixer_t *m)
{
	snd_mixer_elem_t *e;

	for (e = snd_mixer_first_elem(m); e; e = snd_mixer_elem_next(e)) {
		if (!snd_mixer_selem_is_active(e))
			continue;
		printf("'%s',%u%s%s\n",
		       snd_mixer_selem_get_name(e),
		       snd_mixer_selem_get_index(e),
		       snd_mixer_selem_has_playback_volume(e) ? "  [volume]" : "",
		       snd_mixer_selem_has_playback_switch(e) ? "  [switch]" : "");
	}
}

/* ≈ amixer sget 'xxx' */
static void show_elem(snd_mixer_elem_t *e)
{
	printf("'%s',%u\n", snd_mixer_selem_get_name(e),
	       snd_mixer_selem_get_index(e));

	if (snd_mixer_selem_has_playback_volume(e)) {
		long vmin, vmax, vl = 0, vr = 0;

		snd_mixer_selem_get_playback_volume_range(e, &vmin, &vmax);
		if (snd_mixer_selem_has_playback_channel(e, SND_MIXER_SCHN_FRONT_LEFT))
			snd_mixer_selem_get_playback_volume(e, SND_MIXER_SCHN_FRONT_LEFT, &vl);
		if (snd_mixer_selem_has_playback_channel(e, SND_MIXER_SCHN_FRONT_RIGHT))
			snd_mixer_selem_get_playback_volume(e, SND_MIXER_SCHN_FRONT_RIGHT, &vr);
		printf("  volume: L=%ld R=%ld  (range %ld..%ld)\n", vl, vr, vmin, vmax);
	}
	if (snd_mixer_selem_has_playback_switch(e)) {
		int on = 0;

		snd_mixer_selem_get_playback_switch(e, SND_MIXER_SCHN_FRONT_LEFT, &on);
		printf("  switch: %s\n", on ? "on" : "off");
	}
}

/* ≈ amixer sset 'xxx' value */
static int set_elem(snd_mixer_elem_t *e, const char *value)
{
	int is_switch_word = !strcasecmp(value, "on") || !strcasecmp(value, "off");

	/* 音量控件 + 数字 -> 设音量 */
	if (snd_mixer_selem_has_playback_volume(e) && !is_switch_word) {
		char *end;
		long v = strtol(value, &end, 10);

		if (*end != '\0') {
			fprintf(stderr, "bad number '%s'\n", value);
			return -1;
		}
		return snd_mixer_selem_set_playback_volume_all(e, v) < 0 ? -1 : 0;
	}
	/* 开关控件 (或音量控件给 on/off) -> 设开关 */
	if (snd_mixer_selem_has_playback_switch(e)) {
		int on = !strcasecmp(value, "on") || strcmp(value, "1") == 0;

		return snd_mixer_selem_set_playback_switch_all(e, on) < 0 ? -1 : 0;
	}
	fprintf(stderr, "unsupported control type for '%s'\n", value);
	return -1;
}

int main(int argc, char **argv)
{
	const char *card = DEFAULT_CARD;
	snd_mixer_t *m;
	int i = 1;

	if (argc > 1 && strncmp(argv[1], "hw:", 3) == 0) {
		card = argv[1];
		i = 2;
	}

	m = mixer_open_card(card);
	if (!m)
		return EXIT_FAILURE;

	if (i >= argc) {
		list_controls(m);
	} else {
		snd_mixer_elem_t *e = find_elem(m, argv[i]);

		if (!e) {
			fprintf(stderr, "control '%s' not found on %s\n", argv[i], card);
			goto fail;
		}
		if (i + 1 >= argc) {
			show_elem(e); /* sget */
		} else if (set_elem(e, argv[i + 1]) == 0) {
			printf("OK: '%s' -> %s\n", argv[i], argv[i + 1]);
			show_elem(e); /* 回读验证 */
		} else {
			goto fail;
		}
	}

	snd_mixer_close(m);
	return EXIT_SUCCESS;
fail:
	snd_mixer_close(m);
	return EXIT_FAILURE;
}
