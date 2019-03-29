/*
	This file is a part of "Didrole's Update Tool"
	©2k12, Didrole
	
	License : Public domain
*/

#pragma once
typedef unsigned int AppId_t;

const char* GetAppName(AppId_t uAppId);
void FormatSize(char* szOutput, unsigned int uOutputSize, unsigned long long ullBytes);
bool IsDir(const char* cszPath);
void mSleep(unsigned int uMS);
