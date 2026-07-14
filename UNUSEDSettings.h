#pragma once

#define SETTINGS_GESTALT_SELECTOR  	'NLbl'
#define SETTINGS_MAGIC_NUMBER      	'NLbl'
#define SETTINGS_RES_TYPE 			'NLbD'
#define SETTINGS_RES_ID   			-4048

typedef struct {
// (should be even # bytes so GetHandleSize() check matches)
	OSType     magicNumber;
	short      noLabel;
	short  	   noHelp;
	short	   needDrawMenuBarPatch;
	MenuHandle sysHelpMenuHandle;
	Boolean	   weCalledInsertMenu;
} Settings;

