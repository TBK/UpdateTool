/*
	This file is a part of "Didrole's Update Tool"
	©2k12, Didrole
	
	License : Public domain
*/

#include "CCommandLine.h"
#include <stdlib.h>
#include <string.h>

CCommandLine::CCommandLine(int argc, char **argv)
{
	m_argc = 0;
	for(int i = 0; i < argc; i++)
		AddParm(argv[i]);
}

CCommandLine::~CCommandLine()
{
	for(int i = 0; i < m_argc; i++)
	{
		delete[] m_argv[i];
	}
}

void CCommandLine::AddParm(const char *psz)
{
	m_argv[m_argc] = new char[strlen(psz) + 1];
	strcpy(m_argv[m_argc], psz);
	m_argc++;
}

const char* CCommandLine::ParmValue(const char *psz, const char *pDefaultVal) const
{
	int nIndex = FindParm(psz);
	if((nIndex == 0) || (nIndex == m_argc - 1))
		return pDefaultVal;

	if(m_argv[nIndex + 1][0] == '-' || m_argv[nIndex + 1][0] == '+')
		return pDefaultVal;

	return m_argv[nIndex + 1];
}

int CCommandLine::ParmValue(const char *psz, int nDefaultVal) const
{
	const char* cszValue = ParmValue(psz);

	if(!cszValue)
		return nDefaultVal;

	return atoi(cszValue);
}

unsigned int CCommandLine::ParmCount() const
{
	return m_argc;
}

unsigned int CCommandLine::FindParm(const char *psz) const
{
	for(int i = 1; i < m_argc; i++)
	{
		if(strcmp(m_argv[i], psz) == 0)
			return i;
	}

	return 0;
}

const char* CCommandLine::GetParm(unsigned int nIndex) const
{
	if(nIndex < (unsigned int)m_argc)
		return m_argv[nIndex];

	return NULL;
}

