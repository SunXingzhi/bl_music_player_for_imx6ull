/*
 * sound_test.c — alsa-lib 最小验证程序
 * 功能: 枚举声卡/PCM设备, 验证交叉编译+运行环境是否就绪
 * 编译: make (见同目录 Makefile)
 */
#include <alsa/asoundlib.h>
#include <stdio.h>

int main(void)
{
	snd_ctl_t *handle;
	int card_num = -1;
	int err;

	printf("alsa-lib version: %s\n", snd_asoundlib_version());

	/* snd_card_next 遍历 /proc/asound/cards 里的所有声卡 */
	while ((err = snd_card_next(&card_num)) == 0 && card_num >= 0) {
		char name[32];

		snd_ctl_card_info_t *info;

		snprintf(name, sizeof(name), "hw:%d", card_num);
		if ((err = snd_ctl_open(&handle, name, 0)) < 0) {
			fprintf(stderr, "ctl open %s: %s\n", name, snd_strerror(err));
			continue;
		}
		snd_ctl_card_info_alloca(&info);   /* 栈上分配 card info 对象 */
		if ((err = snd_ctl_card_info(handle, info)) < 0) {
			fprintf(stderr, "card info: %s\n", snd_strerror(err));
			snd_ctl_close(handle);
			continue;
		}

		printf("card %d: id=[%s] name=[%s]\n",
		       card_num,
		       snd_ctl_card_info_get_id(info),
		       snd_ctl_card_info_get_name(info));
		snd_ctl_close(handle);
	}

	if (card_num < 0)
		printf("no sound card found (err=%d: %s)\n", err, snd_strerror(err));

	return 0;
}
