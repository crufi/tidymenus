//=============================================================================
//
// TidyMenus INIT & cdev by Steve Crutchfield ©2020-2023.
//
// Requires System 7+.  Label menu hiding only works under System 7 (no Label 
// menu in System 8, it's always a submenu under 'File').
//
// Approach:
//
// - Hide the Label menu by moving it to the hierarchical portion of the MenuList
//   (so Finder can still find it with _GetMHandle).  This can be done from the 
//   cdev even if the INIT didn't run.
//
// - When hidden, stick the Label menu in a hierarchical submenu under 'File',
//   just like in MacOS 8 (this only works if the INIT was installed).
//
// - Prevent the system's default Help menu from getting copied into the menu bar
//	 by removing it from the SystemMenuList (see notes below) and saving its 
//	 handle in our gSettings struct (from which we retrieve it on demand when
//	 GetMHandle is called.
//
// - Hide each application's Help menu by moving it to the hierarchical portion of
//    the MenuList.  (Keeping this up-to-date also requires a _SetMenuBar patch.)
//
// - Patch _DrawMenuBar to update other applications' menu bars correctly after
//   the user has used the cdev to show/hide the Help menu (the cdev itself only
//   updates the foreground application).
//
// cdev features:
//
// 	- can press 'L' or 'H'/'?' to activate NoLabel/NoHelp checkboxes from keyboard
//
// Notes on how the Menu Manager works:
//
// - The SystemMenuList, an undocumented lomem global at 0x286, is a handle to
//   a MenuList structure (also undocumented but see Utilities.h) containing
//   "default copies" of the "system menus" (help, application menu, script menu
//   if applicable, etc.).  The SystemMenuList is used only to store default
//   copies; only the regular MenuList is ever drawn/used directly.  The 
//   regular MenuList ends up (see below) with handles to the system menus
//   after the regular menus (followed by the app's hierarchical menus at the 
//   end).  Of course, each application has its own MenuList.
//
// - The "calc" routine in the standard MBDF automatically adds the system
//   menus to the (regular) MenuList if (1) there is an Apple menu in the MenuList
//   and (2) there aren't any system menus yet (identified as such by large negative
//   menu IDs < -16384).
//
// - GetMenuBar removes any system menus from the returned handle.  SetMenuBar
//   reinstates the "default" versions from the SystemMenuList.
//
// - GetNewMBar doesn't just read in a resource -- instead, calls GetMenuBar to
//   save the current menubar, then creates a new MenuList, populates it with
//   InsertMenu, and returns a handle to it before using SetMenuBar to restore things.
//	 (So any app that calls GetNewMBar could trip our InsertMenu patch before the 
// 	 menu bar is actually visible.)
//
// - ClearMenuBar nukes the whole MenuList including the hier portion.  (THINK
//   reference incorrectly states that it simply visually erases the menubar on the 
//	 screen -- this is wrong.)
//
// - InsertMenu will always (1) first insert the given menu into the hier portion of the 
//   app's menulist if beforeID == hierMenu == -1.  Otherwise, (2) it checks for a large 
//   negative menuID and, if it finds one, sticks the given menu (e.g. Help menu)
//   into the SystemMenuList -- else (3) in to the regular portion of the MenuList.
//	 It's impossible to use InsertMenu to insert a system menu (as indicated by its ID)
//   directly into the regular MenuList.  (SetMenuBar accomplishes this using Munger.)
//
// - DeleteMenu will delete the given menu from anywhere in the app's MenuList
//   (regular or hier portion), but won't touch the SystemMenuList.  It will
//   do nothing if given a bad menuID.
//
// - GetMHandle only returns a handle to the menu if found in the app's MenuList
//   (regular or hierportion), never the SystemMenuList.  However, we can force
//   GetMHandle to get a handle from the SystemMenuList by setting MenuList = 
//   SystemMenuList first (then restoring it).  (This also works for DeleteMenu.)
//	 It will nicely return NULL if given a bad ID.
//
// - For the Help menu in particular, by default the MenuList just ends up with a 
//   handle to the system help menu in the SystemMenuList.  However, if an app
//   calls HMGetHelpMenuHandle, it will always get a handle to a new, app-specific
//   copy of the Help menu, because the Help Manager assumes the app is asking
//   for the handle so it can append its own items.  At the same time, HMGetHelp...
//   appends a divider (dash) to the bottom of the new Help menu.  This is done
//   by checking to see if the app's help menu handle == the one in the system menu
//   list.  If not, presuming the app already has its own copy, the app's handle
//   is just returned.  Otherwise (the handles are currently equal), a copy of
//   the system help menu is made (with HandToHand) and munged into the MenuList
//   (with Munger ... NOT InsertMenu).
//
// - Apple Guide patches the Menu Manager, including a tail patch to _SetMenuBar
//   (which might replace whatever help menu _SetMenuBar re-inserts with something
//   else) and a patch to _InsertMenu.
//
// Note:  Desk Accessories and certain apps (ResEdit, BBEdit) get a dashed line at the 
// bottom of their Help menus when Apple Guide is running.  This is due to someone
// (presumably the Apple Guide patches) calling HMGetHelpMenuHandle needlessly, and
// happens with or without my INIT.  Not my problem!
//
//=============================================================================

#include <SetupA4.h>
#include <Traps.h>
#include <GestaltEqu.h>

#include "TidyMenus.h"
#include "Utilities.h"
#include "CrutchUtilities.h"
#include "CrutchSettings.h"

STATIC_ASSERT(sizeof(Settings) % 2 == 0);  // CrutchSettings' GetHandleSize check
                                           // relies on an even struct size

#define _SetItemMark _SetItmMark  // 'Traps.h' spells it _SetItmMark

DECLARE_PATCH(	InsertMenu,  	 void, 		 (MenuHandle theMenu, short beforeID));
DECLARE_PATCH(	DrawMenuBar, 	 void, 		 (void));
DECLARE_PATCH(	GetMHandle,  	 MenuHandle, (short menuID));
DECLARE_PATCH(	Pack14, 		 void, 	 	 (void));
DECLARE_PATCH(  SetMenuBar,		 void,		 (Handle mbar));
DECLARE_PATCH(  EnableItem,      void,		 (MenuHandle, short));
DECLARE_PATCH(  DisableItem,     void,		 (MenuHandle, short));
DECLARE_PATCH(  CheckItem,       void,		 (MenuHandle, short, Boolean));
DECLARE_PATCH(  SetItemMark,     void,		 (MenuHandle, short, short));
DECLARE_PATCH(  MenuSelect,      long,       (Point));

#define HMGetHelpMenuHandleSelector 0x0200

#define kMaxHierMenuID 				230

#define kGoodIcon					-4064
#define kXOutIcon					-4063
#define kShowInitIconCodeID			-4048

MenuHandle 	gOurLabelMenu;  // if NULL, next 3 globals are not valid
MenuHandle	gFileMenu;
MenuHandle  gOrigLabelMenu;

// function headers

pascal OSErr PatchedHMGetHelpMenuHandle(MenuHandle *menu);

pascal OSErr MoveLabelMenuInOrOutOfFileMenuGestalt(OSType selector, const long *ignore);

short GetLabelSubmenuItemNumberInFileMenu();

//=============================================================================
// This first collection of routines & patches is specific to moving the
// Label menu into a submenu under the File menu.
//=============================================================================

short GetLabelSubmenuItemNumberInFileMenu()
// Returns the item number of the "Label" submenu we added in the File menu
// (by comparing item marks, starting from the bottom, with the ID of our
// copy of the Label menu).  Saving the item number isn't a really clean solution
// because the Finder adds the "Print..." item below "Page Setup" (but above
// our Label item) sometime after startup.  (In point of fact since we actually
// don't add our Label submenu until the first time MenuSelect is called, it's
// probably safe to just save the item number then, but this approach is more
// robust to the possibility that another INIT added an item (like "Quit")
// to the Finder's File menu before/after us.)
{
	int i;
	
	AssertMesgReturn(gOurLabelMenu != NULL, "Our Label submenu isn't installed", 0);
	
	for (i = CountMItems(gFileMenu); i > 0; i--)
	{
		short mark;
		GetItemMark(gFileMenu, i, &mark);
		
		if (mark == (**gOurLabelMenu).menuID)
			break;
	}
	
	AssertMesgReturn(i > 0, "Our Label submenu is missing from the File menu", 0);

	return i;	
}

pascal OSErr MoveLabelMenuInOrOutOfFileMenuGestalt(OSType selector, const long *ignore)
// depending on the selector used, this routine either moves the Label
// menu into a submenu under File, or returns it therefrom
// 
// it does not actually call Insert/DeleteMenu to add/remove the original 'Label'
// menu from the menubar -- we do that either (1) if the INIT is loaded at startup,
// by blocking the Finder from inserting the Label menu into the menubar from our 
// InsertMenu patch or (2) when the cdev checkbox is clicked, directly by calling
// DeleteMenu/InsertMenu from the cdev.
//
// *ignore is not used and it's OK to pass in NULL when calling this function directly.
{
	SetUpA4();

	if (!EqualStr(CurApName, FinderName))
	{
		// Gestalt-enumerating utilities (TattleTech etc.) invoke every installed
		// selector -- quietly do nothing unless the Finder's menus are in context:
		RestoreA4();
		return gestaltUnknownErr;
	}

	gFileMenu      = GetMHandle(kFileMenuID);
	gOrigLabelMenu = GetMHandle(kLabelMenuID);  // might be in hier list
	
	AssertMesgReturn(gFileMenu && gOrigLabelMenu,
		"Couldn't find the original File and/or Label menus — unsupported system version?",
		gestaltUnknownErr);

	if (selector == kHideLabelMenuSelector 
		&& !gOurLabelMenu)
	{
		// hide the Label menu in a hierarchical menu under File
		//			
		// unfortunately the Finder's normal kLabelMenuID > 255, which is the 
		// max allowable ID for hierarchical menus (since the ID must fit in
		// the ItemMark field, which is one byte) -- in fact non-DA hier menus
		// are supposed to have IDs <= 235.  so we find one that isn't being used
		// and copy the Label menu there:

		int newLabelMenuID = kMaxHierMenuID;
		int numItems;
		MenuHandle menuCopy;
		Boolean savedWeCalledInsertMenu;
		
		menuCopy = gOrigLabelMenu;
					
		while (GetMHandle(newLabelMenuID))		// find an unused hier menu ID
			--newLabelMenuID;

		if (!Assert(newLabelMenuID >= 128))		// couldn't find a menu ID???
			goto done;
		
		if (!Check(HandToHand((Handle *) &menuCopy)))  // copy the menu
			goto done;
			
		gOurLabelMenu = menuCopy;	// (only set now that the copy has succeeded, so
									// failure paths above leave gOurLabelMenu NULL)

		(**gOurLabelMenu).menuID = newLabelMenuID;  // change ID of copy to our hier ID

		savedWeCalledInsertMenu = (**gSettings).weCalledInsertMenu;
		(**gSettings).weCalledInsertMenu = true;
		InsertMenu(gOurLabelMenu, hierMenu);		// add copy to hier menu list
		(**gSettings).weCalledInsertMenu = savedWeCalledInsertMenu;  // (restore -- the
										// cdev may have this set around its Gestalt call)
		
		// add a divider then our hier Label menu to end of File menu:
		
		AppendMenu(gFileMenu, "\p(-");
		AppendMenu(gFileMenu, "\px");	// placeholder -- real title set below, since
										// AppendMenu would interpret metacharacters

		numItems = CountMItems(gFileMenu);

		HLock((Handle) gOurLabelMenu);
		SetItem(gFileMenu, numItems, (**gOurLabelMenu).menuData);  // use orig Label menu title
		HUnlock((Handle) gOurLabelMenu);

		SetItemMark(gFileMenu, numItems, newLabelMenuID);
		SetItemCmd (gFileMenu, numItems, hMenuCmd);
		
		if ((**gOrigLabelMenu).enableFlags & 1)  // bit 0 == menu proper enabled?
			EnableItem(gFileMenu, numItems);
		else
			DisableItem(gFileMenu, numItems);
	}
	else if (selector == kShowLabelMenuSelector)
	{
		if (gOurLabelMenu)
		// (it's possible gOurLabelMenu == NULL here in the case where the user
		// had the Label menu hidden from startup, never chose anything from a
		// menu [so our _MenuSelect patch never got called to initialize gOurLabelMenu],
		// then opened the control panel to un-hide the Label menu ... so if
		// gOurLabelMenu == NULL here, we just skip all this and quietly do nothing)
		{
			Str255 itemStr;
			const short ourLabelItemInFileMenu = GetLabelSubmenuItemNumberInFileMenu();
			
			DeleteMenu((**gOurLabelMenu).menuID);	// remove our copy from hier list
			DisposeHandle((Handle) gOurLabelMenu);	// dispose of our copy
			gOurLabelMenu = NULL;
	
			// delete our divider and hier 'Label' item from File menu:
	
			GetItem(gFileMenu, ourLabelItemInFileMenu - 1, itemStr);
			
			if (Assert(itemStr[0] == 1 && itemStr[1] == '-'))
			// is item just a divider?
			{
				GetItem(gFileMenu, ourLabelItemInFileMenu, itemStr);
				
				if (Assert(EqualStr(itemStr, (**gOrigLabelMenu).menuData)))
				// does item text match title of the Label menu?
				{
					DelMenuItem(gFileMenu, ourLabelItemInFileMenu);
					DelMenuItem(gFileMenu, ourLabelItemInFileMenu - 1);
				}
				else
					goto done;
			}
			else
				goto done;
		}
	}
	else  
	{
		// unknown selector or bad gOurLabelMenu state
		ComplainSprintf("Couldn't move Label menu to/from a 'File' submenu (selector '%t', handle %xl)", selector, gOurLabelMenu);
		goto done;
	}

done:			
	RestoreA4();
	return noErr;  // already reported any problems to user
}

//=============================================================================
// _Enable/DisableItem, _CheckItem, and _SetItemMark patches
//
// These patches simply map whatever the Finder is trying to do to the original
// Label menu to our submenu copy of the Label menu under 'File' (if it exists).
//=============================================================================

pascal void PatchedEnableItem(MenuHandle menu, short item)
{
	SetUpA4();
	
	if (gOurLabelMenu && menu == gOrigLabelMenu)
	{
		CALL_ORIG_TRAP(EnableItem) (gOurLabelMenu, item);
		
		if (item == 0)  // whole menu
			CALL_ORIG_TRAP(EnableItem) (gFileMenu, GetLabelSubmenuItemNumberInFileMenu());
	}
	
	RESTORE_A4_AND_JUMP_TO_TOOLTRAP(EnableItem);
}

pascal void PatchedDisableItem(MenuHandle menu, short item)
{
	SetUpA4();

	if (gOurLabelMenu && menu == gOrigLabelMenu)
	{
		CALL_ORIG_TRAP(DisableItem) (gOurLabelMenu, item);

		if (item == 0)  // whole menu
			CALL_ORIG_TRAP(DisableItem) (gFileMenu, GetLabelSubmenuItemNumberInFileMenu());
	}
	
	RESTORE_A4_AND_JUMP_TO_TOOLTRAP(DisableItem);
}

pascal void PatchedCheckItem(MenuHandle menu, short item, Boolean checkIt)
{
	SetUpA4();
	
	if (gOurLabelMenu && menu == gOrigLabelMenu)
		CALL_ORIG_TRAP(CheckItem) (gOurLabelMenu, item, checkIt);
	
	RESTORE_A4_AND_JUMP_TO_TOOLTRAP(CheckItem);
}

pascal void PatchedSetItemMark(MenuHandle menu, short item, short mark)
// Note:  last argument is Pascal 'char', must be 'short' here for proper
// stack management.
{
	SetUpA4();
	
	if (gOurLabelMenu && menu == gOrigLabelMenu)
		CALL_ORIG_TRAP(SetItemMark) (gOurLabelMenu, item, mark);
	
	RESTORE_A4_AND_JUMP_TO_TOOLTRAP(SetItemMark);
}

//=============================================================================
// _MenuSelect patch
//
// This patch -- only needed if we're hiding the Label menu in a hierarchical
// submenu under 'File' -- has two important jobs:
//
// 1. When the user first pulls down a menu after Finder launch, check to see if
//    we need to move the (by now fully-constructed) Label menu into a submenu 
//    under File.
//
// 2. This patch also checks to see if the noLabel flag is set and we're in the 
//    Finder but our Label menu copy (gOurLabelMenu) is NULL -- if so, we haven't 
//    yet gotten around to copying the Label menu and creating a hierarchical 
//    submenu under File.  (We can't do this when the Label menu is inserted 
//    because it's inserted with no items yet -- so any submenu copy made at that 
//    time would have no items.)
//
//=============================================================================

pascal long PatchedMenuSelect(Point p)
{
	long result;
	
	WITH_A4(		
		// 1.  first, we take this opportunity of the user pulling down a menu to
		// see if we need to hide the Label menu for the first time since (this
		// instance of) the Finder was launched:
		
		if ((**gSettings).noLabel				// Label should be hidden
			&& EqualStr(CurApName, FinderName)	// we are in the Finder
			&& !gOurLabelMenu)					// we have never tried to hide it
		{
			// move the Label menu to a hierarchical submenu under File:
			MoveLabelMenuInOrOutOfFileMenuGestalt(kHideLabelMenuSelector, NULL);
		}

		// 2.  now we do the real work of this patch -- if the user selects something from
		// our Label submenu under the Finder's file menu, swap in the original Label
		// menu's menuID in the result and return:
		
		if (gOurLabelMenu					// may have just been set above!
			&& EqualStr(CurApName, FinderName))	// (gOurLabelMenu lives in the Finder's
												// heap -- it's stale if the Finder quit)
		{
			result = CALL_ORIG_TRAP(MenuSelect) (p);
	
			if (FIRST_WORD(result) == (**gOurLabelMenu).menuID
				&& GetMHandle((**gOurLabelMenu).menuID) == gOurLabelMenu)
			{
				FIRST_WORD(result) = kLabelMenuID;
			}
		}
		else		
			RESTORE_A4_AND_JUMP_TO_TOOLTRAP(MenuSelect);
	);  // restore A4
				
	return result;
}

//=============================================================================
// _Pack14 patch
//
// Needed to patch HMGetMenuHandle, which is called via a selector.
//=============================================================================

pascal void PatchedPack14(void)
// _Pack14 uses D0 to jump to a Pascal-style function whose arguments
// should already be on the stack, so we do the same thing here
{
	asm {
		cmp.w #HMGetHelpMenuHandleSelector, d0  // is it HMGetHelpMenuHandle?
		beq   PatchedHMGetHelpMenuHandle		// yes, call our patch		
		JMP_ORIG_TRAP(Pack14)					// no, call original Pack14
	}
}

//=============================================================================
// HMGetHelpMenuHandle patch
//
// If the help menu is hidden, we do one of two things:
//
//	 1. If we've already stuck the app's help menu handle into the hier portion,
//      and it != system menu, just return it
//
//   2. Otherwise we call the original trap to copy the system's help menu into
//      a new handle for the app.  To do so we need to first move the app's help
//      menu back from the hier portion to the MenuList (where the original trap
//      will find it), and put the system help menu back into the SystemMenuList.  
//      Then after calling the original trap, we put everything back.
//=============================================================================

pascal OSErr PatchedHMGetHelpMenuHandle(MenuHandle *menu)
{
	short result = noErr;
	MenuHandle appHelpMenu;

	SetUpA4();

	if ((**gSettings).noHelp
		&& (**gSettings).sysHelpMenuHandle)
	{
		// is app's help menu already different from system menu?
		// if so, just return the app's menu

		appHelpMenu = GetMHandle(kHelpMenuID);  // try to grab app menu from hier portion
		
		if (appHelpMenu 
			&& appHelpMenu != (**gSettings).sysHelpMenuHandle)
		{
			// they are not equal -- just return app's menu
			*menu = appHelpMenu;
		}
		else
		{
			// app's help menu equals system menu or hasn't been added yet
			
			// temp insert system help menu back into system menulist:
			
			(**gSettings).weCalledInsertMenu = true;
			InsertMenu((**gSettings).sysHelpMenuHandle, kApplicationMenuID);
			(**gSettings).weCalledInsertMenu = false;

			// call Get/SetMenuBar to get it into app's list:
			
			{
				const Handle mbar = GetMenuBar();
				SetMenuBar(mbar);
				DisposeHandle(mbar);
			}
			
			// call original trap (which will notice they are equal,
			// and munge in a handle to a new copy of the system help
			// menu into the app's MenuList)
			
			asm {
				clr.w   -(sp)							  	// space for result
				move.l  menu, -(sp)							// push ptr to menuHdl
				move.w 	#HMGetHelpMenuHandleSelector, d0  	// set selector
				movea.l gOrigPack14, a0						// get orig routine addr
				jsr 	(a0)								// call it
				move.w  (sp)+, result						// get result
			}
			
			// move app's new menu (now a newly created handle!) back to hier portion

			appHelpMenu = GetMHandle(kHelpMenuID);
			DeleteMenu(kHelpMenuID);  // remove from regular MenuList

			(**gSettings).weCalledInsertMenu = true;
			InsertMenu(appHelpMenu, hierMenu);  // re-add to hier portion
			(**gSettings).weCalledInsertMenu = false;

			// delete system menu from system menulist
			DeleteMenuFromSystemMenuList(kHelpMenuID);
		}
	}
	else
	{
		// help menus not hidden, or system help menu not initialized --
		// call original trap

		asm {
			clr.w   -(sp)							  	// space for result
			move.l  menu, -(sp)							// push ptr to menuHdl
			move.w 	#HMGetHelpMenuHandleSelector, d0  	// set selector
			movea.l gOrigPack14, a0						// get orig routine addr
			jsr 	(a0)								// call it
			move.w  (sp)+, result						// get result
		}
	}

	RestoreA4();
	
	return result;
}

//=============================================================================
// _SetMenuBar patch
//
// Normally, GetMenuBar removes any system menus (help, application menu...)
// from the menubar, and SetMenuBar replaces them with the "default" system menus
// (e.g. the standard help menu with no added items) from the SystemMenuList.
// Neither of these routines looks at or touches whatever's in the hier portion
// of the app's menulist.
//
// This means that if the help menu is hidden, whatver application help menu I 
// may have previously saved in the hier portion of the app's menulist, if 
// someone calls _SetMenuBar, I need to forget it, because if the help menu 
// were NOT hidden, it would have just gotten wiped and replaced with a plain
// default help menu.  So, this patch does that.
//=============================================================================

pascal void PatchedSetMenuBar(Handle mbar)
// removes any help menu in the hier portion of mbar from the list
// before setting the menubar (normally there would be none)
{
	SetUpA4();
	
	if (MenuList != SystemMenuList)
	{
		const Handle savedMenuList = MenuList;

		// a "normal" menubar handle returned by GetMenuBar would never have 
		// system menus in it -- but ours might, because we stick the app's help
		// menu in the hier portion, which GetMenuBar doesn't strip of system
		// menus!  so before passing to the original SetMenuBar trap, ensure there's
		// no help menu lurking in there (we can't do this after calling SetMenuBar
		// because it will correctly insert the system help menu WITHOUT deleting
		// ours from hier portion):

		MenuList = mbar;
		DeleteMenu(kHelpMenuID);  // remove app's help menu, if any, from hier portion
		MenuList = savedMenuList;
	}
	
	RESTORE_A4_AND_JUMP_TO_TOOLTRAP(SetMenuBar);
	// note the Apple Guide tail-patches _SetMenuBar and may, after this point,
	// replace whatever help menu is in the application's menu bar with its own
	// menu (by swapping in the handle directly into the MenuList -- not with 
	// InsertMenu).  our DrawMenuBar patch corrects for this later.
}

//=============================================================================
// _GetMHandle patch
//
// If original trap can't find system help menu, return our saved handle.
// (We don't need to do anything for the app's help menu, since the original
// trap will find it even in the hier portion.)
//=============================================================================

pascal MenuHandle PatchedGetMHandle(short menuID)
{
	MenuHandle result;	
	SetUpA4();

	result = CALL_ORIG_TRAP(GetMHandle) (menuID);

	if (menuID == kHelpMenuID
		&& (**gSettings).noHelp
		&& !result
		&& MenuList == SystemMenuList)
	{		
		// couldn't find system Help menu, probably because we've hidden it
		// (we remove it from SystemMenuList entirely, we don't just put it 
		// in the hier portion); return our saved handle
		result = (**gSettings).sysHelpMenuHandle;
	}

	RestoreA4();
	
	return result;
}

//=============================================================================
// _InsertMenu patch
//
// If noHelp is set and someone is inserting the Help menu (which always 
// ends up in the SystemMenuList), save away the handle to the system help menu
// (for later use by GetMHandle etc.) and block the insertion.
//
// If noLabel is set and the Finder is inserting the Label menu (checked using
// both ID and English name, just in case), switch beforeID so the insertion
// happens in the hierarchical portion of the Finder's MenuList (so that the
// user won't see it, but GetMHandle will still find it).
//=============================================================================

pascal void PatchedInsertMenu(MenuHandle theMenu, short beforeID)
{
	const short menuID = (**theMenu).menuID;

	SetUpA4();

	if (!(**gSettings).weCalledInsertMenu)
	{
		if (menuID == kHelpMenuID)
		{
			// always check for Help menu -- we save its handle away because GetMHandle
			// doesn't necessarily find it -- it seems we could always grab it with 
			// GetMHandle later if we first swapped out lomem global MenuList with 
			// SystemMenuList (0x286, defined in MenuMgrPriv.a) but we do it this way
			// rather then using that undocumented lomem global in this instance:
	
			if (HandleZone((Handle) theMenu) == SysZone)
			// (above ensures we aren't saving some app's menu here somehow)
				(**gSettings).sysHelpMenuHandle = theMenu;
	
			if ((**gSettings).noHelp)
			{
				// don't insert anything, but we saved the handle				
				RestoreA4();
				return;
			}
		}		
		else if ((**gSettings).noLabel
			&& menuID == kLabelMenuID
			&& EqualStr(CurApName, FinderName))  // (ensure we're in the Finder)
		{
			// hide the menu in the "hierarchical" part of the MenuList
			// (simply not inserting it at all causes a crash, presumably 
			// because Finder looks for it with _GetMHandle)
			beforeID = hierMenu;
			
			// the next line is needed here (not just at boot time) because it's possible
			// the Finder was relaunched -- ensure we know any prior saved Label menu
			// info is stale:
			gOurLabelMenu = NULL;
		}
	}
			
	RESTORE_A4_AND_JUMP_TO_TOOLTRAP(InsertMenu);
}

//=============================================================================
// _DrawMenuBar patch
//
// This patch is only needed if user has used the control panel to hide/
// show help menu on the fly -- if so, we use this to ensure that each app's
// menu bar includes/excludes the help menu appropriately whenever it's next
// drawn (rather than keeping a list of non-updated apps around, we just do this 
// forever once the user uses one of our checkboxes -- maybe could be optimized, 
// but this should be fast enough not to matter):
//=============================================================================

pascal void PatchedDrawMenuBar(void)
{
	SetUpA4();

	if ((**gSettings).needDrawMenuBarPatch)  // (skip unless user has touched our cdev)
	{
		// Get/SetMenuBar deletes then re-adds the standard system menus, including
		// help (if it's in the system menulist).  So this deletes the help menu
		// from app's menu bar if we've deleted it from the system menu bar --
		// or adds back the standard system help menu if we've added it back:
		//
		// (NOTE:  SetMenuBar only re-adds back the STANDARD system help menu,
		// not e.g. the Finder's help menu with any custom items -- oddly, this means
		// that an app calling Get/SetMenuBar deletes its own custom help items,
		// if any!  kind of surprising but confirmed with testing and by looking at
		// the ROM code...)

		MenuLocation whereIsIt;
		const MenuHandle appHelpMenu = GetMHandleAndLocation(kHelpMenuID, &whereIsIt);
		
		if ((**gSettings).noHelp && whereIsIt == kInMainList)
		// there should be no help menu, but there is one in the main MenuList
		{
			// we have already removed help menu from SytemMenuList, so Get/SetMenuBar
			// simply removes the help menu:

			const Handle mbar = GetMenuBar();
			SetMenuBar(mbar);
			DisposeHandle(mbar);

			(**gSettings).weCalledInsertMenu = true;
			InsertMenu(appHelpMenu, hierMenu);  // re-inserts into hier portion
			(**gSettings).weCalledInsertMenu = false;
		}
		else if (!(**gSettings).noHelp && whereIsIt != kInMainList)
		// the app has a help menu, and it's in the hier list -- OR the
		// app doesn't have a help menu, but the system menu hasn't been
		// copied over yet
		{
			const Handle mbar = GetMenuBar();	
			SetMenuBar(mbar);  // (our patch also removes any help menu from hier portion)
			DisposeHandle(mbar);

			// calling Get/SetMenuBar normally adds the system help menu (we've 
			// already re-added it to SystemMenuList), however if Apple Guide is 
			// running it gets confused (?) and thinks we're adding a system help
			// menu for the first time, and tail-patches _SetMenuBar to replace it 
			// with the Macintosh Guide version of the menu (which we normally only 
			// expect in the Finder unless the app has added its own help items?).  
			// so here we replace whatever help menu is in the MenuList with the
			// one we want to be there -- either the app's help menu we grabbed above,
			// or the original, non-Apple-Guide system help menu:
			
			(void) ReplaceHelpMenuInMenuList(appHelpMenu
												? appHelpMenu 
												: (**gSettings).sysHelpMenuHandle);
		}
	}
	
	RESTORE_A4_AND_JUMP_TO_TOOLTRAP(DrawMenuBar);
}

//=============================================================================
// INIT entry point
//
// This code is designed to be runnable by the cdev (after setting up for global
// trap patches -- see InstallINIT.c) or at INIT time.  The BOOT_TIME macro
// tells us when we're running.
//=============================================================================

void main()
{	
	Boolean good = false;
	Ptr 	initPtr;
	Handle 	initHndl;
	
	asm { move.l a0, initPtr }  // get pointer to ourselves
	
	RememberA0();
	RememberA0ForSettings();
	
	SetUpA4();	

	if (SystemVersion() < 0x0700 || !TrapAvailable(_NewGestalt))
		Notify("\p" APP_NAME " requires System 7.0 or higher.");
	else if (AssertMesg(LoadSettingsFromResource(), "couldn't get settings resource"))
	{
		initHndl = RecoverHandle(initPtr);  // get handle to ourselves
		
		// (we set Locked bit in "Set Project Type..." so don't need HLock here)

		DetachResource(initHndl);			 // keep INIT around
		DetachResource((Handle) gSettings);  // keep settings around

		INSTALL_PATCH(Tool, InsertMenu);		
		INSTALL_PATCH(Tool, DrawMenuBar);
		INSTALL_PATCH(Tool, GetMHandle);
		INSTALL_PATCH(Tool, Pack14);
		INSTALL_PATCH(Tool, SetMenuBar);
		
		// initialize settings and globals
		
		(**gSettings).sysHelpMenuHandle = NULL;
		(**gSettings).needDrawMenuBarPatch = false;
		(**gSettings).weCalledInsertMenu = false;
		
		gOurLabelMenu = NULL;
		
		// disable noLabel setting in System 8+ (there's no Label menu; would hide Special)

		if (SystemVersion() >= 0x0800)
			(**gSettings).noLabel = false;
		
		// install gestalt selectors

		(void) InstallGestaltSelector();
	
		if (   Assert(noErr == NewGestalt(kShowLabelMenuSelector, 
											MoveLabelMenuInOrOutOfFileMenuGestalt))
			&& Assert(noErr == NewGestalt(kHideLabelMenuSelector, 
											MoveLabelMenuInOrOutOfFileMenuGestalt)))
		{
			// install patches
			
			INSTALL_PATCH(Tool, EnableItem);
			INSTALL_PATCH(Tool, DisableItem);
			INSTALL_PATCH(Tool, CheckItem);
			INSTALL_PATCH(Tool, SetItemMark);
			INSTALL_PATCH(Tool, MenuSelect);
			
			// loaded OK, can display our icon
			
			good = true;
		}
		else
			(**gSettings).noLabel = false;	// fail safe:  the InsertMenu patch is live
											// but the submenu machinery didn't install
	}

	if (BOOT_TIME)  // only show icon at boot time, not if the cdev is running us later
	{
		// call ShowInitIcon code resource (it loads 'Code' kShowInitIconCodeID itself):
		if (good)
			Assert(CallShowInitIcon(kShowInitIconCodeID, kGoodIcon));
		else
			Assert(CallShowInitIconXedOut(kShowInitIconCodeID, kGoodIcon, kXOutIcon));
	}

	RestoreA4();
}
