#ifdef NRF52
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <Log/my-log_nrf52.h>

using namespace Adafruit_LittleFS_Namespace;

File file(InternalFS);

bool getNickname(char *name)
{
	if (!InternalFS.begin())
	{
		myLog_e("Error starting file system");
		return false;
	}

	if (!InternalFS.exists("/ready.txt"))
	{
		myLog_e("FS was not formatted");
		InternalFS.format();
		file.open("/ready.txt", FILE_O_WRITE);
		file.println("Formatted");
		file.flush();
		file.close();
	}

	if (!file.open("/config.txt", FILE_O_READ))
	{
		myLog_e("No config file saved");
		InternalFS.end();
		return false;
	}

	memset(name, 0, 17);

	int readLen = file.read(name, 16);

	myLog_d("Got username %s with len %d", name, readLen);
	file.close();
	InternalFS.end();
	return true;
}

bool saveNickname(char *name, size_t len)
{
	char existingName[32];
	if (!InternalFS.begin())
	{
		myLog_e("Error starting file system");
		return false;
	}

	if (file.open("/config.txt", FILE_O_READ))
	{
		file.read(existingName, len);

		if (memcmp(existingName, name, len) == 0)
		{
			myLog_d("Username was already saved");
			file.close();
			InternalFS.end();
			// Saved name is the same
			return true;
		}
		file.close();
	}

	if (!file.open("/config.txt", FILE_O_WRITE))
	{
		myLog_e("Could not open file for writing");
		InternalFS.end();
		return false;
	}
	file.seek(0);
	char newName[17] = {0};
	file.write(newName, 17);
	file.seek(0);
	file.write(name, len);
	file.flush();
	file.close();

	file.open("/config.txt", FILE_O_WRITE);
	memset (existingName, 0, 31);
	int savedLen = file.read(existingName, len);
	myLog_d("Saved name: %s len %d", existingName, savedLen);
	InternalFS.end();
	return true;
}
#endif