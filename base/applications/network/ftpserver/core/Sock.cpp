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
// Sock.cpp: implementation of the CSock class.
//
// This class is used for TCP/IP client and server connections.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef WIN32
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/time.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <fcntl.h>
  #include <errno.h>
#endif

#include "Sock.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSock::CSock()
{

}

CSock::~CSock()
{

}

//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

////////////////////////////////////////
// Functions for initializing/
// uninitializing sockets.
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Initialize sockets
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
// NOTE: This method should only be called once in the application.
//       Currently this function only applies to WINDOWS.
//
int CSock::Initialize()
{
    #ifdef WIN32
      WSADATA wsaData;
      WORD wVerReq;

          //Run WSAStartup to initiate use of WS2_32.DLL by a process
      wVerReq = MAKEWORD(2,2);
      if (WSAStartup(wVerReq,&wsaData) != 0) {
          WSACleanup();
          return(0);    //couldn't initialize sockets
      }
    #endif

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Uninitialize sockets
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
// NOTE: This method should only be called once in the application.
//       Currently this function only applies to WINDOWS.
//
int CSock::Uninitialize()
{

    #ifdef WIN32
          //Run WSACleanup before exiting
      if (WSACleanup() == SOCKET_ERROR) {
          return(0);
      }
    #endif

    return(1);
}

////////////////////////////////////////
// Functions for opening and closing
// connections.
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// OpenClient() returns a handle to the connection to the host at
// "hostname" on port "portnum" (this is for the client).
//
// [in] hostname : The name of the host to connect to.
// [in] portnum  : The port number to connect on.
// [in] nsec     : Maximum number of seconds to wait.
//
// Return : On success the client socket desc is returned.
//          On failure SOCK_INVALID is returned.
//
SOCKET CSock::OpenClient(const char *hostname, const char *portnum, int nsec)
{
    SOCKET sd;
    struct hostent *hostp;
	unsigned short port;
    unsigned long addr;
    struct sockaddr saddr;
    unsigned long hostb;  //binary representation of the Internet address

    if (hostname == NULL || portnum == NULL)
        return(SOCK_INVALID);

        //Set the port to connect to --Port MUST be in Network Byte Order
    if (sscanf(portnum,"%hu",&port) != 1)
        return(SOCK_INVALID);

        //Create the socket
    if ((sd = socket(AF_INET,SOCK_STREAM,0)) == SOCK_INVALID)
        return(SOCK_INVALID);

          //Set the IP address to connect to
    if ((hostb = inet_addr(hostname)) == INADDR_NONE) {
        if ((hostp = gethostbyname(hostname)) == 0) { //use this line if you want to enter host names
            Close(sd);
            return(SOCK_INVALID);
        }
        addr = *((unsigned long *)hostp->h_addr);
    } else {
        addr = hostb;  //use this if you enter an IP addr
    }

    saddr.sa_family = AF_INET;
    (*((struct sockaddr_in*)&saddr)).sin_addr.s_addr = addr;
    (*((struct sockaddr_in*)&saddr)).sin_port = htons(port);

    if (Connect(sd,&saddr,nsec) <= 0)  {
        Close(sd);
        return(SOCK_INVALID);
    }

    return(sd);
}

//////////////////////////////////////////////////////////////////////
// OpenServer creates and binds a server to the port specified
// and the listens for a connection.  If a "bindip" is also specified
// the server will attempt to bind to this IP address.  Otherwise,
// the server will bind to all IPs.
//
// [in] portnum : The port number to listen on.
// [in] bindip  : IP address to bind the server to (must be x.x.x.x)
//
// Return : On success the listening socket desc is returned.
//          On failure SOCK_INVALID is returned.
//
SOCKET CSock::OpenServer(const char *portnum, char *bindip /*=NULL*/)
{
    SOCKET sd;
	unsigned short port;
    struct sockaddr servaddr;
    unsigned long hostb;  //binary representation of the Internet address

    if (portnum == NULL)
        return(SOCK_INVALID);

        //Set the port to connect to
    if (sscanf(portnum,"%hu",&port) != 1)
        return(SOCK_INVALID);

    if ((sd = socket(AF_INET,SOCK_STREAM,0)) == SOCK_INVALID)
        return(SOCK_INVALID);
   
    servaddr.sa_family = AF_INET;
    (*(struct sockaddr_in*)(&servaddr)).sin_port = htons(port);  //Port MUST be in Network Byte Order
    if (bindip == NULL) {
        hostb = htonl(INADDR_ANY);  //Set to bind to all IPs
    } else {
            //Set the IP address to bind to
        if ((hostb = inet_addr(bindip)) == INADDR_NONE) {
            Close(sd);
            return(SOCK_INVALID);
        }
    }
    (*(struct sockaddr_in*)(&servaddr)).sin_addr.s_addr = hostb;

    if (bind(sd,&servaddr,sizeof(struct sockaddr)) < 0)  {
        Close(sd);
        return(SOCK_INVALID);
    }

    if (listen(sd,5) < 0)  {
        Close(sd);
        return(SOCK_INVALID);
    }

    return(sd);
}

//////////////////////////////////////////////////////////////////////
// Binds a server to "bindip":"port" and returns the port number that
// was binded to.  If "port" is 0, the port number will automatically
// be chosen by the OS.
//
// [out] bindport  : The port number that was binded to.
// [in] maxportlen : Max size of the bindport string.
// [in] bindip     : IP address to bind the server to (x.x.x.x)
// [in] port       : Port that should be binded to
//                   (if 0 the OS decides).
//
// Return : On success the socket desc that was binded to is returned.
//          On failure SOCK_INVALID is returned.
//
SOCKET CSock::BindServer(char *bindport, int maxportlen, char *bindip /*=NULL*/, short port /*=0*/)
{
    SOCKET sd;
    char bindaddr[SOCK_IPADDRLEN];

    if ((sd = socket(AF_INET,SOCK_STREAM,0)) == SOCK_INVALID)
        return(SOCK_INVALID);

        //allows the port to be binded to again without a delay
    SetAddrReuse(sd);

        //bind to the specified IP and port
    if (BindToAddr(sd,bindip,port) == 0) {
        Close(sd);
        return(SOCK_INVALID);
    }

        //Get the addr and port that was binded to
    if (bindport != NULL)
        GetAddrPort(sd,bindaddr,sizeof(bindaddr),bindport,maxportlen);

    return(sd);
}

//////////////////////////////////////////////////////////////////////
// Creates a new socket.
//
// Return : On success the socket desc is returned.
//          On failure SOCK_INVALID is returned.
//
SOCKET CSock::CreateSocket()
{
    SOCKET sd;

    if ((sd = socket(AF_INET,SOCK_STREAM,0)) == SOCK_INVALID)
        return(SOCK_INVALID);

    return(sd);
}

//////////////////////////////////////////////////////////////////////
// Binds a socket desc to a specified IP and port.
//
// [in] bindip     : IP address to bind to (must be in x.x.x.x form)
// [in] port       : Port that should be binded to
//                   (if 0 the OS decides).
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CSock::BindToAddr(SOCKET sd, char *bindip, short bindport /*=0*/)
{
    struct sockaddr servaddr;
    unsigned long hostb;  //binary representation of the Internet address

    if (sd == SOCK_INVALID)
        return(0);

    servaddr.sa_family = AF_INET;
    (*(struct sockaddr_in*)(&servaddr)).sin_port = htons(bindport); //if 0, OS will auto allocate
    if (bindip == NULL) {
        hostb = htonl(INADDR_ANY);  //Set to bind to all IPs
    } else {
            //Set the IP address to bind to
        if ((hostb = inet_addr(bindip)) == INADDR_NONE)
            return(0);
    }
    (*(struct sockaddr_in*)(&servaddr)).sin_addr.s_addr = hostb;

    if (bind(sd,&servaddr,sizeof(struct sockaddr)) < 0)
        return(0);

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Listens for a connection on the socket specified by "sd".
//
// [in] sd      : The socket desc to listen on -- this is usually
//                called after BindServer().
// [in] backlog : The number of backlogged connections to allow.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CSock::ListenServer(SOCKET sd, int backlog /*=0*/)
{

    if (sd == SOCK_INVALID)
        return(0);

    if (listen(sd,backlog) < 0)
        return(0);

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Accept() takes the server handle (output of OpenServer()) and
// blocks until a client makes a connection.  At that point, it
// returns a handle to the connection to the client.
//
// [in] sd : The socket descriptor to accept the client connection on.
//
// Return : On success the socket desc for the client connection is
//          returned.  On failure SOCK_INVALID is returned.
//
SOCKET CSock::Accept(SOCKET sd)
{
    SOCKET clientsd;
    socklen_t addrlen;
    struct sockaddr cliaddr;

    if (sd == SOCK_INVALID)
        return(SOCK_INVALID);

    addrlen = sizeof(struct sockaddr);
    if ((clientsd = accept(sd,&cliaddr,&addrlen)) < 0) {
        return(SOCK_INVALID);
    }

    return(clientsd);
}

//////////////////////////////////////////////////////////////////////
// Used to shutdown the socket (usually not necessary).
//
// [in] sd   : The socket that should be shutdown.
// [in] flag : How the socket should be shutdown.
//             0, further receives will be disallowed.
//             1, further sends will be disallowed.
//             2, further sends and receives will be disallowed.
//
// Return : VOID
//
void CSock::Shutdown(SOCKET sd, int flag)
{
    
    shutdown(sd,flag);
}

//////////////////////////////////////////////////////////////////////
// Used to close the socket.
//
// [in] sd : The socket desc that should be closed.
//
// Return : VOID
//
void CSock::Close(SOCKET sd)
{

    #ifdef WIN32
      closesocket(sd);
    #else
      close(sd);
    #endif
}

////////////////////////////////////////
// Functions used to get address
// information.
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Returns the local address and port.
//
// [in] sd         : The socket desc to get the addr and port for.
// [out] ipaddr    : IP addr of the socket.
// [in] maxaddrlen : Max size of the "ipaddr" string.
// [out] port      : Port number of the socket.
// [in] maxportlen : Max size of the "port" string.
//
// Return : VOID
//
void CSock::GetAddrPort(SOCKET sd, char *ipaddr, int maxaddrlen, char *port, int maxportlen)
{
	unsigned short a1,a2,a3,a4;
    char *ptr;
    socklen_t addrlen;
    struct sockaddr saddr;
    unsigned short portus;

    if (sd == SOCK_INVALID || ipaddr == NULL || maxaddrlen == 0 || port == NULL || maxportlen == 0)
        return;

    if (maxaddrlen < 16 || maxportlen < 6) {
        *ipaddr = '\0'; *port = '\0';
        return;
    }

        //get the local IP address and port
    addrlen = sizeof(struct sockaddr);
    if (getsockname(sd,&saddr,&addrlen) == 0) {
        ptr = (char *)&(*(struct sockaddr_in*)(&saddr)).sin_addr.s_addr;
        a1 = (unsigned short)*ptr & 0x00FF;
        a2 = (unsigned short)*(ptr+1) & 0x00FF;
        a3 = (unsigned short)*(ptr+2) & 0x00FF;
        a4 = (unsigned short)*(ptr+3) & 0x00FF;
        sprintf(ipaddr,"%hu.%hu.%hu.%hu",a1,a2,a3,a4);
        portus = ntohs((*(struct sockaddr_in*)(&saddr)).sin_port);
        sprintf(port,"%hu",portus);
    } else {
        *ipaddr = '\0';
        *port = '\0';
    }
}

//////////////////////////////////////////////////////////////////////
// Returns the peer's address and port.
//
// [in] sd         : The socket desc to get the addr and port for.
// [out] ipaddr    : IP addr of the socket.
// [in] maxaddrlen : Max size of the "ipaddr" string.
// [out] port      : Port number of the socket.
// [in] maxportlen : Max size of the "port" string.
//
// Return : VOID
//
void CSock::GetPeerAddrPort(SOCKET sd, char *ipaddr, int maxaddrlen, char *port, int maxportlen)
{
	unsigned short a1,a2,a3,a4;
    char *ptr;
    socklen_t addrlen;
    struct sockaddr saddr;
    unsigned short portus;

    if (sd == SOCK_INVALID || ipaddr == NULL || maxaddrlen == 0 || port == NULL || maxportlen == 0)
        return;

    if (maxaddrlen < 16 || maxportlen < 6) {
        *ipaddr = '\0'; *port = '\0';
        return;
    }

        //get the peer's IP address and port
    addrlen = sizeof(struct sockaddr);
    if (getpeername(sd,&saddr,&addrlen) == 0) {
        ptr = (char *)&(*(struct sockaddr_in*)(&saddr)).sin_addr.s_addr;
        a1 = (unsigned short)*ptr & 0x00FF;
        a2 = (unsigned short)*(ptr+1) & 0x00FF;
        a3 = (unsigned short)*(ptr+2) & 0x00FF;
        a4 = (unsigned short)*(ptr+3) & 0x00FF;
        sprintf(ipaddr,"%hu.%hu.%hu.%hu",a1,a2,a3,a4);
        portus = ntohs((*(struct sockaddr_in*)(&saddr)).sin_port);
        sprintf(port,"%hu",portus);
    } else {
        *ipaddr = '\0';
        *port = '\0';
    }
}

//////////////////////////////////////////////////////////////////////
// Retrieves the host name corresponding to the IP address "ipaddr".
// The first "maxnamelen" characters of the host name are stored
// in "name".  This function basically does a reverse DNS lookup.
//
// [in] ipaddr     : IP addr to get the name for.
// [out] name      : Name of the machine with the addr in "ipaddr".
// [in] maxnamelen : Max size of the "name" string.
//
// Return : VOID
//
void CSock::GetAddrName(const char *ipaddr, char *name, int maxnamelen)
{
    unsigned long addr;
    struct hostent *hp;

    if (ipaddr == NULL || name == NULL || maxnamelen == 0)
        return;

    addr = inet_addr(ipaddr);
    if ((hp = gethostbyaddr((char *)&addr,4,AF_INET)) != NULL) {
        strncpy(name,hp->h_name,maxnamelen-1);
        name[maxnamelen-1] = '\0';    //make sure the string is NULL terminated
    } else {
        *name = '\0';
    }
}

//////////////////////////////////////////////////////////////////////
// Returns the IP (dotted notation) for "inputaddr".  This function
// basically does a DNS lookup.
//
// [in] inputaddr : Addr to convert to an IP.
// [out] outputip : IP string in dotted notation (xxx.xxx.xxx.xxx).
// [in] maxiplen  : Max size of the "outputip" string.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CSock::Addr2IP(const char *inputaddr, char *outputip, int maxiplen)
{
    struct hostent *hp;
    struct in_addr in;
    char *ptr;

    if (inputaddr == NULL || outputip == NULL || maxiplen == 0)
        return(0);

    if (inet_addr(inputaddr) == INADDR_NONE) {
            //inputaddr is not an IP
        *outputip = '\0';
        if ((hp = gethostbyname(inputaddr)) != NULL) {
            memcpy((void*)&in,(void*)hp->h_addr,sizeof(in));    //changed from in.S_un.S_addr
            if ((ptr = inet_ntoa(in)) != NULL) {
                strncpy(outputip,ptr,maxiplen-1);
                outputip[maxiplen-1] = '\0';
            }
        }
    } else {
            //inputaddr is an IP
        strncpy(outputip,inputaddr,maxiplen-1);
        outputip[maxiplen-1] = '\0';
    }

    if (*outputip == '\0')
        return(0);
    else
        return(1);
}

//////////////////////////////////////////////////////////////////////
// Validates a "." (dotted) notation IP address.
//
// [in] ipaddr : IP address to validate.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CSock::ValidateIP(const char *ipaddr)
{

    if (inet_addr(ipaddr) == INADDR_NONE)
        return(0);

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Gets the IP address of this machine (not 127.0.0.1)
//
// [out] outputip : IP string in dotted notation (xxx.xxx.xxx.xxx).
// [in] maxiplen  : Max size of the "outputip" string.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CSock::GetMyIPAddress(char *outputip, int maxiplen)
{
	struct hostent *hostp1;
	struct hostent *hostp;
    struct in_addr in;

    if (outputip == NULL || maxiplen == 0)
        return(0);

    *outputip = '\0';

    hostp = gethostbyname("localhost");
        //look up the real name to get the real IP address
    if (hostp) {
        hostp1 = gethostbyname(hostp->h_name);  
        if (hostp1) {
            memcpy((void*)&in,(void*)(hostp1->h_addr_list[0]),sizeof(in));
            strncpy(outputip,inet_ntoa(in),maxiplen-1);
            outputip[maxiplen-1] = '\0';
            return(1);
        }
    }

	return(0);
}

////////////////////////////////////////
// Functions for transfering data.
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Read "nbytes" bytes from a descriptor.
//
// [in] sd     : Socket desc to read from.
// [out] ptr   : Buffer to put the received data into.
// [in] nbytes : Max size of the "ptr" buffer.
//
// Return : On success the number of bytes read is returned.  On
//          failure -1 is returned.
//
int CSock::RecvN(SOCKET sd, char *ptr, int nbytes)
{
    int nleft, nread;

    if (sd == SOCK_INVALID || ptr == NULL || nbytes == 0)
        return(-1);
    *ptr = '\0';    //initialize to an empty string

    nleft = nbytes;
    while (nleft > 0)  {
        nread = recv(sd,ptr,nleft,0);
        if (nread < 0)  {
            return(nread);    // error, return < 0
        } else if (nread == 0) {
            break;    // EOF
        }
        nleft -= nread;
        ptr += nread;
    }

    return(nbytes - nleft);    // return >= 0
}

//////////////////////////////////////////////////////////////////////
// Reads a line of data from a socket -- reads until a '\n' is found.
// Same syntax as fgets.
//
// [in] sd   : Socket desc to read from.
// [out] ptr : Buffer to put the received data into.
// [in] nmax : Max size of the "ptr" buffer.
//
// Return : On success the number of bytes read is returned.  On
//          failure -1 is returned.
//
int CSock::RecvLn(SOCKET sd, char *ptr, int nmax)
{
    int i, rc;
    char c;

    if (sd == SOCK_INVALID || ptr == NULL || nmax == 0)
        return(-1);
    *ptr = '\0';    //initialize to an empty string

    for (i = 0;  i < (nmax - 1);  i++)  {
        if ((rc = recv(sd,&c,1,0)) == 1)  {
            ptr[i] = c;
            if (c == '\n')
                break;
        } else if (rc == 0) {
            if (i == 0)
                return(0);  // EOF no data read
            else
                break;      // EOF some data read
        } else {
            return(-1);     // error
        }
    }

    if (i >= (nmax - 1))
        ptr[i] = '\0';
    else
        ptr[i+1] = '\0';

    return(i);
}

//////////////////////////////////////////////////////////////////////
// Write "nbytes" bytes to a descriptor.  Use in place of write()
// when "sd" is a stream socket.
//
// [in] sd         : Socket desc to write to.
// [in] ptr        : Buffer containing the data to write.
// [in] nbytes     : Number of bytes to write.
// [in] flagurgent : If flagurgent != 0, the data will be sent as urgent.
//
// Return : On success the number of bytes written is returned.  On
//          failure -1 is returned.
//
int CSock::SendN(SOCKET sd, const char *ptr, int nbytes, int flagurgent /*=0*/)
{
    int nleft;
    int nwritten;
    char *offsetptr;

    if (sd == SOCK_INVALID || ptr == NULL)
        return(-1);

    offsetptr = (char *)ptr;
    nleft = nbytes;
    while (nleft > 0) {
        if (flagurgent == 0)
            nwritten = send(sd,offsetptr,nleft,0);
        else
            nwritten = send(sd,offsetptr,nleft,MSG_OOB);
        if (nwritten <= 0)
            return(nwritten);    // error
        nleft -= nwritten;
        offsetptr += nwritten;
    }

    return(nbytes - nleft);
}

//////////////////////////////////////////////////////////////////////
// Write until '\n' is found (inserts a '\n' if necessary).
//
// [in] sd  : Socket desc to write to.
// [in] ptr : Buffer containing the data to write.
//
// Return : On success the number of bytes written is returned.  On
//          failure -1 is returned.
//
// NOTE: "ptr" must be a NULL terminated string.
//
int CSock::SendLn(SOCKET sd, const char *ptr, int flagurgent /*=0*/)
{
    char *offsetptr, *line;
    int retval;

    if (sd == SOCK_INVALID || ptr == NULL)
        return(-1);

    if ((line = (char *)calloc(strlen(ptr)+2,1)) == NULL)
        return(0);
    strcpy(line,ptr);

        //make sure the line ends with '\n'
    if(*(line+strlen(line)-1) != '\n') {
        *(line+strlen(line)+1) = '\0';
        *(line+strlen(line)) = '\n';
    }            

        //terminate the line after any '\n'
    offsetptr = strchr(line,'\n');
    *(offsetptr+1) = '\0';

    retval = SendN(sd,line,strlen(line),flagurgent);

    free(line);
    return(retval);
}

////////////////////////////////////////
// Functions for setting socket options.
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Sets the socket to S_KEEPALIVE -- so the connections won't be
// dropped.
//
// [in] sd : Socket desc to set the option on.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CSock::SetKeepAlive(SOCKET sd)
{
    int ret;
    int val = 1;

    ret = setsockopt(sd,SOL_SOCKET,SO_KEEPALIVE,(char *)&val,sizeof(int));

    if (ret == 0)
        return(1);
    else
        return(0);
}

//////////////////////////////////////////////////////////////////////
// Sets the max time a socket will block when receiving.
//
// [in] sd        : Socket desc to set the option on.
// [in] timeoutms : Recv timeout in milliseconds (0 = infinite)
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CSock::SetTimeout(SOCKET sd, int timeoutms)
{
    int retval;

    retval = setsockopt(sd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeoutms,sizeof(timeoutms));

    if (retval == 0)
        return(1);  //setsockopt() successful
    else
        return(0);  //setsockopt() failed
}

//////////////////////////////////////////////////////////////////////
// Checks to see if a socket will block.
//
// [in] sd    : Socket desc to check.
// [in] nsec  : Maximum number of seconds to wait.
// [in] nmsec ; Maximum number of milliseconds to wait.
//
// Return : 1 if the socket will not block. 0 if the socket will
//          block. -1 on error.  If the timeout period is set to
//          0, 1 is returned.
//
int CSock::CheckStatus(SOCKET sd, int nsec, int nmsec /*=0*/)
{
    int retval = 0;

    fd_set sdset;
    struct timeval tv;

    if (nsec == 0 && nmsec == 0)
        return(1);

    FD_ZERO(&sdset);    //initialize the sdset structure
    FD_SET(sd,&sdset);  //set the structure
    tv.tv_sec = nsec;   //set server to wait for nsec seconds
    tv.tv_usec = nmsec * 1000;

    retval = select(sd+1,&sdset,NULL,NULL,&tv);

    return(retval);
}

//////////////////////////////////////////////////////////////////////
// Sets the socket to receive OOB data in the normal data stream.
//
// [in] sd : Socket desc to set the option on.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CSock::SetRecvOOB(SOCKET sd)
{
    int ret;
    int val = 1;

    ret = setsockopt(sd,SOL_SOCKET,SO_OOBINLINE,(char *)&val,sizeof(int));

    if (ret == 0)
        return(1);
    else
        return(0);
}

//////////////////////////////////////////////////////////////////////
// Sets the socket to allow immediate reuse of an address/port.
//
// [in] sd : Socket desc to set the option on.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CSock::SetAddrReuse(SOCKET sd)
{
    int ret;
    int val = 1;

    ret = setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,(char *)&val,sizeof(int));

    if (ret == 0)
        return(1);
    else
        return(0);
}

////////////////////////////////////////
// Functions used for network byte
// order conversions.
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Converts an unsigned long from host to TCP/IP network byte order
// (which is big-endian).
//
// [in] lval : value to convert.
//
// Return : Converted value.
//
unsigned long CSock::HtoNl(unsigned long lval)
{

    return(htonl(lval));
}

//////////////////////////////////////////////////////////////////////
// Converts an unsigned short from host to TCP/IP network byte order.
//
// [in] sval : value to convert.
//
// Return : Converted value.
//
unsigned short CSock::HtoNs(unsigned short sval)
{

    return(htons(sval));
}

//////////////////////////////////////////////////////////////////////
// Converts an unsigned long from TCP/IP network byte order to host
// byte order.
//
// [in] lval : value to convert.
//
// Return : Converted value.
//
unsigned long CSock::NtoHl(unsigned long lval)
{

    return(ntohl(lval));
}

//////////////////////////////////////////////////////////////////////
// Converts an unsigned short from TCP/IP network byte order to host
// byte order.
//
// [in] sval : value to convert.
//
// Return : Converted value.
//
unsigned short CSock::NtoHs(unsigned short sval)
{

    return(ntohs(sval));
}


//////////////////////////////////////////////////////////////////////
// Private Methods
//////////////////////////////////////////////////////////////////////

    //implements the standard connect() function with a timeout
int CSock::Connect(SOCKET sd, struct sockaddr *saddr, int nsec)
{
    int retval, flagconnected = 0;
    fd_set writefds, exceptfds;
    struct timeval tv;
    long sockopt;
    socklen_t len;

        //set socket to non-blocking
    #ifdef WIN32
      unsigned long ularg;
      ularg = 1;
      retval = ioctlsocket(sd,FIONBIO,&ularg);
    #else
      int fflags;
      fflags = fcntl(sd,F_GETFL);
      retval = fcntl(sd,F_SETFL,fflags|O_NONBLOCK);
    #endif

    if (retval != SOCK_ERROR) {
        if (connect(sd,saddr,sizeof(struct sockaddr)) != SOCK_ERROR) {
            flagconnected = 1;
        } else {
            #ifdef WIN32
              if (GetLastError() == WSAEWOULDBLOCK) {
            #else
              if (GetLastError() == EINPROGRESS) {
            #endif
                    //connect succeeded as a non-blocking call
                    //now select to wait for the connection to be completed
                FD_ZERO(&writefds);
                FD_SET(sd,&writefds);
                FD_ZERO(&exceptfds);
                FD_SET(sd,&exceptfds);
                tv.tv_sec = nsec;
                tv.tv_usec = 0;
                sockopt = 0;
                len = sizeof(sockopt);
                    //Windows sets the exceptfds if a nonblocking connect fails.
                    //retval = select(sd+1,NULL,&writefds,NULL,&tv);    //possible non-windows ver
                if (select(sd+1,NULL,&writefds,&exceptfds,&tv) > 0) {
                    if (getsockopt(sd,SOL_SOCKET,SO_ERROR,(char *)&sockopt,&len) != SOCK_ERROR) {
                        if (sockopt == 0 && FD_ISSET(sd,&writefds) != 0)
                            flagconnected = 1;
                    }
                }
            }
        }
    }

    if (flagconnected != 0) {
            //unset non-blocking
        #ifdef WIN32
          ularg = 0;
          retval = ioctlsocket(sd,FIONBIO,&ularg);
        #else
          retval = fcntl(sd,F_SETFL,fflags);
        #endif

    }

    return(flagconnected);
}

    //gets the error code of the last error
int CSock::GetLastError()
{
    int errval;

    #ifdef WIN32
        errval = WSAGetLastError();
    #else
        errval = errno;
    #endif

    return(errval);
}
