#pragma once

// Menu IDs in the System 7 Finder (tested in 7.1 and 7.5.5)
#define kFileMenuID			   257
#define kLabelMenuID           260

// System menu IDs, defined in ROM file MenuMgrPriv.h	
#define kHelpMenuID 		-16490  // help menu
#define kApplicationMenuID  -16489  // application menu (needed to re-insert Help menu)

// Undocumented lo-mem global
Handle SystemMenuList : 0x286;

// Show/hide label menu Gestalt selectors (for cdev to call INIT code)
#define kHideLabelMenuSelector	'-Låb'
#define kShowLabelMenuSelector	'+Låb'