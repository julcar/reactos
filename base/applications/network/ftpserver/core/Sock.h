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
// Sock.h: interface for the CSock class.
//
//////////////////////////////////////////////////////////////////////
#ifndef SOCK_H
#define SOCK_H

#ifdef WIN32
  #include <winsock2.h>
  typedef int socklen_t;    //Make compatible with UNIX
#else
  typedef int SOCKET;       //Make compatible with MSVC.
#endif

#ifdef INVALID_SOCKET
  #define SOCK_INVALID INVALID_SOCKET
#else
  #define SOCK_INVALID -1
#endif

#ifdef SOCKET_ERROR
  #define SOCK_ERROR SOCKET_ERROR
#else
  #define SOCK_ERROR -1
#endif

#ifndef INADDR_NONE
  #define INADDR_NONE 0xFFFFFFFF
#endif

#define SOCK_IPADDRLEN 32       //max string length of an IP address
#define SOCK_PORTLEN   16       //max string length of a port number


class CSock
{
public:
    CSock();
    virtual ~CSock();

        //Socket Initialization
    int Initialize();
    int Uninitialize();

        //Functions for opening and closing connections
    SOCKET OpenClient(const char *hostname, const char *portnum, int nsec);
    SOCKET OpenServer(const char *portnum, char *bindip = NULL);
    SOCKET BindServer(char *bindport, int maxportlen, char *bindip = NULL, short port = 0);
    SOCKET CreateSocket();
    int BindToAddr(SOCKET sd, char *bindip, short bindport);
    int ListenServer(SOCKET sd, int backlog = 0);
    SOCKET Accept(SOCKET sd);
    void Shutdown(SOCKET sd, int flag);
    void Close(SOCKET sd);

        //Functions used to get address information
    void GetAddrPort(SOCKET sd, char *ipaddr, int maxaddrlen, char *port, int maxportlen);
    void GetPeerAddrPort(SOCKET sd, char *ipaddr, int maxaddrlen, char *port, int maxportlen);
    void GetAddrName(const char *ipaddr, char *name, int maxnamelen);
    int Addr2IP(const char *inputaddr, char *outputip, int maxiplen);
    int ValidateIP(const char *ipaddr);
    int GetMyIPAddress(char *outputip, int maxiplen);

        //Functions for transfering data
    int RecvN(SOCKET sd, char *ptr, int nbytes);
    int RecvLn(SOCKET sd, char *ptr, int nmax);
    int SendN(SOCKET sd, const char *ptr, int nbytes, int flagurgent = 0);
    int SendLn(SOCKET sd, const char *ptr, int flagurgent = 0);

        //Functions for setting socket options
    int SetKeepAlive(SOCKET sd);
    int SetTimeout(SOCKET sd, int timeoutms);
    int CheckStatus(SOCKET sd, int nsec, int nmsec = 0);
    int SetRecvOOB(SOCKET sd);
    int SetAddrReuse(SOCKET sd);

        //Functions used for network byte order conversions
    unsigned long HtoNl(unsigned long lval);
    unsigned short HtoNs(unsigned short sval);
    unsigned long NtoHl(unsigned long lval);
    unsigned short NtoHs(unsigned short sval);

private:
    int Connect(SOCKET sd, struct sockaddr *saddr, int nsec);
    int GetLastError();
};

#endif //SOCK_H
