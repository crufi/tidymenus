/*
 *  cdev class.c - standard cdev object
 *
 *  Copyright (c) 1991 Symantec Corporation.  All rights reserved.
 *
 */

#include "cdev.h"

#include "::CrutchUtilities ƒ:CrutchError.h"
#include "::CrutchUtilities ƒ:CrutchSettings.h"

pascal long _main(short, short, short, short, EventRecord *, cdev *, WindowPeek);

/*
 *  _main - cdev entry point
 *
 *  The correct value for A4 must be in A0 on entry.  When built as
 *  an application (for debugging purposes), "cdev runner" calls this
 *  routine with A0=0; this signals us NOT to try to unload A4-based
 *  segments.  When built as a cdev, "cdev stub" loads A0 with the
 *  address of the cdev before calling this routine.  (Note that this
 *  file MUST be in the same segment as "cdev stub.c".)
 *
 */

pascal long
_main(short msg, short item, short nitems, short id, EventRecord *event, cdev *value, WindowPeek dp)
{
	long result;
	static short level;	 // re-entrancy count
	
	// set up A4

	asm {
		move.l	a4,-(sp)
		movea.l	a0,a4
	}
	
	// respond to message
	
	++level;
	if (msg == macDev)
		result = Runnable();
	else {
		if (msg == initDev) {
			if (value = New()) {
				value->dp = dp;
				value->refnum = dp->windowKind;
				value->rsrcID = id;
				value->lastItem = nitems;
			}
		}
		
		if ((result = (long) value)
			&& result != cdevUnset)  // *** I added this line, see below
		{
			value->event = event;

			// IMPORTANT - if the Init() method puts up a ModalDialog, we will get
			// reentered (see IM V:330, bottom of page) before returning 'result'
			// from the value->Message() call below, therefore on the reentry 
			// 'value' will again be cdevUnset (== 3).
			//
			// The original Symantec sample code would jump to garbage memory as
			// a result, by seeing a nonzero value then happily calling 
			// value->Message() again.  The line *** I added above fixes this.
			
			result = value->Message(msg, item);
		}
	}
	--level;
	
	// restore A4
	
	asm {
		move.l	a4, d0
		beq.s	@1
	}

	if (!level)
		UnloadA4Seg(0L);

	asm {
@1:		movea.l	(sp)+, a4
	}

	return result;
}


/*
 *  cdev::Message - respond to message
 *
 *  The message is dispatched to the appropriate handler.
 *
 *  Few subclasses will need to override this method.
 *
 */

long
cdev::Message(short msg, short item)
{
	long result;
	
	switch (msg) {
		case initDev:
			Init();
			break;
		case hitDev:
			ItemHit(item - lastItem);
			break;
		case closeDev:
			status = 0;
			break;
		case nulDev:
			Idle();
			break;
		case updateDev:
			Update();
			break;
		case activDev:
			Activate();
			break;
		case deactivDev:
			Deactivate();
			break;
		case keyEvtDev:
			if (!(event->modifiers & cmdKey))
				Key((unsigned char) event->message);
			else if (event->message != autoKey)
				CmdKey((unsigned char) event->message);
			break;
		case undoDev:
			Undo();
			break;
		case cutDev:
			Cut();
			break;
		case copyDev:
			Copy();
			break;
		case pasteDev:
			Paste();
			break;
		case clearDev:
			Clear();
			break;
	}
	if ((result = status) != (long) this)
		Close();

	return result;
}


/*
 *  cdev::Error - signal error
 *
 *  A typical call might be "Error(cdevResErr)".
 *
 *  Few subclasses will need to override this method.
 *
 */

void
cdev::Error(long code)
{
	status = code;
}


/*
 *  cdev::Init - handle "initDev" message
 *
 *  Subclasses overriding this method should call "inherited::Init()".
 *
 */

void cdev::Init()
{
	status = (long) this;
}

/*
 *  cdev::Close - handle "closeDev" message
 *
 *  Subclasses overriding this method should call "inherited::Close()".
 *
 */

void cdev::Close()
{
	delete(this);
}

/*
 *  cdev::CmdKey - process "keyEvtDev" (command key depressed)
 *
 *  Subclasses overriding this method should call "inherited::CmdKey(c)"
 *  if the command key is not recognized.
 *
 */

void
cdev::CmdKey(short c)
{
	switch (c) {
		case 'z': case 'Z':
			Undo();
			break;
		case 'x': case 'X':
			Cut();
			break;
		case 'c': case 'C':
			Copy();
			break;
		case 'v': case 'V':
			Paste();
			break;
	}
}

/*
 *  null methods
 *
 *  The remainder of the methods in this file are stubs.  Subclasses
 *  may choose to implement appropriate behavior.
 *
 */

void
cdev::Activate()
{
}


void
cdev::Update()
{
}


void
cdev::Idle()
{
}


void
cdev::ItemHit(short item)
{
}


void
cdev::Key(short c)
{
}


void
cdev::Deactivate()
{
}


void
cdev::Undo()
{
}


void
cdev::Cut()
{
}


void
cdev::Copy()
{
}


void
cdev::Paste()
{
}


void
cdev::Clear()
{
}
