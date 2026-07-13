#pragma once

// The Menu Manager's undocumented MenuList format (the MenuList and SystemMenuList
// globals are actually MenuListHandles):

typedef enum {
	kNowhere = 0,
	kInMainList,
	kInHierList
} MenuLocation;

typedef struct {
	MenuHandle		menuHdl;
	short			leftEdge;
} MenuPair;

typedef struct {
	// offset from start of *this* HierMenuListStruct to the 
	// MenuPair for the last hier menu (if 0, no hier menus):
	unsigned short	lastHierMenu;
	
	long			iDunnoWhatThisIs;
	MenuPair		hierMenuPairs[];
} HierMenuListStruct, *HierMenuListPtr;

typedef struct {
	// lastMenu is the offset from start of this struct to the MenuPair
	// for the last menu in the menulist (if 0, there are no menus)
	unsigned short	lastMenu;
	
	short			lastRight;
	short			notUsed;
	MenuPair		menuPairs[];
	
	// ... followed by the hier portion of the menu list, which is an
	// embedded HierMenuListStruct (see above) and starts at an offset
	// of lastMenu + 6 from beginning of the MenuListStruct

} MenuListStruct, *MenuListPtr, **MenuListHandle;

void DeleteMenuFromSystemMenuList(short id);
MenuHandle GetMHandleAndLocation(const short id, MenuLocation * const whereIsIt);
Boolean ReplaceHelpMenuInMenuList(MenuHandle replace);
