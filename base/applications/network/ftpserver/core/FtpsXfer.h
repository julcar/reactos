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
// FtpsXfer.h: interface for the CFtpsXfer class.
//
//////////////////////////////////////////////////////////////////////
#ifndef FTPSXFER_H
#define FTPSXFER_H

#include "Ftps.h"
#include "SSLSock.h"

#define FTPSXFER_MAXDIRENTRY   256  //max length of a line in a dir listing
#define FTPSXFER_MAXPACKETSIZE 4096 //max size of a sending packet (4KB)

#define FTPSXFER_XFERRATEUPDTRATE .5   //transfer rate update rate (in seconds)

class CSiteInfo;    //dummy declaration (needed to avoid compiler errors)


///////////////////////////////////////////////////////////////////////////////
// Supplemental class used for directory listing/file transfer
///////////////////////////////////////////////////////////////////////////////

    //function used to start the transfer thread
#ifdef WIN32
  void ftpsXferThread(void *vp);    //WINDOWS must return "void" to run as a new thread
#else
  void *ftpsXferThread(void *vp);   //UNIX must return "void *" to run as a new thread
#endif

typedef struct {
    char command;       //L = dir list, R = receive data, S = send data
    int mode;           //0 = STOR, 1 = STOU, 2 = APPE (only applies to FTP STOR command)
    char options[16];   //command options (Ex. ls -la -> options = "la")
    char *path;         //file or dir path for the transfer command (Must free())
    char *cwd;          //current working directory of the user (Must free())
    char *userroot;     //user's root directory (Must free())
    int flagpasv;       //0 = active mode, 1 = passive mode
    SOCKET pasvsd;      //socket desc for the passive data connection
    char portaddr[SOCK_IPADDRLEN];  //stores the PORT addr to use for data connect
    char portport[SOCK_PORTLEN];    //stores the PORT port to use for data connect
    char type;          //A = ASCII, I = Image/Binary
    long restoffset;    //offset to use for resuming transfers
    int flagencdata;    //if flagencdata != 0, encrypt the data channel (set w/ PROT)
    long timemod;       //used to force the mod time of a STORed file (0 = let OS set the mod time)
    CFtps *pftps;           //FTP server class pointer
    CSiteInfo *psiteinfo;   //site info class (needed for sync and xfer rate functions)
} ftpsXferInfo_t;

class CFtpsXfer
{
public:
    CFtpsXfer();
    virtual ~CFtpsXfer();

    int SendListCtrl(ftpsXferInfo_t *xferinfo);
    int SendList(ftpsXferInfo_t *xferinfo);
    int RecvFile(ftpsXferInfo_t *xferinfo);
    int SendFile(ftpsXferInfo_t *xferinfo);

private:
    void SendListData(ftpsXferInfo_t *xferinfo, long *nbytes);
    long RecvFileData(ftpsXferInfo_t *xferinfo, int fdw);
    long SendFileData(ftpsXferInfo_t *xferinfo, int fdr);
    int AddToSendBuffer(char *sendbuf, int maxsendbuf, int *sendbufoffset, char *data, int datasize, int flagencdata, int flagforcesend = 0);
    char *BuildListLine(char *fullpath, char *filename, char *listline, int maxlinesize, ftpsXferInfo_t *xferinfo, int flagdir, char *thisyear);
    char *BtoA(char lastchar, char *inbuf, int inbufsize, int *outbufsize);
    char *AtoU(char *inbuf, int inbufsize, int *outbufsize);
    void UpdateXferRate(ftpsXferInfo_t *xferinfo, long bytessent);
    int SendData(ftpsXferInfo_t *xferinfo, char *buffer, int bufsize);
    int RecvData(ftpsXferInfo_t *xferinfo, char *buffer, int bufsize);
    int GetUniqueExtNum(char *filepath);
    int CheckFXP(ftpsXferInfo_t *xferinfo, int flagupload);
    int NegSSLConnection(ftpsXferInfo_t *xferinfo);

private:
    CSock m_sock;       //create the sockets class
    CSSLSock m_sslsock; //create the SSL connection class

    SOCKET m_datasd;    //socket desc for the data connection
    sslsock_t m_sslinfo;    //SSL connection information

    long m_maxulspeed;  //max upload speed (bytes/sec)
    long m_maxdlspeed;  //max download speed (bytes/sec)
    long m_bytesxfered; //num bytes xfered in the current sending cycle
    unsigned long m_xfertimer;   //used for transfer rate timing

    unsigned long m_timexferrateupdt;   //last time the transfer rate was updated
    double m_bytessincerateupdt;        //num bytes xfered since the last xfer rate update
};

#endif //FTPSXFER_H
