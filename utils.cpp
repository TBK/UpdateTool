/*
	This file is a part of "Didrole's Update Tool"
	©2k12, Didrole
	
	License : Public domain
*/

#include <sys/stat.h>
#include "../../Open Steamworks/Steamworks.h"

extern IClientApps* g_pClientApps;

const char* GetAppName(AppId_t uAppId)
{
	static char szName[25][256];
	static unsigned int i = 0;
	i = (i + 1) % (sizeof(szName) / sizeof(*szName));

	int iSize = g_pClientApps->GetAppData(uAppId, "common/name", szName[i], sizeof(szName[i]));
	if(iSize <= 0)
		strcpy(szName[i], "Unknown App");

	return szName[i];
}

void FormatSize(char* szOutput, unsigned int uOutputSize, unsigned long long ullBytes)
{
	static const char *suffixes[] = {"iB", "KiB", "MiB", "GiB", "TiB", "PiB"};

	int i;
	long double flSize = ullBytes;
	for(i = 0; flSize >= 1024 && i < sizeof(suffixes) / sizeof(*suffixes) - 1 ; flSize /= 1024, i++);

	szOutput[uOutputSize - 1] = '\0';
	snprintf(szOutput, uOutputSize - 1, "%.2Lf %s", flSize, suffixes[i]);
}

bool IsDir(const char* cszPath)
{
#ifdef _WIN32
	DWORD dwAttrib = GetFileAttributesA(cszPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
	struct stat dirStat;
	if(stat(cszPath, &dirStat) != 0 || !S_ISDIR(dirStat.st_mode))
		return false;

	return true;
#endif
}

void mSleep(unsigned int uMS)
{
#ifdef _WIN32
	Sleep(uMS);
#else
	usleep(uMS * 1000);
#endif
}
