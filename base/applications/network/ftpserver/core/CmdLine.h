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
// CmdLine.h: interface for the CCmdLine class.
//
//////////////////////////////////////////////////////////////////////
#ifndef CMDLINE_H
#define CMDLINE_H

class CCmdLine  
{
public:
    CCmdLine(int cmdlinesize = 1024);
    virtual ~CCmdLine();

    void SetCmdLine(const char *cmdline);
    void GetCmdLine(char *outbuff, int maxbuffsize);
    void SetCmdLineSize(int cmdlinesize);
    int GetCmdLineSize();

    char **ParseCmdLine(int *argc, char delimiter = ' ', int usepadding = 0);

    char *CombineArgs(int argc, char **argv, char optionchar = 0, int startarg = 1);
    void FreeCombineArgs(char *argbuffer);

public:
    char *m_cmdline;    //pointer to the command line buffer

private:
    char *m_cmdbuffer;  //stores the contents of the command line
    char **m_argptr;    //points to the individual parameters in m_cmdbuffer

    int m_cmdlinesize;  //size of the command line buffer
};

#endif //CMDLINE_H
