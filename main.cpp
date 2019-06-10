/*
	This file is a part of "Didrole's Update Tool"
	�2k12, Didrole
	
	License : Public domain
*/

#include <stdarg.h>
#include <math.h>

#include <Steamworks.h>
#include <Interface_OSW.h>

#include "CCommandLine.h"
#include "enum2string.h"
#include "utils.h"

#include "ModuleScanner.h"

#ifdef _WIN32
	#define isfinite(x) _finite(x)
	#define strcasecmp	_stricmp
#endif

IClientEngine* g_pClientEngine = NULL;
IClientUser* g_pClientUser = NULL;
IClientAppManager* g_pClientAppManager = NULL;
IClientApps* g_pClientApps = NULL;
IClientBilling* g_pClientBilling = NULL;
IClientConfigStore* g_pClientConfigStore = NULL;
IClientUtils* g_pClientUtils = NULL;

HSteamPipe g_hPipe = 0;
HSteamUser g_hUser = 0;

class CApplication
{
public:
	CApplication(int argc, char** argv);

	bool InitSteam();
	bool CheckCommandline();
	bool ParseScript(const char* szFilename);
	bool LogOn();
	bool RunFrame();
	void ShutdownSteam();

	void Exit(int iCode);
	int GetExitCode();

private:
	enum EUpdateResult
	{
		k_EUpdateResultFailed,
		k_EUpdateResultSuccess,
		k_EUpdateResultAlreadyUpToDate,
	};
	EUpdateResult InstallOrUpdateApp(AppId_t uAppId, bool bVerifyAll = false, const char* cszBetaKey = NULL, const char* cszBetaPassword = NULL);
	bool UninstallApp(AppId_t uAppId);
	void ShowAvailableApps();

	void Msg(const char* cszFormat, ...);
	void Error(const char* cszFormat, ...);
	void ProgressMsg(const char* cszFormat, ...);
	bool m_bWasProgressMsg;
	unsigned int m_uLastProgressMsgSize;

	CCommandLine commandLine;

	CSteamAPILoader m_steamLoader;
	AppId_t m_uInstallingAppId;
	AppId_t m_uUninstallingAppId;

	bool m_bExit;
	int m_iExitCode;

	bool m_bWaitingForAppInfoUpdate;
	bool m_bWaitingForLicensesUpdate;
	bool m_bInitialConnection;
	bool m_bWaitingForCredentials;
	STEAM_CALLBACK(CApplication, OnConnected, SteamServersConnected_t, m_onConnected);
	STEAM_CALLBACK(CApplication, OnConnectFailure, SteamServerConnectFailure_t, m_onConnectFailure);
	STEAM_CALLBACK(CApplication, OnDisconnected, SteamServersDisconnected_t, m_onConnectDisconnected);
	STEAM_CALLBACK(CApplication, OnAppInfoUpdateComplete, AppInfoUpdateComplete_t, m_onAppInfoUpdateComplete);
	STEAM_CALLBACK(CApplication, OnAppEventStateChange, AppEventStateChange_t, m_onAppEventStateChange);
	STEAM_CALLBACK(CApplication, OnLogOnCredentialsChanged, LogOnCredentialsChanged_t, m_onLogOnCredentialsChanged);
	STEAM_CALLBACK(CApplication, OnLicensesUpdated, LicensesUpdated_t, m_onLicensesUpdated);
};

void CApplication::Msg(const char* cszFormat, ...)
{
	if(m_bWasProgressMsg)
	{
		printf("\n");
	}

	va_list args;
	va_start(args, cszFormat);
	vprintf(cszFormat, args);
	va_end(args);

	m_bWasProgressMsg = false;
}

void CApplication::Error(const char* cszFormat, ...)
{
	if(m_bWasProgressMsg)
	{
		printf("\n");
	}

	va_list args;
	va_start(args, cszFormat);
	vfprintf(stderr, cszFormat, args);
	va_end(args);

	m_bWasProgressMsg = false;
}

void CApplication::ProgressMsg(const char* cszFormat, ...)
{
	if(m_bWasProgressMsg)
	{
		printf("\r%*s\r", m_uLastProgressMsgSize, "");
	}

	va_list args;
	va_start(args, cszFormat);
	m_uLastProgressMsgSize = vprintf(cszFormat, args);
	va_end(args);

	fflush(stdout);

	m_bWasProgressMsg = true;
}

int CApplication::GetExitCode()
{
	return m_iExitCode;
}

void CApplication::Exit(int iCode)
{
	if(m_bWaitingForCredentials && g_pClientUser->BLoggedOn())
	{
		Msg("Waiting for credentials to cache.\n");
	}

	m_iExitCode = iCode;
	m_bExit = true;
}

bool CApplication::UninstallApp(AppId_t uAppId)
{
	if(g_pClientAppManager->GetAppInstallState(uAppId) & k_EAppStateUninstalled)
	{
		Error("This app (%u:%s) isn't installed\n", uAppId, GetAppName(uAppId));
		return false;
	}

	EAppUpdateError eError = g_pClientAppManager->UninstallApp(uAppId, true);
	if(eError != k_EAppUpdateErrorNoError)
	{
		Error("Uninstallation failed: %s\n", EAppUpdateError2String(eError));
		return false;
	}

	Msg("Uninstalling %u:%s ...\n", uAppId, GetAppName(uAppId));

	m_uUninstallingAppId = uAppId;

	return true;
}

void CApplication::ShowAvailableApps()
{
	uint32 cLicenses = g_pClientBilling->GetNumLicenses();
	for(uint32 i = 0; i < cLicenses; i++)
	{
		PackageId_t uPackageID = g_pClientBilling->GetLicensePackageID(i);

		uint32 cAppIds = 0;
		g_pClientBilling->GetPackageInfo(uPackageID, &cAppIds, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

		AppId_t* pAppIds = new AppId_t[cAppIds];
		g_pClientBilling->GetAppsInPackage(uPackageID, pAppIds, cAppIds);

		char szAppName[256];
		char szContentType[2];
		for(uint32 j = 0; j < cAppIds; j++)
		{
			if(!g_pClientApps->GetAppData(pAppIds[j], "config/contenttype", szContentType, sizeof(szContentType)) || strcmp(szContentType, "3") != 0)
				continue;

			// That's an ugly way to filter dedicated servers, but I haven't found a better way (yet)
			if(!g_pClientApps->GetAppData(pAppIds[j], "common/name", szAppName, sizeof(szAppName)) || strstr(szAppName, "Dedicated Server") == NULL)
				continue;

			Msg("%u: %s, %s\n", pAppIds[j], szAppName, AppState2String(g_pClientAppManager->GetAppInstallState(pAppIds[j])));
		}
		delete[] pAppIds;
	}
}

void CApplication::OnLogOnCredentialsChanged(LogOnCredentialsChanged_t* pParam)
{
	m_bWaitingForCredentials = false;
}

void CApplication::OnLicensesUpdated(LicensesUpdated_t* pParam)
{
	if(m_bWaitingForLicensesUpdate)
	{
		m_bWaitingForLicensesUpdate = false;

		const char* cszCommand = commandLine.ParmValue("-command", "");
		if(strcasecmp(cszCommand, "list") == 0)
		{
			ShowAvailableApps();
			this->Exit(EXIT_SUCCESS);
		}
		else if(strcasecmp(cszCommand, "update") == 0)
		{
			Msg("Requesting appinfo update.\n");

			AppId_t uAppId = commandLine.ParmValue("-game", (int)k_uAppIdInvalid);

			if(!g_pClientApps->RequestAppInfoUpdate(&uAppId, 1))
			{
				Error("Failed to request appinfo update.\n");
				this->Exit(EXIT_FAILURE);
			}
			m_bWaitingForAppInfoUpdate = true;
		}
		else if(strcasecmp(cszCommand, "uninstall") == 0)
		{
			AppId_t uAppId = commandLine.ParmValue("-game", (int)k_uAppIdInvalid);

			if(!UninstallApp(uAppId))
				this->Exit(EXIT_FAILURE);
		}
		else
		{
			Error("Unknown command: %s\n", cszCommand);
			this->Exit(EXIT_FAILURE);
		}
	}
}


void CApplication::OnAppEventStateChange(AppEventStateChange_t* pParam)
{
 	if(pParam->m_nAppID == this->m_uInstallingAppId)
	{
		if(pParam->m_eAppError != k_EAppUpdateErrorNoError)
		{
			Msg("Update failed: %s\n", EAppUpdateError2String(pParam->m_eAppError));
			m_uInstallingAppId = k_uAppIdInvalid;
			g_pClientAppManager->SetDownloadingEnabled(false);
			this->Exit(EXIT_FAILURE);
		}
		else
		{
			if(!(pParam->m_eNewState & k_EAppStateUpdateRequired) && !(pParam->m_eNewState & k_EAppStateUpdateRunning) && !(pParam->m_eNewState & k_EAppUpdateStateValidating) && pParam->m_eNewState & k_EAppStateFullyInstalled)
			{
				Msg("Up to date.\n", m_uInstallingAppId);
				m_uInstallingAppId = k_uAppIdInvalid;
				g_pClientAppManager->SetDownloadingEnabled(false);
				this->Exit(EXIT_SUCCESS);
			}
		}
	}
	else if(pParam->m_nAppID == this->m_uUninstallingAppId)
	{
		if(pParam->m_eAppError != k_EAppUpdateErrorNoError)
		{
			Msg("Uninstallation failed: %s\n", EAppUpdateError2String(pParam->m_eAppError));
			m_uUninstallingAppId = k_uAppIdInvalid;
			this->Exit(EXIT_FAILURE);
		}
		else
		{
			if(pParam->m_eNewState & k_EAppStateUninstalled)
			{
				Msg("App uninstalled successfully.\n");
				m_uUninstallingAppId = k_uAppIdInvalid;
				this->Exit(EXIT_SUCCESS);
			}
		}
	}
	else
	{
		if(pParam->m_eNewState & k_EAppStateUpdateRequired || pParam->m_eNewState & k_EAppStateUpdateRunning || pParam->m_eNewState & k_EAppUpdateStateValidating)
		{
			g_pClientAppManager->ChangeAppPriority(pParam->m_nAppID, k_EAppDownloadPriorityPaused);
		}
	}
}

CApplication::EUpdateResult CApplication::InstallOrUpdateApp(AppId_t uAppId, bool bVerifyAll, const char* cszBetaKey, const char* cszBetaPassword)
{
	if(!cszBetaKey)
		cszBetaKey = "Public";

	char szKeyName[256];
	szKeyName[sizeof(szKeyName) - 1] = '\0';
	char szBuildID[11];
	snprintf(szKeyName, sizeof(szKeyName) - 1, "depots/branches/%s/buildid", cszBetaKey);
		
	int32 iResult = g_pClientApps->GetAppData(uAppId, szKeyName, szBuildID, sizeof(szBuildID));
	if(iResult <= 0)
	{
		Error("There is no beta named '%s' for this app, using public branch instead.\n", cszBetaKey);
	}
	else
	{
		if(cszBetaPassword && !g_pClientAppManager->BHasCachedBetaPassword(uAppId, cszBetaPassword))
		{
			Error("Invalid beta password, using public branch instead.\n");
		}
		else
		{
			int32 iKVSize = 1 + sizeof("internal") + sizeof("\x01""BetaKey") + strlen(cszBetaKey) + 3;
			uint8 *pKV = new uint8[iKVSize];
			pKV[0] = '\0';
			memcpy(pKV + 1, "internal", sizeof("internal"));
			memcpy(pKV + 1 + sizeof("internal"), "\x01""BetaKey", sizeof("\x01""BetaKey"));
			strcpy((char*)pKV + 1 + sizeof("internal") + sizeof("\x01""BetaKey"), cszBetaKey);
			memcpy(pKV + 1 + sizeof("internal") + sizeof("\x01""BetaKey") + strlen(cszBetaKey) + 1, "\x08\x08", 2);

			g_pClientAppManager->SetAppConfig(uAppId, pKV, iKVSize, false);
		}
	}

	EAppState eState = g_pClientAppManager->GetAppInstallState(uAppId);
	if(eState == k_EAppStateInvalid || eState & k_EAppStateUninstalled)
	{
		Msg("Installing %u:%s ...\n", uAppId, GetAppName(uAppId));
		EAppUpdateError eError = g_pClientAppManager->InstallApp(uAppId, NULL, false);
		if(eError != k_EAppUpdateErrorNoError)
		{
			Error("Installation failed: %s\n", EAppUpdateError2String(eError));
			return k_EUpdateResultFailed;
		}
	}
	else if(bVerifyAll)
	{
		Msg("Validating %u:%s ...\n", uAppId, GetAppName(uAppId));
		if(!g_pClientAppManager->StartValidatingApp(uAppId))
		{
			Error("StartValidatingApp returned false\n");
			//return k_EUpdateResultFailed;
		}
	}
	else
	{
		if(g_pClientAppManager->BIsAppUpToDate(uAppId))
		{
			Msg("%u:%s is already up to date.\n", uAppId, GetAppName(uAppId));
			return k_EUpdateResultAlreadyUpToDate;
		}
		else
			Msg("Updating %u:%s ...\n", uAppId, GetAppName(uAppId));
	}

	g_pClientAppManager->SetDownloadingEnabled(true);
	g_pClientAppManager->ChangeAppPriority(uAppId, k_EAppDownloadPriorityFirst);

	m_uInstallingAppId = uAppId;

	return k_EUpdateResultSuccess;
}

CApplication::CApplication(int argc, char** argv) :
	m_onConnected(this, &CApplication::OnConnected),
	m_onConnectFailure(this, &CApplication::OnConnectFailure),
	m_onConnectDisconnected(this, &CApplication::OnDisconnected),
	m_onAppInfoUpdateComplete(this, &CApplication::OnAppInfoUpdateComplete),
	m_onAppEventStateChange(this, &CApplication::OnAppEventStateChange),
	m_onLogOnCredentialsChanged(this, &CApplication::OnLogOnCredentialsChanged),
	m_onLicensesUpdated(this, &CApplication::OnLicensesUpdated),
	commandLine(argc, argv)
{
	m_iExitCode = 0;
	m_bExit = false;
	m_bInitialConnection = true;
	m_bWaitingForAppInfoUpdate = false;
	m_bWaitingForLicensesUpdate = false;
	m_bWaitingForCredentials = false;
	m_bWasProgressMsg = false;
	m_uInstallingAppId = k_uAppIdInvalid;
	m_uUninstallingAppId = k_uAppIdInvalid;
}

void CApplication::OnAppInfoUpdateComplete(AppInfoUpdateComplete_t* pParam)
{
	if(m_bWaitingForAppInfoUpdate && strcasecmp(commandLine.ParmValue("-command", ""), "update") == 0)
	{
		m_bWaitingForAppInfoUpdate = false;

		AppId_t uAppId = commandLine.ParmValue("-game", (int)k_uAppIdInvalid);

		bool bVerifyAll = commandLine.FindParm("-verify_all") != 0;
		EUpdateResult eResult = InstallOrUpdateApp(uAppId, bVerifyAll, commandLine.ParmValue("-beta"), commandLine.ParmValue("-beta_password"));
		if(eResult == k_EUpdateResultFailed)
			this->Exit(EXIT_FAILURE);
		else if(eResult == k_EUpdateResultAlreadyUpToDate)
			this->Exit(EXIT_SUCCESS);
	}
}

void CApplication::OnDisconnected(SteamServersDisconnected_t* pParam)
{
	Msg("Disconnected from Steam: %s\n", EResult2String(pParam->m_eResult));
}

void CApplication::OnConnectFailure(SteamServerConnectFailure_t* pParam)
{
	if(pParam->m_eResult == k_EResultAccountLogonDenied)
	{
		Error("Steam Guard has rejected the connection. An access code has been sent to your email address.\n");
	}
	else
	{
		Error("Logon failed: %s\n", EResult2String(pParam->m_eResult));
	}

	this->Exit(EXIT_FAILURE);
}

void CApplication::OnConnected(SteamServersConnected_t* pParam)
{
	Msg("Logged on\n");

	if(!g_pClientUser->GetSteamID().BAnonAccount())
	{
		// At this point the steamclient has saved the SteamID of the account in Software/Valve/Steam/Accounts/[AccountName]/SteamID
		// But we are logged with the console instance, we don't want to save it with this instance since that could interfere with an existing Steam installation.
		CSteamID userSteamID = g_pClientUser->GetSteamID();
		userSteamID.SetAccountInstance(k_unSteamUserDesktopInstance);

		char szSteamIDKey[256];
		snprintf(szSteamIDKey, sizeof(szSteamIDKey) - 1, "Software/Valve/Steam/Accounts/%s/SteamID", commandLine.ParmValue("-username", ""));
		szSteamIDKey[sizeof(szSteamIDKey) - 1] = '\0';

		g_pClientConfigStore->SetUint64(k_EConfigStoreInstall, szSteamIDKey, userSteamID.ConvertToUint64());
	}

	if(!m_bInitialConnection)
		return;

	m_bInitialConnection = false;

	m_bWaitingForLicensesUpdate = true;
	Msg("Waiting for licenses update...\n");
}

void CApplication::ShutdownSteam()
{
	if(g_pClientUser && g_pClientUser->BLoggedOn())
	{
		Msg("Logging off\n");
		g_pClientUser->LogOff();
		while(g_pClientUser->GetLogonState() != k_ELogonStateNotLoggedOn)
			mSleep(100);
	}

	if(g_pClientEngine)
	{
		if(g_hPipe && g_hUser)
			g_pClientEngine->ReleaseUser(g_hPipe, g_hUser);
		if(g_hPipe)
			g_pClientEngine->BReleaseSteamPipe(g_hPipe);
		g_pClientEngine->BShutdownIfAllPipesClosed();
	}
}

void* g_pUsePICS = NULL;

bool CApplication::InitSteam()
{
	CreateInterfaceFn pCreateInterface = m_steamLoader.GetSteam3Factory();

	if(!pCreateInterface)
	{
		Error("Unable to get Steam3 factory.\n");
		return false;
	}

	g_pClientEngine = (IClientEngine*)pCreateInterface(CLIENTENGINE_INTERFACE_VERSION, NULL);

	if(!g_pClientEngine)
	{
		Error("Unable to get IClientEngine.\n");
		return false;
	}

	g_hUser = g_pClientEngine->CreateLocalUser(&g_hPipe, k_EAccountTypeIndividual);

	if(!g_hPipe || !g_hUser)
	{
		Error("Unable to create a local user.\n");
		return false;
	}

	g_pClientUser = g_pClientEngine->GetIClientUser(g_hUser, g_hPipe);

	if(!g_pClientUser)
	{
		Error("Unable to get IClientUser.\n");
		return false;
	}

	g_pClientAppManager = g_pClientEngine->GetIClientAppManager(g_hUser, g_hPipe);

	if(!g_pClientAppManager)
	{
		Error("Unable to get IClientAppManager.\n");
		return false;
	}

	g_pClientApps = g_pClientEngine->GetIClientApps(g_hUser, g_hPipe);

	if(!g_pClientApps)
	{
		Error("Unable to get IClientApps.\n");
		return false;
	}

	g_pClientBilling = g_pClientEngine->GetIClientBilling(g_hUser, g_hPipe);

	if(!g_pClientBilling)
	{
		Error("Unable to get IClientBilling.\n");
		return false;
	}

	g_pClientConfigStore = g_pClientEngine->GetIClientConfigStore(g_hUser, g_hPipe);

	if(!g_pClientConfigStore)
	{
		Error("Unable to get IClientConfigStore.\n");
		return false;
	}

	g_pClientUtils = g_pClientEngine->GetIClientUtils(g_hPipe);
	
	if(!g_pClientUtils)
	{
		Error("Unable to get IClientUtils.\n");
		return false;
	}

	// Reset the appid to 0 in case someone put a steam_appid.txt in our directory.
	g_pClientUtils->SetAppIDForCurrentPipe(k_uAppIdInvalid, false);

	CModuleScanner steamclientScanner((void*)pCreateInterface);

#ifdef _WIN32
	void* pppUsePICS = steamclientScanner.FindSignature("\x00\x00\x00\x00\x83\x78\x34\x00\x75\x00\xc6\x81\x74\x0c\x00\x00\x00\x5d\xc2\x04\x00", "????xxxxx?xxxxxxxxxxx");
	g_pUsePICS = **(void***)pppUsePICS;
#else
	unsigned char* pSig = (unsigned char*)steamclientScanner.FindSignature("\x55\x89\xE5\xE8\x00\x00\x00\x00\x81\xC1\x00\x00\x00\x00\x8B\x45\x08\x80\x7D\x0C\x00", "xxxx????xx????xxxxxxx");
	int uOffset = *(int*)(pSig + 10);
	int uOffset2 = *(int*)(pSig + 34);

	void* ppUsePICS = pSig + 8 + uOffset + uOffset2;
	g_pUsePICS = *(void**)ppUsePICS;
#endif

	const char* cszDir = commandLine.ParmValue("-dir");
	if(cszDir)
		g_pClientAppManager->ForceInstallDirOverride(cszDir);

	return true;
}

bool CApplication::ParseScript(const char* szFilename)
{
	FILE* hFile = fopen(szFilename, "rb");
	if(!hFile)
	{
		Error("Unable to open '%s'.\n", szFilename);
		return false;
	}

	char szLine[256];
	while(!feof(hFile))
	{
		if(!fgets(szLine, sizeof(szLine), hFile))
			break;

		int argc = 0;
		char* argv[64];

		char* pchCurrent = szLine;
		while(argc < 64)
		{
			while(isspace(*pchCurrent))
				pchCurrent++;

			if(!*pchCurrent)
				break;

			bool bQuoted = false;

			if(*pchCurrent == '"')
			{
				bQuoted = true;
			}

			char* szArg = pchCurrent;

			while(1)
			{
				while(*pchCurrent && !isspace(*pchCurrent))
					pchCurrent++;

				if(bQuoted)
				{
					if(*(pchCurrent - 1) == '"')
					{
						szArg++;
						*(pchCurrent - 1) = '\0';
						break;
					}
					else if(!*pchCurrent)
					{
						break;
					}
					else
					{
						while(isspace(*pchCurrent))
							pchCurrent++;
					}
				}
				else
				{
					if(*pchCurrent)
						*pchCurrent++ = '\0';
					break;
				}
			}

			argv[argc++] = szArg;
		}

		if(argc > 0)
		{
			if(strcasecmp(argv[0], "login") == 0)
			{
				if(argc >= 2 || argc <= 4)
				{
					if(strcasecmp(argv[1], "anonymous") != 0)
					{
						commandLine.AddParm("-username");
						commandLine.AddParm(argv[1]);
						if(argc >= 3)
						{
							commandLine.AddParm("-password");
							commandLine.AddParm(argv[2]);
						}
						if(argc == 4)
						{
							commandLine.AddParm("-steam_guard_code");
							commandLine.AddParm(argv[3]);
						}
					}
				}
			}
			else if(strcasecmp(argv[0], "set_steam_guard_code") == 0)
			{
				if(argc == 2)
				{
					commandLine.AddParm("-steam_guard_code");
					commandLine.AddParm(argv[1]);
				}
			}
			else if(strcasecmp(argv[0], "force_install_dir") == 0)
			{
				if(argc == 2)
				{
					commandLine.AddParm("-dir");
					commandLine.AddParm(argv[1]);
				}
			}
			else if(strcasecmp(argv[0], "app_update") == 0)
			{
				if(argc >= 2)
				{
					commandLine.AddParm("-command");
					commandLine.AddParm("update");
					commandLine.AddParm("-game");
					commandLine.AddParm(argv[1]);

					for(int i = 2; i < argc; i++)
					{
						if(strcasecmp(argv[i], "validate") == 0 || strcasecmp(argv[i], "-validate") == 0)
						{
							commandLine.AddParm("-verify_all");
						}
						else if(strcasecmp(argv[i], "-beta") == 0 && i + 1 < argc)
						{
							commandLine.AddParm("-beta");
							commandLine.AddParm(argv[i + 1]);
						}
						else if(strcasecmp(argv[i], "-betapassword") == 0 && i + 1 < argc)
						{
							commandLine.AddParm("-beta_password");
							commandLine.AddParm(argv[i + 1]);
						}
					}
				}
			}
		}
	}
	fclose(hFile);

	return true;
}

bool CApplication::CheckCommandline()
{
	if(commandLine.ParmCount() <= 1)
	{
		printf
		(
			"Use: %s%s -command <command> [parameters] [flags]\n"
			"Or: %s%s +runscript <steamcmd_script_file>\n"
			"\n"
			"Commands:\n"
			"   update: Install or update a game\n"
			"   uninstall: Remove a game\n"
			"   list: View available games and their status\n"
			"\n"
			"Parameters:\n"
			"   -game <appid>             -   Game AppID to install / update / uninstall\n"
			"      (use '-command list' to see available games)\n"
			"   -dir <installdir>         -   Game install dir\n"
			"      (if dir not specified, will use %s/steamapps/common/<gamename>/)\n"
			"   -username <username>      -   Steam account username\n"
			"   -password <password>      -   Steam account password\n"
			"   -steam_guard_code <code>  -   Steam guard authorization code\n"
			"   -beta <beta_name>         -   Beta name to use\n"
			"   -beta_password <password> -   Beta password (if any)\n"
			"\n"
			"Flags:\n"
			"   -verify_all               -   Verify all game files are up to date\n"
			"   -remember_password        -   Remember password\n"
			"\n"
			"For example: %s%s -command update -game 740 -dir %s -username foo -password bar\n"
			, commandLine.GetParm(0),
#ifdef _WIN32
			""
#else
			".sh"
#endif
			, commandLine.GetParm(0),
#ifdef _WIN32
			""
#else
			".sh"
#endif
			,
#ifdef _WIN32
			".",
#else
			"~/Steam",
#endif
			commandLine.GetParm(0),
#ifdef _WIN32
			""
#else
			".sh"
#endif
			,
#ifdef _WIN32
			"C:\\csgo"
#else
			"/csgo"
#endif
		);
		return false;
	}

	const char* cszScript = commandLine.ParmValue("+runscript");
	if(cszScript)
	{
		if(!ParseScript(cszScript))
			return false;
	}

	const char* cszCommand = commandLine.ParmValue("-command");
	const char* cszUsername = commandLine.ParmValue("-username");
	const char* cszPassword = commandLine.ParmValue("-password");
	const char* cszDir = commandLine.ParmValue("-dir");

	if(!cszCommand)
	{
		Error("No command specified.\n");
		return false;
	}

	if(strcasecmp(cszCommand, "list") != 0 && strcasecmp(cszCommand, "update") != 0 && strcasecmp(cszCommand, "uninstall") != 0)
	{
		Error("Invalid command specified.\n");
		return false;
	}

	if(strcasecmp(cszCommand, "update") == 0 || strcasecmp(cszCommand, "uninstall") == 0)
	{
		if(commandLine.FindParm("-game") == 0)
		{
			Error("No game specified.\n");
			return false;
		}
		else if(commandLine.ParmValue("-game", (int)k_uAppIdInvalid) == k_uAppIdInvalid)
		{
			Error("Invalid game specified %s.\n", commandLine.ParmValue("-game", ""));
			return false;
		}
	}

	if(cszDir && !IsDir(cszDir))
	{
		Error("'%s' doesn't exist or isn't a directory.\n", cszDir);
		return false;
	}

	return true;
}

bool CApplication::LogOn()
{
	g_pClientAppManager->SetDownloadingEnabled(false);
	m_bInitialConnection = true;

	const char* cszUsername = commandLine.ParmValue("-username");
	if(!cszUsername)
	{
#ifdef _WIN32
		void (__thiscall* pSetValue)(void* pThis, const char* cszValue) = (void (__thiscall *)(void *,const char *))  (*(void***)g_pUsePICS)[12];
#else
		void (*pSetValue)(void* pThis, const char* cszValue) = (void (*)(void *,const char *))  (*(void***)g_pUsePICS)[13];
#endif
		pSetValue(g_pUsePICS, "1");

		g_pClientUser->LogOn(CSteamID(-1, k_EUniversePublic, k_EAccountTypeAnonUser)); // HACK: The accountID should be 0 not -1, but 0 triggers asserts in the config store.

		Msg("Logging on Steam anonymously ...\n");
	}
	else
	{
		const char* cszPassword = commandLine.ParmValue("-password");

		m_bWaitingForCredentials = false;

		if(!cszPassword)
		{
			bool bSuccess = g_pClientUser->SetAccountNameForCachedCredentialLogin(cszUsername, false);
			if(!bSuccess)
			{
				Error("No cached credential found for account %s.\n", cszUsername);
				return false;
			}
		}
		else
		{
			bool bRememberPassword = commandLine.FindParm("-remember_password") != 0;

			m_bWaitingForCredentials = bRememberPassword;
			g_pClientUser->SetLoginInformation(cszUsername, cszPassword, bRememberPassword);

			const char* cszSteamGuardCode = commandLine.ParmValue("-steam_guard_code");
			if(cszSteamGuardCode)
			{
				g_pClientUser->Set2ndFactorAuthCode(cszSteamGuardCode, false);
			}
		}
	
		char szSteamIDKey[256];
		snprintf(szSteamIDKey, sizeof(szSteamIDKey) - 1, "Software/Valve/Steam/Accounts/%s/SteamID", cszUsername);
		szSteamIDKey[sizeof(szSteamIDKey) - 1] = '\0';

		uint64 ullSteamID = g_pClientConfigStore->GetUint64(k_EConfigStoreInstall, szSteamIDKey, 0);

		g_pClientUser->LogOn(CSteamID(ullSteamID & 0xFFFFFFFF, k_unSteamUserConsoleInstance, k_EUniversePublic, k_EAccountTypeIndividual));

		Msg("Logging on Steam with account %s ...\n", cszUsername);
	}

	return true;
}

bool CApplication::RunFrame()
{
	if(!m_bWaitingForCredentials || !g_pClientUser->BLoggedOn())
	{
		if(m_bExit)
		{
			return false;
		}
	}

	SteamAPI_RunCallbacks();

	if(m_uInstallingAppId != k_uAppIdInvalid)
	{
		EAppState eState = g_pClientAppManager->GetAppInstallState(m_uInstallingAppId);
		if(eState & k_EAppStateUpdateRequired || eState & k_EAppUpdateStateValidating || eState & k_EAppStateUpdateRunning)
		{
			AppUpdateInfo_s updateInfo;
			if(g_pClientAppManager->GetUpdateInfo(m_uInstallingAppId, &updateInfo))
			{
				uint64 ullCurrent;
				uint64 ullMax;

				if(updateInfo.m_unBytesDownloaded == updateInfo.m_unBytesToDownload)
				{
					ullCurrent = updateInfo.m_unBytesProcessed;
					ullMax = updateInfo.m_unBytesToProcess;
				}
				else
				{
					ullCurrent = updateInfo.m_unBytesDownloaded;
					ullMax = updateInfo.m_unBytesToDownload;
				}

				float flProgress = float(ullCurrent * (double)100.00f / ullMax);
				if(!isfinite(flProgress))
					flProgress = 0.0;

				char szCurrent[128];
				char szMax[128];
				FormatSize(szCurrent, sizeof(szCurrent), ullCurrent);
				FormatSize(szMax, sizeof(szMax), ullMax);

				ProgressMsg("%s, progress: %.2f%% (%s / %s)", AppState2String(eState), flProgress, szCurrent, szMax);
			}
		}
	}

	return true;
}

int main(int argc, char** argv)
{
	CApplication app(argc, argv);

	if(!app.CheckCommandline() || !app.InitSteam() || !app.LogOn())
	{
		app.ShutdownSteam();
		return EXIT_FAILURE;
	}

	while(app.RunFrame())
	{
		mSleep(100);
	}

	app.ShutdownSteam();

	return app.GetExitCode();
}
