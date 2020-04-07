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
// FtpsXfer.cpp: implementation of the CFtpsXfer class.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>      //added for O_RDONLY, O_WRONLY, O_TRUNC, O_CREAT, O_BINARY

#ifdef WIN32
  #include <io.h>       //for open(), read(), write(), close()
#else
  #include <unistd.h>   //for open(), read(), write(), close()
#endif

#include "FtpsXfer.h"
#include "Timer.h"
#include "StrUtils.h"
#include "FSUtils.h"
#include "SiteInfo.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// Class for Transfering Directory listings/Files (CFtpsXfer)
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Function used to start the new transfer thread
//////////////////////////////////////////////////////////////////////

    //NOTE: This function does not check the validity of the buffers
    //      xferinfo->path, xferinfo->cwd, or xferinfo->userroot.
    //      The pointers psiteinfo and pftps are also not checked.
#ifdef WIN32
  void ftpsXferThread(void *vp)     //WINDOWS must return "void" to run as a new thread
#else
  void *ftpsXferThread(void *vp)    //UNIX must return "void *" to run as a new thread
#endif
{
    ftpsXferInfo_t *xferinfo;
    CFtpsXfer *pxfer;
    char *args[2], cmd[] = "XFER", arg[] = "UNKNOWN";
    
    args[0] = cmd; args[1] = arg;

    if (vp == NULL)
        #ifdef WIN32
          return;
        #else
          return(NULL);
        #endif

    xferinfo = (ftpsXferInfo_t *)vp;

    if ((pxfer = new CFtpsXfer()) != NULL) {

        switch (xferinfo->command) {
        
            case 'L': {
                pxfer->SendList(xferinfo);
            } break;

            case 'R': {
                pxfer->RecvFile(xferinfo);
            } break;

            case 'S': {
                pxfer->SendFile(xferinfo);
            } break;

            default: {
                xferinfo->pftps->EventHandler(2,args,"ERROR: unknown command type.","501",1,1,0);
            } break;

        } // end switch

        delete pxfer;
    } else {
        xferinfo->pftps->EventHandler(2,args,"ERROR: out of memory.","451",1,1,1);
    }

    free(xferinfo->path);
    free(xferinfo->cwd);
    free(xferinfo->userroot);
    free(vp);

    #ifndef WIN32
      return(NULL);
    #endif
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFtpsXfer::CFtpsXfer()
{

        //initialize the connection parameters
    m_datasd = SOCK_INVALID;
    memset(&m_sslinfo,0,sizeof(sslsock_t));

        //initialize the transfer speed parameters
    m_maxulspeed = 0;
    m_maxdlspeed = 0;
    m_bytesxfered = 0;
    m_xfertimer = 0;

        //initialize the transfer rate parameters
    m_timexferrateupdt = 0;
    m_bytessincerateupdt = 0;
}

CFtpsXfer::~CFtpsXfer()
{

}

//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

    //Sends the directory listing using the control connection
    //This is used by STAT
int CFtpsXfer::SendListCtrl(ftpsXferInfo_t *xferinfo)
{
    long nbytes = 0;
    
    if (xferinfo->pftps->GetSocketDesc() == SOCK_INVALID)
        return(0);

        //use the control socket desc as the data socket desc
    m_datasd = xferinfo->pftps->GetSocketDesc();
    memcpy(&m_sslinfo,xferinfo->pftps->GetSSLInfo(),sizeof(sslsock_t));

    SendListData(xferinfo,&nbytes);

    return(1);
}

int CFtpsXfer::SendList(ftpsXferInfo_t *xferinfo)
{
    CStrUtils strutils;
    CTimer timer;
    char *buffer;
    unsigned long ltime1, ltime2;
    long nbytes = 0;    //number of bytes sent in the dir listing
    char *args[2], cmd[] = "LIST";

    args[0] = cmd; args[1] = xferinfo->path;

        //create the data connection
    if (xferinfo->flagpasv != 0) {
            //wait for FTPS_STDSOCKTIMEOUT seconds to see if
            //m_sock.Accept() will block.
        if (m_sock.CheckStatus(xferinfo->pasvsd,FTPS_STDSOCKTIMEOUT) <= 0) {
            xferinfo->pftps->EventHandler(2,args,"Can't open data connection","425",1,1,0);
            return(0);
        }
            //accept the connection
        if ((m_datasd = m_sock.Accept(xferinfo->pasvsd)) == SOCK_INVALID) {
            xferinfo->pftps->EventHandler(2,args,"Can't open data connection","425",1,1,0);
            return(0);
        }
    } else {
            //open a connection (as a client)
        if ((m_datasd = m_sock.OpenClient(xferinfo->portaddr,xferinfo->portport,FTPS_STDSOCKTIMEOUT)) == SOCK_INVALID) {
            xferinfo->pftps->EventHandler(2,args,"Can't open data connection","425",1,1,0);
            return(0);
        }
    }

    if (CheckFXP(xferinfo,0) == 0) {
        xferinfo->pftps->EventHandler(2,args,"Can't open data connection","426",1,1,0);
        m_sock.Close(m_datasd);
        return(0);  //FXP is not allowed for the user
    }

    xferinfo->pftps->EventHandler(2,args,"Opening ASCII mode data connection for directory listing.","150",1,1,0);

        //create the encrypted data connection if necessary
    if (xferinfo->flagencdata != 0) {
        if (NegSSLConnection(xferinfo) == 0) {
            xferinfo->pftps->EventHandler(2,args,"Unable to establish an encrypted data channel.","427",1,1,0);
            m_sock.Close(m_datasd);
            return(0);  //SSL negotiation failed
        }
    }

    ltime1 = timer.Get();   //time how long it takes to send the listing
    SendListData(xferinfo,&nbytes);
    ltime2 = timer.Get();   //stop timing

        //close the data connection
    if (xferinfo->flagencdata != 0) m_sslsock.SSLClose(&m_sslinfo);
    m_sock.Close(m_datasd);

    xferinfo->pftps->SetLastActive((time(NULL))); //update the last active time

    if ((buffer = (char *)malloc(80)) != NULL) {
            //TODO: GENERALIZE THIS
        strutils.SNPrintf(buffer,79,"[Bytes: %lu][Time: %.2f s][Speed: %.2f K/s]",nbytes,timer.DiffSec(ltime1,ltime2),(nbytes/1024.0)/timer.DiffSec(ltime1,ltime2));
        buffer[79] = '\0';
        xferinfo->pftps->EventHandler(2,args,buffer,"226",1,1,0);
        free(buffer);
    } else {
        xferinfo->pftps->EventHandler(2,args,"Transfer complete.","226",1,1,0);
    }

        //decrement the list transfer thread counter
    if (xferinfo->pftps->GetNumListThreads() > 0)
        xferinfo->pftps->SetNumListThreads(xferinfo->pftps->GetNumListThreads() - 1);

    return(1);
}

int CFtpsXfer::RecvFile(ftpsXferInfo_t *xferinfo)
{
    CStrUtils strutils;
    CFSUtils fsutils;
    CTimer timer;
    char *filepath, *buffer;
    unsigned long ltime1, ltime2;
    long nbytes = 0;    //number of bytes received
    int fdw, oflag, extnum;
    char *args[2], cmd[5] = "STOR";
    fsutilsFileInfo_t fileinfo;

    args[0] = cmd; args[1] = xferinfo->path;

        //allocate memory/build the full directory/file path
    if ((filepath = fsutils.BuildPath(xferinfo->userroot,xferinfo->cwd,xferinfo->path)) == NULL) {
        xferinfo->pftps->EventHandler(2,args,"ERROR: out of memory.","425",1,1,1);
        return(0);
    }

    if (xferinfo->mode == 1) {
            //if STOU was used (mode = 1), make sure the filename is unique
        strcpy(cmd,"STOU");
        if ((extnum = GetUniqueExtNum(filepath)) != 0) {
                //build the new file path
            if ((buffer = (char *)malloc(strlen(filepath)+12)) == NULL) {
                xferinfo->pftps->EventHandler(2,args,"ERROR: out of memory.","425",1,1,1);
                free(filepath); return(0);
            }
            sprintf(buffer,"%s.%u",filepath,extnum);
            free(filepath);
            filepath = buffer;
                //build the new display path
            if ((buffer = (char *)malloc(strlen(xferinfo->path)+12)) != NULL) {
                sprintf(buffer,"%s.%u",xferinfo->path,extnum);
                free(xferinfo->path);
                args[1] = xferinfo->path = buffer;
            }
        }
    } else if (xferinfo->mode == 2) {
            //if APPE was used (mode = 2), set the reset offset to the end of the file
        strcpy(cmd,"APPE");
        if (fsutils.GetFileStats(filepath,&fileinfo) != 0)
            xferinfo->restoffset = fileinfo.size;
    }

        //open a file for writing
    oflag = ((xferinfo->restoffset > 0) ? (O_APPEND | O_RDWR) : (O_TRUNC | O_WRONLY)) | O_CREAT;
    #ifdef WIN32 //include O_BINARY for WINDOWS
      oflag |= O_BINARY;
    #endif
    if ((fdw = open(filepath,oflag,0666)) < 0) {
        fsutils.FreePath(filepath);
        if ((buffer = (char *)malloc(strlen(xferinfo->path)+49)) != NULL) {
            sprintf(buffer,"%s: The system cannot write to the file specified.",xferinfo->path);
            xferinfo->pftps->EventHandler(2,args,buffer,"425",1,1,0);
            free(buffer);
        } else {
            xferinfo->pftps->EventHandler(2,args,"ERROR: out of memory.","425",1,1,1);
        }
        return(0);
    }

        //add the file to the server (initial size is 0 bytes)
    xferinfo->psiteinfo->AddFile(filepath,xferinfo->pftps->GetLogin(),0);

        //create the data connection
    if (xferinfo->flagpasv != 0) {
            //wait for FTPS_STDSOCKTIMEOUT seconds to see if
            //m_sock.Accept() will block.
        if (m_sock.CheckStatus(xferinfo->pasvsd,FTPS_STDSOCKTIMEOUT) <= 0) {
            xferinfo->pftps->EventHandler(2,args,"Can't open data connection","425",1,1,0);
            close(fdw); fsutils.FreePath(filepath);
            return(0);
        }
            //accept the connection
        if ((m_datasd = m_sock.Accept(xferinfo->pasvsd)) == SOCK_INVALID) {
            xferinfo->pftps->EventHandler(2,args,"Can't open data connection","425",1,1,0);
            close(fdw); fsutils.FreePath(filepath);
            return(0);
        }
    } else {
            //open a connection (as a client)
        if ((m_datasd = m_sock.OpenClient(xferinfo->portaddr,xferinfo->portport,FTPS_STDSOCKTIMEOUT)) == SOCK_INVALID) {
            xferinfo->pftps->EventHandler(2,args,"Can't open data connection","425",1,1,0);
            close(fdw); fsutils.FreePath(filepath);
            return(0);
        }
    }

    if (CheckFXP(xferinfo,1) == 0) {
        xferinfo->pftps->EventHandler(2,args,"Can't open data connection","426",1,1,0);
        close(fdw); fsutils.FreePath(filepath);
        m_sock.Close(m_datasd);
        return(0);  //FXP is not allowed
    }

        //display which mode of transfer is being used.
    if (xferinfo->type == 'A') {
        if ((buffer = (char *)malloc(strlen(xferinfo->path)+41)) != NULL) {
            sprintf(buffer,"Opening ASCII mode data connection for %s.",xferinfo->path);
            xferinfo->pftps->EventHandler(2,args,buffer,"150",1,1,0);
            free(buffer);
        } else {
            xferinfo->pftps->EventHandler(2,args,"Opening ASCII mode data connection.","150",1,1,0);
        }
    } else {
        if ((buffer = (char *)malloc(strlen(xferinfo->path)+42)) != NULL) {
            sprintf(buffer,"Opening BINARY mode data connection for %s.",xferinfo->path);
            xferinfo->pftps->EventHandler(2,args,buffer,"150",1,1,0);
            free(buffer);
        } else {
            xferinfo->pftps->EventHandler(2,args,"Opening BINARY mode data connection.","150",1,1,0);
        }
    }

        //create the encrypted data connection if necessary
    if (xferinfo->flagencdata != 0) {
        if (NegSSLConnection(xferinfo) == 0) {
            xferinfo->pftps->EventHandler(2,args,"Unable to establish an encrypted data channel.","427",1,1,0);
            close(fdw); fsutils.FreePath(filepath);
            m_sock.Close(m_datasd);
            return(0);  //SSL negotiation failed
        }
    }
    
        //set the max upload speed for the user
    m_maxulspeed = xferinfo->psiteinfo->GetMaxULSpeed(xferinfo->pftps->GetLogin());

    ltime1 = timer.Get();   //time how long it takes to receive the data
    nbytes = RecvFileData(xferinfo,fdw);
    ltime2 = timer.Get();   //stop timing

        //close the data connection and file descriptor
    if (xferinfo->flagencdata != 0) m_sslsock.SSLClose(&m_sslinfo);
    m_sock.Close(m_datasd);
    close(fdw);

        //Update the file on the server (get the filesize from the OS)
    xferinfo->psiteinfo->AddFile(filepath,xferinfo->pftps->GetLogin());

        //reset the current transfer rate to 0
    xferinfo->psiteinfo->SetCurrentXferRate(xferinfo->pftps->GetUserID(),0);

    xferinfo->pftps->SetLastActive((time(NULL))); //update the last active time

        //set the modification time, if one was specified
    if (xferinfo->timemod != 0)
        fsutils.SetFileTime(filepath,xferinfo->timemod);

        //update the upload statistics
    xferinfo->psiteinfo->UpdateULStats(xferinfo->pftps->GetUserID(),xferinfo->pftps->GetLogin(),nbytes,(int)(nbytes/timer.DiffSec(ltime1,ltime2)));
        //update the user's credits
    xferinfo->psiteinfo->AddCredits(filepath,xferinfo->pftps->GetUserID(),xferinfo->pftps->GetLogin(),nbytes,1);

    fsutils.FreePath(filepath);

    if ((buffer = (char *)malloc(80)) != NULL) {
            //TODO: GENERALIZE THIS
        strutils.SNPrintf(buffer,79,"[Bytes: %lu][Time: %.2f s][Speed: %.2f K/s]",nbytes,timer.DiffSec(ltime1,ltime2),(nbytes/1024.0)/timer.DiffSec(ltime1,ltime2));
        buffer[79] = '\0';
        xferinfo->pftps->EventHandler(2,args,buffer,"226",1,1,0);
        free(buffer);
    } else {
        xferinfo->pftps->EventHandler(2,args,"Transfer complete.","226",1,1,0);
    }

        //decrement the data transfer thread counter
    if (xferinfo->pftps->GetNumDataThreads() > 0)
        xferinfo->pftps->SetNumDataThreads(xferinfo->pftps->GetNumDataThreads() - 1);

    return(1);
}

int CFtpsXfer::SendFile(ftpsXferInfo_t *xferinfo)
{
    CStrUtils strutils;
    CFSUtils fsutils;
    CTimer timer;
    fsutilsFileInfo_t finfo;
    char *filepath, *buffer;
    unsigned long ltime1, ltime2;
    long nbytes = 0;    //number of bytes sent
    int fdr;
    char *args[2], cmd[] = "RETR";

    args[0] = cmd; args[1] = xferinfo->path;

        //allocate memory/build the full directory/file path
    if ((filepath = fsutils.BuildPath(xferinfo->userroot,xferinfo->cwd,xferinfo->path)) == NULL) {
        xferinfo->pftps->EventHandler(2,args,"ERROR: out of memory.","425",1,1,1);
        return(0);
    }

        //open a file for reading
    #ifdef WIN32 //include O_BINARY for WINDOWS
      if ((fdr = open(filepath,O_RDONLY | O_BINARY,0666)) < 0) {
    #else
      if ((fdr = open(filepath,O_RDONLY,0666)) < 0) {
    #endif
        fsutils.FreePath(filepath);
        if ((buffer = (char *)malloc(strlen(xferinfo->path)+45)) != NULL) {
            sprintf(buffer,"%s: The system cannot find the file specified.",xferinfo->path);
            xferinfo->pftps->EventHandler(2,args,buffer,"425",1,1,0);
            free(buffer);
        } else {
            xferinfo->pftps->EventHandler(2,args,"ERROR: out of memory.","425",1,1,1);
        }
        return(0);
    }

    if (fsutils.GetFileStats(filepath,&finfo) == 0) {
        xferinfo->pftps->EventHandler(2,args,"The system cannot find the file specified.","425",1,1,0);
        close(fdr); fsutils.FreePath(filepath);
        return(0);
    }

        //update the user's credits (deduct the credits for this file)
    if (xferinfo->psiteinfo->RemoveCredits(filepath,xferinfo->pftps->GetUserID(),xferinfo->pftps->GetLogin(),finfo.size,0) == 0) {
        xferinfo->pftps->EventHandler(2,args,"Insufficient credits.","550",1,1,0);
        close(fdr); fsutils.FreePath(filepath);
        return(0);
    }

    if (xferinfo->flagpasv != 0) {
            //wait for FTPS_STDSOCKTIMEOUT seconds to see if
            //m_sock.Accept() will block.
        if (m_sock.CheckStatus(xferinfo->pasvsd,FTPS_STDSOCKTIMEOUT) <= 0) {
            xferinfo->psiteinfo->AddCredits(filepath,xferinfo->pftps->GetUserID(),xferinfo->pftps->GetLogin(),finfo.size,0); //restore the credits
            xferinfo->pftps->EventHandler(2,args,"Can't open data connection","425",1,1,0);
            close(fdr); fsutils.FreePath(filepath);
            return(0);
        }
            //accept the connection
        if ((m_datasd = m_sock.Accept(xferinfo->pasvsd)) == SOCK_INVALID) {
            xferinfo->psiteinfo->AddCredits(filepath,xferinfo->pftps->GetUserID(),xferinfo->pftps->GetLogin(),finfo.size,0); //restore the credits
            xferinfo->pftps->EventHandler(2,args,"Can't open data connection","425",1,1,0);
            close(fdr); fsutils.FreePath(filepath);
            return(0);
        }
    } else {
            //open a connection (as a client)
        if ((m_datasd = m_sock.OpenClient(xferinfo->portaddr,xferinfo->portport,FTPS_STDSOCKTIMEOUT)) == SOCK_INVALID) {
            xferinfo->psiteinfo->AddCredits(filepath,xferinfo->pftps->GetUserID(),xferinfo->pftps->GetLogin(),finfo.size,0); //restore the credits
            xferinfo->pftps->EventHandler(2,args,"Can't open data connection","425",1,1,0);
            close(fdr); fsutils.FreePath(filepath);
            return(0);
        }
    }

    if (CheckFXP(xferinfo,0) == 0) {
        xferinfo->psiteinfo->AddCredits(filepath,xferinfo->pftps->GetUserID(),xferinfo->pftps->GetLogin(),finfo.size,0); //restore the credits
        xferinfo->pftps->EventHandler(2,args,"Can't open data connection","426",1,1,0);
        close(fdr); m_sock.Close(m_datasd); fsutils.FreePath(filepath);
        return(0);  //FXP is not allowed
    }

        //display which mode of transfer is being used.
    if (xferinfo->type == 'A') {
        if ((buffer = (char *)malloc(strlen(xferinfo->path)+16+47)) != NULL) {
            sprintf(buffer,"Opening ASCII mode data connection for %s (%lu bytes).",xferinfo->path,finfo.size);
            xferinfo->pftps->EventHandler(2,args,buffer,"150",1,1,0);
            free(buffer);
        } else {
            xferinfo->pftps->EventHandler(2,args,"Opening ASCII mode data connection.","150",1,1,0);
        }
    } else {
        if ((buffer = (char *)malloc(strlen(xferinfo->path)+16+48)) != NULL) {
            sprintf(buffer,"Opening BINARY mode data connection for %s (%lu bytes).",xferinfo->path,finfo.size);
            xferinfo->pftps->EventHandler(2,args,buffer,"150",1,1,0);
            free(buffer);
        } else {
            xferinfo->pftps->EventHandler(2,args,"Opening BINARY mode data connection.","150",1,1,0);
        }
    }

        //create the encrypted data connection if necessary
    if (xferinfo->flagencdata != 0) {
        if (NegSSLConnection(xferinfo) == 0) {
            xferinfo->pftps->EventHandler(2,args,"Unable to establish an encrypted data channel.","427",1,1,0);
            close(fdr); m_sock.Close(m_datasd); fsutils.FreePath(filepath);
            return(0);  //SSL negotiation failed
        }
    }

        //set the max download speed for the user
    m_maxdlspeed = xferinfo->psiteinfo->GetMaxDLSpeed(xferinfo->pftps->GetLogin());

    ltime1 = timer.Get();   //time how long it takes to send the data
    nbytes = SendFileData(xferinfo,fdr);
    ltime2 = timer.Get();   //stop timing

        //close the data connection and file descriptor
    if (xferinfo->flagencdata != 0) m_sslsock.SSLClose(&m_sslinfo);
    m_sock.Close(m_datasd);
    close(fdr);

        //reset the current transfer rate to 0
    xferinfo->psiteinfo->SetCurrentXferRate(xferinfo->pftps->GetUserID(),0);

    xferinfo->pftps->SetLastActive((time(NULL))); //update the last active time

        //if the transfer was successful
    if ((nbytes + xferinfo->restoffset) >= finfo.size) {
            //update the download statistics
        xferinfo->psiteinfo->UpdateDLStats(xferinfo->pftps->GetUserID(),xferinfo->pftps->GetLogin(),finfo.size,(int)(nbytes/timer.DiffSec(ltime1,ltime2)));
    } else {
            //restore the credits
        xferinfo->psiteinfo->AddCredits(filepath,xferinfo->pftps->GetUserID(),xferinfo->pftps->GetLogin(),finfo.size,0);
    }

    fsutils.FreePath(filepath);

    if ((buffer = (char *)malloc(80)) != NULL) {
            //TODO: GENERALIZE THIS
        strutils.SNPrintf(buffer,79,"[Bytes: %lu][Time: %.2f s][Speed: %.2f K/s]",nbytes,timer.DiffSec(ltime1,ltime2),(nbytes/1024.0)/timer.DiffSec(ltime1,ltime2));
        buffer[79] = '\0';
        xferinfo->pftps->EventHandler(2,args,buffer,"226",1,1,0);
        free(buffer);
    } else {
        xferinfo->pftps->EventHandler(2,args,"Transfer complete.","226",1,1,0);
    }

        //decrement the data transfer thread counter
    if (xferinfo->pftps->GetNumDataThreads() > 0)
        xferinfo->pftps->SetNumDataThreads(xferinfo->pftps->GetNumDataThreads() - 1);

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Private Methods
//////////////////////////////////////////////////////////////////////

    //Sends the directory listing to the client
    //"nbytes" will contain the total number of bytes sent
void CFtpsXfer::SendListData(ftpsXferInfo_t *xferinfo, long *nbytes)
{
    CFSUtils fsutils;
    ftpsXferInfo_t *tmpxferinfo;
    char *linebuf, *fullpath, *packet, *listline, thisyear[8], *tmpbuf, *ptr;
    int listlinelen, packetlen = 0, len;
    long handle;

    if ((linebuf = (char *)malloc(FTPSXFER_MAXDIRENTRY)) == NULL)
        return;

        //allocate memory/build the full directory/file path
    if ((fullpath = fsutils.BuildPath(xferinfo->userroot,xferinfo->cwd,xferinfo->path)) == NULL) {
        free(linebuf);
        return;
    }

        //allocate mem for the buffer used to store data that is sent
    if ((packet = (char *)malloc(FTPSXFER_MAXPACKETSIZE)) == NULL) {
        fsutils.FreePath(fullpath); free(linebuf);
        return;
    }
        //allocate mem for a line in the listing
    listlinelen = FTPSXFER_MAXDIRENTRY;
    if ((listline = (char *)malloc(listlinelen)) == NULL) {
        free(packet); fsutils.FreePath(fullpath); free(linebuf);
        return;
    }

        //Get the current year.  This is needed to determine if a file is more
        //than a year old
    xferinfo->psiteinfo->GetTimeString(thisyear,sizeof(thisyear),"%Y",time(NULL));

        //check if the fullpath is a file.
    BuildListLine(fullpath,"",listline,listlinelen,xferinfo,2,thisyear);
    if (*listline != '\0') {
        if ((ptr = strchr(listline,'\r')) != NULL) {
            *ptr = '\0';
            len = strlen(listline);
            strncat(listline,xferinfo->path,listlinelen-len-3);
            listline[listlinelen-3] = '\0';
            strcat(listline,"\r\n");    //end the line
            *nbytes += AddToSendBuffer(packet,FTPSXFER_MAXPACKETSIZE,&packetlen,listline,strlen(listline),xferinfo->flagencdata,1);
        }
        free(listline); free(packet); fsutils.FreePath(fullpath); free(linebuf);
        return; //send the listing for the file and return
    }

        //If the recursive option (-R) was selected, display the path and check permissions
    if (strchr(xferinfo->options,'R') != NULL) {
        fsutils.CheckSlashEnd(fullpath,strlen(fullpath)+2);
            //check if the user has permission to list the directory
        if (xferinfo->psiteinfo->CheckPermissions(xferinfo->pftps->GetLogin(),fullpath,"lvx") != 0) {
                //user has permission
            if ((tmpbuf = (char *)malloc(strlen(xferinfo->path)+4)) != NULL) {
                sprintf(tmpbuf,"%s:\r\n",xferinfo->path);
                fsutils.CheckSlashUNIX(tmpbuf);
                *nbytes += AddToSendBuffer(packet,FTPSXFER_MAXPACKETSIZE,&packetlen,tmpbuf,strlen(tmpbuf),xferinfo->flagencdata);
                free(tmpbuf);
            }
        } else {
                //permission denied
            if ((tmpbuf = (char *)malloc(strlen(xferinfo->path)+23)) != NULL) {
                sprintf(tmpbuf,"%s: Permission denied.\r\n",xferinfo->path);
                fsutils.CheckSlashUNIX(tmpbuf);
                *nbytes += AddToSendBuffer(packet,FTPSXFER_MAXPACKETSIZE,&packetlen,tmpbuf,strlen(tmpbuf),xferinfo->flagencdata,1);
                free(tmpbuf);
            }
            free(listline); free(packet); fsutils.FreePath(fullpath); free(linebuf);
            return; //send the permission denied message and return
        }
    }

        //List all the directories
    if ((handle = fsutils.DirGetFirstFile(fullpath,linebuf,FTPSXFER_MAXDIRENTRY)) <= 0) {
        free(listline); free(packet); fsutils.FreePath(fullpath); free(linebuf);
        return;
    }
    do {
        if (xferinfo->pftps->GetFlagAbor() != 0) {   //check for ABOR
            fsutils.DirClose(handle); free(listline); free(packet); fsutils.FreePath(fullpath); free(linebuf);
            return;
        }
        BuildListLine(fullpath,linebuf,listline,listlinelen,xferinfo,1,thisyear);
        *nbytes += AddToSendBuffer(packet,FTPSXFER_MAXPACKETSIZE,&packetlen,listline,strlen(listline),xferinfo->flagencdata);
    } while (fsutils.DirGetNextFile(handle,linebuf,FTPSXFER_MAXDIRENTRY) > 0);
    fsutils.DirClose(handle);

        //List all the files
    if ((handle = fsutils.DirGetFirstFile(fullpath,linebuf,FTPSXFER_MAXDIRENTRY)) <= 0) {
        free(listline); free(packet); fsutils.FreePath(fullpath); free(linebuf);
        return;
    }
    do {
        if (xferinfo->pftps->GetFlagAbor() != 0) {   //check for ABOR
            fsutils.DirClose(handle); free(listline); free(packet); fsutils.FreePath(fullpath); free(linebuf);
            return;
        }
        BuildListLine(fullpath,linebuf,listline,listlinelen,xferinfo,2,thisyear);
        *nbytes += AddToSendBuffer(packet,FTPSXFER_MAXPACKETSIZE,&packetlen,listline,strlen(listline),xferinfo->flagencdata);
    } while (fsutils.DirGetNextFile(handle,linebuf,FTPSXFER_MAXDIRENTRY) > 0);
    fsutils.DirClose(handle);

        //if the recursive option (-R) was selected do a second pass
        //to go through the sub-directories.
    if (strchr(xferinfo->options,'R') != NULL) {
            //force any remaining data in the buffer to be sent (and add a \r\n)
        *nbytes += AddToSendBuffer(packet,FTPSXFER_MAXPACKETSIZE,&packetlen,"\r\n",2,xferinfo->flagencdata,1);
        free(packet);
            //List all the directories
        if ((handle = fsutils.DirGetFirstFile(fullpath,linebuf,FTPSXFER_MAXDIRENTRY)) <= 0) {
            free(listline); fsutils.FreePath(fullpath); free(linebuf);
            return;
        }
        do {
            BuildListLine(fullpath,linebuf,listline,listlinelen,xferinfo,1,thisyear);
            if (*listline != '\0') {
                if ((tmpxferinfo = (ftpsXferInfo_t *)malloc(sizeof(ftpsXferInfo_t))) != NULL) {
                    memcpy(tmpxferinfo,xferinfo,sizeof(ftpsXferInfo_t));
                    if ((tmpxferinfo->path = (char *)malloc(strlen(xferinfo->path)+strlen(linebuf)+2)) != NULL) {
                        strcpy(tmpxferinfo->path,xferinfo->path);
                        if (*(tmpxferinfo->path) != '\0')
                            fsutils.CheckSlashEnd(tmpxferinfo->path,strlen(xferinfo->path)+strlen(linebuf)+2);
                        strcat(tmpxferinfo->path,linebuf);
                        fsutils.CheckSlashUNIX(tmpxferinfo->path);
                        SendListData(tmpxferinfo,nbytes);   //Recurse
                        free(tmpxferinfo->path);
                    }
                    free(tmpxferinfo);
                }
            }
        } while (fsutils.DirGetNextFile(handle,linebuf,FTPSXFER_MAXDIRENTRY) > 0);
        fsutils.DirClose(handle);
    } else {
            //force any remaining data in the buffer to be sent
        *nbytes += AddToSendBuffer(packet,FTPSXFER_MAXPACKETSIZE,&packetlen,"",0,xferinfo->flagencdata,1);
        free(packet);
    }

    free(listline);
    fsutils.FreePath(fullpath);
    free(linebuf);
}

long CFtpsXfer::RecvFileData(ftpsXferInfo_t *xferinfo, int fdw)
{
    char *packet, *tmppacket;
    long nbytes = 0;
    int packetlen = 0, tmppacketlen = 0;
    CTimer timer;
    #ifdef WIN32
      char lastchar = ' ';
    #endif

        //move the file pointer to the proper restart position
        //(used if command is APPE or REST was specified)
    if (xferinfo->restoffset > 0)
        lseek(fdw,xferinfo->restoffset,0);

        //allocate mem for the buffer used to store data that is received
    if ((packet = (char *)malloc(FTPSXFER_MAXPACKETSIZE)) == NULL) {
        return(0);
    }

        //initialize the transfer rate parameters
    xferinfo->psiteinfo->SetCurrentXferRate(xferinfo->pftps->GetUserID(),0);
    m_bytessincerateupdt = 0;
    m_timexferrateupdt = timer.Get();
    m_bytesxfered = 0;
    m_xfertimer = timer.Get();

    do {
        if (xferinfo->pftps->GetFlagAbor() != 0) {
            free(packet);
            return(0);
        }
        if (m_sock.CheckStatus(m_datasd,10*FTPS_STDSOCKTIMEOUT,0) <= 0) {
            free(packet);
            return(0);  //if the data stops being sent
        }
        if ((packetlen = RecvData(xferinfo,packet,FTPSXFER_MAXPACKETSIZE)) < 0) {
            free(packet);
            return(0);  //if there was an error receiving the data
        }
        if (xferinfo->type == 'A') {  //Recv in ASCI mode
            #ifdef WIN32
                  //For WINDOWS OS an ASCII file will always contain \r\n (NOT \n)
              if ((tmppacket = BtoA(lastchar,packet,packetlen,&tmppacketlen)) != NULL) {
                  lastchar = tmppacket[tmppacketlen-1];
                  write(fdw,tmppacket,tmppacketlen);
                  free(tmppacket);
              } else {
                  free(packet);
                  return(0);    //out of memory
              }
            #else                   //Recv in BINARY mode
                  //For UNIX OS an ASCII file will always contain \n (NOT \r\n)
              if ((tmppacket = AtoU(packet,packetlen,&tmppacketlen)) != NULL) {
                  write(fdw,tmppacket,tmppacketlen);
                  free(tmppacket);
              } else {
                  free(packet);
                  return(0);    //out of memory
              }
            #endif
        } else {
            write(fdw,packet,packetlen);
        }
        nbytes += packetlen;
    } while(packetlen == FTPSXFER_MAXPACKETSIZE);

    free(packet);
    return(nbytes);
}

long CFtpsXfer::SendFileData(ftpsXferInfo_t *xferinfo, int fdr)
{
    char lastchar = ' ', *packet, *tmppacket;
    long nsent, nbytes = 0;
    int packetlen = 0, tmppacketlen = 0;
    CTimer timer;

        //move the file pointer to the proper restart position if REST was specified
    if (xferinfo->restoffset > 0)
        lseek(fdr,xferinfo->restoffset,0);

        //allocate mem for the buffer used to store data that is sent
    if ((packet = (char *)malloc(FTPSXFER_MAXPACKETSIZE)) == NULL) {
        return(0);
    }

        //initialize the transfer rate parameters
    xferinfo->psiteinfo->SetCurrentXferRate(xferinfo->pftps->GetUserID(),0);
    m_bytessincerateupdt = 0;
    m_timexferrateupdt = timer.Get();
    m_bytesxfered = 0;
    m_xfertimer = timer.Get();

    do {
        if (xferinfo->pftps->GetFlagAbor() != 0) {
            free(packet);
            return(0);
        }
        packetlen = read(fdr,packet,FTPSXFER_MAXPACKETSIZE);
        if (xferinfo->type == 'A') {
            if ((tmppacket = BtoA(lastchar,packet,packetlen,&tmppacketlen)) != NULL) {
                lastchar = tmppacket[tmppacketlen-1];
                if ((nsent = SendData(xferinfo,tmppacket,tmppacketlen)) < 0) {
                    free(tmppacket); free(packet);
                    return(0);
                } else {
                    nbytes += nsent;
                }
                free(tmppacket);
            } else {
                lastchar = ' ';
            }
        } else {
            if ((nsent = SendData(xferinfo,packet,packetlen)) < 0) {
                free(packet);
                return(0);
            } else {
                nbytes += nsent;
            }
        }
    } while(packetlen == FTPSXFER_MAXPACKETSIZE);

    free(packet);
    return(nbytes);
}

    //Adds "data" to the "sendbuf" and sends out the buffer on "m_datasd"/"m_sslinfo"
    //when "sendbuf" reaches "maxsendbuf".
    //The "sendbufoffset" is used to keep track of the current position
    //in "sendbuf" where the data ends.
    //if "flagforcesend" != 0, the buffer will always be sent.
    //if flagencdata != 0, the data will be sent over the ssl connection.
    //Returns the number of bytes sent.  -1 is returned on error.
int CFtpsXfer::AddToSendBuffer(char *sendbuf, int maxsendbuf, int *sendbufoffset, char *dataptr, int datasize, int flagencdata, int flagforcesend /*= 0*/)
{
    int nleftsend, dataoffset = 0, nbytes = 0;

    if (sendbuf == NULL || maxsendbuf == 0)
        return(0);

    while (1) {
        nleftsend = maxsendbuf - (*sendbufoffset);  //number of bytes left on the send buffer
        if ((datasize - dataoffset) > nleftsend) {
                //fill the remaining free space in the send buffer
            memcpy(sendbuf+(*sendbufoffset),dataptr+dataoffset,nleftsend);
            dataoffset += nleftsend;
                //send the buffer
            if (flagencdata == 0)
                nbytes += m_sock.SendN(m_datasd,sendbuf,maxsendbuf);
            else
                nbytes += m_sslsock.SSLSendN(&m_sslinfo,sendbuf,maxsendbuf);
            *sendbufoffset = 0;
        } else {
                //copy the data to the send buffer
            memcpy(sendbuf+(*sendbufoffset),dataptr+dataoffset,datasize-dataoffset);
            *sendbufoffset += (datasize - dataoffset);
            break;
        }
    }

        //empty the send buffer
    if (flagforcesend != 0) {
        if (flagencdata == 0)
            nbytes += m_sock.SendN(m_datasd,sendbuf,*sendbufoffset);
        else
            nbytes += m_sslsock.SSLSendN(&m_sslinfo,sendbuf,*sendbufoffset);
        *sendbufoffset = 0;
    }

    return(nbytes);
}

    //Loads "listline" with a directory listing line that should be returned.
    //"options" are the directory listing options that were specified.
    //"flagdir" is used for selecting only directories (=1), files (=2), or
    //both (=3).
char *CFtpsXfer::BuildListLine(char *fullpath, char *filename, char *listline, int maxlinesize, ftpsXferInfo_t *xferinfo, int flagdir, char *thisyear)
{
    CFSUtils fsutils;
    fsutilsFileInfo_t finfo;
    char *filepath;

    *listline = '\0';   //initialize listline to an empty string

        //do not display the "." and ".." entries
    if (strcmp(filename,".") == 0 || strcmp(filename,"..") == 0)
        return(listline);

    if ((filepath = (char *)malloc(strlen(fullpath)+strlen(filename)+2)) == NULL)
        return(listline);

    if (strchr(fullpath,'*') != NULL)   //check if wildcard
        fsutils.GetDirPath(filepath,strlen(fullpath)+1,fullpath); //get only the directory name
    else
        strcpy(filepath,fullpath);
    fsutils.CheckSlashEnd(filepath,strlen(fullpath)+strlen(filename)+2);
    strcat(filepath,filename);
    fsutils.CheckSlash(filepath);

    if (fsutils.GetFileStats(filepath,&finfo) == 0) {
        free(filepath);
        return(listline);
    }

    if (finfo.mode & S_IFDIR) {
        if ((flagdir & 1) == 0) {
            free(filepath);
            return(listline);   //only file lines should be returned
        }
    } else {
        if ((flagdir & 2) == 0) {
            free(filepath);
            return(listline);   //only directory lines should be returned
        }
    }

    if (strchr(xferinfo->options,'N') != NULL) {  //do NLST (names only)
        strncpy(listline,filename,maxlinesize-3);
    } else {    //do long list
        xferinfo->psiteinfo->BuildFullListLine(listline,maxlinesize-3,filepath,thisyear);
    }
    listline[maxlinesize-3] = '\0'; //make sure the string is terminated
    strcat(listline,"\r\n");        //end the line

    free(filepath);

    return(listline);
}

    //Converts the contents of inbuf from a BINARY mode to an
    //ASCII mode (all "\n" are converted to "\r\n").
    //"lastchar" is the last character in the previous buffer
    //-- this is needed to detect a "\r\n" across buffers
    //A buffer containing the ASCII version is returned.
    //The size of the returned buffer is loaded into "outbufsize"
    //NOTE: The returned buffer must be free()
char *CFtpsXfer::BtoA(char lastchar, char *inbuf, int inbufsize, int *outbufsize) {
    char *outbuf = NULL;
    int i, nlcount = 0, noutbuf = 0;

    if (outbufsize != NULL)
        *outbufsize = 0;    //initialize the out buffer size

    if (inbuf == NULL || inbufsize == 0)
        return(NULL);

    if (*inbuf == '\n' && lastchar != '\r')
        nlcount++;
    for (i = 1; i < inbufsize; i++) {
        if (*(inbuf+i) == '\n' && *(inbuf+i-1) != '\r')
            nlcount++;
    }

    if ((outbuf = (char *)malloc(inbufsize+nlcount)) == NULL)
        return(NULL);

        //copy data to the new buffer
    if (*inbuf == '\n' && lastchar != '\r') {
        memcpy(outbuf,"\r\n",2);
        noutbuf+= 2;
    } else {
        *outbuf = *inbuf;
        noutbuf++;
    }
    for (i = 1; i < inbufsize; i++) {
        if (*(inbuf+i) == '\n' && *(inbuf+i-1) != '\r') {
            memcpy(outbuf+noutbuf,"\r\n",2);
            noutbuf += 2;
        } else {
            *(outbuf + noutbuf) = *(inbuf + i);
            noutbuf++;
        }
    }

    if (outbufsize != NULL)
        *outbufsize = noutbuf;
    
    return(outbuf);
}

    //Converts the contents of inbuf from a FTP ASCII mode (\r\n) to a
    //Unix ASCII mode (\n) (all "\r\n" are converted to "\n").
    //A buffer containing the Unix ASCII version is returned.
    //The size of the returned buffer is loaded into "outbufsize"
    //NOTE: The returned buffer must be free()
char *CFtpsXfer::AtoU(char *inbuf, int inbufsize, int *outbufsize) {
    char *outbuf = NULL;
    int i, noutbuf = 0;

    if (outbufsize != NULL)
        *outbufsize = 0;    //initialize the out buffer size

    if (inbuf == NULL || inbufsize == 0)
        return(NULL);

    if ((outbuf = (char *)malloc(inbufsize)) == NULL)
        return(NULL);

        //copy data to the new buffer
    for (i = 0; i < inbufsize; i++) {
        if (*(inbuf+i) == '\r') {
            if (i < (inbufsize-1)) {
                if (*(inbuf+i+1) != '\n') {
                    *(outbuf + noutbuf) = *(inbuf+i);   //keep only '\r' w/o following '\n'
                    noutbuf++;
                }
            }
        } else {
            *(outbuf + noutbuf) = *(inbuf+i);
            noutbuf++;
        }
    }

    if (outbufsize != NULL)
        *outbufsize = noutbuf;

    return(outbuf);
}

    //updates the current transfer rate
void CFtpsXfer::UpdateXferRate(ftpsXferInfo_t *xferinfo, long bytessent)
{
    CTimer timer;
    double currxferrate;

    if (bytessent > 0)
        m_bytessincerateupdt += bytessent;

    if (timer.DiffSec(m_timexferrateupdt,timer.Get()) >= FTPSXFER_XFERRATEUPDTRATE) {
        currxferrate = m_bytessincerateupdt / timer.DiffSec(m_timexferrateupdt,timer.Get());
        xferinfo->psiteinfo->SetCurrentXferRate(xferinfo->pftps->GetUserID(),(long)currxferrate);
        m_bytessincerateupdt = 0;
        m_timexferrateupdt = timer.Get();
    }
}

int CFtpsXfer::SendData(ftpsXferInfo_t *xferinfo, char *buffer, int bufsize)
{
    CTimer timer;
    CThr thr;
    int nbytessent = 0, nbytestosend, offset = 0, packetlen = 0;
    int usermaxbytesperinterval, sitemaxbytesperinterval, useravail, siteavail;
    unsigned long xferupdtmsec, timediff;

        //Max number of bytes that can be sent within "FTPSXFER_XFERRATEUPDTRATE" sec.
        //Max bytes for the current user.
    usermaxbytesperinterval = (int)((double)m_maxdlspeed * FTPSXFER_XFERRATEUPDTRATE);
        //Max bytes for the whole site (all users combined).
    sitemaxbytesperinterval = (int)((double)xferinfo->psiteinfo->m_maxdlspeed * FTPSXFER_XFERRATEUPDTRATE);

    xferupdtmsec = (unsigned)(FTPSXFER_XFERRATEUPDTRATE * 1000); //convert to msec

    if (usermaxbytesperinterval == 0 && sitemaxbytesperinterval == 0) {
        if (xferinfo->flagencdata == 0)
            nbytessent = m_sock.SendN(m_datasd,buffer,bufsize);
        else
            nbytessent = m_sslsock.SSLSendN(&m_sslinfo,buffer,bufsize);
        UpdateXferRate(xferinfo,nbytessent);
        return(nbytessent); //no limits
    }

    usermaxbytesperinterval = (usermaxbytesperinterval == 0) ? 0x7FFFFFFF : usermaxbytesperinterval;
    sitemaxbytesperinterval = (sitemaxbytesperinterval == 0) ? 0x7FFFFFFF : sitemaxbytesperinterval;

    while (offset < bufsize) {

        useravail = usermaxbytesperinterval - m_bytesxfered;
        siteavail = sitemaxbytesperinterval - xferinfo->psiteinfo->m_bytessent;
        useravail = (useravail <= 0) ? 0 : useravail;
        siteavail = (siteavail <= 0) ? 0 : siteavail;

        if (useravail <= siteavail) {
                //user limitation
            nbytestosend = ((bufsize - offset) <= useravail) ? (bufsize - offset) : useravail;
            m_bytesxfered += nbytestosend;
            useravail = usermaxbytesperinterval - m_bytesxfered;
            if (useravail <= 0) {
                if (xferinfo->flagencdata == 0)
                    packetlen = m_sock.SendN(m_datasd,buffer+offset,nbytestosend);
                else
                    packetlen = m_sslsock.SSLSendN(&m_sslinfo,buffer+offset,nbytestosend);
                if (packetlen < 0)
                    return(packetlen);
                timediff = timer.Diff(m_xfertimer,timer.Get());
                if (xferupdtmsec > timediff)
                    timer.Sleep(xferupdtmsec - timediff);
                m_xfertimer = timer.Get();
                m_bytesxfered = 0;
            } else {
                if (xferinfo->flagencdata == 0)
                    packetlen = m_sock.SendN(m_datasd,buffer+offset,nbytestosend);
                else
                    packetlen = m_sslsock.SSLSendN(&m_sslinfo,buffer+offset,nbytestosend);
                if (packetlen < 0)
                    return(packetlen);
            }
            packetlen = (packetlen <= 0) ? 0 : packetlen;
            nbytessent += packetlen;
            offset += packetlen;
            UpdateXferRate(xferinfo,packetlen);
        } else {
                //site limitation
                //NOTE: maybe send out data in smaller pieces in case of slow connection
            nbytestosend = ((bufsize - offset) <= siteavail) ? (bufsize - offset) : siteavail;
            xferinfo->psiteinfo->m_bytessent += nbytestosend;
            siteavail = sitemaxbytesperinterval - xferinfo->psiteinfo->m_bytessent;
            if (siteavail <= 0) {
                thr.P(&(xferinfo->psiteinfo->m_mutexsend));
                if (xferinfo->flagencdata == 0)
                    packetlen = m_sock.SendN(m_datasd,buffer+offset,nbytestosend);
                else
                    packetlen = m_sslsock.SSLSendN(&m_sslinfo,buffer+offset,nbytestosend);
                if (packetlen < 0)
                    return(packetlen);
                timediff = timer.Diff(xferinfo->psiteinfo->m_sendtimer,timer.Get());
                if (xferupdtmsec > timediff)
                    timer.Sleep(xferupdtmsec - timediff);
                xferinfo->psiteinfo->m_sendtimer = timer.Get();
                xferinfo->psiteinfo->m_bytessent = 0;
                thr.V(&(xferinfo->psiteinfo->m_mutexsend));
            } else {
                if (xferinfo->flagencdata == 0)
                    packetlen = m_sock.SendN(m_datasd,buffer+offset,nbytestosend);
                else
                    packetlen = m_sslsock.SSLSendN(&m_sslinfo,buffer+offset,nbytestosend);
                if (packetlen < 0)
                    return(packetlen);
            }
            packetlen = (packetlen <= 0) ? 0 : packetlen;
            nbytessent += packetlen;
            offset += packetlen;
            UpdateXferRate(xferinfo,packetlen);
        }

    }

    return(nbytessent);
}

int CFtpsXfer::RecvData(ftpsXferInfo_t *xferinfo, char *buffer, int bufsize)
{
    CTimer timer;
    CThr thr;
    int nbytesrecv = 0, nbytestorecv = 0, offset = 0, packetlen = 0;
    int usermaxbytesperinterval, sitemaxbytesperinterval, useravail, siteavail;
    unsigned long xferupdtmsec, timediff;

        //Max number of bytes that can be sent within "FTPSXFER_XFERRATEUPDTRATE" sec.
        //Max bytes for the current user.
    usermaxbytesperinterval = (int)((double)m_maxulspeed * FTPSXFER_XFERRATEUPDTRATE);
        //Max bytes for the whole site (all users combined).
    sitemaxbytesperinterval = (int)((double)xferinfo->psiteinfo->m_maxulspeed * FTPSXFER_XFERRATEUPDTRATE);

    xferupdtmsec = (unsigned)(FTPSXFER_XFERRATEUPDTRATE * 1000); //convert to msec

    if (usermaxbytesperinterval == 0 && sitemaxbytesperinterval == 0) {
        if (xferinfo->flagencdata == 0)
            nbytesrecv = m_sock.RecvN(m_datasd,buffer,bufsize);
        else
            nbytesrecv = m_sslsock.SSLRecvN(&m_sslinfo,buffer,bufsize);
        UpdateXferRate(xferinfo,nbytesrecv);
        return(nbytesrecv);  //no limits
    }

    usermaxbytesperinterval = (usermaxbytesperinterval == 0) ? 0x7FFFFFFF : usermaxbytesperinterval;
    sitemaxbytesperinterval = (sitemaxbytesperinterval == 0) ? 0x7FFFFFFF : sitemaxbytesperinterval;

    while (offset < bufsize && packetlen == nbytestorecv) {

        useravail = usermaxbytesperinterval - m_bytesxfered;
        siteavail = sitemaxbytesperinterval - xferinfo->psiteinfo->m_bytesrecv;
        useravail = (useravail <= 0) ? 0 : useravail;
        siteavail = (siteavail <= 0) ? 0 : siteavail;

        if (useravail <= siteavail) {
                //user limitation
            nbytestorecv = ((bufsize - offset) <= useravail) ? (bufsize - offset) : useravail;
            m_bytesxfered += nbytestorecv;
            useravail = usermaxbytesperinterval - m_bytesxfered;
            if (useravail <= 0) {
                if (xferinfo->flagencdata == 0)
                    packetlen = m_sock.RecvN(m_datasd,buffer+offset,nbytestorecv);
                else
                    packetlen = m_sslsock.SSLRecvN(&m_sslinfo,buffer+offset,nbytestorecv);
                if (packetlen < 0)
                    return(packetlen);
                timediff = timer.Diff(m_xfertimer,timer.Get());
                if (xferupdtmsec > timediff)
                    timer.Sleep(xferupdtmsec - timediff);
                m_xfertimer = timer.Get();
                m_bytesxfered = 0;
            } else {
                if (xferinfo->flagencdata == 0)
                    packetlen = m_sock.RecvN(m_datasd,buffer+offset,nbytestorecv);
                else
                    packetlen = m_sslsock.SSLRecvN(&m_sslinfo,buffer+offset,nbytestorecv);
                if (packetlen < 0)
                    return(packetlen);
            }
            packetlen = (packetlen <= 0) ? 0 : packetlen;
            nbytesrecv += packetlen;
            offset += packetlen;
            UpdateXferRate(xferinfo,packetlen);
        } else {
                //site limitation
                //NOTE: maybe receive in smaller pieces in case of slow connection
            nbytestorecv = ((bufsize - offset) <= siteavail) ? (bufsize - offset) : siteavail;
            xferinfo->psiteinfo->m_bytesrecv += nbytestorecv;
            siteavail = sitemaxbytesperinterval - xferinfo->psiteinfo->m_bytesrecv;
            if (siteavail <= 0) {
                thr.P(&(xferinfo->psiteinfo->m_mutexrecv));
                if (xferinfo->flagencdata == 0)
                    packetlen = m_sock.RecvN(m_datasd,buffer+offset,nbytestorecv);
                else
                    packetlen = m_sslsock.SSLRecvN(&m_sslinfo,buffer+offset,nbytestorecv);
                if (packetlen < 0)
                    return(packetlen);
                timediff = timer.Diff(xferinfo->psiteinfo->m_recvtimer,timer.Get());
                if (xferupdtmsec > timediff)
                    timer.Sleep(xferupdtmsec - timediff);
                xferinfo->psiteinfo->m_recvtimer = timer.Get();
                xferinfo->psiteinfo->m_bytesrecv = 0;
                thr.V(&(xferinfo->psiteinfo->m_mutexrecv));
            } else {
                if (xferinfo->flagencdata == 0)
                    packetlen = m_sock.RecvN(m_datasd,buffer+offset,nbytestorecv);
                else
                    packetlen = m_sslsock.SSLRecvN(&m_sslinfo,buffer+offset,nbytestorecv);
                if (packetlen < 0)
                    return(packetlen);
            }
            packetlen = (packetlen <= 0) ? 0 : packetlen;
            nbytesrecv += packetlen;
            offset += packetlen;
            UpdateXferRate(xferinfo,packetlen);
        }

    }

    return(nbytesrecv);
}

    //returns the number to add as an extension to make the filename unique
    //if the file DNE, 0 is returned (no extension needed)
int CFtpsXfer::GetUniqueExtNum(char *filepath)
{
    CFSUtils fsutils;
    char *buffer;
    int i, extoffset;

    if (fsutils.ValidatePath(filepath) == 0)
        return(0);  //the file DNE

    if ((buffer = (char *)malloc(strlen(filepath)+12)) == NULL)
        return(0);

    strcpy(buffer,filepath);
    extoffset = strlen(filepath);

    for (i = 1; i < 0x7FFFFFFF; i++) {
        sprintf(buffer+extoffset,".%u",i);
        if (fsutils.ValidatePath(buffer) == 0)
            break;
    }

    free(buffer);
    return(i);
}

    //checks if FXP is being used and if FXP is allowed
int CFtpsXfer::CheckFXP(ftpsXferInfo_t *xferinfo, int flagupload)
{
    char ipaddr[SOCK_IPADDRLEN], port[SOCK_PORTLEN];
    int retval = 0;

        //get the client's address and port based on the data socket desc
    m_sock.GetPeerAddrPort(m_datasd,ipaddr,sizeof(ipaddr),port,sizeof(port));

    if (strcmp(ipaddr,xferinfo->pftps->GetClientIP()) == 0)
        return(1);  //FXP is not being used
    
    retval = xferinfo->psiteinfo->IsAllowedFXP(xferinfo->pftps->GetLogin(),flagupload,xferinfo->flagpasv);

    return(retval);
}

    //negotiates the SSL connection
    //returns !0 on success and 0 on failure
int CFtpsXfer::NegSSLConnection(ftpsXferInfo_t *xferinfo)
{
    sslsock_t *sslinfo;
    int retval = 0, sslerrcode = 0;

    if (xferinfo->flagpasv != 0) {
            //negotiate a server SSL connection (use the SSL context from the control connection)
        sslinfo = m_sslsock.SSLServerNeg(m_datasd,xferinfo->pftps->GetSSLInfo(),&sslerrcode);
    } else {
            //negotiate a client SSL connection
        m_sock.SetTimeout(m_datasd,FTPS_STDSOCKTIMEOUT*1000);
        sslinfo = m_sslsock.SSLClientNeg(m_datasd,&sslerrcode);
        m_sock.SetTimeout(m_datasd,0);
    }
    if (sslinfo != NULL) {
        memcpy(&m_sslinfo,sslinfo,sizeof(sslsock_t));   //copy to the local var
        m_sslsock.FreeSSLInfo(sslinfo);
        retval = 1;
    }

    return(retval);
}
