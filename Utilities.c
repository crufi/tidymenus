#include "::CrutchUtilities [f]:CrutchError.h"

#include "TidyMenus.h"
#include "Utilities.h"

void DeleteMenuFromSystemMenuList(short id)
// Delete menu with the given ID from the system MenuList, which normally contains
// the "master copy" of the system menus (help menu, application menu, script menu).
//
// Like DeleteMenu() normally, it will just do nothing if the ID can't be found.
{
	void * const savedMenuList = MenuList;
	MenuList = SystemMenuList;
	DeleteMenu(id);
	MenuList = savedMenuList;
}

MenuHandle GetMHandleAndLocation(const short id, MenuLocation * const whereIsIt)
// Get the MenuHandle for the given ID in the current MenuList, and also
// return an enum indicating where in the MenuList the menu was found (in the 
// main portion, hierarchical portion, or nowhere).
{
	const register MenuListPtr mList = *((MenuListHandle) MenuList);
	register unsigned short lastMenu;

	*whereIsIt = kNowhere;
	
	// check main list

	{	
		for (lastMenu = mList->lastMenu; lastMenu; lastMenu -= sizeof(MenuPair))
		{
			const register MenuPair * const mPair = (MenuPair *) ((Ptr) mList + lastMenu);
			
			if ((**mPair->menuHdl).menuID == id)
			{
				*whereIsIt = kInMainList;
				return mPair->menuHdl;
			}
		}
	}
		
	// not in main list, check hier list

	{
		const register HierMenuListPtr hList = 
			(HierMenuListPtr) (((Ptr) mList) + mList->lastMenu + 6);
	
		for (lastMenu = hList->lastHierMenu; lastMenu; lastMenu -= sizeof(MenuPair))
		{
			const register MenuPair * const mPair = (MenuPair *) ((Ptr) hList + lastMenu);
			
			if ((**mPair->menuHdl).menuID == id)
			{
				*whereIsIt = kInHierList;
				return mPair->menuHdl;
			}
		}	
	}
	
	return NULL;
}

Boolean ReplaceHelpMenuInMenuList(MenuHandle replace)
// replace any menu in the MenuList whose ID matches kHelpMenuID
// with a new one in the MenuList if it's there, else do nothing; 
// returns true if found
{
	const register MenuListPtr mList = *((MenuListHandle) MenuList);
	register unsigned short lastMenu;
	
	for (lastMenu = (*mList).lastMenu; lastMenu; lastMenu -= sizeof(MenuPair))
	{
		register MenuPair * const mPair = (MenuPair *) ((Ptr) mList + lastMenu);
		
		if ((**mPair->menuHdl).menuID == kHelpMenuID)
		{
			//Debug("replaced menuhandle in %p menulist %xl with %xl", 
			//			CurApName, mPair->menuHdl, replace);

			mPair->menuHdl = replace;
			return true;
		}
	}
	
	//Debug("ReplaceMenu... failed to find a help menu in %p menulist", CurApName);

	return false;
}
