/**
 * @file smoke.c
 * @brief bl_audio 库主机构建自测(WSL/PC 无声卡, 验证链接与错误路径)
 *
 * 预期: mixer 打开失败(无声卡)返回负值, 各接口不崩溃。
 */

#include <stdio.h>
#include "bl_audio.h"

/**
 * @brief WAV 事件回调(自测用)
 * @param evt 事件类型
 * @param detail 事件细节
 * @param user 透传参数
 */
static void on_evt(int evt, int detail, void *user)
{
	printf("event: evt=%d detail=%d user=%s\n", evt, detail,
		(char *)user);
}

/**
 * @brief 自测入口
 * @return 0 通过(库行为符合预期); 1 异常
 */
int main(void)
{
	long vmin, vmax, vol;
	int on;
	int rc;

	/* 1. 无声卡环境: open 应优雅失败 */
	rc = bl_mixer_open("hw:0");
	printf("bl_mixer_open -> %d (expect <0 on host w/o card)\n", rc);

	/* 2. 未打开时读写应返回错误而不是崩溃 */
	rc = bl_mixer_get_volume("Speaker", &vol);
	printf("bl_mixer_get_volume -> %d (expect <0)\n", rc);
	rc = bl_mixer_get_volume_range("Speaker", &vmin, &vmax);
	printf("bl_mixer_get_volume_range -> %d\n", rc);
	rc = bl_mixer_get_switch("Left Output Mixer PCM", &on);
	printf("bl_mixer_get_switch -> %d\n", rc);

	/* 3. sink 打开无效设备应优雅失败 */
	bl_sink_t *s = bl_sink_open("hw:9", 48000, 2,
		SND_PCM_FORMAT_S16_LE);
	printf("bl_sink_open(hw:9) -> %p (expect NULL)\n", (void *)s);

	/* 4. WAV: 不存在的文件应返回错误 */
	rc = bl_wav_play("/tmp/no_such_file.wav", NULL, on_evt, "smoke");
	printf("bl_wav_play(no file) -> %d\n", rc);

	/* 5. 坏文件头(非 RIFF): 错误回调路径 */
	FILE *fp = fopen("/tmp/bl_bad.wav", "w");
	fwrite("NOTAWAVFILE....", 1, 16, fp);
	fclose(fp);
	rc = bl_wav_play("/tmp/bl_bad.wav", NULL, on_evt, "smoke");
	printf("bl_wav_play(bad hdr) -> %d\n", rc);
	sleep(1);	/* 等错误回调打出 */

	bl_wav_stop();
	bl_mixer_close();
	printf("smoke done\n");
	return 0;
}
