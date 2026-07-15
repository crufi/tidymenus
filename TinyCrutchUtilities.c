/* auto-generated (do not modify): type=TEXT creator=KAHL hex=544558544B41484C000000000000000000000000000000000000000000000000 */
//==================================================================================
// TinyCrutchUtilities.c
// ©2024 Steve Crutchfield
//
// STRIPPED copy of CrutchUtilities.c -- Symantec C++ smart link doesn't drop
// unused functions, so this hand-stripped version exists to shrink the binary.
// Only the ShowInitIcon call wrappers survive; everything else this project
// needs lives in TinyCrutchError.c.  Re-strip after editing the original.
//
// A handy library of utilities for INITs, patches, and applications.
// Depends on the lower-level routines in CrutchError.c.
//
// I include this file and CrutchError.c directly in projects.  Precompiling a 
// library prevents THINK C's Smart Link option from working (though it doesn't seem
// to do much anyway...) and also prevents this code from seeing project-level
// settings like the APP_NAME #define.
//==================================================================================

#include <stdarg.h>
#include <QDOffscreen.h>
#include <Traps.h>
#include <Sound.h>
#include <Files.h>
#include <GestaltEqu.h>

#include "CrutchUtilities.h"




// =========== Syntactic sugar for ShowInitIcon (linked separately)

bool CallShowInitIcon(short code_id, short icon_id) {
	const Handle showIconCode = Get1Resource('Code', code_id);
	if (!showIconCode) return false;
	HLock(showIconCode);
	((pascal void (*) (short, Boolean)) *(showIconCode)) ((icon_id), true);
	HUnlock(showIconCode);
	return true;
}

bool CallShowInitIconXedOut(short code_id, short icon_id, short x_id) {
	const Handle showIconCode = Get1Resource('Code', code_id);
	if (!showIconCode) return false;
	HLock(showIconCode);
	((pascal void (*) (short, Boolean)) *(showIconCode)) ((icon_id), false);
    ((pascal void (*) (short, Boolean)) *(showIconCode)) ((x_id), true);
    HUnlock(showIconCode);
	return true;
}
