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
// Termsrv.h: interface for the CTermsrv class.
//
//////////////////////////////////////////////////////////////////////
#ifndef TERMSRV_H
#define TERMSRV_H

#include "Sock.h"
#include "SSLSock.h"


///////////////////////////////////////////////////////////////////////////////
// Supplemental class used to send and receive Terminal commands as a server
///////////////////////////////////////////////////////////////////////////////

class CTermsrv
{
public:
    CTermsrv(SOCKET sd = SOCK_INVALID);
    virtual ~CTermsrv();

        //Used to set the socket descriptor
    void SetSocketDesc(SOCKET sd);

        //Used to set the SSL connection information
    void SetSSLInfo(sslsock_t *sslinfo);

        //Used to Set/Get the type of connection being used (SSL or Clear).
        //By default a clear text connection is used.
    void SetUseSSL(int flagssl);
    int GetUseSSL();

        //Functions used for sending and receiving Terminal commands 
    int ResponseSend(const char *response, const char *code, int i = 0);
    int CommandRecv(char *command, int maxcmdsize);

private:
    CSock m_sock;           //create the sockets class
    CSSLSock m_sslsock;     //create the SSL connection class
    SOCKET m_sd;            //socket desc for the control connection
    sslsock_t m_sslinfo;    //SSL contection information
    int m_flagssl;          //0 = use clear text, !0 = use SSL
};

#endif //TERMSRV_H
