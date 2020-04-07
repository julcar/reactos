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
// Service.h: interface for the CService class.
//
// This class contains methods to setup an application to run as a
// service.
//
//////////////////////////////////////////////////////////////////////
#ifndef SERVICE_H
#define SERVICE_H

#ifdef WIN32    //Windows includes
  #include <windows.h>
  #include <winsvc.h>
#else           //UNIX includes
#endif


class CService
{
public:
    CService(char *servicename);
    virtual ~CService();

    int StartService(int argc, char **argv, int *flagshutdown, int (* runptr)(int, char **));
    int StopService();

private:
    int DoStartSrv(int argc, char **argv);
    int DoStopSrv();

private:
    char *m_servicename;
    int *m_flagshutdown;
    int (* m_runptr)(int, char **);

#ifdef WIN32    //Windows specific functions/variables
private:
    BOOL ReportStatus(DWORD dwCurrentState, DWORD dwWaitHint = 3000, DWORD dwErrExit = 0);
        //static functions
    static void WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv);
    static void WINAPI ServiceCtrl(DWORD dwCtrlCode);
private:
    DWORD m_checkpoint;                     //service increments this periodically to report its progress
    SERVICE_STATUS m_currentstat;           //current status of the service
    SERVICE_STATUS_HANDLE m_srvstathandle;  //service status handle
    DWORD m_controlsaccepted;               //bit-field of what ctrl req the srv will accept
#else           //UNIX specific functions/variables

#endif

};

#endif //SERVICE_H
