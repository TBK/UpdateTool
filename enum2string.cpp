/*
	This file is a part of "Didrole's Update Tool"
	�2k12, Didrole
	
	License : Public domain
*/

#include <Steamworks.h>

const char* AppState2String(EAppState eState)
{
	if(eState & k_EAppUpdateStateReconfiguring)
		return "Reconfiguring";
	if(eState & k_EAppUpdateStatePreallocating)
		return "Preallocating";
	if(eState & k_EAppUpdateStateDownloading)
		return "Downloading";
	if(eState & k_EAppUpdateStateStaging)
		return "Staging";
	if(eState & k_EAppUpdateStateCommitting)
		return "Committing";
	if(eState & k_EAppUpdateStateValidating)
		return "Validating";
	if(eState & k_EAppStateFullyInstalled)
		return "Installed";
	if (eState & k_EAppStateUpdateRequired)
		return "Update required";
	if (eState & k_EAppStateUninstalled)
		return "Not installed";
	
	return "Unknown state";
}

const char* EResult2String(EResult eResult)
{
	static const char* s_szStrings[] = 
	{
		"Invalid EResult",
		"OK",
		"Failure",
		"No Connection",
		"Invalid EResult",
		"Invalid Password",
		"Logged In Elsewhere",
		"Invalid Protocol",
		"Invalid Parameter",
		"File Not Found",
		"Busy",
		"Invalid State",
		"Invalid Name",
		"Invalid Email",
		"Duplicate Name",
		"Access Denied",
		"Timeout",
		"Banned",
		"Account Not Found",
		"Invalid Steam ID",
		"Service Unavailable",
		"Not Logged On",
		"Pending",
		"Encryption Failure",
		"Insufficient Privilege",
		"Limit exceeded",
		"Request revoked",
		"License expired",
		"Already Redeemed",
		"Duplicated Request",
		"Already Owned",
		"IP Address Not Found",
		"Persistence Failed",
		"Locking Failed",
		"Session Replaced",
		"Connection Failed",
		"Handshake Failed",
		"I/O Operation Failed",
		"Disconnected By Remote Host",
		"Shopping Cart Not Found",
		"Blocked",
		"Ignored",
		"No match",
		"Account Disabled",
		"Service Read only",
		"Account Not Featured",
		"Administrator OK",
		"Content version mismatch",
		"Try another CM",
		"Password required to kick session",
		"Already Logged In Elsewhere",
		"Request suspended/paused",
		"Request has been canceled",
		"Corrupted or unrecoverable data error",
		"Not enough free disk space",
		"Remote call failed",
		"Password is not set",
		"External Account is not linked to a Steam account",
		"PSN Ticket is invalid",
		"External Account linked to another Steam account",
		"Remote File Conflict",
		"Illegal password",
		"Same as previous value",
		"Account Logon Denied",
		"Cannot Use Old Password",
		"Invalid Login Auth Code",
		"Account Logon Denied no mail sent",
		"Hardware not capable of IPT",
		"IPT init error",
		"Operation failed due to parental control restrictions for current user",
		"Facebook query returned an error",
		"Expired Login Auth Code",
		"IP Login Restriction Failed",
		"Account Locked Down",
		"Account Logon Denied Verified Email Required",
		"No matching URL",
		"Bad response",
	};

	if(eResult >= k_EResultOK && eResult <= k_EResultBadResponse)
		return s_szStrings[eResult];
	else
		return s_szStrings[0];
}

const char* EAppUpdateError2String(EAppUpdateError eError)
{
	static const char* s_szStrings[] = 
	{
		"No Error",
		"Unspecified Error",
		"Paused",
		"Canceled",
		"Suspended",
		"No subscription",
		"No connection",
		"Connection timeout",
		"Missing decryption key",
		"Missing configuration",
		"Missing content",
		"Disk IO failure",
		"Not enough disk space",
		"Corrupt game files",
		"Waiting for disk",
		"Invalid install path",
		"Application running",
		"Dependency failure",
		"Not installed",
		"Update required",
		"Still busy",
		"No connection to content servers",
		"Invalid application configuration",
		"Invalid content configuration",
		"Missing manifest",
		"Not released",
		"Region restricted",
		"Corrupt depot cache",
		"Missing executable",
		"Invalid platform",
	};

	if(eError >= k_EAppUpdateErrorNoError && eError <= k_EAppUpdateErrorInvalidPlatform)
		return s_szStrings[eError];
	else
		return "Invalid EAppUpdateError";
}
