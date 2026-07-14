/* auto-generated (do not modify): type=TEXT creator=KAHL hex=544558544B41484C000000000000000000000000000000000000000000000000 */

/*
 *  cdev stub.c - standard cdev header
 *
 *  Copyright (c) 1991 Symantec Corporation.  All rights reserved.
 *
 */


main()
{
	extern _main();
	
	asm {
@0		bra.s	@1
		dc.w	0
		dc.l	'cdev'
		dc.w	0xF020,0,0,0
@1		lea		@0,a0			;  for A4
		jmp		_main
;		rts
	}
}
