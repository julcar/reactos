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
// Termsrv.cpp: implementation of the CTermsrv class.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Termsrv.h"
#include "StrUtils.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// Class for sending and receiving Terminal commands as a server
// Ex. Commands for FTP, SMTP, etc.
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Constructor for the CTermsrv class
//
// [in] sd : Socket descriptor that will be used for sending and
//           receiving terminal commands.
//
CTermsrv::CTermsrv(SOCKET sd /*=SOCK_INVALID*/)
{

    m_sd = sd;  //set the socket desc for the ctrl connection

    memset(&m_sslinfo,0,sizeof(sslsock_t)); //init the SSL info obj

    m_flagssl = 0;  //SSL is not used by default
}

CTermsrv::~CTermsrv()
{

}

//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Set the socket desc for the ctrl connection
//
// [in] sd : Socket descriptor that will be used for sending and
//           receiving terminal commands.
//
// Return : VOID
//
void CTermsrv::SetSocketDesc(SOCKET sd)
{

    m_sd = sd;
}

//////////////////////////////////////////////////////////////////////
// Set the information for the SSL connection.
//
// [in] sslinfo : The information for the SSL connection that will be
//                used for sending and receiving terminal commands.
//
// Return : VOID
//
void CTermsrv::SetSSLInfo(sslsock_t *sslinfo)
{

    if (sslinfo != NULL) 
        memcpy(&m_sslinfo,sslinfo,sizeof(sslsock_t));
}

//////////////////////////////////////////////////////////////////////
// Set the connection type (SSL or Clear).
//
// [in] flagssl : If flagssl != 0, use SSL otherwise use clear.
//
// Return : VOID
//
void CTermsrv::SetUseSSL(int flagssl)
{

    m_flagssl = flagssl;
}

//////////////////////////////////////////////////////////////////////
// Get the connection type (SSL or Clear).
//
// Return : 0 is returned if clear text is being used and 1 is
//          returned if SSL is being used.
//
int CTermsrv::GetUseSSL()
{

    return(m_flagssl);
}

//////////////////////////////////////////////////////////////////////
// Send a line of text to the client.
//
// [in] response : Response to send back to the client.
// [in] code     : The 3 digit prefix code for the command.
// [in] i        : Flag used to indicate how to send the response.
//                 i = 0: the line is a terminating line
//                 i = 1: the line is an intermediate line (with
//                        prefix code)
//                 i = 2: the line is an intermediate line (without
//                        prefix code)
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CTermsrv::ResponseSend(const char *response, const char *code, int i /*=0*/)
{
    CStrUtils strutils;
    char *line;
    int n, retval;

    if ((m_flagssl == 0 && m_sd == SOCK_INVALID) || (m_flagssl != 0 && m_sslinfo.ssl == NULL))
        return(0);

    if (i != 2) {
        if (strlen(code) != 3)  //the code must be 3 characters
            return(0);
    }

        //add 7 for the addition of the "code" and \r\n
    if ((line = (char *)malloc(strlen(response)+7)) == NULL) {
        if (m_flagssl == 0)
            m_sock.SendN(m_sd,"530 ERROR: Server out of memory.\r\n",34);
        else
            m_sslsock.SSLSendN(&m_sslinfo,"530 ERROR: Server out of memory.\r\n",34);
        return(0);
    }

    if (i == 1)
        sprintf(line,"%s-%s",code,response);
    else if (i == 2)
        sprintf(line,"    %s",response);
    else
        sprintf(line,"%s %s",code,response);

    n = strutils.AddEOL(line,strlen(response)+7);

    if (m_flagssl == 0)
        retval = m_sock.SendN(m_sd,line,n);
    else
        retval = m_sslsock.SSLSendN(&m_sslinfo,line,n);
    free(line);

    if (retval != n)
        return(0);
    else
        return(1);
}

//////////////////////////////////////////////////////////////////////
// Receives the command from the client
//
// [out] command   : Command that was sent by the client.
// [in] maxcmdsize : Max size of the command.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CTermsrv::CommandRecv(char *command, int maxcmdsize)
{
	char *line;
    CStrUtils strutils;
    int retval;

    if ((m_flagssl == 0 && m_sd == SOCK_INVALID) || (m_flagssl != 0 && m_sslinfo.ssl == NULL))
        return(0);

    if ((line = (char *)malloc(maxcmdsize)) == NULL)
        return(0);

    if (m_flagssl == 0)
        retval = m_sock.RecvLn(m_sd,line,maxcmdsize);
    else
        retval = m_sslsock.SSLRecvLn(&m_sslinfo,line,maxcmdsize);

    if (retval <= 0) {
        free(line);
        return(0);
    }

    strutils.RemoveEOL(line);    //remove '\r' and '\n'

    if (command != NULL) {
        strncpy(command,line,maxcmdsize-1);
        command[maxcmdsize-1] = '\0';
    }

    free(line);
    return(1);
}
