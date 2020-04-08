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
// Service.cpp: implementation of the CService class.
//
// This class contains methods to setup an application to run as a
// service.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "Service.h"


static CService *_serviceptr = NULL;    //the one and only instance


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CService::CService(char *servicename)
{

    if ((m_servicename = (char *)malloc(strlen(servicename)+1)) != NULL)
        strcpy(m_servicename,servicename);

    m_flagshutdown = NULL;

    _serviceptr = this;

        //Windows initialization
        m_checkpoint = 0;
            //SERVICE_STATUS members that rarely change
        m_currentstat.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        m_currentstat.dwServiceSpecificExitCode = 0;
            //set the service status handle
        m_srvstathandle = 0;
            //by default only accept SERVICE_ACCEPT_STOP
        m_controlsaccepted = SERVICE_ACCEPT_STOP;
}

CService::~CService(void)
{

    if (m_servicename != NULL)
        free(m_servicename);

    _serviceptr = NULL;
}

//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Starts the application as a service and calls the function
// specified by runptr().
//
// [in] flagshutdown : Flag used to signal that the service was stopped
// [in] runptr       : Pointer to the starting point of the application
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CService::StartService(int argc, char **argv, int *flagshutdown, int (* runptr)(int, char **))
{

    if (flagshutdown == NULL || runptr == NULL)
        return(0);

    *flagshutdown = 0;      //initialize the shutdown flag

    m_runptr = runptr;      //set the pointer to the starting point of the application

    m_flagshutdown = flagshutdown;

    return(DoStartSrv(argc,argv));
}

//////////////////////////////////////////////////////////////////////
// Called by the service application to indicate it has stopped.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CService::StopService()
{

    return(DoStopSrv());
}

//////////////////////////////////////////////////////////////////////
// Private Methods
//////////////////////////////////////////////////////////////////////

////////////////////////////////////////
// Windows Methods
////////////////////////////////////////

int CService::DoStartSrv(int argc, char **argv)
{
    SERVICE_TABLE_ENTRY dispatchTable[] = {{LPTSTR(m_servicename),(LPSERVICE_MAIN_FUNCTION)ServiceMain},{0,0}};
    int retval = 0;

    if (StartServiceCtrlDispatcher(dispatchTable) != 0)
        retval = 1;

    return(retval);
}

int CService::DoStopSrv()
{

    return(_serviceptr->ReportStatus(SERVICE_STOPPED));
}

BOOL CService::ReportStatus(DWORD dwCurrentState, DWORD dwWaitHint /*=3000*/, DWORD dwErrExit /*=0*/)
{
    BOOL retval = TRUE;

        //set the accept type for the service
    if (dwCurrentState == SERVICE_START_PENDING)
        m_currentstat.dwControlsAccepted = 0;
    else
        m_currentstat.dwControlsAccepted = m_controlsaccepted;

        //set the current state of the service
    m_currentstat.dwCurrentState = dwCurrentState;
    m_currentstat.dwWin32ExitCode = NO_ERROR;
    m_currentstat.dwWaitHint = dwWaitHint;

        //code to support error exiting
    m_currentstat.dwServiceSpecificExitCode = dwErrExit;
    if (dwErrExit != 0)
        m_currentstat.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;

        //set the progress checkpoint
    if (dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED)
        m_currentstat.dwCheckPoint = 0;
    else
        m_currentstat.dwCheckPoint = ++m_checkpoint;

        //Report the status of the service to the service control manager.
    retval = SetServiceStatus(m_srvstathandle,&m_currentstat);

    return(retval);
}

//////// Static Functions ////////

void WINAPI CService::ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{

        //register the service control handler:
    _serviceptr->m_srvstathandle = RegisterServiceCtrlHandler(_serviceptr->m_servicename,CService::ServiceCtrl);

    if (_serviceptr->m_srvstathandle) {
            //report the status to the service control manager.
        if (_serviceptr->ReportStatus(SERVICE_RUNNING))
            _serviceptr->m_runptr(dwArgc,lpszArgv); //call the starting point of the application
    }
}

void WINAPI CService::ServiceCtrl(DWORD dwCtrlCode)
{

        //Handle the requested control code.
    switch (dwCtrlCode) {
        case SERVICE_CONTROL_STOP:
                //Stop the service.
            _serviceptr->m_currentstat.dwCurrentState = SERVICE_STOP_PENDING;
            _serviceptr->ReportStatus(SERVICE_STOP_PENDING);
            *(_serviceptr->m_flagshutdown) = 1; //set the shutdown flag
            break;
        case SERVICE_CONTROL_PAUSE:
            _serviceptr->m_currentstat.dwCurrentState = SERVICE_PAUSE_PENDING;
            break;
        case SERVICE_CONTROL_CONTINUE:
            _serviceptr->m_currentstat.dwCurrentState = SERVICE_CONTINUE_PENDING;
            break;
        case SERVICE_CONTROL_SHUTDOWN:
            _serviceptr->ReportStatus(SERVICE_STOP_PENDING);
            *(_serviceptr->m_flagshutdown) = 1; //set the shutdown flag
            break;
        case SERVICE_CONTROL_INTERROGATE:
                //Update the service status.
            _serviceptr->ReportStatus(_serviceptr->m_currentstat.dwCurrentState);
            break;
        default:
            break;  //invalid control code
    }
}

