#ifdef ESP32
#include <Preferences.h>
#include <Log/my-log.h>
#include <nvs.h>
#include <nvs_flash.h>

Preferences preferences;

bool getNickname(char *name)
{
	if (!preferences.begin("my-app", false))
	{
		myLog_e("Error opening preferences");
		return false;
	}

	if (preferences.getString("user", name, 16) == 0)
	{
		myLog_e("No name saved > %s", name);
		name[0] = 0;
		if (!preferences.getBool("ok", false));
		{
			preferences.end();
			myLog_e("nvs_flash_init: %d", nvs_flash_init());
			preferences.begin("my-app", false);
			preferences.putBool("ok", true);
		}
		preferences.end();
		return false;
	}
	myLog_d("Got username %s", name);
	preferences.end();
	return true;
}

bool saveNickname(char *name, size_t len)
{
	if (!preferences.begin("my-app", false))
	{
		myLog_e("Error opening preferences");
		return false;
	}

	char existingName[32] = {0};
	if (preferences.getString("user", existingName, 16) != 0)
	{
		if (memcmp(existingName, name, len) == 0)
		{
			myLog_w("Username was already saved");
			preferences.end();
			// Saved name is the same
			return true;
		}
	}

	if (preferences.putString("user", name) != len)
	{
		myLog_e("Could not save username");
		preferences.end();
		return false;
	}

	// Check if the save was good
	int savedLen = preferences.getString("user", existingName, 16);
	myLog_v("Saved name %s with len %d", existingName, savedLen);
	preferences.end();
	return true;
}
#endif