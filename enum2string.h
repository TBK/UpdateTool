/*
	This file is a part of "Didrole's Update Tool"
	©2k12, Didrole
	
	License : Public domain
*/

#pragma once

enum EAppState;
enum EResult;
enum EAppUpdateError;

const char* AppState2String(EAppState eState);
const char* EResult2String(EResult eResult);
const char* EAppUpdateError2String(EAppUpdateError eError);
