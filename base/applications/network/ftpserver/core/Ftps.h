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
// Ftps.h: interface for the CFtps class.
//
//////////////////////////////////////////////////////////////////////
#ifndef FTPS_H
#define FTPS_H

#include "Termsrv.h"
#include "SSLSock.h"

#define FTPS_COREVERSION "1.0.9"    //core FTP server version number

#define FTPS_MAXLOGINSIZE 16    //max num of char a login can be
#define FTPS_MAXPWSIZE    16    //max num of char a password can be

#define FTPS_STDSOCKTIMEOUT 5   //standard timeout for a connection (in sec)

#define FTPS_MAXCWDSIZE  256    //max length of the CWD path

#define FTPS_LINEBUFSIZE 256    //max length of a FTP command/response line

#define FTPS_SUBSYSTEMNAME "FTPD"   //subsystem name used for the program log

#define FTPS_LOGINFAILDELAY 3   //number of seconds to delay after a failed login attempt

class CSiteInfo;    //dummy declaration (needed to avoid compiler errors)


///////////////////////////////////////////////////////////////////////////////
// Primary FTP server class
///////////////////////////////////////////////////////////////////////////////

class CFtps  
{
public:
    CFtps(SOCKET sd, CSiteInfo *psiteinfo, char *serverip = NULL, int resolvedns = 1);
    virtual ~CFtps();

        //Executes the FTP commands
    virtual int ExecFTP(int argc, char **argv);

        //FTP functions
    virtual int DoABOR(int argc, char **argv);
    virtual int DoACCT(int argc, char **argv);
    virtual int DoALLO(int argc, char **argv);
    virtual int DoAPPE(int argc, char **argv);
    virtual int DoAUTH(int argc, char **argv);
    virtual int DoCDUP(int argc, char **argv);
    virtual int DoCWD(int argc, char **argv);
    virtual int DoDELE(int argc, char **argv);
    virtual int DoFEAT(int argc, char **argv);
    virtual int DoHELP(int argc, char **argv);
    virtual int DoLIST(int argc, char **argv);
    virtual int DoMDTM(int argc, char **argv);
    virtual int DoMKD(int argc, char **argv);
    virtual int DoMODE(int argc, char **argv);
    virtual int DoNLST(int argc, char **argv);
    virtual int DoNOOP(int argc, char **argv);
    virtual int DoOPTS(int argc, char **argv);
    virtual int DoPASS(int argc, char **argv);
    virtual int DoPASV(int argc, char **argv);
    virtual int DoPBSZ(int argc, char **argv);
    virtual int DoPORT(int argc, char **argv);
    virtual int DoPROT(int argc, char **argv);
    virtual int DoPWD(int argc, char **argv);
    virtual int DoQUIT(int argc, char **argv);
    virtual int DoREIN(int argc, char **argv);
    virtual int DoREST(int argc, char **argv);
    virtual int DoRETR(int argc, char **argv);
    virtual int DoRMD(int argc, char **argv);
    virtual int DoRNFR(int argc, char **argv);
    virtual int DoRNTO(int argc, char **argv);
    virtual int DoSIZE(int argc, char **argv);
    virtual int DoSMNT(int argc, char **argv);
    virtual int DoSTAT(int argc, char **argv);
    virtual int DoSTOR(int argc, char **argv, int mode = 0);
    virtual int DoSTOU(int argc, char **argv);
    virtual int DoSYST(int argc, char **argv);
    virtual int DoTYPE(int argc, char **argv);
    virtual int DoUSER(int argc, char **argv);

        //Functions that should be overwritten
        //by a class that inherits from CFtps
    virtual int DoSITE(int argc, char **argv);

        //Functions used to set/get FTP server state
    SOCKET GetSocketDesc();
    void SetSSLInfo(sslsock_t *sslinfo);
    sslsock_t *GetSSLInfo();
    int GetUseSSL();
    int GetFlagEncData();
    char *GetServerName();
    char *GetServerIP();
    char *GetServerPort();
    char *GetClientName();
    char *GetClientIP();
    char *GetClientPort();
    CSiteInfo *GetSiteInfoPtr();
    void SetLogin(char *login);
    char *GetLogin();
    void SetUserID(int uid);
    int GetUserID();
    int GetFlagPASV();
    void SetFlagQuit(int flagquit);
    int GetFlagQuit();
    void SetFlagAbor(int flagabor);
    int GetFlagAbor();
    void SetNumListThreads(int numlistthreads);
    int GetNumListThreads();
    void SetNumDataThreads(int numdatathreads);
    int GetNumDataThreads();
    void SetLastActive(long lastactive = 0);
    long GetLastActive();

        //Used for logging and to send responses back to the FTP client
    int EventHandler(int argc, char **argv, char *mesg, char *ftpcode, int flagclient, int flagaccess, int flagprog, int respmode = 0);
        //Used to build response lines to send back to the FTP client (writes to m_linebuffer)
    int WriteToLineBuffer(char *lineformat, ...);

        //Used to parse the address in the format used in the PORT (and PASV) command
    static int ParsePORTAddr(char *rawaddr, char *addr, int maxaddr, char *port, int maxport);

protected:
    void DefaultResp(int argc, char **argv);
    int CheckPermissions(char *cmd, char *arg, char *pflags, int flagdir = 0);

protected:
    CSiteInfo *m_psiteinfo;     //pointer to the class containing the site information

    CTermsrv m_ftpscmd; //create the FTP command class

    CSock m_sock;       //create the sockets class

    CSSLSock m_sslsock; //create the SSL connection class

    SOCKET m_sd;        //socket desc of the client connection

    sslsock_t m_sslinfo;    //SSL connection information

    int m_flagencdata;  //if m_flagencdata != 0, encrypt the data channel (set w/ PROT)

        //Client information
    char m_clientip[SOCK_IPADDRLEN];    //ip address the client is connecting from
    char m_clientport[SOCK_PORTLEN];    //port number the client is using
    char m_clientname[256];             //name of the client machine

        //Server information
    char m_serverip[SOCK_IPADDRLEN];    //IP addr the server is running on
    char m_serverport[SOCK_PORTLEN];    //port number the server is running on
    char m_servername[256];             //name of the server machine

        //User information
    char m_login[FTPS_LINEBUFSIZE];     //client login name
    int m_userid;                       //User ID
    char *m_cwd;                        //user's current working directory
    char *m_userroot;                   //root path for the user (absolute path)
    
        //Control information
    int m_flagquit;             //if m_flagquit != 0, the client wants to quit
    int m_flagabor;             //used to signal an ABOR command
    long m_lastactive;          //last time the user was active

        //Counters used to keep track of the number of transfer threads (total threads = m_numlistthreads + m_numdatathreads)
    int m_numlistthreads;       //number of active list threads (Ex. LIST, NLST)
    int m_numdatathreads;       //number of active data transfer threads (Ex. RETR, STOR)
    
        //Transfer Mode Information
    int m_flagpasv;         //is the mode set to PASV (default is active mode)
    SOCKET m_pasvsd;        //socket desc for socket created in PASV cmnd
    char m_portaddr[SOCK_IPADDRLEN];    //stores the PORT addr to use for data connect
    char m_portport[SOCK_PORTLEN];      //stores the PORT port to use for data connect
    char m_type;            //type of transfer "A" = ascii and "I" = image
    long m_restoffset;      //offset to use for resuming transfers

        //buffer used for renaming a file
    char *m_renbuffer;

        //buffer used to store temporary strings
    char m_linebuffer[FTPS_LINEBUFSIZE];
};

#endif //FTPS_H
