/* auto-generated (do not modify): type=TEXT creator=KAHL hex=544558544B41484C000000000000000000000000000000000000000000000000 */
#ifndef CRUTCHSETTINGS_H
#define CRUTCHSETTINGS_H

#include "Settings.h"

#ifdef __cplusplus
extern "C" {
#endif 

extern Settings **gSettings;

typedef enum {
	kGotSettingsFromGestalt,
	kGotSettingsFromResource,
	kErrorGettingSettings
} CdevSettingsResult;

void RememberA0ForSettings(void);

Boolean InstallGestaltSelector(void);
Boolean IsOurGestaltInstalled(void);

Boolean LoadSettingsFromResource(void);
Boolean LoadSettingsFromGestalt(void);

CdevSettingsResult GetCdevSettings(void);

void SaveSettings(void);

#ifdef __cplusplus
}
#endif 

#endif