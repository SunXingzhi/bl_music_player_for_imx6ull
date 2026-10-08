#include <alsa/asoundlib.h>
#include <linux/errno.h>

// configured information
// use the plugin to automatically convert the original value to compatible hw params.
#define PCM_DEVICE_NAME "plughw:0,0"
#define TEST_RATE 48000
#define CHANNELS 2

typedef struct
{
        int rate;
        int dir;
        int channel;
        int access;
        int sample_bytes;
        snd_pcm_uframes_t periodsize; // TODO : what is it ?

} hw_config_t;

// static int set_hwparams(snd_pcm_t * handle, snd_pcm_hw_params_t * params, snd_pcm_access_t access)
// {
//         unsigned int rrate;
//         snd_pcm_uframes_t size;
//         int err, dir;

//         /* choose all parameters */
//         err = snd_pcm_hw_params_any(handle, params);
//         if(err < 0) {
//                 printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
//                 return err;
//         }
//         /* set hardware resampling */
//         err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
//         if(err < 0) {
//                 printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
//                 return err;
//         }
//         /* set the interleaved read/write format */
//         err = snd_pcm_hw_params_set_access(handle, params, access);
//         if(err < 0) {
//                 printf("Access type not available for playback: %s\n", snd_strerror(err));
//                 return err;
//         }
//         /* set the sample format */
//         err = snd_pcm_hw_params_set_format(handle, params, format);
//         if(err < 0) {
//                 printf("Sample format not available for playback: %s\n", snd_strerror(err));
//                 return err;
//         }
//         /* set the count of channels */
//         err = snd_pcm_hw_params_set_channels(handle, params, channels);
//         if(err < 0) {
//                 printf("Channels count (%u) not available for playbacks: %s\n", channels, snd_strerror(err));
//                 return err;
//         }
//         /* set the stream rate */
//         rrate = rate;
//         err   = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
//         if(err < 0) {
//                 printf("Rate %uHz not available for playback: %s\n", rate, snd_strerror(err));
//                 return err;
//         }
//         if(rrate != rate) {
//                 printf("Rate doesn't match (requested %uHz, get %iHz)\n", rate, err);
//                 return -EINVAL;
//         }
//         /* set the buffer time */
//         err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, &dir);
//         if(err < 0) {
//                 printf("Unable to set buffer time %u for playback: %s\n", buffer_time, snd_strerror(err));
//                 return err;
//         }
//         err = snd_pcm_hw_params_get_buffer_size(params, &size);
//         if(err < 0) {
//                 printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
//                 return err;
//         }
//         buffer_size = size;
//         /* set the period time */
//         err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, &dir);
//         if(err < 0) {
//                 printf("Unable to set period time %u for playback: %s\n", period_time, snd_strerror(err));
//                 return err;
//         }
//         err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
//         if(err < 0) {
//                 printf("Unable to get period size for playback: %s\n", snd_strerror(err));
//                 return err;
//         }
//         period_size = size;
//         /* write the parameters to device */
//         err = snd_pcm_hw_params(handle, params);
//         if(err < 0) {
//                 printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
//                 return err;
//         }
//         return 0;
// }

int main()
{
        snd_pcm_t * pcm_handle;
        int err;
        snd_pcm_hw_params_t * pcm_hw_params;

        hw_config_t hw_params;

        // create a playback pcm stream.
        snd_pcm_stream_t pcm_stream = SND_PCM_STREAM_PLAYBACK;

        // sound card hardware parmas.
        snd_pcm_hw_params_t * pcm_params;

        // open the sound card device.
        char * pcm_name;

        pcm_name = strdup(PCM_DEVICE_NAME);

        err = snd_pcm_open(&pcm_handle, pcm_name, pcm_stream, 0);
        if(err < 0) {
                fprintf(stderr, "Failed to open: err:%d\n", err);
        }

        // set the hardware params: rate, channels,
        snd_pcm_hw_params_alloca(&pcm_hw_params); // Because of a demo, we allocate the params at the stack.
        err = snd_pcm_hw_params_any(pcm_handle, pcm_hw_params);
        if(err < 0) {
                fprintf(stderr, "Failed to init params: err:%d\n", err);
        }

        hw_params.rate       = TEST_RATE;
        hw_params.channel    = CHANNELS;
        hw_params.dir        = SND_PCM_STREAM_PLAYBACK;
        hw_params.periodsize = 0; // take in a pos

        return 0;
}
