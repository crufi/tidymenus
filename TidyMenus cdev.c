/* auto-generated (do not modify): type=TEXT creator=KAHL hex=544558544B41484C000000000000000000000000000000000000000000000000 */
//=============================================================================
//
// TidyMenus cdev by Steve Crutchfield ©2020-2023.
//
// See 'TidyMenus INIT.c' for notes.
//
//=============================================================================

#include <Balloons.h>
#include <stddef.h>
#include <GestaltEqu.h>

#include "cdev.h"

#include "TidyMenus.h"
#include "Utilities.h"
#include "::CrutchUtilities ƒ:CrutchError.h"
#include "::CrutchUtilities ƒ:CrutchSettings.h"

// gestalt selectors

#define kCdevGestaltSelector	'NoLå'

// function prototypes

static pascal OSErr CdevHasRunGestaltSelector(OSType gestaltSelector, long *response);
static Boolean CdevGestaltIsRunning(void);
static void InstallCdevGestalt(void);

// cdev subclass

struct NoLabelCdev : cdev {
	void			Init(void);
	void			Close(void);
	void			ItemHit(short);
	void			Key(short);

	Boolean			HandleNoLabelCheckbox(void);
	Boolean			HandleNoHelpCheckbox(void);
	void			UpdateInfoBox(Boolean justCheckedNoLabel);

	enum {
		kAlreadyInstalled,
		kJustInstalled,
		kNotInstalled
	} installedState;
		
	ControlHandle 	noLabelCheckBox, noHelpCheckBox;
	Handle			infoBox;
};

enum { 
	itmNoLabelCheckBox = 1, 
	itmNoHelpCheckBox  = 2, 
	itmInfoBox         = 4
};

Boolean Runnable(void)
// should cdev appear in the control panel?  (implements the 'macDev' message)
// this only gets called if the 'mach' -4064 resource is 0000 FFFF
{
	return SystemVersion() > 0x0700;
}

cdev *New(void)
// creates a new cdev object
{
	return new NoLabelCdev;
}

static void InstallCdevGestalt(void)
// Install a dummy Gestalt selector function into the System heap -- we implement
// it here in hex and just BlockMove it to the sys heap to avoid the tiny hassle
// of having to compile separately into an extrmely tiny resource.
// 
// The only point of this Gestalt selector function is to indicate -- by its very
// existence in the System heap as confirmed by a later call to Gestalt() -- that
// the cdev has been run and the user has clicked the noLabel checkbox, and that 
// therefore the status of the noLabel flag in the Settings resource is valid even
// if the INIT hasn't run.
{
	const short dummyFunction[] = {
		// here we implement 'OSErr Func(OSType t, long *response) { return noErr; }'
		0x205F,		// movea.l  (sp)+, a0	; pop return address
		0x508F,		// addq.l   #8, a7      ; remove arguments from stack
		0x4257,		// clr.w	(a7)		; set return value to 'noErr'
		0x4ED0      // jmp      (a0)        ; return
	};

	const Ptr ptrToGestaltFuncInSysHeap = NewPtrSys(sizeof dummyFunction);
	BlockMove(dummyFunction, ptrToGestaltFuncInSysHeap, sizeof dummyFunction);

	AssertMesgReturn(TrapAvailable(_NewGestalt),
					 "Gestalt Manager not available", 
					 );

	AssertMesgReturn(noErr == NewGestalt(kCdevGestaltSelector, ptrToGestaltFuncInSysHeap),
					 "couldn't install Gestalt selector (possible duplicate?)", 
					 );
}

static Boolean CdevGestaltIsRunning(void)
// Checks for Gestalt selector added by the cdev if the noLabel checkbox is changed
{
	long ignore;
	return noErr == Gestalt(kCdevGestaltSelector, &ignore);
}

void NoLabelCdev::UpdateInfoBox(Boolean justCheckedNoLabel)
{
	if (installedState == kNotInstalled)
	{
		if (justCheckedNoLabel && (**gSettings).noLabel)
			SetIText(infoBox, "\pThe Label menu is hidden; install and restart to move it a submenu under “File”.");
		else
			SetIText(infoBox, "\pChanges take effect instantly.\rInstall and restart to hide the Help menu.");
	}
	else
	{
		if (justCheckedNoLabel && (**gSettings).noLabel)
			SetIText(infoBox, "\pThe Label menu is available in a submenu under the Finder’s “File” menu.");
		else
			SetIText(infoBox, "\pChanges take effect instantly.\rNo restart required.");
	}
}

void NoLabelCdev::Init(void)
// initialize the cdev:
//
// install INIT if not already installed at startup time, load settings,
// and initialize dialog items
{
	// do this first
	inherited::Init();
	
	installedState = IsOurGestaltInstalled() 
							? kAlreadyInstalled : kNotInstalled;

	if (installedState != kAlreadyInstalled)
	{
#ifdef ON_THE_FLY_INSTALL_ALLOWED  // always false for now 

		#error 
/*
		const InstallINITErrorCode err = InstallINIT();

		if (err == kNoErr)
			// all set
			installedState = kJustInstalled;
		else
			ComplainSprintf(
					 "Couldn’t do on-the-fly install (error #%d):  drag " APP_NAME " to "
					 "the System Folder and restart for a full install.  Until then, "
					 "you can hide the Label menu, but Help menu hiding may not work "
					 "correctly.",
			         err);
*/
#else
		MessageBoxSprintf(
		         APP_NAME " is not currently installed:  you can still hide/show the "
		         "Finder’s Label menu, but to hide the Help menu (and to move the Label "
		         "menu to a submenu while hidden), please install "
		         "by dragging to the System Folder and restart.");
#endif
	}

	if (GetCdevSettings() == kErrorGettingSettings 
		|| !gSettings)
	{
		// couldn't get settings -- tell OS to close out the cdev
		Error(cdevGenErr);
		return;
	}

	if (installedState != kAlreadyInstalled)
	{
		// config settings we missed at startup time:
		
		{
			const Handle menuList = MenuList;
			MenuList = SystemMenuList;
			(**gSettings).sysHelpMenuHandle = GetMHandle(kHelpMenuID);
			MenuList = menuList;
		}
		
		// unlike the Label menu (which can be hidden just with the cdev), 
		// we can never hide the Help menu if the INIT wasn't previously running, so 
		// always clear that flag here:
		
		(**gSettings).noHelp = false;
		
		// now decide if we need to clear the noLabel flag also:
		
		if (installedState == kNotInstalled)
		// (currently always true if we get here, since we aren't yet allowing
		// on-the-fly installs)
		{
			// INIT still not running ... has the noLabel checkbox been clicked 
			// in any prior invocation of the cdev?
			
			if (!CdevGestaltIsRunning())
			{
				// no -- clear the noLabel flag from the settings resource since 
				// whatever it says, the Label menu is in fact not hidden:
				
				(**gSettings).noLabel = false;
			}
		}				
	}					

	// set up dialog items:
	
	{
		Rect r;
		short itemType;
			
		GetDItemHandle(dp, itmNoLabelCheckBox, &noLabelCheckBox);
		GetDItemHandle(dp, itmNoHelpCheckBox,  &noHelpCheckBox);
		GetDItemHandle(dp, itmInfoBox,         &infoBox);
		
		// set up noLabel checkbox:
		
		if (SystemVersion() < 0x0800)
			SetCtlValue(noLabelCheckBox, (**gSettings).noLabel != 0);
		else
		{
			// disable noLabel checkbox in System 8+ (there is no Label menu):
			Str255 s;
			GetCTitle(noLabelCheckBox, s);
			AppendStr(s, "\p (System 7 only)");
			SetCTitle(noLabelCheckBox, s);

			HiliteControl(noLabelCheckBox, 255);
			SetCtlValue(noLabelCheckBox, 0);
		}

		// tweak dialog items based on installed state:
		
		if (installedState == kNotInstalled)
		{
			// disable noHelp if not installed
			HiliteControl(noHelpCheckBox, 255);
			SetCtlValue(noHelpCheckBox, 0);
		}
		else
		{
			// we are installed, update checkboxes accordingly
			SetCTitle(noLabelCheckBox, "\pHide Label menu under “File”");
			SetCtlValue(noHelpCheckBox,  (**gSettings).noHelp != 0);
		}
		
		UpdateInfoBox(false);
	}
}

void NoLabelCdev::Close(void)
// close out the cdev
{
	inherited::Close();
}

Boolean NoLabelCdev::HandleNoLabelCheckbox(void)
// noLabel option was changed -- show/hide the Label menu in the Finder
{	
	const MenuHandle origLabelMenu = GetMHandle(kLabelMenuID);  // might be in hier list
	const Boolean hideLabelMenu = !(**gSettings).noLabel;
	long ignore;	

	AssertMesgReturn(origLabelMenu != NULL, 
		"Couldn't find the original Label menu!",
		false);
	
	(**gSettings).weCalledInsertMenu = true;
	
	if (hideLabelMenu)
	{
		DeleteMenu(kLabelMenuID);						// remove original Label menu
		InsertMenu(origLabelMenu, hierMenu);			// move it to hier portion

		// invoke our Gestalt function to copy the Label menu
		// into a new hierarchical menu under the File menu if
		// possible -- it's OK if this fails, we have already 
		// hidden the Label menu (it just won't end up as a submenu):

		if (installedState != kNotInstalled)		
			Check(Gestalt(kHideLabelMenuSelector, &ignore));
	}
	else
	{
		DeleteMenu(kLabelMenuID);						// remove orig Label menu from hier list
		InsertMenu(origLabelMenu, kLabelMenuID + 1);	// restore to original spot
		
		// invoke our Gestalt function to remove our copy
		// of the Label menu as a submenu of the File menu:
		
		if (installedState != kNotInstalled)		
			Check(Gestalt(kShowLabelMenuSelector, &ignore));
	}

	(**gSettings).weCalledInsertMenu = false;

	(**gSettings).noLabel = hideLabelMenu;
	SetCtlValue(noLabelCheckBox, hideLabelMenu ? 1 : 0);

	return true;  // caller will DrawMenuBar and SaveSettings
}

Boolean NoLabelCdev::HandleNoHelpCheckbox(void)
{
	// noHelp item was changed
	
	// first, just note the new setting here -- don't change in gSettings yet
	// or would modify behavior of my patches as called below!
	const Boolean hideHelpMenu = !(**gSettings).noHelp;

	if ((**gSettings).sysHelpMenuHandle)  // ensure we've saved a system menu handle
	{
		// 1. add/remove Help menu to/from SystemMenuList
		
		if (hideHelpMenu)
		{
			DeleteMenuFromSystemMenuList(kHelpMenuID);
		}
		else
		{
			// re-insert Help menu into system menubar	
			(**gSettings).weCalledInsertMenu = true;
			InsertMenu((**gSettings).sysHelpMenuHandle, kApplicationMenuID);			
			(**gSettings).weCalledInsertMenu = false;
		}

		// 2. activate our _DrawMenuBar patch to update all apps
		// 	  as they are brought to the front
		
		(**gSettings).needDrawMenuBarPatch = true;
		
		// all done -- update gSettings and checkbox: 
		
		(**gSettings).noHelp = hideHelpMenu;  // needed before _DrawMenuBar patch!
		SetCtlValue(noHelpCheckBox, hideHelpMenu ? 1 : 0);	
		
		return true;  // caller will DrawMenuBar and SaveSettings
	}
	
	return false;
}

void NoLabelCdev::ItemHit(short item)
// process a dialog item click
{
	if (item == itmNoLabelCheckBox
		&& (**noLabelCheckBox).contrlHilite != 255)  // (not disabled)
	{
		if (HandleNoLabelCheckbox())
		{			
			DrawMenuBar();
			SaveSettings();
	
			// finally, if not already done, install our gestalt selector so subsequent 
			// invocations of the cdev know that the noLabel settings flag is valid even 
			// if the INIT didn't run:
			
			if (!CdevGestaltIsRunning())
				InstallCdevGestalt();
		}
		
		UpdateInfoBox(true);
	}
	else if (item == itmNoHelpCheckBox
		&& (**noHelpCheckBox).contrlHilite != 255)  // (not disabled)
	{
		if (HandleNoHelpCheckbox())
		{
			DrawMenuBar();  // force front app to update
			SaveSettings();
		}
		
		UpdateInfoBox(false);
	}
}

void NoLabelCdev::Key(short c)
// allow keyboard shortcuts -- translate keystrokes to dialog item clicks
{
	switch (c)
	{
		case 'L':
		case 'l':
			ItemHit(itmNoLabelCheckBox);
			break;
		
		case 'H':
		case 'h':
		case '?':
			ItemHit(itmNoHelpCheckBox);
			break;
	}
}
