/*
	This file is a part of "Didrole's Update Tool"
	©2k12, Didrole
	
	License : Public domain
*/

#pragma once

class CCommandLine
{
public:
	CCommandLine(int argc, char **argv);
	~CCommandLine();

	const char* ParmValue(const char *psz, const char *pDefaultVal = 0) const;
	int ParmValue(const char *psz, int nDefaultVal) const;

	unsigned int ParmCount() const;
	unsigned int FindParm(const char *psz) const;
	const char* GetParm(unsigned int nIndex) const;

	void AddParm(const char *psz);

private:

	static const unsigned int k_nMaxArgs = 64;

	int m_argc;
	char *m_argv[k_nMaxArgs];
};
