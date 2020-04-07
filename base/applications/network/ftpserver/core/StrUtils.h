// Copyright (C) 2003 by Oren Avissar
// (go to www.dftpd.com for more information)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
//
// StrUtils.h: interface for the CStrUtils class.
//
//////////////////////////////////////////////////////////////////////
#ifndef STRUTILS_H
#define STRUTILS_H


class CStrUtils  
{
public:
    CStrUtils();
    virtual ~CStrUtils();

    int CaseCmp(const char *str1, const char *str2);
    int NCaseCmp(const char *str1, const char *str2, int len);
    void ToLower(char *buffer);
    void ToUpper(char *buffer);
    int AddEOL(char *buffer, int maxbuffersize);
    int RemoveEOL(char *buffer);
    int Append(char *linebuf, const char *str, int linebufsize);
    int IsNum(const char *buffer);
    char *RemoveComments(char *line, const char *commentstr);
    char *GetStrHidden(char *buffer, int maxbuffersize);
    char *BuildFormattedString(const char *format, const char *specifiers, char *values[], int nparams, char escchar = '%');
    int MatchWildcard(const char *wildcard, const char *filename);
    int SNPrintf(char *buffer, unsigned int size, const char *format, ...);
    char *CheckQuotedString(char *buffer);
    char *BuildQuotedString(char *buffer);
};

#endif //STRUTILS_H
