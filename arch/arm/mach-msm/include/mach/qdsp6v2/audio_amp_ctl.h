#ifndef __MACH_QDSP6V2_AUDIO_AMP_H__
#define __MACH_QDSP6V2_AUDIO_AMP_H__

#include "audio_amp_def.h"

struct audio_amp_ops {
	void (*init)(void);
	void (*exit)(void);	
	void (*set)(int type, int value);
	void (*get)(int type, int * value);
	void (*enable)(AMP_PATH_TYPE_E path);	
	void (*disable)(AMP_PATH_TYPE_E path);
};

void audio_amp_init(AMP_DEVICE_E dev);
void audio_amp_exit(void);
void audio_amp_set(int type, int value);
void audio_amp_get(int type, int * value);
void audio_amp_on(AMP_PATH_TYPE_E path);
void audio_amp_off(AMP_PATH_TYPE_E path);

#ifndef KTTECH_FINAL_BUILD // for final build
#define APM_DBG(fmt, args...)  pr_debug("[%s] " fmt, __func__, ##args)
#define APM_INFO(fmt, args...) pr_info("[%s] " fmt, __func__, ##args)
#define APM_ERR(fmt, args...)  pr_err("[%s] " fmt, __func__, ##args)
#else
#define APM_DBG(x...) do{}while(0)
#define APM_INFO(x...) do{}while(0)
#define APM_ERR(x...) do{}while(0)
#endif

#endif /*__MACH_QDSP6V2_AUDIO_AMP_H__*/
