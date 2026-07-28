#ifndef _RK_MULTIAUDIO_H_
#define _RK_MULTIAUDIO_H_

#include <system/audio.h>

    void multiaudio_A(audio_session_t sessionid, uint32_t *flags, uint32_t *flags1, bool *boo, audio_devices_t *device);
    void multiaudio_B(audio_session_t sessionid, uint32_t *flags, uint32_t *flags1, bool *boo, audio_devices_t *device);
    void multiaudio_C(audio_session_t sessionid, uint32_t *flags, uint32_t *flags1, bool *boo, audio_devices_t *device);
    void multiaudio_D(audio_session_t sessionid, uint32_t *flags, uint32_t *flags1, bool *boo, audio_devices_t *device);

#endif //_RK_MULTIAUDIO_H_
