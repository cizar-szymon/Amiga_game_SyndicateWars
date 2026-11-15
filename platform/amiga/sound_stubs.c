/* Amiga build, first stage: no sound, music, CD audio, joystick or Smacker
 * video. These stand in for bfsoundlib, bfsmacklib and the joystick driver.
 * The game sees "no sound hardware". */
#include "bftypes.h"
#include "bfjoyst.h"
#include "bfaudio.h"
#include "bfdanger.h"
#include "bfscd.h"
#include "bfsound.h"
#include "bfmusic.h"
#include "ssampply.h"
#include "bfsvaribl.h"
#include "bfsmack.h"

/* bfaudio */
void InitAudio(AudioInitOptions *audOpts) { (void)audOpts; }
void FreeAudio(void) { }
void SetSoundMasterVolume(long vol) { (void)vol; }
void SetMusicMasterVolume(long vol) { (void)vol; }
int LoadSounds(ubyte bank_no) { (void)bank_no; return 0; }
int LoadMusic(ushort bankNo) { (void)bankNo; return 0; }

/* bfsound */
TbBool GetSoundAble(void) { return false; }
TbBool GetSoundActive(void) { return false; }
TbBool GetSoundInstalled(void) { return false; }
ubyte GetSoundTpNo(ushort snd_type) { (void)snd_type; return 0; }

/* samples */
TbBool IsSamplePlaying(long source_id, short smp_id, TbSampleHandle handle)
{ (void)source_id; (void)smp_id; (void)handle; return false; }
void ReleaseLoopedSample(ushort source_id, short smp_id) { (void)source_id; (void)smp_id; }
void SetSamplePitch(long source_id, short smp_id, short pitch)
{ (void)source_id; (void)smp_id; (void)pitch; }
void StopAllSamples(void) { }
void SetSampleVolume(long source_id, short smp_id, short volume)
{ (void)source_id; (void)smp_id; (void)volume; }
void SetSamplePan(long source_id, short smp_id, ushort pan)
{ (void)source_id; (void)smp_id; (void)pan; }
void StopSample(long source_id, short smp_id) { (void)source_id; (void)smp_id; }
struct SampleInfo *PlaySampleFromAddress(long source_id, short smp_id,
  short volume, ushort pan, short pitch, sbyte loop_count, ubyte a7, void *address)
{ (void)source_id; (void)smp_id; (void)volume; (void)pan; (void)pitch;
  (void)loop_count; (void)a7; (void)address; return NULL; }
/* one empty sample info structure, so the game's loops over them end at once */
static struct SampleInfo stub_sample_info;
struct SampleInfo *GetFirstSampleInfoStructure(void) { return &stub_sample_info; }
struct SampleInfo *GetLastSampleInfoStructure(void) { return &stub_sample_info; }

/* music */
void StartMusic(int songNo, ubyte volume) { (void)songNo; (void)volume; }
void StopMusicIfActive(void) { }
void SetMusicTempo(int tempo, int msec) { (void)tempo; (void)msec; }
void SetMusicTempoNormal(void) { }
void SetMusicVolume(int msec, ubyte volume) { (void)msec; (void)volume; }
void DangerMusicFadeSwitch(ubyte direction, ubyte freq) { (void)direction; (void)freq; }

/* CD audio */
TbBool GetCDAble(void) { return false; }
void PlayCDTrack(ushort trkno) { (void)trkno; }
void StopCD(void) { }
void SetCDVolume(short vol) { (void)vol; }

/* Smacker video */
TbResult play_smk(char *fname, u32 smkflags, ushort plyflags)
{ (void)fname; (void)smkflags; (void)plyflags; return Lb_FAIL; }
void set_smack_malloc(void *(*cb)(uint32_t)) { (void)cb; }
void set_smack_free(void (*cb)(void *ptr)) { (void)cb; }

/* joystick */
struct DevInput joy;
TbResult JoyDriverInit(void) { return Lb_FAIL; }
TbResult JoyDriverShutdown(void) { return Lb_OK; }
TbResult JoySetInterrupt(short val) { (void)val; return Lb_OK; }
TbResult JoySetupDevice(struct DevInput *dinp, int jtype) { (void)dinp; (void)jtype; return Lb_FAIL; }
TbResult JoyUpdateInputs(struct DevInput *dinp) { (void)dinp; return Lb_FAIL; }
TbResult JoyGetDeviceName(char *textbuf) { textbuf[0] = '\0'; return Lb_FAIL; }
const char *JoyGetButtonLabel(int button) { (void)button; return ""; }

/* PNG screenshots: no libpng in the Amiga build */
#include "bfpng.h"
TbResult LbPngSave(const char *fname, const TbPixel *inp_buffer,
  ulong width, ulong height, const ubyte *pal, TbBool force_fname)
{ (void)fname; (void)inp_buffer; (void)width; (void)height; (void)pal; (void)force_fname; return Lb_FAIL; }
TbResult LbPngSaveScreen(const char *fname, const TbPixel *inp_buffer,
  const ubyte *pal, TbBool force_fname)
{ (void)fname; (void)inp_buffer; (void)pal; (void)force_fname; return Lb_FAIL; }
