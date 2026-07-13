/*
 *  cdev.h - standard cdev object
 *
 *  Copyright (c) 1991 Symantec Corporation.  All rights reserved.
 *
 */

#pragma once

struct cdev : indirect {

		/*  instance variables  */
		
	WindowPeek		dp;				/*  the Control Panel  */
	short			refnum;			/*  refnum of Control Panel DA  */
	short			rsrcID;			/*  base resource ID  */
	short			lastItem;		/*  # of last Control Panel item  */
	EventRecord		*event;			/*  ==> last event  */
	long			status;			/*  value to be returned  */
	
		/*  methods  */
		
	long			Message(short, short);	/*  dispatch message  */
	void			Error(long);		/*  set error return  */
	void			Init(void);			/*  "initDev"  */
	void			Close(void);		/*  "closeDev"  */
	void			Activate(void);		/*  "activDev"  */
	void			Update(void);		/*  "updateDev"  */
	void			Idle(void);			/*  "nulDev"  */
	void			ItemHit(short);		/*  "hitDev"  */
	void			Key(short);			/*  "keyEvtDev"  */
	void			CmdKey(short);		/*  "keyEvtDev" (command)  */
	void			Deactivate(void);	/*  "deactiveDev"  */
	void			Undo(void);			/*  "undoDev"  */
	void			Cut(void);			/*  "cutDev"  */
	void			Copy(void);			/*  "copyDev"  */
	void			Paste(void);		/*  "pasteDev"  */
	void			Clear(void);		/*  "clearDev"  */
};


/*  each cdev must implement these  */

Boolean Runnable(void);			/*  "macDev"  */
cdev *New(void);				/*  new(cdev)  */
