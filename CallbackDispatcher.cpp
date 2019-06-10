/*
	This file is a part of "Didrole's Update Tool"
	�2k12, Didrole
	
	License : Public domain
*/

#include <map>
#include <Steamworks.h>

extern HSteamPipe g_hPipe;


std::multimap<int, CCallbackBase *> g_callbacksMap;

class Callback : public CCallbackBase
{
public:
	void AddRegisteredFlag()
	{
		m_nCallbackFlags |= k_ECallbackFlagsRegistered;
	}
};

void STEAM_CALL SteamAPI_RegisterCallback( class CCallbackBase *pCallback, int iCallback )
{
	((Callback*)pCallback)->AddRegisteredFlag();
	g_callbacksMap.insert(std::pair<int, CCallbackBase *>(iCallback, pCallback));
}

void STEAM_CALL SteamAPI_UnregisterCallback( class CCallbackBase *pCallback )
{
	for(std::multimap<int, CCallbackBase *>::iterator i = g_callbacksMap.begin(); i != g_callbacksMap.end();)
	{
		if(i->second == pCallback)
		{
			g_callbacksMap.erase(i++);
		}
		else
			i++;
	}
}

void STEAM_CALL SteamAPI_RunCallbacks()
{
	CallbackMsg_t callbackMsg;
	while(Steam_BGetCallback(g_hPipe, &callbackMsg))
	{
		// We are making a copy of the data to be able to call Steam_FreeLastCallback before sending the data to the callback handlers
		// That will allow recursive calls to this function to work properly (ie: no infinite processing of the same callback)

		unsigned char* pubData = new unsigned char[callbackMsg.m_cubParam];
		memcpy(pubData, callbackMsg.m_pubParam, callbackMsg.m_cubParam);

		Steam_FreeLastCallback(g_hPipe);

		std::pair<std::multimap<int,CCallbackBase *>::iterator, std::multimap<int, CCallbackBase *>::iterator> range = g_callbacksMap.equal_range(callbackMsg.m_iCallback);

		for(std::multimap<int, CCallbackBase *>::const_iterator i = range.first; i != range.second; ++i)
		{
			i->second->Run(pubData);
		}

		delete[] pubData;
	}
}
