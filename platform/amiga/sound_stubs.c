/* Amiga build: no joystick yet; these stand in for
 * bfsmacklib and the joystick driver.  Sound is bfsoundlib with the Paula
 * driver (snd_bfs.c, snd_engine.c; on the Warp ARM snd_proxy.c). */
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


/* Smacker video: play_smk() in smk_play.c (libsmacker) */
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
