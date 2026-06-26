/* nval.cpp - OpenAL plugin for NVGT (nvAL)
 *
 * Features: device, context, buffer, source, streaming,
 *           EFX effects/filters/aux slots, HRTF, audio capture.
 *
 * NVGT - NonVisual Gaming Toolkit
 * https://nvgt.dev
 * This software is provided "as-is", without any express or implied warranty.
 */

#include <nvgt_plugin.h>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <unordered_map>
#include <scriptarray.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#define AL_NO_PROTOTYPES
#define ALC_NO_PROTOTYPES
#include "AL/al.h"
#include "AL/alc.h"

// ============================================================
// EFX / Capture / HRTF function pointer typedefs
// ============================================================

typedef void (AL_APIENTRY *LPALGENEFFECTS)(ALsizei, ALuint*);
typedef void (AL_APIENTRY *LPALDELETEEFFECTS)(ALsizei, const ALuint*);
typedef void (AL_APIENTRY *LPALEFFECTI)(ALuint, ALenum, ALint);
typedef void (AL_APIENTRY *LPALEFFECTF)(ALuint, ALenum, ALfloat);
typedef void (AL_APIENTRY *LPALGETEFFECTI)(ALuint, ALenum, ALint*);
typedef void (AL_APIENTRY *LPALGETEFFECTF)(ALuint, ALenum, ALfloat*);
typedef void (AL_APIENTRY *LPALGENFILTERS)(ALsizei, ALuint*);
typedef void (AL_APIENTRY *LPALDELETEFILTERS)(ALsizei, const ALuint*);
typedef void (AL_APIENTRY *LPALFILTERI)(ALuint, ALenum, ALint);
typedef void (AL_APIENTRY *LPALFILTERF)(ALuint, ALenum, ALfloat);
typedef void (AL_APIENTRY *LPALGETFILTERI)(ALuint, ALenum, ALint*);
typedef void (AL_APIENTRY *LPALGETFILTERF)(ALuint, ALenum, ALfloat*);
typedef void (AL_APIENTRY *LPALGENAUXILIARYEFFECTSLOTS)(ALsizei, ALuint*);
typedef void (AL_APIENTRY *LPALDELETEAUXILIARYEFFECTSLOTS)(ALsizei, const ALuint*);
typedef void (AL_APIENTRY *LPALAUXILIARYEFFECTSLOTI)(ALuint, ALenum, ALint);
typedef void (AL_APIENTRY *LPALAUXILIARYEFFECTSLOTF)(ALuint, ALenum, ALfloat);
typedef void (AL_APIENTRY *LPALGETAUXILIARYEFFECTSLOTI)(ALuint, ALenum, ALint*);
typedef void (AL_APIENTRY *LPALGETAUXILIARYEFFECTSLOTF)(ALuint, ALenum, ALfloat*);
// LPALSOURCE3I, LPALCCAPTURE*, LPALCGETINTEGERV already defined in al.h / alc.h
typedef ALCboolean     (ALC_APIENTRY *LPALCRESETDEVICESOFT)(ALCdevice*, const ALCint*);
typedef const ALCchar* (ALC_APIENTRY *LPALCGETSTRINGISOFT)(ALCdevice*, ALCenum, ALCsizei);

// ============================================================
// EFX constants
// ============================================================

#define AL_SOURCE_SPATIALIZE_SOFT            0x1214
#define AL_AUTO_SOFT                         0x0002

#define AL_DIRECT_FILTER                     0x20005
#define AL_AUXILIARY_SEND_FILTER             0x20006
#define AL_EFFECT_TYPE                       0x8001
#define AL_EFFECT_NULL                       0x0000
#define AL_EFFECT_REVERB                     0x0001
#define AL_EFFECT_CHORUS                     0x0002
#define AL_EFFECT_DISTORTION                 0x0003
#define AL_EFFECT_ECHO                       0x0004
#define AL_EFFECT_FLANGER                    0x0005
#define AL_EFFECT_PITCH_SHIFTER              0x0008
#define AL_EFFECT_RING_MODULATOR             0x0009
#define AL_EFFECT_COMPRESSOR                 0x000B
#define AL_EFFECT_EQUALIZER                  0x000C
#define AL_EFFECT_EAXREVERB                  0x8000
#define AL_REVERB_DENSITY                    0x0001
#define AL_REVERB_DIFFUSION                  0x0002
#define AL_REVERB_GAIN                       0x0003
#define AL_REVERB_GAINHF                     0x0004
#define AL_REVERB_DECAY_TIME                 0x0005
#define AL_REVERB_DECAY_HFRATIO              0x0006
#define AL_REVERB_REFLECTIONS_GAIN           0x0007
#define AL_REVERB_REFLECTIONS_DELAY          0x0008
#define AL_REVERB_LATE_REVERB_GAIN           0x0009
#define AL_REVERB_LATE_REVERB_DELAY          0x000A
#define AL_REVERB_AIR_ABSORPTION_GAINHF      0x000B
#define AL_REVERB_ROOM_ROLLOFF_FACTOR        0x000C
#define AL_REVERB_DECAY_HFLIMIT              0x000D
#define AL_ECHO_DELAY                        0x0001
#define AL_ECHO_LRDELAY                      0x0002
#define AL_ECHO_DAMPING                      0x0003
#define AL_ECHO_FEEDBACK                     0x0004
#define AL_ECHO_SPREAD                       0x0005
#define AL_CHORUS_WAVEFORM                   0x0001
#define AL_CHORUS_PHASE                      0x0002
#define AL_CHORUS_RATE                       0x0003
#define AL_CHORUS_DEPTH                      0x0004
#define AL_CHORUS_FEEDBACK                   0x0005
#define AL_CHORUS_DELAY                      0x0006
#define AL_FLANGER_WAVEFORM                  0x0001
#define AL_FLANGER_PHASE                     0x0002
#define AL_FLANGER_RATE                      0x0003
#define AL_FLANGER_DEPTH                     0x0004
#define AL_FLANGER_FEEDBACK                  0x0005
#define AL_FLANGER_DELAY                     0x0006
#define AL_COMPRESSOR_ONOFF                  0x0001
#define AL_EQUALIZER_LOW_GAIN                0x0001
#define AL_EQUALIZER_LOW_CUTOFF              0x0002
#define AL_EQUALIZER_MID1_GAIN               0x0003
#define AL_EQUALIZER_MID1_CENTER             0x0004
#define AL_EQUALIZER_MID1_WIDTH              0x0005
#define AL_EQUALIZER_MID2_GAIN               0x0006
#define AL_EQUALIZER_MID2_CENTER             0x0007
#define AL_EQUALIZER_MID2_WIDTH              0x0008
#define AL_EQUALIZER_HIGH_GAIN               0x0009
#define AL_EQUALIZER_HIGH_CUTOFF             0x000A
#define AL_FILTER_TYPE                       0x8001
#define AL_FILTER_NULL                       0x0000
#define AL_FILTER_LOWPASS                    0x0001
#define AL_FILTER_HIGHPASS                   0x0002
#define AL_FILTER_BANDPASS                   0x0003
#define AL_LOWPASS_GAIN                      0x0001
#define AL_LOWPASS_GAINHF                    0x0002
#define AL_HIGHPASS_GAIN                     0x0001
#define AL_HIGHPASS_GAINLF                   0x0002
#define AL_BANDPASS_GAIN                     0x0001
#define AL_BANDPASS_GAINLF                   0x0002
#define AL_BANDPASS_GAINHF                   0x0003
#define AL_EFFECTSLOT_NULL                   0x0000
#define AL_EFFECTSLOT_EFFECT                 0x0001
#define AL_EFFECTSLOT_GAIN                   0x0002
#define AL_EFFECTSLOT_AUXILIARY_SEND_AUTO    0x0003
#define ALC_MAX_AUXILIARY_SENDS              0x20003

// HRTF constants
#define ALC_HRTF_SOFT                        0x1992
#define ALC_HRTF_STATUS_SOFT                 0x1993
#define ALC_NUM_HRTF_SPECIFIERS_SOFT         0x1994
#define ALC_HRTF_SPECIFIER_SOFT              0x1995
#define ALC_HRTF_ID_SOFT                     0x1996
#define ALC_HRTF_DISABLED_SOFT               0x0000
#define ALC_HRTF_ENABLED_SOFT                0x0001
#define ALC_HRTF_DENIED_SOFT                 0x0002
#define ALC_HRTF_REQUIRED_SOFT               0x0003
#define ALC_HRTF_HEADPHONES_DETECTED_SOFT    0x0004
#define ALC_HRTF_UNSUPPORTED_FORMAT_SOFT     0x0005

// Capture constants
#define ALC_CAPTURE_SAMPLES                  0x312
#define ALC_CAPTURE_DEVICE_SPECIFIER         0x310
#define ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER 0x311

// ============================================================
// Dynamic loader
// ============================================================

#ifdef _WIN32
static HMODULE g_al_lib = nullptr;
static void* al_load_sym(const char* name) { return (void*)GetProcAddress(g_al_lib, name); }
#else
static void* g_al_lib = nullptr;
static void* al_load_sym(const char* name) { return dlsym(g_al_lib, name); }
#endif

// Core AL
static void     (AL_APIENTRY *p_alGenBuffers)(ALsizei, ALuint*) = nullptr;
static void     (AL_APIENTRY *p_alDeleteBuffers)(ALsizei, const ALuint*) = nullptr;
static void     (AL_APIENTRY *p_alBufferData)(ALuint, ALenum, const ALvoid*, ALsizei, ALsizei) = nullptr;
static void     (AL_APIENTRY *p_alGetBufferi)(ALuint, ALenum, ALint*) = nullptr;
static void     (AL_APIENTRY *p_alGetBufferf)(ALuint, ALenum, ALfloat*) = nullptr;
static void     (AL_APIENTRY *p_alGenSources)(ALsizei, ALuint*) = nullptr;
static void     (AL_APIENTRY *p_alDeleteSources)(ALsizei, const ALuint*) = nullptr;
static void     (AL_APIENTRY *p_alSourcef)(ALuint, ALenum, ALfloat) = nullptr;
static void     (AL_APIENTRY *p_alSource3f)(ALuint, ALenum, ALfloat, ALfloat, ALfloat) = nullptr;
static void     (AL_APIENTRY *p_alSourcei)(ALuint, ALenum, ALint) = nullptr;
static void     (AL_APIENTRY *p_alGetSourcef)(ALuint, ALenum, ALfloat*) = nullptr;
static void     (AL_APIENTRY *p_alGetSource3f)(ALuint, ALenum, ALfloat*, ALfloat*, ALfloat*) = nullptr;
static void     (AL_APIENTRY *p_alGetSourcei)(ALuint, ALenum, ALint*) = nullptr;
static void     (AL_APIENTRY *p_alSourcePlay)(ALuint) = nullptr;
static void     (AL_APIENTRY *p_alSourceStop)(ALuint) = nullptr;
static void     (AL_APIENTRY *p_alSourceRewind)(ALuint) = nullptr;
static void     (AL_APIENTRY *p_alSourcePause)(ALuint) = nullptr;
static void     (AL_APIENTRY *p_alSourcePlayv)(ALsizei, const ALuint*) = nullptr;
static void     (AL_APIENTRY *p_alSourceStopv)(ALsizei, const ALuint*) = nullptr;
static void     (AL_APIENTRY *p_alSourcePausev)(ALsizei, const ALuint*) = nullptr;
static void     (AL_APIENTRY *p_alSourceQueueBuffers)(ALuint, ALsizei, const ALuint*) = nullptr;
static void     (AL_APIENTRY *p_alSourceUnqueueBuffers)(ALuint, ALsizei, ALuint*) = nullptr;
static void     (AL_APIENTRY *p_alListenerf)(ALenum, ALfloat) = nullptr;
static void     (AL_APIENTRY *p_alListener3f)(ALenum, ALfloat, ALfloat, ALfloat) = nullptr;
static void     (AL_APIENTRY *p_alListenerfv)(ALenum, const ALfloat*) = nullptr;
static void     (AL_APIENTRY *p_alGetListenerf)(ALenum, ALfloat*) = nullptr;
static void     (AL_APIENTRY *p_alGetListener3f)(ALenum, ALfloat*, ALfloat*, ALfloat*) = nullptr;
static void     (AL_APIENTRY *p_alGetListenerfv)(ALenum, ALfloat*) = nullptr;
static ALenum   (AL_APIENTRY *p_alGetError)() = nullptr;
static const ALchar* (AL_APIENTRY *p_alGetString)(ALenum) = nullptr;
static ALboolean (AL_APIENTRY *p_alIsExtensionPresent)(const ALchar*) = nullptr;
static void     (AL_APIENTRY *p_alDopplerFactor)(ALfloat) = nullptr;
static void     (AL_APIENTRY *p_alSpeedOfSound)(ALfloat) = nullptr;
static void     (AL_APIENTRY *p_alDistanceModel)(ALenum) = nullptr;

// Core ALC
static ALCdevice*   (ALC_APIENTRY *p_alcOpenDevice)(const ALCchar*) = nullptr;
static ALCboolean   (ALC_APIENTRY *p_alcCloseDevice)(ALCdevice*) = nullptr;
static ALCcontext*  (ALC_APIENTRY *p_alcCreateContext)(ALCdevice*, const ALCint*) = nullptr;
static ALCboolean   (ALC_APIENTRY *p_alcMakeContextCurrent)(ALCcontext*) = nullptr;
static void         (ALC_APIENTRY *p_alcDestroyContext)(ALCcontext*) = nullptr;
static ALCcontext*  (ALC_APIENTRY *p_alcGetCurrentContext)() = nullptr;
static ALCdevice*   (ALC_APIENTRY *p_alcGetContextsDevice)(ALCcontext*) = nullptr;
static ALCenum      (ALC_APIENTRY *p_alcGetError)(ALCdevice*) = nullptr;
static const ALCchar* (ALC_APIENTRY *p_alcGetString)(ALCdevice*, ALCenum) = nullptr;
static ALCboolean   (ALC_APIENTRY *p_alcIsExtensionPresent)(ALCdevice*, const ALCchar*) = nullptr;
static void*        (ALC_APIENTRY *p_alcGetProcAddress)(ALCdevice*, const ALCchar*) = nullptr;
static LPALCGETINTEGERV p_alcGetIntegerv = nullptr;

// EFX (loaded lazily per-device via alcGetProcAddress)
static LPALGENEFFECTS               p_alGenEffects = nullptr;
static LPALDELETEEFFECTS            p_alDeleteEffects = nullptr;
static LPALEFFECTI                  p_alEffecti = nullptr;
static LPALEFFECTF                  p_alEffectf = nullptr;
static LPALGETEFFECTI               p_alGetEffecti = nullptr;
static LPALGETEFFECTF               p_alGetEffectf = nullptr;
static LPALGENFILTERS               p_alGenFilters = nullptr;
static LPALDELETEFILTERS            p_alDeleteFilters = nullptr;
static LPALFILTERI                  p_alFilteri = nullptr;
static LPALFILTERF                  p_alFilterf = nullptr;
static LPALGETFILTERI               p_alGetFilteri = nullptr;
static LPALGETFILTERF               p_alGetFilterf = nullptr;
static LPALGENAUXILIARYEFFECTSLOTS  p_alGenAuxiliaryEffectSlots = nullptr;
static LPALDELETEAUXILIARYEFFECTSLOTS p_alDeleteAuxiliaryEffectSlots = nullptr;
static LPALAUXILIARYEFFECTSLOTI     p_alAuxiliaryEffectSloti = nullptr;
static LPALAUXILIARYEFFECTSLOTF     p_alAuxiliaryEffectSlotf = nullptr;
static LPALGETAUXILIARYEFFECTSLOTI  p_alGetAuxiliaryEffectSloti = nullptr;
static LPALGETAUXILIARYEFFECTSLOTF  p_alGetAuxiliaryEffectSlotf = nullptr;
static LPALSOURCE3I                 p_alSource3i = nullptr;
static bool g_efx_loaded = false;

// Capture (standard ALC 1.1, loaded from DLL directly)
static LPALCCAPTUREOPENDEVICE  p_alcCaptureOpenDevice  = nullptr;
static LPALCCAPTURECLOSEDEVICE p_alcCaptureCloseDevice = nullptr;
static LPALCCAPTURESTART       p_alcCaptureStart       = nullptr;
static LPALCCAPTURESTOP        p_alcCaptureStop        = nullptr;
static LPALCCAPTURESAMPLES     p_alcCaptureSamples     = nullptr;
static bool g_capture_loaded = false;

// HRTF (ALC_SOFT_HRTF extension, loaded per-device)
static LPALCRESETDEVICESOFT p_alcResetDeviceSOFT = nullptr;
static LPALCGETSTRINGISOFT  p_alcGetStringiSOFT  = nullptr;

static bool g_al_loaded = false;

static bool load_openal(const std::string& path) {
	#ifdef _WIN32
	g_al_lib = LoadLibraryA(path.empty() ? "openal32.dll" : path.c_str());
	#elif defined(__APPLE__)
	g_al_lib = dlopen(path.empty() ? "libopenal.dylib" : path.c_str(), RTLD_LAZY);
	#else
	g_al_lib = dlopen(path.empty() ? "libopenal.so.1" : path.c_str(), RTLD_LAZY);
	#endif
	if (!g_al_lib) return false;

	#define LOAD(fn) p_##fn = (decltype(p_##fn))al_load_sym(#fn); if (!p_##fn) return false;
	LOAD(alGenBuffers) LOAD(alDeleteBuffers) LOAD(alBufferData)
	LOAD(alGetBufferi) LOAD(alGetBufferf)
	LOAD(alGenSources) LOAD(alDeleteSources)
	LOAD(alSourcef) LOAD(alSource3f) LOAD(alSourcei)
	LOAD(alGetSourcef) LOAD(alGetSource3f) LOAD(alGetSourcei)
	LOAD(alSourcePlay) LOAD(alSourceStop) LOAD(alSourceRewind) LOAD(alSourcePause)
	LOAD(alSourcePlayv) LOAD(alSourceStopv) LOAD(alSourcePausev)
	LOAD(alSourceQueueBuffers) LOAD(alSourceUnqueueBuffers)
	LOAD(alListenerf) LOAD(alListener3f) LOAD(alListenerfv)
	LOAD(alGetListenerf) LOAD(alGetListener3f) LOAD(alGetListenerfv)
	LOAD(alGetError) LOAD(alGetString) LOAD(alIsExtensionPresent)
	LOAD(alDopplerFactor) LOAD(alSpeedOfSound) LOAD(alDistanceModel)
	LOAD(alcOpenDevice) LOAD(alcCloseDevice)
	LOAD(alcCreateContext) LOAD(alcMakeContextCurrent) LOAD(alcDestroyContext)
	LOAD(alcGetCurrentContext) LOAD(alcGetContextsDevice)
	LOAD(alcGetError) LOAD(alcGetString) LOAD(alcIsExtensionPresent) LOAD(alcGetProcAddress)
	LOAD(alcGetIntegerv)
	#undef LOAD

	// Capture functions are optional — don't fail if absent
	#define LOADOPT(fn) p_##fn = (decltype(p_##fn))al_load_sym(#fn);
	LOADOPT(alcCaptureOpenDevice) LOADOPT(alcCaptureCloseDevice)
	LOADOPT(alcCaptureStart) LOADOPT(alcCaptureStop) LOADOPT(alcCaptureSamples)
	#undef LOADOPT
	g_capture_loaded = p_alcCaptureOpenDevice && p_alcCaptureCloseDevice &&
	                   p_alcCaptureStart && p_alcCaptureStop && p_alcCaptureSamples;

	g_al_loaded = true;
	return true;
}

static bool try_load_efx(ALCdevice* dev) {
	if (g_efx_loaded) return true;
	if (!p_alcGetProcAddress || !p_alcIsExtensionPresent) return false;
	if (p_alcIsExtensionPresent(dev, "ALC_EXT_EFX") != ALC_TRUE) return false;
	#define LEFX(fn) p_##fn = (decltype(p_##fn))p_alcGetProcAddress(dev, #fn); if (!p_##fn) return false;
	LEFX(alGenEffects) LEFX(alDeleteEffects) LEFX(alEffecti) LEFX(alEffectf)
	LEFX(alGetEffecti) LEFX(alGetEffectf)
	LEFX(alGenFilters) LEFX(alDeleteFilters) LEFX(alFilteri) LEFX(alFilterf)
	LEFX(alGetFilteri) LEFX(alGetFilterf)
	LEFX(alGenAuxiliaryEffectSlots) LEFX(alDeleteAuxiliaryEffectSlots)
	LEFX(alAuxiliaryEffectSloti) LEFX(alAuxiliaryEffectSlotf)
	LEFX(alGetAuxiliaryEffectSloti) LEFX(alGetAuxiliaryEffectSlotf)
	LEFX(alSource3i)
	#undef LEFX
	g_efx_loaded = true;
	return true;
}

static void try_load_hrtf(ALCdevice* dev) {
	if (!p_alcGetProcAddress || !p_alcIsExtensionPresent) return;
	if (p_alcIsExtensionPresent(dev, "ALC_SOFT_HRTF") != ALC_TRUE) return;
	p_alcResetDeviceSOFT = (LPALCRESETDEVICESOFT)p_alcGetProcAddress(dev, "alcResetDeviceSOFT");
	p_alcGetStringiSOFT  = (LPALCGETSTRINGISOFT) p_alcGetProcAddress(dev, "alcGetStringiSOFT");
}

// ============================================================
// Reference counting base
// ============================================================

class nval_refcounted {
	mutable int m_refs;
public:
	nval_refcounted() : m_refs(1) {}
	virtual ~nval_refcounted() = default;
	void add_ref() { asAtomicInc(m_refs); }
	void release() { if (asAtomicDec(m_refs) < 1) delete this; }
};

// Forward declarations needed by al_source
class al_filter;
class al_aux_slot;

// ============================================================
// al_filter
// ============================================================

class al_filter : public nval_refcounted {
public:
	ALuint m_id;
	al_filter() : m_id(0) { if (g_efx_loaded) p_alGenFilters(1, &m_id); }
	static al_filter* factory() { return new al_filter(); }
	bool get_is_valid() const { return m_id != 0; }
	void set_type(int t) { if (m_id) p_alFilteri(m_id, AL_FILTER_TYPE, t); }
	int  get_type() const { ALint v = 0; if (m_id) p_alGetFilteri(m_id, AL_FILTER_TYPE, &v); return v; }
	void  set_f(int param, float v) { if (m_id) p_alFilterf(m_id, (ALenum)param, v); }
	float get_f(int param) const { float v = 0; if (m_id) p_alGetFilterf(m_id, (ALenum)param, &v); return v; }
	void set_i(int param, int v) { if (m_id) p_alFilteri(m_id, (ALenum)param, v); }
	int  get_i(int param) const { ALint v = 0; if (m_id) p_alGetFilteri(m_id, (ALenum)param, &v); return v; }
	~al_filter() override { if (m_id) p_alDeleteFilters(1, &m_id); }
};

// ============================================================
// al_effect
// ============================================================

class al_effect : public nval_refcounted {
public:
	ALuint m_id;
	al_effect() : m_id(0) { if (g_efx_loaded) p_alGenEffects(1, &m_id); }
	static al_effect* factory() { return new al_effect(); }
	bool get_is_valid() const { return m_id != 0; }
	void set_type(int t) { if (m_id) p_alEffecti(m_id, AL_EFFECT_TYPE, t); }
	int  get_type() const { ALint v = 0; if (m_id) p_alGetEffecti(m_id, AL_EFFECT_TYPE, &v); return v; }
	void  set_f(int param, float v) { if (m_id) p_alEffectf(m_id, (ALenum)param, v); }
	float get_f(int param) const { float v = 0; if (m_id) p_alGetEffectf(m_id, (ALenum)param, &v); return v; }
	void set_i(int param, int v) { if (m_id) p_alEffecti(m_id, (ALenum)param, v); }
	int  get_i(int param) const { ALint v = 0; if (m_id) p_alGetEffecti(m_id, (ALenum)param, &v); return v; }
	~al_effect() override { if (m_id) p_alDeleteEffects(1, &m_id); }
};

// ============================================================
// al_aux_slot
// ============================================================

class al_aux_slot : public nval_refcounted {
public:
	ALuint m_id;
	al_effect* m_effect;
	al_aux_slot() : m_id(0), m_effect(nullptr) {
		if (g_efx_loaded) p_alGenAuxiliaryEffectSlots(1, &m_id);
	}
	static al_aux_slot* factory() { return new al_aux_slot(); }
	bool get_is_valid() const { return m_id != 0; }
	void set_effect(al_effect* eff) {
		if (!m_id) return;
		if (m_effect) m_effect->release();
		m_effect = eff;
		if (m_effect) {
			m_effect->add_ref();
			p_alAuxiliaryEffectSloti(m_id, AL_EFFECTSLOT_EFFECT, (ALint)m_effect->m_id);
		} else {
			p_alAuxiliaryEffectSloti(m_id, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);
		}
	}
	void  set_gain(float v) { if (m_id) p_alAuxiliaryEffectSlotf(m_id, AL_EFFECTSLOT_GAIN, v); }
	float get_gain() const { float v = 1.0f; if (m_id) p_alGetAuxiliaryEffectSlotf(m_id, AL_EFFECTSLOT_GAIN, &v); return v; }
	void set_send_auto(bool v) { if (m_id) p_alAuxiliaryEffectSloti(m_id, AL_EFFECTSLOT_AUXILIARY_SEND_AUTO, v ? AL_TRUE : AL_FALSE); }
	bool get_efx_available() const { return g_efx_loaded; }
	~al_aux_slot() override {
		if (m_id) p_alDeleteAuxiliaryEffectSlots(1, &m_id);
		if (m_effect) m_effect->release();
	}
};

// ============================================================
// al_device
// ============================================================

class al_device : public nval_refcounted {
public:
	ALCdevice* m_dev;
	std::string m_name;

	al_device() : m_dev(nullptr) {}
	al_device(const std::string& name) : m_dev(nullptr), m_name(name) {}
	static al_device* factory() { return new al_device(); }
	static al_device* factory_named(const std::string& name) { return new al_device(name); }

	bool open() {
		if (m_dev) return true;
		if (!g_al_loaded) return false;
		m_dev = p_alcOpenDevice(m_name.empty() ? nullptr : m_name.c_str());
		if (!m_dev) return false;
		try_load_efx(m_dev);
		try_load_hrtf(m_dev);
		return true;
	}
	bool open_named(const std::string& name) { m_name = name; return open(); }
	void close() { if (m_dev) { p_alcCloseDevice(m_dev); m_dev = nullptr; } }

	bool get_is_open() const { return m_dev != nullptr; }
	std::string get_name() const {
		if (!m_dev) return m_name;
		const ALCchar* s = p_alcGetString(m_dev, ALC_DEVICE_SPECIFIER);
		return s ? s : "";
	}
	int get_error() const { return m_dev ? (int)p_alcGetError(m_dev) : ALC_INVALID_DEVICE; }
	bool is_extension_present(const std::string& ext) const {
		return m_dev && p_alcIsExtensionPresent(m_dev, ext.c_str()) == ALC_TRUE;
	}

	// EFX
	bool get_efx_available() const { return g_efx_loaded; }
	int get_max_auxiliary_sends() const {
		if (!m_dev || !p_alcGetIntegerv || !g_efx_loaded) return 0;
		ALCint v = 0; p_alcGetIntegerv(m_dev, ALC_MAX_AUXILIARY_SENDS, 1, &v); return v;
	}

	// HRTF
	bool get_hrtf_available() const {
		return m_dev && p_alcIsExtensionPresent && p_alcIsExtensionPresent(m_dev, "ALC_SOFT_HRTF") == ALC_TRUE;
	}
	int get_hrtf_count() const {
		if (!m_dev || !p_alcGetIntegerv || !get_hrtf_available()) return 0;
		ALCint n = 0; p_alcGetIntegerv(m_dev, ALC_NUM_HRTF_SPECIFIERS_SOFT, 1, &n); return n;
	}
	std::string get_hrtf_name(int index) const {
		if (!m_dev || !p_alcGetStringiSOFT) return "";
		const ALCchar* s = p_alcGetStringiSOFT(m_dev, ALC_HRTF_SPECIFIER_SOFT, (ALCsizei)index);
		return s ? s : "";
	}
	int get_hrtf_status() const {
		if (!m_dev || !p_alcGetIntegerv) return ALC_HRTF_DISABLED_SOFT;
		ALCint s = 0; p_alcGetIntegerv(m_dev, ALC_HRTF_STATUS_SOFT, 1, &s); return s;
	}
	bool enable_hrtf(int id) {
		if (!m_dev || !p_alcResetDeviceSOFT) return false;
		ALCint attrs[] = { ALC_HRTF_SOFT, ALC_TRUE, ALC_HRTF_ID_SOFT, id, 0 };
		return p_alcResetDeviceSOFT(m_dev, attrs) == ALC_TRUE;
	}
	bool disable_hrtf() {
		if (!m_dev || !p_alcResetDeviceSOFT) return false;
		ALCint attrs[] = { ALC_HRTF_SOFT, ALC_FALSE, 0 };
		return p_alcResetDeviceSOFT(m_dev, attrs) == ALC_TRUE;
	}

	~al_device() override { close(); }
};

// ============================================================
// al_context
// ============================================================

class al_context : public nval_refcounted {
public:
	ALCcontext* m_ctx;
	al_device* m_device;

	al_context(al_device* dev) : m_ctx(nullptr), m_device(nullptr) {
		if (dev && dev->m_dev) {
			m_device = dev; m_device->add_ref();
			m_ctx = p_alcCreateContext(dev->m_dev, nullptr);
		}
	}
	static al_context* factory(al_device* dev) { return new al_context(dev); }

	bool make_current() { return m_ctx && p_alcMakeContextCurrent(m_ctx) == ALC_TRUE; }
	void clear_current() { if (get_is_current()) p_alcMakeContextCurrent(nullptr); }
	bool get_is_current() const { return m_ctx && p_alcGetCurrentContext() == m_ctx; }
	bool get_is_valid() const { return m_ctx != nullptr; }

	void set_listener_x(float v) { p_alListener3f(AL_POSITION, v, get_listener_y(), get_listener_z()); }
	void set_listener_y(float v) { p_alListener3f(AL_POSITION, get_listener_x(), v, get_listener_z()); }
	void set_listener_z(float v) { p_alListener3f(AL_POSITION, get_listener_x(), get_listener_y(), v); }
	void set_listener_position(float x, float y, float z) { p_alListener3f(AL_POSITION, x, y, z); }
	float get_listener_x() const { float x,y,z; p_alGetListener3f(AL_POSITION,&x,&y,&z); return x; }
	float get_listener_y() const { float x,y,z; p_alGetListener3f(AL_POSITION,&x,&y,&z); return y; }
	float get_listener_z() const { float x,y,z; p_alGetListener3f(AL_POSITION,&x,&y,&z); return z; }
	void set_listener_velocity(float x, float y, float z) { p_alListener3f(AL_VELOCITY, x, y, z); }
	float get_listener_velocity_x() const { float x,y,z; p_alGetListener3f(AL_VELOCITY,&x,&y,&z); return x; }
	float get_listener_velocity_y() const { float x,y,z; p_alGetListener3f(AL_VELOCITY,&x,&y,&z); return y; }
	float get_listener_velocity_z() const { float x,y,z; p_alGetListener3f(AL_VELOCITY,&x,&y,&z); return z; }
	void  set_listener_gain(float v) { p_alListenerf(AL_GAIN, v); }
	float get_listener_gain() const { float v = 1.0f; p_alGetListenerf(AL_GAIN, &v); return v; }
	void set_listener_orientation(float fx, float fy, float fz, float ux, float uy, float uz) {
		float d[6] = {fx, fy, fz, ux, uy, uz}; p_alListenerfv(AL_ORIENTATION, d);
	}
	void set_doppler_factor(float v) { p_alDopplerFactor(v); }
	void set_speed_of_sound(float v) { p_alSpeedOfSound(v); }
	void set_distance_model(int m) { p_alDistanceModel((ALenum)m); }
	int  get_al_error() const { return (int)p_alGetError(); }
	bool is_extension_present(const std::string& ext) const { return p_alIsExtensionPresent(ext.c_str()) == AL_TRUE; }

	~al_context() override {
		if (m_ctx) { if (get_is_current()) p_alcMakeContextCurrent(nullptr); p_alcDestroyContext(m_ctx); }
		if (m_device) m_device->release();
	}
};

// ============================================================
// al_buffer
// ============================================================

class al_buffer : public nval_refcounted {
public:
	ALuint m_id;
	al_context* m_ctx;

	al_buffer(al_context* ctx) : m_id(0), m_ctx(nullptr) {
		if (ctx) { m_ctx = ctx; m_ctx->add_ref(); p_alGenBuffers(1, &m_id); }
	}
	static al_buffer* factory(al_context* ctx) { return new al_buffer(ctx); }

	bool set_data(const std::string& data, int format, int sample_rate) {
		if (!m_id) return false;
		p_alBufferData(m_id, (ALenum)format, data.c_str(), (ALsizei)data.size(), (ALsizei)sample_rate);
		return p_alGetError() == AL_NO_ERROR;
	}

	// Upload float PCM (as returned by NVGT's audio_decoder.read) directly, converting to 16-bit
	// natively and optionally downmixing stereo->mono so OpenAL can spatialize it. This avoids the
	// per-sample AngelScript conversion loop, which dominated load time on big maps.
	bool set_data_floats(CScriptArray* floats, int channels, int sample_rate, bool downmix) {
		if (!m_id || !floats) return false;
		unsigned int n = floats->GetSize();
		if (n == 0 || channels < 1) return false;
		const float* in = (const float*)floats->GetBuffer();
		bool mono = downmix && channels == 2;
		ALenum format = (channels == 1 || mono) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
		std::vector<short> pcm;
		if (mono) {
			pcm.reserve(n / 2);
			for (unsigned int i = 0; i + 1 < n; i += 2) {
				float s = (in[i] + in[i + 1]) * 0.5f;
				if (s > 1.0f) s = 1.0f; else if (s < -1.0f) s = -1.0f;
				pcm.push_back((short)(s * 32767.0f));
			}
		} else {
			pcm.reserve(n);
			for (unsigned int i = 0; i < n; i++) {
				float s = in[i];
				if (s > 1.0f) s = 1.0f; else if (s < -1.0f) s = -1.0f;
				pcm.push_back((short)(s * 32767.0f));
			}
		}
		if (pcm.empty()) return false;
		p_alBufferData(m_id, format, pcm.data(), (ALsizei)(pcm.size() * sizeof(short)), (ALsizei)sample_rate);
		return p_alGetError() == AL_NO_ERROR;
	}
	bool get_is_valid() const { return m_id != 0; }
	int get_bits()      const { ALint v = 0; if (m_id) p_alGetBufferi(m_id, AL_BITS,      &v); return v; }
	int get_channels()  const { ALint v = 0; if (m_id) p_alGetBufferi(m_id, AL_CHANNELS,  &v); return v; }
	int get_frequency() const { ALint v = 0; if (m_id) p_alGetBufferi(m_id, AL_FREQUENCY, &v); return v; }
	int get_size()      const { ALint v = 0; if (m_id) p_alGetBufferi(m_id, AL_SIZE,      &v); return v; }

	~al_buffer() override { if (m_id) p_alDeleteBuffers(1, &m_id); if (m_ctx) m_ctx->release(); }
};

// ============================================================
// al_source
// ============================================================

class al_source : public nval_refcounted {
public:
	ALuint m_id;
	al_context* m_ctx;
	al_buffer* m_buffer;
	al_filter* m_direct_filter;
	std::unordered_map<ALuint, al_buffer*> m_queued;

	al_source(al_context* ctx) : m_id(0), m_ctx(nullptr), m_buffer(nullptr), m_direct_filter(nullptr) {
		if (ctx) { m_ctx = ctx; m_ctx->add_ref(); p_alGenSources(1, &m_id); }
	}
	static al_source* factory(al_context* ctx) { return new al_source(ctx); }

	bool get_is_valid() const { return m_id != 0; }
	void play()   { if (m_id) p_alSourcePlay(m_id); }
	void stop()   { if (m_id) p_alSourceStop(m_id); }
	void pause()  { if (m_id) p_alSourcePause(m_id); }
	void rewind() { if (m_id) p_alSourceRewind(m_id); }

	int  get_state()      const { ALint v = AL_INITIAL; if (m_id) p_alGetSourcei(m_id, AL_SOURCE_STATE, &v); return v; }
	bool get_is_playing() const { return get_state() == AL_PLAYING; }
	bool get_is_paused()  const { return get_state() == AL_PAUSED; }
	bool get_is_stopped() const { return get_state() == AL_STOPPED; }

	void set_buffer(al_buffer* buf) {
		if (!m_id) return;
		if (m_buffer) m_buffer->release();
		m_buffer = buf;
		if (m_buffer) { m_buffer->add_ref(); p_alSourcei(m_id, AL_BUFFER, (ALint)m_buffer->m_id); }
		else p_alSourcei(m_id, AL_BUFFER, AL_NONE);
	}
	al_buffer* get_buffer() const { if (m_buffer) m_buffer->add_ref(); return m_buffer; }

	// Streaming
	void queue_buffer(al_buffer* buf) {
		if (!m_id || !buf || !buf->m_id) return;
		ALuint bid = buf->m_id;
		p_alSourceQueueBuffers(m_id, 1, &bid);
		if (p_alGetError() == AL_NO_ERROR) { buf->add_ref(); m_queued[bid] = buf; }
	}
	al_buffer* unqueue_buffer() {
		if (!m_id) return nullptr;
		ALuint bid = 0;
		p_alSourceUnqueueBuffers(m_id, 1, &bid);
		if (p_alGetError() != AL_NO_ERROR) return nullptr;
		auto it = m_queued.find(bid);
		if (it == m_queued.end()) return nullptr;
		al_buffer* buf = it->second;
		m_queued.erase(it);
		return buf; // caller owns this ref
	}
	int get_buffers_queued()     const { ALint v = 0; if (m_id) p_alGetSourcei(m_id, AL_BUFFERS_QUEUED,     &v); return v; }
	int get_buffers_processed()  const { ALint v = 0; if (m_id) p_alGetSourcei(m_id, AL_BUFFERS_PROCESSED,  &v); return v; }

	// Gain / pitch / looping
	void  set_gain(float v)    { if (m_id) p_alSourcef(m_id, AL_GAIN,    v); }
	float get_gain()     const { float v = 1.0f; if (m_id) p_alGetSourcef(m_id, AL_GAIN,    &v); return v; }
	void  set_pitch(float v)   { if (m_id) p_alSourcef(m_id, AL_PITCH,   v); }
	float get_pitch()    const { float v = 1.0f; if (m_id) p_alGetSourcef(m_id, AL_PITCH,   &v); return v; }
	void  set_looping(bool v)  { if (m_id) p_alSourcei(m_id, AL_LOOPING, v ? AL_TRUE : AL_FALSE); }
	bool  get_looping()  const { ALint v = AL_FALSE; if (m_id) p_alGetSourcei(m_id, AL_LOOPING, &v); return v == AL_TRUE; }
	void  set_min_gain(float v){ if (m_id) p_alSourcef(m_id, AL_MIN_GAIN, v); }
	float get_min_gain() const { float v = 0.0f; if (m_id) p_alGetSourcef(m_id, AL_MIN_GAIN, &v); return v; }
	void  set_max_gain(float v){ if (m_id) p_alSourcef(m_id, AL_MAX_GAIN, v); }
	float get_max_gain() const { float v = 1.0f; if (m_id) p_alGetSourcef(m_id, AL_MAX_GAIN, &v); return v; }

	// Offsets
	void  set_sec_offset(float v)    { if (m_id) p_alSourcef(m_id, AL_SEC_OFFSET,    v); }
	float get_sec_offset()     const { float v = 0; if (m_id) p_alGetSourcef(m_id, AL_SEC_OFFSET,    &v); return v; }
	void  set_sample_offset(float v) { if (m_id) p_alSourcef(m_id, AL_SAMPLE_OFFSET, v); }
	float get_sample_offset()  const { float v = 0; if (m_id) p_alGetSourcef(m_id, AL_SAMPLE_OFFSET, &v); return v; }

	// 3D position
	void set_position(float x, float y, float z) { if (m_id) p_alSource3f(m_id, AL_POSITION, x, y, z); }
	void  set_x(float v) { float x,y,z; if (!m_id) return; p_alGetSource3f(m_id,AL_POSITION,&x,&y,&z); p_alSource3f(m_id,AL_POSITION,v,y,z); }
	void  set_y(float v) { float x,y,z; if (!m_id) return; p_alGetSource3f(m_id,AL_POSITION,&x,&y,&z); p_alSource3f(m_id,AL_POSITION,x,v,z); }
	void  set_z(float v) { float x,y,z; if (!m_id) return; p_alGetSource3f(m_id,AL_POSITION,&x,&y,&z); p_alSource3f(m_id,AL_POSITION,x,y,v); }
	float get_x() const { float x=0,y=0,z=0; if (m_id) p_alGetSource3f(m_id,AL_POSITION,&x,&y,&z); return x; }
	float get_y() const { float x=0,y=0,z=0; if (m_id) p_alGetSource3f(m_id,AL_POSITION,&x,&y,&z); return y; }
	float get_z() const { float x=0,y=0,z=0; if (m_id) p_alGetSource3f(m_id,AL_POSITION,&x,&y,&z); return z; }

	// 3D velocity
	void  set_velocity(float x, float y, float z) { if (m_id) p_alSource3f(m_id, AL_VELOCITY, x, y, z); }
	float get_velocity_x() const { float x=0,y=0,z=0; if (m_id) p_alGetSource3f(m_id,AL_VELOCITY,&x,&y,&z); return x; }
	float get_velocity_y() const { float x=0,y=0,z=0; if (m_id) p_alGetSource3f(m_id,AL_VELOCITY,&x,&y,&z); return y; }
	float get_velocity_z() const { float x=0,y=0,z=0; if (m_id) p_alGetSource3f(m_id,AL_VELOCITY,&x,&y,&z); return z; }

	// 3D direction
	void  set_direction(float x, float y, float z) { if (m_id) p_alSource3f(m_id, AL_DIRECTION, x, y, z); }
	float get_direction_x() const { float x=0,y=0,z=0; if (m_id) p_alGetSource3f(m_id,AL_DIRECTION,&x,&y,&z); return x; }
	float get_direction_y() const { float x=0,y=0,z=0; if (m_id) p_alGetSource3f(m_id,AL_DIRECTION,&x,&y,&z); return y; }
	float get_direction_z() const { float x=0,y=0,z=0; if (m_id) p_alGetSource3f(m_id,AL_DIRECTION,&x,&y,&z); return z; }

	// Distance model
	void  set_reference_distance(float v) { if (m_id) p_alSourcef(m_id, AL_REFERENCE_DISTANCE, v); }
	float get_reference_distance() const  { float v=1; if (m_id) p_alGetSourcef(m_id, AL_REFERENCE_DISTANCE, &v); return v; }
	void  set_rolloff_factor(float v)     { if (m_id) p_alSourcef(m_id, AL_ROLLOFF_FACTOR, v); }
	float get_rolloff_factor() const      { float v=1; if (m_id) p_alGetSourcef(m_id, AL_ROLLOFF_FACTOR, &v); return v; }
	void  set_max_distance(float v)       { if (m_id) p_alSourcef(m_id, AL_MAX_DISTANCE, v); }
	float get_max_distance() const        { float v=3.40282e+38f; if (m_id) p_alGetSourcef(m_id, AL_MAX_DISTANCE, &v); return v; }

	// Cone
	void  set_cone_inner_angle(float v) { if (m_id) p_alSourcef(m_id, AL_CONE_INNER_ANGLE, v); }
	float get_cone_inner_angle() const  { float v=360; if (m_id) p_alGetSourcef(m_id, AL_CONE_INNER_ANGLE, &v); return v; }
	void  set_cone_outer_angle(float v) { if (m_id) p_alSourcef(m_id, AL_CONE_OUTER_ANGLE, v); }
	float get_cone_outer_angle() const  { float v=360; if (m_id) p_alGetSourcef(m_id, AL_CONE_OUTER_ANGLE, &v); return v; }
	void  set_cone_outer_gain(float v)  { if (m_id) p_alSourcef(m_id, AL_CONE_OUTER_GAIN,  v); }
	float get_cone_outer_gain() const   { float v=0;   if (m_id) p_alGetSourcef(m_id, AL_CONE_OUTER_GAIN,  &v); return v; }

	// Relative
	void set_source_relative(bool v) { if (m_id) p_alSourcei(m_id, AL_SOURCE_RELATIVE, v ? AL_TRUE : AL_FALSE); }
	bool get_source_relative() const { ALint v=AL_FALSE; if (m_id) p_alGetSourcei(m_id, AL_SOURCE_RELATIVE, &v); return v==AL_TRUE; }

	// AL_SOFT_source_spatialize: AL_FALSE=never, AL_TRUE=always force mono, AL_AUTO_SOFT=default
	void set_spatialize(int v)  { if (m_id) p_alSourcei(m_id, AL_SOURCE_SPATIALIZE_SOFT, (ALint)v); }
	int  get_spatialize() const { ALint v = AL_AUTO_SOFT; if (m_id) p_alGetSourcei(m_id, AL_SOURCE_SPATIALIZE_SOFT, &v); return (int)v; }

	// EFX — direct filter
	void set_direct_filter(al_filter* flt) {
		if (!m_id || !g_efx_loaded) return;
		if (m_direct_filter) m_direct_filter->release();
		m_direct_filter = flt;
		if (m_direct_filter) {
			m_direct_filter->add_ref();
			p_alSourcei(m_id, AL_DIRECT_FILTER, (ALint)m_direct_filter->m_id);
		} else {
			p_alSourcei(m_id, AL_DIRECT_FILTER, AL_FILTER_NULL);
		}
	}
	// EFX — auxiliary send (send_num is the send index, 0-based)
	void set_aux_send(int send_num, al_aux_slot* slot, al_filter* flt) {
		if (!m_id || !g_efx_loaded || !p_alSource3i) return;
		p_alSource3i(m_id, AL_AUXILIARY_SEND_FILTER,
			slot ? (ALint)slot->m_id : AL_EFFECTSLOT_NULL,
			send_num,
			flt  ? (ALint)flt->m_id  : AL_FILTER_NULL);
	}
	bool get_efx_available() const { return g_efx_loaded; }

	~al_source() override {
		if (m_id) { p_alSourceStop(m_id); p_alDeleteSources(1, &m_id); }
		if (m_buffer) m_buffer->release();
		if (m_direct_filter) m_direct_filter->release();
		for (auto& kv : m_queued) kv.second->release();
		if (m_ctx) m_ctx->release();
	}
};

// ============================================================
// al_capture_device
// ============================================================

class al_capture_device : public nval_refcounted {
public:
	ALCdevice* m_dev;
	int m_bytes_per_sample;

	al_capture_device(const std::string& name, int frequency, int fmt, int buffer_size)
		: m_dev(nullptr), m_bytes_per_sample(2) {
		if (!g_capture_loaded) return;
		switch (fmt) {
			case AL_FORMAT_MONO8:    m_bytes_per_sample = 1; break;
			case AL_FORMAT_MONO16:   m_bytes_per_sample = 2; break;
			case AL_FORMAT_STEREO8:  m_bytes_per_sample = 2; break;
			case AL_FORMAT_STEREO16: m_bytes_per_sample = 4; break;
		}
		m_dev = p_alcCaptureOpenDevice(
			name.empty() ? nullptr : name.c_str(),
			(ALCuint)frequency, (ALCenum)fmt, (ALCsizei)buffer_size);
	}
	static al_capture_device* factory(const std::string& name, int freq, int fmt, int bufsz) {
		return new al_capture_device(name, freq, fmt, bufsz);
	}
	bool get_is_open() const { return m_dev != nullptr; }
	void start() { if (m_dev) p_alcCaptureStart(m_dev); }
	void stop()  { if (m_dev) p_alcCaptureStop(m_dev); }
	int get_available() const {
		if (!m_dev || !p_alcGetIntegerv) return 0;
		ALCint n = 0; p_alcGetIntegerv(m_dev, ALC_CAPTURE_SAMPLES, 1, &n); return (int)n;
	}
	std::string read_samples(int count) {
		if (!m_dev || count <= 0) return "";
		std::string buf(count * m_bytes_per_sample, '\0');
		p_alcCaptureSamples(m_dev, &buf[0], (ALCsizei)count);
		return buf;
	}
	void close() { if (m_dev) { p_alcCaptureCloseDevice(m_dev); m_dev = nullptr; } }
	~al_capture_device() override { close(); }
};

// ============================================================
// Global utility functions
// ============================================================

static bool nval_load(const std::string& path) { return load_openal(path); }
static bool nval_is_loaded() { return g_al_loaded; }
static bool nval_efx_available() { return g_efx_loaded; }
static bool nval_capture_available() { return g_capture_loaded; }

static std::string nval_get_default_device() {
	if (!g_al_loaded) return "";
	const ALCchar* s = p_alcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER);
	return s ? s : "";
}
static std::string nval_get_devices() {
	if (!g_al_loaded) return "";
	const ALCchar* list = p_alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);
	if (!list) list = p_alcGetString(nullptr, ALC_DEVICE_SPECIFIER);
	std::string result;
	while (list && *list) {
		if (!result.empty()) result += '\n';
		result += list;
		list += strlen(list) + 1;
	}
	return result;
}
static std::string nval_get_capture_devices() {
	if (!g_al_loaded) return "";
	const ALCchar* list = p_alcGetString(nullptr, ALC_CAPTURE_DEVICE_SPECIFIER);
	std::string result;
	while (list && *list) {
		if (!result.empty()) result += '\n';
		result += list;
		list += strlen(list) + 1;
	}
	return result;
}
static std::string nval_get_default_capture_device() {
	if (!g_al_loaded) return "";
	const ALCchar* s = p_alcGetString(nullptr, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
	return s ? s : "";
}
static int nval_get_al_error() { return g_al_loaded ? (int)p_alGetError() : AL_INVALID_OPERATION; }
static std::string nval_get_al_error_string(int err) {
	switch (err) {
		case AL_NO_ERROR:          return "AL_NO_ERROR";
		case AL_INVALID_NAME:      return "AL_INVALID_NAME";
		case AL_INVALID_ENUM:      return "AL_INVALID_ENUM";
		case AL_INVALID_VALUE:     return "AL_INVALID_VALUE";
		case AL_INVALID_OPERATION: return "AL_INVALID_OPERATION";
		case AL_OUT_OF_MEMORY:     return "AL_OUT_OF_MEMORY";
		default:                   return "AL_UNKNOWN_ERROR";
	}
}
static std::string nval_get_version_string()  { if (!g_al_loaded) return ""; const ALchar* s=p_alGetString(AL_VERSION);  return s?s:""; }
static std::string nval_get_renderer_string() { if (!g_al_loaded) return ""; const ALchar* s=p_alGetString(AL_RENDERER); return s?s:""; }
static std::string nval_get_vendor_string()   { if (!g_al_loaded) return ""; const ALchar* s=p_alGetString(AL_VENDOR);   return s?s:""; }

// ============================================================
// Registration
// ============================================================

#define CHECK(r) if ((r) < 0) return false
#define NVAL_CONST(name, val) CHECK(e->RegisterGlobalProperty("const int " #name, (void*)new int(val)))

static bool register_al_device(asIScriptEngine* e) {
	CHECK(e->RegisterObjectType("al_device", 0, asOBJ_REF));
	CHECK(e->RegisterObjectBehaviour("al_device", asBEHAVE_FACTORY, "al_device@ f()", asFUNCTION(al_device::factory), asCALL_CDECL));
	CHECK(e->RegisterObjectBehaviour("al_device", asBEHAVE_FACTORY, "al_device@ f(const string& in)", asFUNCTION(al_device::factory_named), asCALL_CDECL));
	CHECK(e->RegisterObjectBehaviour("al_device", asBEHAVE_ADDREF,  "void f()", asMETHOD(al_device, add_ref), asCALL_THISCALL));
	CHECK(e->RegisterObjectBehaviour("al_device", asBEHAVE_RELEASE, "void f()", asMETHOD(al_device, release), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "bool open()", asMETHOD(al_device, open), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "bool open(const string& in)", asMETHOD(al_device, open_named), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "void close()", asMETHOD(al_device, close), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "bool get_is_open() const property", asMETHOD(al_device, get_is_open), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "string get_name() const property", asMETHOD(al_device, get_name), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "int get_error() const property", asMETHOD(al_device, get_error), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "bool is_extension_present(const string& in) const", asMETHOD(al_device, is_extension_present), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "bool get_efx_available() const property", asMETHOD(al_device, get_efx_available), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "int get_max_auxiliary_sends() const property", asMETHOD(al_device, get_max_auxiliary_sends), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "bool get_hrtf_available() const property", asMETHOD(al_device, get_hrtf_available), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "int get_hrtf_count() const property", asMETHOD(al_device, get_hrtf_count), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "string get_hrtf_name(int) const", asMETHOD(al_device, get_hrtf_name), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "int get_hrtf_status() const property", asMETHOD(al_device, get_hrtf_status), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "bool enable_hrtf(int)", asMETHOD(al_device, enable_hrtf), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_device", "bool disable_hrtf()", asMETHOD(al_device, disable_hrtf), asCALL_THISCALL));
	return true;
}

static bool register_al_context(asIScriptEngine* e) {
	CHECK(e->RegisterObjectType("al_context", 0, asOBJ_REF));
	CHECK(e->RegisterObjectBehaviour("al_context", asBEHAVE_FACTORY, "al_context@ f(al_device@)", asFUNCTION(al_context::factory), asCALL_CDECL));
	CHECK(e->RegisterObjectBehaviour("al_context", asBEHAVE_ADDREF,  "void f()", asMETHOD(al_context, add_ref), asCALL_THISCALL));
	CHECK(e->RegisterObjectBehaviour("al_context", asBEHAVE_RELEASE, "void f()", asMETHOD(al_context, release), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "bool make_current()", asMETHOD(al_context, make_current), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "void clear_current()", asMETHOD(al_context, clear_current), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "bool get_is_current() const property", asMETHOD(al_context, get_is_current), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "bool get_is_valid() const property", asMETHOD(al_context, get_is_valid), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "void set_listener_x(float) property", asMETHOD(al_context, set_listener_x), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "float get_listener_x() const property", asMETHOD(al_context, get_listener_x), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "void set_listener_y(float) property", asMETHOD(al_context, set_listener_y), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "float get_listener_y() const property", asMETHOD(al_context, get_listener_y), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "void set_listener_z(float) property", asMETHOD(al_context, set_listener_z), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "float get_listener_z() const property", asMETHOD(al_context, get_listener_z), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "void set_listener_position(float, float, float)", asMETHOD(al_context, set_listener_position), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "void set_listener_velocity(float, float, float)", asMETHOD(al_context, set_listener_velocity), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "float get_listener_velocity_x() const property", asMETHOD(al_context, get_listener_velocity_x), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "float get_listener_velocity_y() const property", asMETHOD(al_context, get_listener_velocity_y), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "float get_listener_velocity_z() const property", asMETHOD(al_context, get_listener_velocity_z), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "void set_listener_gain(float) property", asMETHOD(al_context, set_listener_gain), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "float get_listener_gain() const property", asMETHOD(al_context, get_listener_gain), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "void set_listener_orientation(float, float, float, float, float, float)", asMETHOD(al_context, set_listener_orientation), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "void set_doppler_factor(float)", asMETHOD(al_context, set_doppler_factor), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "void set_speed_of_sound(float)", asMETHOD(al_context, set_speed_of_sound), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "void set_distance_model(int)", asMETHOD(al_context, set_distance_model), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "int get_al_error() const", asMETHOD(al_context, get_al_error), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_context", "bool is_extension_present(const string& in) const", asMETHOD(al_context, is_extension_present), asCALL_THISCALL));
	return true;
}

static bool register_al_buffer(asIScriptEngine* e) {
	CHECK(e->RegisterObjectType("al_buffer", 0, asOBJ_REF));
	CHECK(e->RegisterObjectBehaviour("al_buffer", asBEHAVE_FACTORY, "al_buffer@ f(al_context@)", asFUNCTION(al_buffer::factory), asCALL_CDECL));
	CHECK(e->RegisterObjectBehaviour("al_buffer", asBEHAVE_ADDREF,  "void f()", asMETHOD(al_buffer, add_ref), asCALL_THISCALL));
	CHECK(e->RegisterObjectBehaviour("al_buffer", asBEHAVE_RELEASE, "void f()", asMETHOD(al_buffer, release), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_buffer", "bool set_data(const string& in, int, int)", asMETHOD(al_buffer, set_data), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_buffer", "bool set_data_floats(array<float>@, int, int, bool)", asMETHOD(al_buffer, set_data_floats), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_buffer", "bool get_is_valid() const property", asMETHOD(al_buffer, get_is_valid), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_buffer", "int get_bits() const property", asMETHOD(al_buffer, get_bits), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_buffer", "int get_channels() const property", asMETHOD(al_buffer, get_channels), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_buffer", "int get_frequency() const property", asMETHOD(al_buffer, get_frequency), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_buffer", "int get_size() const property", asMETHOD(al_buffer, get_size), asCALL_THISCALL));
	return true;
}

static bool register_al_filter(asIScriptEngine* e) {
	CHECK(e->RegisterObjectType("al_filter", 0, asOBJ_REF));
	CHECK(e->RegisterObjectBehaviour("al_filter", asBEHAVE_FACTORY, "al_filter@ f()", asFUNCTION(al_filter::factory), asCALL_CDECL));
	CHECK(e->RegisterObjectBehaviour("al_filter", asBEHAVE_ADDREF,  "void f()", asMETHOD(al_filter, add_ref), asCALL_THISCALL));
	CHECK(e->RegisterObjectBehaviour("al_filter", asBEHAVE_RELEASE, "void f()", asMETHOD(al_filter, release), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_filter", "bool get_is_valid() const property", asMETHOD(al_filter, get_is_valid), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_filter", "void set_type(int) property", asMETHOD(al_filter, set_type), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_filter", "int get_type() const property", asMETHOD(al_filter, get_type), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_filter", "void set_f(int, float)", asMETHOD(al_filter, set_f), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_filter", "float get_f(int) const", asMETHOD(al_filter, get_f), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_filter", "void set_i(int, int)", asMETHOD(al_filter, set_i), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_filter", "int get_i(int) const", asMETHOD(al_filter, get_i), asCALL_THISCALL));
	return true;
}

static bool register_al_effect(asIScriptEngine* e) {
	CHECK(e->RegisterObjectType("al_effect", 0, asOBJ_REF));
	CHECK(e->RegisterObjectBehaviour("al_effect", asBEHAVE_FACTORY, "al_effect@ f()", asFUNCTION(al_effect::factory), asCALL_CDECL));
	CHECK(e->RegisterObjectBehaviour("al_effect", asBEHAVE_ADDREF,  "void f()", asMETHOD(al_effect, add_ref), asCALL_THISCALL));
	CHECK(e->RegisterObjectBehaviour("al_effect", asBEHAVE_RELEASE, "void f()", asMETHOD(al_effect, release), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_effect", "bool get_is_valid() const property", asMETHOD(al_effect, get_is_valid), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_effect", "void set_type(int) property", asMETHOD(al_effect, set_type), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_effect", "int get_type() const property", asMETHOD(al_effect, get_type), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_effect", "void set_f(int, float)", asMETHOD(al_effect, set_f), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_effect", "float get_f(int) const", asMETHOD(al_effect, get_f), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_effect", "void set_i(int, int)", asMETHOD(al_effect, set_i), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_effect", "int get_i(int) const", asMETHOD(al_effect, get_i), asCALL_THISCALL));
	return true;
}

static bool register_al_aux_slot(asIScriptEngine* e) {
	CHECK(e->RegisterObjectType("al_aux_slot", 0, asOBJ_REF));
	CHECK(e->RegisterObjectBehaviour("al_aux_slot", asBEHAVE_FACTORY, "al_aux_slot@ f()", asFUNCTION(al_aux_slot::factory), asCALL_CDECL));
	CHECK(e->RegisterObjectBehaviour("al_aux_slot", asBEHAVE_ADDREF,  "void f()", asMETHOD(al_aux_slot, add_ref), asCALL_THISCALL));
	CHECK(e->RegisterObjectBehaviour("al_aux_slot", asBEHAVE_RELEASE, "void f()", asMETHOD(al_aux_slot, release), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_aux_slot", "bool get_is_valid() const property", asMETHOD(al_aux_slot, get_is_valid), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_aux_slot", "void set_effect(al_effect@)", asMETHOD(al_aux_slot, set_effect), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_aux_slot", "void set_gain(float) property", asMETHOD(al_aux_slot, set_gain), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_aux_slot", "float get_gain() const property", asMETHOD(al_aux_slot, get_gain), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_aux_slot", "void set_send_auto(bool)", asMETHOD(al_aux_slot, set_send_auto), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_aux_slot", "bool get_efx_available() const property", asMETHOD(al_aux_slot, get_efx_available), asCALL_THISCALL));
	return true;
}

static bool register_al_source(asIScriptEngine* e) {
	CHECK(e->RegisterObjectType("al_source", 0, asOBJ_REF));
	CHECK(e->RegisterObjectBehaviour("al_source", asBEHAVE_FACTORY, "al_source@ f(al_context@)", asFUNCTION(al_source::factory), asCALL_CDECL));
	CHECK(e->RegisterObjectBehaviour("al_source", asBEHAVE_ADDREF,  "void f()", asMETHOD(al_source, add_ref), asCALL_THISCALL));
	CHECK(e->RegisterObjectBehaviour("al_source", asBEHAVE_RELEASE, "void f()", asMETHOD(al_source, release), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "bool get_is_valid() const property", asMETHOD(al_source, get_is_valid), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void play()", asMETHOD(al_source, play), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void stop()", asMETHOD(al_source, stop), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void pause()", asMETHOD(al_source, pause), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void rewind()", asMETHOD(al_source, rewind), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "int get_state() const property", asMETHOD(al_source, get_state), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "bool get_is_playing() const property", asMETHOD(al_source, get_is_playing), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "bool get_is_paused() const property", asMETHOD(al_source, get_is_paused), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "bool get_is_stopped() const property", asMETHOD(al_source, get_is_stopped), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_buffer(al_buffer@)", asMETHOD(al_source, set_buffer), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "al_buffer@ get_buffer()", asMETHOD(al_source, get_buffer), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void queue_buffer(al_buffer@)", asMETHOD(al_source, queue_buffer), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "al_buffer@ unqueue_buffer()", asMETHOD(al_source, unqueue_buffer), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "int get_buffers_queued() const property", asMETHOD(al_source, get_buffers_queued), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "int get_buffers_processed() const property", asMETHOD(al_source, get_buffers_processed), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_gain(float) property", asMETHOD(al_source, set_gain), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_gain() const property", asMETHOD(al_source, get_gain), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_pitch(float) property", asMETHOD(al_source, set_pitch), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_pitch() const property", asMETHOD(al_source, get_pitch), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_looping(bool) property", asMETHOD(al_source, set_looping), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "bool get_looping() const property", asMETHOD(al_source, get_looping), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_min_gain(float) property", asMETHOD(al_source, set_min_gain), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_min_gain() const property", asMETHOD(al_source, get_min_gain), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_max_gain(float) property", asMETHOD(al_source, set_max_gain), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_max_gain() const property", asMETHOD(al_source, get_max_gain), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_sec_offset(float) property", asMETHOD(al_source, set_sec_offset), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_sec_offset() const property", asMETHOD(al_source, get_sec_offset), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_sample_offset(float) property", asMETHOD(al_source, set_sample_offset), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_sample_offset() const property", asMETHOD(al_source, get_sample_offset), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_position(float, float, float)", asMETHOD(al_source, set_position), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_x(float) property", asMETHOD(al_source, set_x), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_x() const property", asMETHOD(al_source, get_x), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_y(float) property", asMETHOD(al_source, set_y), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_y() const property", asMETHOD(al_source, get_y), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_z(float) property", asMETHOD(al_source, set_z), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_z() const property", asMETHOD(al_source, get_z), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_velocity(float, float, float)", asMETHOD(al_source, set_velocity), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_velocity_x() const property", asMETHOD(al_source, get_velocity_x), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_velocity_y() const property", asMETHOD(al_source, get_velocity_y), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_velocity_z() const property", asMETHOD(al_source, get_velocity_z), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_direction(float, float, float)", asMETHOD(al_source, set_direction), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_direction_x() const property", asMETHOD(al_source, get_direction_x), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_direction_y() const property", asMETHOD(al_source, get_direction_y), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_direction_z() const property", asMETHOD(al_source, get_direction_z), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_reference_distance(float) property", asMETHOD(al_source, set_reference_distance), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_reference_distance() const property", asMETHOD(al_source, get_reference_distance), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_rolloff_factor(float) property", asMETHOD(al_source, set_rolloff_factor), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_rolloff_factor() const property", asMETHOD(al_source, get_rolloff_factor), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_max_distance(float) property", asMETHOD(al_source, set_max_distance), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_max_distance() const property", asMETHOD(al_source, get_max_distance), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_cone_inner_angle(float) property", asMETHOD(al_source, set_cone_inner_angle), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_cone_inner_angle() const property", asMETHOD(al_source, get_cone_inner_angle), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_cone_outer_angle(float) property", asMETHOD(al_source, set_cone_outer_angle), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_cone_outer_angle() const property", asMETHOD(al_source, get_cone_outer_angle), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_cone_outer_gain(float) property", asMETHOD(al_source, set_cone_outer_gain), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "float get_cone_outer_gain() const property", asMETHOD(al_source, get_cone_outer_gain), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_source_relative(bool) property", asMETHOD(al_source, set_source_relative), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "bool get_source_relative() const property", asMETHOD(al_source, get_source_relative), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_spatialize(int) property", asMETHOD(al_source, set_spatialize), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "int get_spatialize() const property", asMETHOD(al_source, get_spatialize), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_direct_filter(al_filter@)", asMETHOD(al_source, set_direct_filter), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "void set_aux_send(int, al_aux_slot@, al_filter@)", asMETHOD(al_source, set_aux_send), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_source", "bool get_efx_available() const property", asMETHOD(al_source, get_efx_available), asCALL_THISCALL));
	return true;
}

static bool register_al_capture_device(asIScriptEngine* e) {
	CHECK(e->RegisterObjectType("al_capture_device", 0, asOBJ_REF));
	CHECK(e->RegisterObjectBehaviour("al_capture_device", asBEHAVE_FACTORY, "al_capture_device@ f(const string& in, int, int, int)", asFUNCTION(al_capture_device::factory), asCALL_CDECL));
	CHECK(e->RegisterObjectBehaviour("al_capture_device", asBEHAVE_ADDREF,  "void f()", asMETHOD(al_capture_device, add_ref), asCALL_THISCALL));
	CHECK(e->RegisterObjectBehaviour("al_capture_device", asBEHAVE_RELEASE, "void f()", asMETHOD(al_capture_device, release), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_capture_device", "bool get_is_open() const property", asMETHOD(al_capture_device, get_is_open), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_capture_device", "void start()", asMETHOD(al_capture_device, start), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_capture_device", "void stop()", asMETHOD(al_capture_device, stop), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_capture_device", "int get_available() const property", asMETHOD(al_capture_device, get_available), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_capture_device", "string read_samples(int)", asMETHOD(al_capture_device, read_samples), asCALL_THISCALL));
	CHECK(e->RegisterObjectMethod("al_capture_device", "void close()", asMETHOD(al_capture_device, close), asCALL_THISCALL));
	return true;
}

static bool register_globals(asIScriptEngine* e) {
	CHECK(e->RegisterGlobalFunction("bool nval_load(const string& in = \"\")", asFUNCTION(nval_load), asCALL_CDECL));
	CHECK(e->RegisterGlobalFunction("bool nval_is_loaded()", asFUNCTION(nval_is_loaded), asCALL_CDECL));
	CHECK(e->RegisterGlobalFunction("bool nval_efx_available()", asFUNCTION(nval_efx_available), asCALL_CDECL));
	CHECK(e->RegisterGlobalFunction("bool nval_capture_available()", asFUNCTION(nval_capture_available), asCALL_CDECL));
	CHECK(e->RegisterGlobalFunction("string nval_get_default_device()", asFUNCTION(nval_get_default_device), asCALL_CDECL));
	CHECK(e->RegisterGlobalFunction("string nval_get_devices()", asFUNCTION(nval_get_devices), asCALL_CDECL));
	CHECK(e->RegisterGlobalFunction("string nval_get_capture_devices()", asFUNCTION(nval_get_capture_devices), asCALL_CDECL));
	CHECK(e->RegisterGlobalFunction("string nval_get_default_capture_device()", asFUNCTION(nval_get_default_capture_device), asCALL_CDECL));
	CHECK(e->RegisterGlobalFunction("int nval_get_al_error()", asFUNCTION(nval_get_al_error), asCALL_CDECL));
	CHECK(e->RegisterGlobalFunction("string nval_get_al_error_string(int)", asFUNCTION(nval_get_al_error_string), asCALL_CDECL));
	CHECK(e->RegisterGlobalFunction("string nval_get_version()", asFUNCTION(nval_get_version_string), asCALL_CDECL));
	CHECK(e->RegisterGlobalFunction("string nval_get_renderer()", asFUNCTION(nval_get_renderer_string), asCALL_CDECL));
	CHECK(e->RegisterGlobalFunction("string nval_get_vendor()", asFUNCTION(nval_get_vendor_string), asCALL_CDECL));

	// Buffer formats
	NVAL_CONST(AL_FORMAT_MONO8,    AL_FORMAT_MONO8);
	NVAL_CONST(AL_FORMAT_MONO16,   AL_FORMAT_MONO16);
	NVAL_CONST(AL_FORMAT_STEREO8,  AL_FORMAT_STEREO8);
	NVAL_CONST(AL_FORMAT_STEREO16, AL_FORMAT_STEREO16);
	// Source states
	NVAL_CONST(AL_INITIAL, AL_INITIAL);
	NVAL_CONST(AL_PLAYING, AL_PLAYING);
	NVAL_CONST(AL_PAUSED,  AL_PAUSED);
	NVAL_CONST(AL_STOPPED, AL_STOPPED);
	// Distance models
	NVAL_CONST(AL_NONE,                      AL_NONE);
	NVAL_CONST(AL_INVERSE_DISTANCE,          AL_INVERSE_DISTANCE);
	NVAL_CONST(AL_INVERSE_DISTANCE_CLAMPED,  AL_INVERSE_DISTANCE_CLAMPED);
	NVAL_CONST(AL_LINEAR_DISTANCE,           AL_LINEAR_DISTANCE);
	NVAL_CONST(AL_LINEAR_DISTANCE_CLAMPED,   AL_LINEAR_DISTANCE_CLAMPED);
	NVAL_CONST(AL_EXPONENT_DISTANCE,         AL_EXPONENT_DISTANCE);
	NVAL_CONST(AL_EXPONENT_DISTANCE_CLAMPED, AL_EXPONENT_DISTANCE_CLAMPED);
	// Effect types
	NVAL_CONST(AL_EFFECT_NULL,         AL_EFFECT_NULL);
	NVAL_CONST(AL_EFFECT_REVERB,       AL_EFFECT_REVERB);
	NVAL_CONST(AL_EFFECT_CHORUS,       AL_EFFECT_CHORUS);
	NVAL_CONST(AL_EFFECT_DISTORTION,   AL_EFFECT_DISTORTION);
	NVAL_CONST(AL_EFFECT_ECHO,         AL_EFFECT_ECHO);
	NVAL_CONST(AL_EFFECT_FLANGER,      AL_EFFECT_FLANGER);
	NVAL_CONST(AL_EFFECT_PITCH_SHIFTER,AL_EFFECT_PITCH_SHIFTER);
	NVAL_CONST(AL_EFFECT_COMPRESSOR,   AL_EFFECT_COMPRESSOR);
	NVAL_CONST(AL_EFFECT_EQUALIZER,    AL_EFFECT_EQUALIZER);
	NVAL_CONST(AL_EFFECT_EAXREVERB,    AL_EFFECT_EAXREVERB);
	// Reverb params
	NVAL_CONST(AL_REVERB_DENSITY,               AL_REVERB_DENSITY);
	NVAL_CONST(AL_REVERB_DIFFUSION,             AL_REVERB_DIFFUSION);
	NVAL_CONST(AL_REVERB_GAIN,                  AL_REVERB_GAIN);
	NVAL_CONST(AL_REVERB_GAINHF,                AL_REVERB_GAINHF);
	NVAL_CONST(AL_REVERB_DECAY_TIME,            AL_REVERB_DECAY_TIME);
	NVAL_CONST(AL_REVERB_DECAY_HFRATIO,         AL_REVERB_DECAY_HFRATIO);
	NVAL_CONST(AL_REVERB_REFLECTIONS_GAIN,      AL_REVERB_REFLECTIONS_GAIN);
	NVAL_CONST(AL_REVERB_REFLECTIONS_DELAY,     AL_REVERB_REFLECTIONS_DELAY);
	NVAL_CONST(AL_REVERB_LATE_REVERB_GAIN,      AL_REVERB_LATE_REVERB_GAIN);
	NVAL_CONST(AL_REVERB_LATE_REVERB_DELAY,     AL_REVERB_LATE_REVERB_DELAY);
	NVAL_CONST(AL_REVERB_AIR_ABSORPTION_GAINHF, AL_REVERB_AIR_ABSORPTION_GAINHF);
	NVAL_CONST(AL_REVERB_ROOM_ROLLOFF_FACTOR,   AL_REVERB_ROOM_ROLLOFF_FACTOR);
	NVAL_CONST(AL_REVERB_DECAY_HFLIMIT,         AL_REVERB_DECAY_HFLIMIT);
	// Echo params
	NVAL_CONST(AL_ECHO_DELAY,    AL_ECHO_DELAY);
	NVAL_CONST(AL_ECHO_LRDELAY,  AL_ECHO_LRDELAY);
	NVAL_CONST(AL_ECHO_DAMPING,  AL_ECHO_DAMPING);
	NVAL_CONST(AL_ECHO_FEEDBACK, AL_ECHO_FEEDBACK);
	NVAL_CONST(AL_ECHO_SPREAD,   AL_ECHO_SPREAD);
	// Chorus params
	NVAL_CONST(AL_CHORUS_WAVEFORM, AL_CHORUS_WAVEFORM);
	NVAL_CONST(AL_CHORUS_PHASE,    AL_CHORUS_PHASE);
	NVAL_CONST(AL_CHORUS_RATE,     AL_CHORUS_RATE);
	NVAL_CONST(AL_CHORUS_DEPTH,    AL_CHORUS_DEPTH);
	NVAL_CONST(AL_CHORUS_FEEDBACK, AL_CHORUS_FEEDBACK);
	NVAL_CONST(AL_CHORUS_DELAY,    AL_CHORUS_DELAY);
	// Flanger params
	NVAL_CONST(AL_FLANGER_WAVEFORM, AL_FLANGER_WAVEFORM);
	NVAL_CONST(AL_FLANGER_PHASE,    AL_FLANGER_PHASE);
	NVAL_CONST(AL_FLANGER_RATE,     AL_FLANGER_RATE);
	NVAL_CONST(AL_FLANGER_DEPTH,    AL_FLANGER_DEPTH);
	NVAL_CONST(AL_FLANGER_FEEDBACK, AL_FLANGER_FEEDBACK);
	NVAL_CONST(AL_FLANGER_DELAY,    AL_FLANGER_DELAY);
	// Filter types
	NVAL_CONST(AL_FILTER_NULL,     AL_FILTER_NULL);
	NVAL_CONST(AL_FILTER_LOWPASS,  AL_FILTER_LOWPASS);
	NVAL_CONST(AL_FILTER_HIGHPASS, AL_FILTER_HIGHPASS);
	NVAL_CONST(AL_FILTER_BANDPASS, AL_FILTER_BANDPASS);
	// Filter params
	NVAL_CONST(AL_LOWPASS_GAIN,    AL_LOWPASS_GAIN);
	NVAL_CONST(AL_LOWPASS_GAINHF,  AL_LOWPASS_GAINHF);
	NVAL_CONST(AL_HIGHPASS_GAIN,   AL_HIGHPASS_GAIN);
	NVAL_CONST(AL_HIGHPASS_GAINLF, AL_HIGHPASS_GAINLF);
	NVAL_CONST(AL_BANDPASS_GAIN,   AL_BANDPASS_GAIN);
	NVAL_CONST(AL_BANDPASS_GAINLF, AL_BANDPASS_GAINLF);
	NVAL_CONST(AL_BANDPASS_GAINHF, AL_BANDPASS_GAINHF);
	// Aux slot
	NVAL_CONST(AL_EFFECTSLOT_NULL,                AL_EFFECTSLOT_NULL);
	NVAL_CONST(AL_EFFECTSLOT_EFFECT,              AL_EFFECTSLOT_EFFECT);
	NVAL_CONST(AL_EFFECTSLOT_GAIN,                AL_EFFECTSLOT_GAIN);
	NVAL_CONST(AL_EFFECTSLOT_AUXILIARY_SEND_AUTO, AL_EFFECTSLOT_AUXILIARY_SEND_AUTO);
	// HRTF status
	NVAL_CONST(ALC_HRTF_DISABLED_SOFT,            ALC_HRTF_DISABLED_SOFT);
	NVAL_CONST(ALC_HRTF_ENABLED_SOFT,             ALC_HRTF_ENABLED_SOFT);
	NVAL_CONST(ALC_HRTF_DENIED_SOFT,              ALC_HRTF_DENIED_SOFT);
	NVAL_CONST(ALC_HRTF_REQUIRED_SOFT,            ALC_HRTF_REQUIRED_SOFT);
	NVAL_CONST(ALC_HRTF_HEADPHONES_DETECTED_SOFT, ALC_HRTF_HEADPHONES_DETECTED_SOFT);
	NVAL_CONST(ALC_HRTF_UNSUPPORTED_FORMAT_SOFT,  ALC_HRTF_UNSUPPORTED_FORMAT_SOFT);
	// AL_SOFT_source_spatialize
	NVAL_CONST(AL_SOURCE_SPATIALIZE_SOFT, AL_SOURCE_SPATIALIZE_SOFT);
	NVAL_CONST(AL_AUTO_SOFT,             AL_AUTO_SOFT);

	return true;
}

// ============================================================
// Plugin entry point
// ============================================================

plugin_main(nvgt_plugin_shared* shared) {
	if (!prepare_plugin(shared)) return false;
	asIScriptEngine* e = shared->script_engine;
	CScriptArray::SetMemoryFunctions(std::malloc, std::free); // for CScriptArray used by set_data_floats

	load_openal("");
	nvgt_bundle_shared_library("openal32");

	if (!register_al_device(e))         return false;
	if (!register_al_context(e))        return false;
	if (!register_al_buffer(e))         return false;
	if (!register_al_filter(e))         return false;
	if (!register_al_effect(e))         return false;
	if (!register_al_aux_slot(e))       return false;
	if (!register_al_source(e))         return false;
	if (!register_al_capture_device(e)) return false;
	if (!register_globals(e))           return false;
	return true;
}
