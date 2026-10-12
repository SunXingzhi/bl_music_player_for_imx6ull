/**
 * @file bl_audio.h
 * @brief bl_music_player 音频库对外接口(LVGL 应用只需要 include 本文件)
 *
 * 模块:
 *   bl_mixer : 声卡音量/开关控制(同步, 线程安全, 可从 LVGL 线程直接调用)
 *   bl_sink  : PCM 输出流(阻塞式写入, 供 WAV/蓝牙等数据源喂数据)
 *   bl_wav   : WAV 播放便利层(内部线程, 回调在库线程上下文!)
 *
 * 线程契约:
 *   - bl_mixer_* 全部可以在 LVGL 线程直接调用(内部加锁, 单次 ioctl 微秒级)
 *   - bl_sink_write() 会阻塞直到全部写入(自然背压), 由数据源线程调用
 *   - bl_wav_play() 的回调运行在库内部线程, LVGL 侧必须用
 *     lv_async_call() 把通知转投到 UI 线程, 禁止在回调里直接碰 lvgl 对象
 */
#ifndef BL_AUDIO_H
#define BL_AUDIO_H

#include <stdint.h>
#include <alsa/asoundlib.h>

/* ==================================================================== */
/* mixer: 音量/开关控制                                                  */
/* ==================================================================== */

/**
 * @brief 打开某张声卡的混音器
 * @param card 控制设备名, 如 "hw:1"(wm8960-audio)
 * @return 0 成功; 负值 失败
 */
int bl_mixer_open(const char *card);

/**
 * @brief 关闭混音器
 */
void bl_mixer_close(void);

/**
 * @brief 设置回放音量控件的值(原始刻度, 如 'Speaker' 0..127)
 * @param ctl 控件名, 如 "Speaker", "Playback Volume"
 * @param value 目标值
 * @return 0 成功; 负值 失败(控件不存在/无音量能力)
 */
int bl_mixer_set_volume(const char *ctl, long value);

/**
 * @brief 读取回放音量控件的当前值
 * @param ctl 控件名
 * @param value [out] 当前值
 * @return 0 成功; 负值 失败
 */
int bl_mixer_get_volume(const char *ctl, long *value);

/**
 * @brief 查询音量控件刻度范围
 * @param ctl 控件名
 * @param vmin [out] 最小值
 * @param vmax [out] 最大值(如 'Speaker' 为 127, 对应 0dB)
 * @return 0 成功; 负值 失败
 */
int bl_mixer_get_volume_range(const char *ctl, long *vmin, long *vmax);

/**
 * @brief 设置回放开关控件(如 'Left Output Mixer PCM')
 * @param ctl 控件名
 * @param on 1 开 / 0 关
 * @return 0 成功; 负值 失败
 */
int bl_mixer_set_switch(const char *ctl, int on);

/**
 * @brief 读取回放开关控件状态
 * @return 0 成功; 负值 失败
 */
int bl_mixer_get_switch(const char *ctl, int *on);

/**
 * @brief 一键拉起 wm8960 默认播放路由(板级预设, 控件名针对 wm8960)
 *
 * 等价于:
 *   amixer sset 'Playback Volume' 100%
 *   amixer sset 'Speaker' 127        (127 = 0dB, 注意 100 只有 -21dB)
 *   amixer sset 'Left  Output Mixer PCM' on
 *   amixer sset 'Right Output Mixer PCM' on
 *
 * @return 0 全部成功; 负值 首个失败项(其余仍尝试执行)
 */
int bl_mixer_route_default(void);

/* ==================================================================== */
/* sink: PCM 输出流                                                      */
/* ==================================================================== */

typedef struct bl_sink bl_sink_t;

/**
 * @brief 打开 PCM 输出流并完成参数协商
 * @param device PCM 设备名, 如 "plughw:1,0"
 * @param rate 采样率(如 48000)
 * @param channels 声道数(如 2)
 * @param format 采样格式(如 SND_PCM_FORMAT_S16_LE)
 * @return 句柄; NULL 失败
 *
 * 硬件约束(本板已验证): 直连链路(hw:1,0)要求 48000/stereo;
 * 任意采样率请用 ASRC 前端设备 "plughw:1,1"。
 */
bl_sink_t *bl_sink_open(const char *device, unsigned int rate,
	unsigned int channels, snd_pcm_format_t format);

/**
 * @brief 阻塞式写入(直到全部写入或被 stop 打断), 自然背压
 * @return 0 全部写入; 负值 失败/被停止
 */
int bl_sink_write(bl_sink_t *s, const void *buf, uint32_t frames);

/**
 * @brief 暂停: 后续 write 挂起(最多等待当前一块写完)
 * @return 0 成功; 负值 失败
 */
int bl_sink_pause(bl_sink_t *s);

/**
 * @brief 恢复暂停的 write
 * @return 0 成功; 负值 失败
 */
int bl_sink_resume(bl_sink_t *s);

/**
 * @brief 请求停止: 唤醒挂起的 write 使其返回负值(协作式)
 */
int bl_sink_stop(bl_sink_t *s);

/**
 * @brief 等待缓冲区播完(收尾用)
 * @return 0 成功; 负值 失败
 */
int bl_sink_drain(bl_sink_t *s);

/**
 * @brief 关闭并释放(未 drain 则丢弃剩余数据)
 */
void bl_sink_close(bl_sink_t *s);

/* ==================================================================== */
/* wav: WAV 播放便利层(内部线程)                                          */
/* ==================================================================== */

#define BL_WAV_EVT_DONE	1	/**< 播放完成 */
#define BL_WAV_EVT_ERROR	2	/**< 播放失败, detail 为负错误码 */

/**
 * @brief 播放一个 WAV 文件(单实例, 新播放会先停掉旧任务)
 *
 * @param path wav 文件路径
 * @param device PCM 设备名, NULL 则用 "plughw:1,1"(ASRC 前端, 兼容任意采样率)
 * @param cb 事件回调(完成/失败), 运行在库线程! 见文件头线程契约
 * @param user 回调透传参数
 * @return 0 已启动; 负值 启动失败(文件/设备错误)
 */
int bl_wav_play(const char *path, const char *device,
	void (*cb)(int evt, int detail, void *user), void *user);

/**
 * @brief 停止当前 WAV 播放(幂等)
 */
void bl_wav_stop(void);

#endif /* BL_AUDIO_H */
