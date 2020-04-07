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
// Termcli.cpp: implementation of the CTermcli class.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Termcli.h"
#include "StrUtils.h"
#include "Dll.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// Class for sending and receiving Terminal commands as a client
// Ex. Commands for FTP, SMTP, etc.
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Constructor for the CTermcli class
//
// [in] sd : Socket descriptor that will be used for sending and
//           receiving terminal commands.
//
CTermcli::CTermcli(SOCKET sd /*=SOCK_INVALID*/)
{

    m_sd = sd;  //set the socket desc for the ctrl connection

    m_recvtimeout = TERMCLI_DEFRECVTIMEOUT; //set the recv timeout

    memset(&m_sslinfo,0,sizeof(sslsock_t)); //init the SSL info obj

    m_flagssl = 0;  //SSL is not used by default
}

CTermcli::~CTermcli()
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
void CTermcli::SetSocketDesc(SOCKET sd)
{

    m_sd = sd;
}

//////////////////////////////////////////////////////////////////////
// Set the receive timeout (max number of seconds to wait for data)
void CTermcli::SetRecvTimeout(int nsec)
{

    m_recvtimeout = nsec;   //0 = block indefinitely
}

//////////////////////////////////////////////////////////////////////
// Set the information for the SSL connection.
//
// [in] sslinfo : The information for the SSL connection that will be
//                used for sending and receiving terminal commands.
//
// Return : VOID
//
void CTermcli::SetSSLInfo(sslsock_t *sslinfo)
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
void CTermcli::SetUseSSL(int flagssl)
{

    m_flagssl = flagssl;
}

//////////////////////////////////////////////////////////////////////
// Get the connection type (SSL or Clear).
//
// Return : 0 is returned if clear text is being used and 1 is
//          returned if SSL is being used.
//
int CTermcli::GetUseSSL()
{

    return(m_flagssl);
}

//////////////////////////////////////////////////////////////////////
// Send a command to the server.
//
// [in] command    : Command to send to the server.
// [in] flagurgent : 1 = send as urgent data, 0 = normal data
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CTermcli::CommandSend(const char *command, int flagurgent /*=0*/)
{
    CStrUtils strutils;
    char *line;
    int n, retval;

    if ((m_flagssl == 0 && m_sd == SOCK_INVALID) || (m_flagssl != 0 && m_sslinfo.ssl == NULL))
        return(0);

        //add 3 for the addition of the \r\n
    if ((line = (char *)malloc(strlen(command)+3)) == NULL)
        return(0);
    strcpy(line,command);

    n = strutils.AddEOL(line,strlen(command)+3);

    if (m_flagssl == 0)
        retval = m_sock.SendN(m_sd,line,n,flagurgent);
    else
        retval = m_sslsock.SSLSendN(&m_sslinfo,line,n);
    free(line);

    if (retval != n)
        return(0);
    else
        return(1);
}

//////////////////////////////////////////////////////////////////////
// Receives the response from the server
//
// [out] ftpreturncode : The return code from the FTP response
//
// Return : An array of strings containing the response lines.
//          The last element in the array is NULL.
//
char **CTermcli::ResponseRecv(char ftpreturncode[4])
{
    CStrUtils strutils;
    CDll dll;
	char **response, *recvline;
    dll_t *responselines, *node;
    int retval, i = 0, nresponselines = 0;

    if ((m_flagssl == 0 && m_sd == SOCK_INVALID) || (m_flagssl != 0 && m_sslinfo.ssl == NULL))
        return(0);

    if ((recvline = (char *)malloc(TERMCLI_RECVBUFFERSIZE)) == NULL)
        return(NULL);

    if ((responselines = dll.Create(NULL,0,"Response Lines List")) == NULL) {
        free(recvline);
        return(NULL);
    }

    memset(ftpreturncode,'\0',4);

        //Recv
    if (m_sock.CheckStatus(m_sd,m_recvtimeout,0) > 0) {
        for (;;) {
            if (m_flagssl == 0)
                retval = m_sock.RecvLn(m_sd,recvline,TERMCLI_RECVBUFFERSIZE);
            else
                retval = m_sslsock.SSLRecvLn(&m_sslinfo,recvline,TERMCLI_RECVBUFFERSIZE);
            if (retval <= 0)
                break;
            nresponselines++;
            strutils.RemoveEOL(recvline);   //remove '\r' and '\n'
            dll.Create(responselines,nresponselines,recvline);
            if (strlen(recvline) >= 4) {
                if ((recvline[0] != ' ') && (recvline[3] == ' ')) {
                    recvline[3] = '\0';
                    strcpy(ftpreturncode,recvline);
                    break;
                }
            }
        }
    }

    free(recvline);

        //return NULL if nothing was received
    if (dll.GetListLength(responselines) == 0) {
        dll.Destroy(responselines);
        return(NULL);
    }

    if ((response = (char **)calloc(nresponselines+1,sizeof(char *))) != NULL) {
        node = dll.NodeNext(responselines);
        while (node != responselines && i < nresponselines) {
            if ((response[i] = (char *)malloc(strlen(node->name)+1)) != NULL) {
                if (strlen(node->name) >= 4)
                    strcpy(response[i++],(node->name)+4);
                else
                    strcpy(response[i++],"");
            }
            node = dll.NodeNext(node);
        }
    }

    dll.Destroy(responselines);

    return(response);
}

//////////////////////////////////////////////////////////////////////
// Frees the response returned by ResponseRecv()
//
// [in] response : The array of strings returned by ResponseRecv()
//
// Return : void
//
void CTermcli::FreeResponse(char **response)
{
    int i;

    if (response == NULL)
        return;

    for (i = 0; response[i] != NULL; i++)
        free(response[i]);
    free(response);
}
