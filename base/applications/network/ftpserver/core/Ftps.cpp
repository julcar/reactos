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
// Ftps.cpp: implementation of the CFtps class.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>  //for isdigit()
#include <stdarg.h> //for va_list, etc.

#include "Ftps.h"
#include "FtpsXfer.h"

#include "StrUtils.h"
#include "SiteInfo.h"
//#include "Crypto.h"
#include "FSUtils.h"
#include "Timer.h"
#include "CmdLine.h"
#include "Thr.h"


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// Primary FTP server class
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFtps::CFtps(SOCKET sd, CSiteInfo *psiteinfo, char *serverip /*=NULL*/, int resolvedns /*=1*/)
{

        //set the site information class pointer
    m_psiteinfo = psiteinfo;

        //set the client's socked descriptor
    m_sd = sd;
        //set the socket descriptor for the FTP command class
    m_ftpscmd.SetSocketDesc(sd);

    memset(&m_sslinfo,0,sizeof(sslsock_t)); //init the SSL info obj

    m_flagencdata = 0;  //initialize to use a clear data connection

        //Initialize the user information
    *m_login = '\0';        //initialize the login to an empty string
    m_userid = 0;           //initialize the user ID to 0
    *m_clientname = '\0';   //initialize the user's address name
    *m_servername = '\0';   //initialize the server's address name
    m_cwd = (char *)calloc(1,1);        //initialize the user's CWD (to an empty string)
        //initialize the user's root directory to the default home directory
    m_userroot = psiteinfo->BuildPathHome("");      //absolute path for the site home dir

        //Initialize the control information
    m_flagquit = 0;             //if m_flagquit != 0, the client wants to quit
    m_flagabor = 0;             //used to signal an ABOR command
    m_lastactive = time(NULL);  //last time the user was active

        //Initialize the thread counters (total threads = m_numlistthreads + m_numdatathreads)
    m_numlistthreads = 0;       //number of active list threads
    m_numdatathreads = 0;       //number of active data threads

        //Initialize the transfer mode information
    m_flagpasv = 0;         //initialize to active mode
    m_pasvsd = SOCK_INVALID;    //initialize the socket desc to an invalid socket
    *m_portaddr = '\0';     //initialize the PORT addr to use for data connect
    *m_portport = '\0';     //initialize the PORT port to use for data connect
    m_type = 'A';           //initialize the transfer type to 'A' -> ASCII mode
    m_restoffset = 0;       //initialize the offset to use for resuming transfers

    m_renbuffer = NULL;     //initialize the renaming buffer

    *m_linebuffer = '\0';   //initialize the line buffer

        //get the user's address
    m_sock.GetPeerAddrPort(m_sd,m_clientip,sizeof(m_clientip),m_clientport,sizeof(m_clientport));

        //set the local address
    m_sock.GetAddrPort(m_sd,m_serverip,sizeof(m_serverip),m_serverport,sizeof(m_serverport));
    if (serverip != NULL) {
        strncpy(m_serverip,serverip,sizeof(m_serverip)-1);
        m_serverip[sizeof(m_serverip)-1] = '\0';
    }

        //get the user's address name if resolvedns is not 0
    if (resolvedns != 0) {
        m_sock.GetAddrName(m_clientip,m_clientname,sizeof(m_clientname));
        m_sock.GetAddrName(m_serverip,m_servername,sizeof(m_servername));
    }
}

CFtps::~CFtps()
{

        //If a PASV socket has been created, close it.
    if (m_pasvsd != SOCK_INVALID) {
        if (m_psiteinfo->FreetDataPort(m_pasvsd) != 1)
            m_sock.Close(m_pasvsd); //close directly if FreetDataPort() is unimplemented/fails
    }

    if (m_cwd != NULL)
        free(m_cwd);
    if (m_userroot != NULL)
        free(m_userroot);
    if (m_renbuffer != NULL)
        free(m_renbuffer);
}

//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

////////////////////////////////////////
// Executes the FTP commands
////////////////////////////////////////

int CFtps::ExecFTP(int argc, char **argv)
{
    int retval = 0;     //Stores the return value of the FTP function
    CStrUtils strutils;     //String utilities class

    if (argc < 1)
        return(0);

    switch (*(argv[0])) {

        case 'A': case 'a': {
            if (strutils.CaseCmp(argv[0],"ABOR") == 0) {
                retval = DoABOR(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"ACCT") == 0) {
                retval = DoACCT(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"ALLO") == 0) {
                retval = DoALLO(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"APPE") == 0) {
                retval = DoAPPE(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"AUTH") == 0) {
                retval = DoAUTH(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'C': case 'c': {
            if (strutils.CaseCmp(argv[0],"CDUP") == 0) {
                retval = DoCDUP(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"CWD") == 0) {
                retval = DoCWD(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'D': case 'd': {
            if (strutils.CaseCmp(argv[0],"DELE") == 0) {
                retval = DoDELE(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'F': case 'f': {
            if (strutils.CaseCmp(argv[0],"FEAT") == 0) {
                retval = DoFEAT(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'H': case 'h': {
            if (strutils.CaseCmp(argv[0],"HELP") == 0) {
                retval = DoHELP(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'L': case 'l': {
            if (strutils.CaseCmp(argv[0],"LIST") == 0) {
                retval = DoLIST(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'M': case 'm': {
            if (strutils.CaseCmp(argv[0],"MDTM") == 0) {
                retval = DoMDTM(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"MKD") == 0) {
                retval = DoMKD(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"MODE") == 0) {
                retval = DoMODE(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'N': case 'n': {
            if (strutils.CaseCmp(argv[0],"NLST") == 0) {
                retval = DoNLST(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"NOOP") == 0) {
                retval = DoNOOP(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'O': case 'o': {
            if (strutils.CaseCmp(argv[0],"OPTS") == 0) {
                retval = DoOPTS(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'P': case 'p': {
            if (strutils.CaseCmp(argv[0],"PASS") == 0) {
                retval = DoPASS(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"PASV") == 0) {
                retval = DoPASV(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"PBSZ") == 0) {
                retval = DoPBSZ(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"PORT") == 0) {
                retval = DoPORT(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"PROT") == 0) {
                retval = DoPROT(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"PWD") == 0) {
                retval = DoPWD(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'Q': case 'q': {
            if (strutils.CaseCmp(argv[0],"QUIT") == 0) {
                DoQUIT(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'R': case 'r': {
            if (strutils.CaseCmp(argv[0],"REIN") == 0) {
                retval = DoREIN(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"REST") == 0) {
                retval = DoREST(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"RETR") == 0) {
                retval = DoRETR(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"RMD") == 0) {
                retval = DoRMD(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"RNFR") == 0) {
                retval = DoRNFR(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"RNTO") == 0) {
                retval = DoRNTO(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'S': case 's': {
            if (strutils.CaseCmp(argv[0],"SITE") == 0) {
                retval = DoSITE(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"SIZE") == 0) {
                retval = DoSIZE(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"SMNT") == 0) {
                retval = DoSMNT(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"STAT") == 0) {
                retval = DoSTAT(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"STOR") == 0) {
                retval = DoSTOR(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"STOU") == 0) {
                retval = DoSTOU(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"SYST") == 0) {
                retval = DoSYST(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'T': case 't': {
            if (strutils.CaseCmp(argv[0],"TYPE") == 0) {
                retval = DoTYPE(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'U': case 'u': {
            if (strutils.CaseCmp(argv[0],"USER") == 0) {
                retval = DoUSER(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        case 'X': case 'x': {
            if (strutils.CaseCmp(argv[0],"XCUP") == 0) {
                retval = DoCDUP(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"XCWD") == 0) {
                retval = DoCWD(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"XMKD") == 0) {
                retval = DoMKD(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"XPWD") == 0) {
                retval = DoPWD(argc,argv);
            } else if (strutils.CaseCmp(argv[0],"XRMD") == 0) {
                retval = DoRMD(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        default: {
                //Recheck for the ABOR command.
                //When ABOR is sent as an urgent message it usually has some misc
                //characters preceeding it.
            if (strstr(argv[0],"ABOR") != NULL || strstr(argv[0],"abor") != NULL)
                retval = DoABOR(argc,argv);
            else
                DefaultResp(argc,argv);
        } break;

    } // end switch

    return(retval);
}

////////////////////////////////////////
// FTP functions
//
// Implementations of the FTP commands
////////////////////////////////////////

int CFtps::DoABOR(int argc, char **argv)
{

        //indicate all operations should be aborted
    m_flagabor = 1;

    EventHandler(argc,argv,"ABOR command successful.","225",1,1,0);

    return(1);
}

int CFtps::DoACCT(int argc, char **argv)
{

    EventHandler(argc,argv,"ACCT command not implemented.","502",1,1,0);

    return(1);
}

int CFtps::DoALLO(int argc, char **argv)
{

    EventHandler(argc,argv,"ALLO command ignored.","202",1,1,0);

    return(1);
}

int CFtps::DoAPPE(int argc, char **argv)
{

    if (argc < 2) {
        EventHandler(argc,argv,"'APPE': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

    return(DoSTOR(argc,argv,2));
}

int CFtps::DoAUTH(int argc, char **argv)
{
    CStrUtils strutils;
    sslsock_t *sslinfo;
    char *privkeybuf, *certbuf, privpw[32];
    int retval = 0, errcode = 0, privkeysize = 0, certsize = 0;

    if (argc < 2) {
        EventHandler(argc,argv,"'AUTH': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

    if (m_ftpscmd.GetUseSSL() != 0) {
        EventHandler(argc,argv,"Control connection is already encrypted.","500",1,1,0);
        return(0);
    }

    if (strutils.CaseCmp(argv[1],"SSL") == 0) {
        if ((privkeybuf = m_psiteinfo->GetPrivKey(&privkeysize)) == NULL) {
            EventHandler(argc,argv,"Unable to initialize SSL.","421",1,1,0);
            return(0);
        }
        if ((certbuf = m_psiteinfo->GetCert(&certsize)) == NULL) {
            EventHandler(argc,argv,"Unable to initialize SSL.","421",1,1,0);
            m_psiteinfo->FreePrivKey(privkeybuf);
            return(0);
        }
        EventHandler(argc,argv,"Security data exchange complete.","234",1,1,0);
        m_psiteinfo->GetPrivKeyPW(privpw,sizeof(privpw));
        if ((sslinfo = m_sslsock.SSLServerNeg(m_sd,privkeybuf,privkeysize,privpw,certbuf,certsize,&errcode)) != NULL) {
            memcpy(&m_sslinfo,sslinfo,sizeof(sslsock_t));
            m_sslsock.FreeSSLInfo(sslinfo);
            m_ftpscmd.SetSSLInfo(&m_sslinfo);   //set the SSL info
            m_ftpscmd.SetUseSSL(1);             //set to use SSL
            retval = 1;
        }
        m_psiteinfo->FreeCert(certbuf);
        m_psiteinfo->FreePrivKey(privkeybuf);
    } else {
        EventHandler(argc,argv,"Command not implemented for that parameter.","504",1,1,0);
    }

    return(retval);
}

int CFtps::DoCDUP(int argc, char **argv)
{
    char *tmpargv[3], cwd[] = "CWD", dotdot[] = "..";

    tmpargv[0] = cwd;
    tmpargv[1] = dotdot;
    tmpargv[2] = NULL;

    DoCWD(2,tmpargv);

    return(1);
}

int CFtps::DoCWD(int argc, char **argv)
{
    CFSUtils fsutils;
    CCmdLine cmdline(0);
    char *buffer, *tmppath, *dirname;
    int retval = 0, mode = 0;

    if (argc < 2) {
        EventHandler(argc,argv,"CWD command successful.","250",1,1,0);
        return(1);
    }

    if (m_cwd == NULL) {
        if ((m_cwd = (char *)calloc(1,1)) == NULL) {
            EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
            return(0);
        }
    }

        //combine all the arguments
    if ((dirname = cmdline.CombineArgs(argc,argv)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        return(0);
    }
    fsutils.CheckSlashEnd(dirname,strlen(dirname)+2);

        //check if the user has permission to change into the directory
    if (CheckPermissions("CWD",dirname,"cx") == 0) {
        cmdline.FreeCombineArgs(dirname);
        return(0);  //permission denied
    }

        //build the new CWD
    if ((tmppath = fsutils.BuildPath("",m_cwd,dirname)) == NULL) {
        EventHandler(argc,argv,"Unable to set CWD.","530",1,1,0);
        cmdline.FreeCombineArgs(dirname);
        return(0);
    }
    cmdline.FreeCombineArgs(dirname);

        //make sure the CWD is not too long
    if (strlen(tmppath) > FTPS_MAXCWDSIZE) {
        EventHandler(argc,argv,"Unable to set CWD.","531",1,1,0);
        fsutils.FreePath(tmppath);
        return(0);
    }

        //validate the new path
    if ((buffer = (char *)malloc(strlen(m_userroot)+strlen(tmppath)+1)) != NULL) {
        strcpy(buffer,m_userroot);
        strcat(buffer,tmppath);
        if ((mode = fsutils.ValidatePath(buffer)) == 1) { //if a valid directory
            if (m_cwd != NULL)
                free(m_cwd);    //free the old CWD
            m_cwd = tmppath;    //set the new CWD (the path is valid)
            retval = 1;
            //printf("CWD = %s\n",buffer); //DEBUG
        }
        free(buffer);
    }

    if (retval != 0) {
        EventHandler(argc,argv,"CWD command successful.","250",1,1,0);
        return(1);
    } else {
        fsutils.CheckSlashUNIX(tmppath);    //display the path w/UNIX slashes
        if (mode == 2) {    //if the path points to a file
            fsutils.RemoveSlashEnd(tmppath);
            WriteToLineBuffer("/%s: Not a directory.",tmppath);
        } else {            //if the path DNE
            WriteToLineBuffer("/%s: No such file or directory.",tmppath);
        }
        EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
        fsutils.FreePath(tmppath);
        return(0);
    }
}

int CFtps::DoDELE(int argc, char **argv)
{
    CFSUtils fsutils;
    CCmdLine cmdline(0);
    char *filename, *tmppath;
    int retval;

    if (argc < 2) {
        EventHandler(argc,argv,"'DELE': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

    if ((filename = cmdline.CombineArgs(argc,argv)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        return(0);
    }

        //check the user's permissions for the path
    if (CheckPermissions("DELE",filename,"wo") == 0) {
        cmdline.FreeCombineArgs(filename);
        return(0);
    }
    
    if ((tmppath = fsutils.BuildPath(m_userroot,m_cwd,filename)) == NULL) {
        EventHandler(argc,argv,"ERROR: unable to set file path.","550",1,1,0);
        cmdline.FreeCombineArgs(filename);
        return(0);
    }

    retval = fsutils.DelFile(tmppath);  //delete the file

    fsutils.FreePath(tmppath);

    if (retval == 0) {
        fsutils.CheckSlashUNIX(filename);    //display the path w/UNIX slashes
        WriteToLineBuffer("%s: The system cannot find the file specified.",filename);
        EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
    } else {
        EventHandler(argc,argv,"DELE command successful.","250",1,1,0);
    }

    cmdline.FreeCombineArgs(filename);
    return(retval);
}

int CFtps::DoFEAT(int argc, char **argv)
{

    m_ftpscmd.ResponseSend("Extensions supported","211",1);
    m_ftpscmd.ResponseSend("  AUTH SSL","211",1);
    m_ftpscmd.ResponseSend("  PBSZ","211",1);
    m_ftpscmd.ResponseSend("  PROT","211",1);
    EventHandler(argc,argv,"FEAT command successful.","211",1,1,0);

    return(1);
}

int CFtps::DoHELP(int argc, char **argv)
{

    m_ftpscmd.ResponseSend("The following commands are recognized (* =>'s unimplemented).","214",1);
    m_ftpscmd.ResponseSend("  ABOR   *ACCT   *ALLO    APPE ","214",1);
    m_ftpscmd.ResponseSend("  CDUP    CWD     DELE    FEAT ","214",1);
    m_ftpscmd.ResponseSend("  HELP    LIST    MDTM    MKD  ","214",1);
    m_ftpscmd.ResponseSend("  MODE    NLST    NOOP   *OPTS ","214",1);
    m_ftpscmd.ResponseSend("  PASS    PASV    PORT    PWD  ","214",1);
    m_ftpscmd.ResponseSend("  QUIT   *REIN    REST    RETR ","214",1);
    m_ftpscmd.ResponseSend("  RMD     RNFR    RNTO    SITE ","214",1);
    m_ftpscmd.ResponseSend("  SIZE   *SMNT    STAT    STOR ","214",1);
    m_ftpscmd.ResponseSend("  STOU    SYST    TYPE    USER ","214",1);
    EventHandler(argc,argv,"HELP command successful.","214",1,1,0);

    return(1);
}

int CFtps::DoLIST(int argc, char **argv)
{
    CFSUtils fsutils;
    CCmdLine cmdline(0);
    CThr thr;
    ftpsXferInfo_t *xferinfo;
    char *tmppath, permissions[4];
    int i, optionlen;

        //allocate the transfer information structure
        //this structure is properly free() by ftpsXferThread()
    if ((xferinfo = (ftpsXferInfo_t *)calloc(sizeof(ftpsXferInfo_t),1)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        return(0);
    }

        //extract the options
    optionlen = 0;
    for (i = 1; i < argc; i++) {
        if (*argv[i] == '-') {  //if the argument is an option
            optionlen += strlen(argv[i]+1);
            if (optionlen < (int)sizeof(xferinfo->options))
                strcat(xferinfo->options,argv[i]+1);
        }
    }

        //allocate memory for the path
    if ((xferinfo->path = cmdline.CombineArgs(argc,argv,'-')) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        free(xferinfo);
        return(0);
    }

        //check the user's permissions for the path
    if (strchr(xferinfo->options,'R') != NULL)
        strcpy(permissions,"vx");   //permissions needed for recursive listing
    else
        strcpy(permissions,"lvx");   //permissions needed for non-recursive listing
    if (CheckPermissions("LIST",xferinfo->path,permissions,1) == 0) {
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }

        //check if the path is valid
    if ((tmppath = fsutils.BuildPath(m_userroot,m_cwd,xferinfo->path)) == NULL) {
        EventHandler(argc,argv,"ERROR: unable to set directory path.","550",1,1,0);
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }
    if (fsutils.ValidatePath(tmppath) == 0 && strchr(tmppath,'*') == NULL) {
        fsutils.FreePath(tmppath);
        fsutils.CheckSlashUNIX(xferinfo->path); //display the path w/UNIX slashes
        WriteToLineBuffer("%s: No such file or directory.",xferinfo->path);
        EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }
    fsutils.FreePath(tmppath);

        //allocate memory for the current working dir
    if ((xferinfo->cwd = (char *)malloc(strlen(m_cwd)+1)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }
    strcpy(xferinfo->cwd,m_cwd);    //set the current working directory

        //allocate memory for the user's root directory
    if ((xferinfo->userroot = (char *)malloc(strlen(m_userroot)+1)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        free(xferinfo->cwd); cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }
    strcpy(xferinfo->userroot,m_userroot);  //set the user's root directory

        //set the remaining values of the transfer info
    xferinfo->command = 'L';    //indicate the command is a LIST
    xferinfo->flagpasv = m_flagpasv;
    xferinfo->pasvsd = m_pasvsd;
    strcpy(xferinfo->portaddr,m_portaddr);
    strcpy(xferinfo->portport,m_portport);
    xferinfo->type = m_type;
    xferinfo->restoffset = m_restoffset;
    xferinfo->flagencdata = m_flagencdata;
    xferinfo->pftps = this;
    xferinfo->psiteinfo = m_psiteinfo;

    m_flagabor = 0;     //reset the abort flag
    m_numlistthreads++; //increment the number of listing threads

        //create the thread to transfer the directory listing
    if (thr.Create(ftpsXferThread,(void *)xferinfo) == 0) {
            //if unable to create the new thread, clean up.
        EventHandler(argc,argv,"ERROR: unable to create a new transfer thread.","530",1,1,1);
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo->cwd);
        free(xferinfo->userroot); free(xferinfo);
        m_numlistthreads--;
        return(0);
    }

    return(1);
}

int CFtps::DoMDTM(int argc, char **argv)
{
    CFSUtils fsutils;
    CCmdLine cmdline(0);
    fsutilsFileInfo_t fileinfo;
    char *filename, *path, datebuffer[64];
    int retval = 0;

    if (argc < 2) {
        EventHandler(argc,argv,"'MDTM': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

    if ((filename = cmdline.CombineArgs(argc,argv)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        return(0);
    }

    if (CheckPermissions("MDTM",filename,"drlvx") == 0) {
        cmdline.FreeCombineArgs(filename);
        return(0);
    }

    if ((path = fsutils.BuildPath(m_userroot,m_cwd,filename)) == NULL) {
        EventHandler(argc,argv,"ERROR: unable to set directory path.","550",1,1,0);
        cmdline.FreeCombineArgs(filename);
        return(0);
    }

    if (fsutils.GetFileStats(path,&fileinfo) != 0) {
        m_psiteinfo->GetTimeString(datebuffer,sizeof(datebuffer),"%Y%m%d%H%M%S",fileinfo.timemod,1);
        EventHandler(argc,argv,datebuffer,"213",1,1,0);
    } else {
        WriteToLineBuffer("%s: No such file or directory.",filename);
        EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
    }

    fsutils.FreePath(path);
    cmdline.FreeCombineArgs(filename);

    return(retval);
}

int CFtps::DoMKD(int argc, char **argv)
{
    CFSUtils fsutils;
    CCmdLine cmdline(0);
    char *dirname, *tmppath;
    int i, retval = 0;

    if (argc < 2) {
        EventHandler(argc,argv,"'MKD': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

    if ((dirname = cmdline.CombineArgs(argc,argv)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        return(0);
    }
    fsutils.RemoveSlashEnd(dirname);

        //check the user's permissions for the path
    if (CheckPermissions("MKD",dirname,"wm",1) == 0) {
        cmdline.FreeCombineArgs(dirname);
        return(0);
    }
    
    if ((tmppath = fsutils.BuildPath(m_userroot,m_cwd,dirname)) == NULL) {
        EventHandler(argc,argv,"ERROR: unable to set directory path.","550",1,1,0);
        cmdline.FreeCombineArgs(dirname);
        return(0);
    }

    fsutils.CheckSlashUNIX(dirname);    //display the path w/UNIX slashes

    i = fsutils.ValidatePath(tmppath);
    if (i == 1) {   //if the directory already exists
        WriteToLineBuffer("%s: Directory exits.",dirname);
        EventHandler(argc,argv,m_linebuffer,"521",1,1,0);
    } else if (i == 2) {    //if a file was specified
        WriteToLineBuffer("%s: Is a file.",dirname);
        EventHandler(argc,argv,m_linebuffer,"521",1,1,0);
    } else {
        if (fsutils.MkDir(tmppath) == 0) {  //make the directory
            WriteToLineBuffer("%s: Unable to create directory.",dirname);
            EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
        } else {
            WriteToLineBuffer("\"%s\" new directory created.",dirname);
            EventHandler(argc,argv,m_linebuffer,"257",1,1,0);
            retval = 1;
        }
    }

    fsutils.FreePath(tmppath);
    cmdline.FreeCombineArgs(dirname);
    return(retval);
}

int CFtps::DoMODE(int argc, char **argv)
{
    int retval = 0;

    if (argc < 2) {
        EventHandler(argc,argv,"'MODE': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

    if (strcmp(argv[1],"b") == 0 || strcmp(argv[1],"B") == 0) {
        EventHandler(argc,argv,"Unimplemented MODE type.","502",1,1,0);
    } else if (strcmp(argv[1],"s") == 0 || strcmp(argv[1],"S") == 0) {
        EventHandler(argc,argv,"MODE S ok.","200",1,1,0);
        retval = 1;
    } else {
        WriteToLineBuffer("'MODE %s': command not understood.",argv[1]);
        EventHandler(argc,argv,m_linebuffer,"500",1,1,0);
    }

    return(retval);
}

int CFtps::DoNLST(int argc, char **argv)
{
    int i, retval;
    char **tmpargv, list[5] = "LIST", nameopt[3] = "-N";

    if ((tmpargv = (char **)malloc(sizeof(char *)*(argc+2))) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        return(0);
    }

        //add a -N option to the entered arguments
        //NOTE: the -N option indicates only the names should be displayed
    tmpargv[0] = list;
    tmpargv[1] = nameopt;
    for (i = 1; i < argc; i++)
        tmpargv[i+1] = argv[i];
    tmpargv[i+1] = NULL;

    retval = DoLIST(argc+1,tmpargv);

    free(tmpargv);
    return(retval);
}

int CFtps::DoNOOP(int argc, char **argv)
{

    EventHandler(argc,argv,"NOOP command successful.","200",1,1,0);

    return(1);
}

int CFtps::DoOPTS(int argc, char **argv)
{

    EventHandler(argc,argv,"OPTS command not implemented.","502",1,1,0);

    return(1);
}

int CFtps::DoPASS(int argc, char **argv)
{
    CFSUtils fsutils;
    //CCrypto crypto;
    CTimer timer;
    char *pwdec, *tmppath, password[FTPS_MAXPWSIZE+1] = "";
    siteinfoPWInfo_t *pwinfo;
    int flagauth = 0;

        //Check if ths user exists locally
    if (m_psiteinfo->CheckUserProp(m_login) == 0) {
        timer.Sleep(FTPS_LOGINFAILDELAY * 1000); //delay after failed login
        WriteToLineBuffer("User %s cannot login.",m_login);
        EventHandler(1,argv,m_linebuffer,"530",1,1,0);
        return(0);
    }

    if (argc >= 2) {
        strncpy(password,argv[1],FTPS_MAXPWSIZE);
        password[FTPS_MAXPWSIZE] = '\0';
    }

    if ((pwinfo = m_psiteinfo->GetPWInfo(m_login)) != NULL) {
            //if anonymous, don't check password
        if (atol(pwinfo->groupnumber) != SITEINFO_ANONYMOUSGROUPNUM) {
#if 0
            if ((pwdec = crypto.DecryptHexStr(pwinfo->passwordraw,m_login,password)) != NULL) {
                if (strcmp(pwdec,password) == 0)
                    flagauth = 1;   //the password is correct
                crypto.FreeDataBuffer(pwdec);
            }
#else
            pwdec = pwinfo->passwordraw;
            if (strcmp(pwdec,password) == 0)
                flagauth = 1;   //the password is correct
#endif
        } else {
            flagauth = 1;   //no password necessary for anonymous
        }
            //Set the user's home directory (root)
        if (flagauth != 0) {
            if ((tmppath = (char *)malloc(strlen(pwinfo->homedir)+2)) != NULL) {
                strcpy(tmppath,pwinfo->homedir);
                fsutils.CheckSlash(tmppath);    //make sure the slashes are correct
                fsutils.CheckSlashEnd(tmppath,strlen(pwinfo->homedir)+2); //add ending '/' if necessary
                    //check if the specified path is valid
                if (fsutils.ValidatePath(tmppath) == 1) { //if tmppath is a valid directory
                    if (m_userroot != NULL)
                        free(m_userroot);   //free the old user root path
                    m_userroot = tmppath;   //set the new user root path
                    if (m_cwd != NULL)
                        *m_cwd = '\0';      //reset the CWD
                } else {
                    m_psiteinfo->WriteToProgLog(FTPS_SUBSYSTEMNAME,"User %s has an invalid home directory (%s).",m_login,tmppath);
                    WriteToLineBuffer("User %s has an invalid home directory.",m_login);
                    EventHandler(1,argv,m_linebuffer,"530",1,1,0,1);
                    free(tmppath);
                    flagauth = 0;   //do not allow login without a valid home dir
                }
            }
        }
        m_psiteinfo->FreePWInfo(pwinfo);
    }

    if (flagauth != 0) {    //user was successfully authenticated
        WriteToLineBuffer("User %s logged in.",m_login);
        EventHandler(1,argv,m_linebuffer,"230",1,1,0);
    } else {    //user was not authenticated
        timer.Sleep(FTPS_LOGINFAILDELAY * 1000); //delay after failed login
        WriteToLineBuffer("User %s cannot login.",m_login);
        EventHandler(1,argv,m_linebuffer,"530",1,1,0);
    }

    return(flagauth);
}

    //NOTE: If a PASV socket is left open when the user logs out,
    //      it will be closed at logout.
int CFtps::DoPASV(int argc, char **argv)
{
    char *ptr1, *ptr2, bindport[SOCK_PORTLEN], bindaddr[SOCK_IPADDRLEN];
    unsigned short p, ph,pl;
    int retval;

        //if a PASV socket has already been created, close it.
    if (m_pasvsd != SOCK_INVALID) {
        if (m_psiteinfo->FreetDataPort(m_pasvsd) != 1)
            m_sock.Close(m_pasvsd); //close directly if FreetDataPort() is unimplemented/fails
    }
    m_flagpasv = 1;     //set to use passive mode

        //Create a socket and bind to it and get the port number
        //that was binded to.
        //NOTE: NULL is used for the "bindip" field to allow PASV mode when
        //      acting as an external (IP) machine from behind a NATed firewall.
        //      This allows PASV mode to be used with an IP that is not assigned
        //      to the machine running the server (using NULL binds to all IPs).
    retval = m_psiteinfo->GetDataPort(bindport,sizeof(bindport),(int *)(&m_pasvsd),NULL);
    if (retval == -1) {
            //GetDataPort() is not implemented.  Try to bind directly
        if ((m_pasvsd = m_sock.BindServer(bindport,sizeof(bindport),m_serverip)) == SOCK_INVALID) {
            EventHandler(argc,argv,"Unable to bind to a port.","500",1,1,1);
            return(0);      //there was an error in the binding
        }
    } else if (retval == 0) {
            //Unable to bind to an available port
        EventHandler(argc,argv,"Unable to bind to a port.","500",1,1,1);
        return(0);      //there was an error in the binding
    }

        //listen on the socket descriptor
    if (m_sock.ListenServer(m_pasvsd) == 0) {
        WriteToLineBuffer("Unable to listen on port %s (sd = %d).",bindport,m_pasvsd);
        EventHandler(argc,argv,m_linebuffer,"500",1,1,1);
        if (m_psiteinfo->FreetDataPort(m_pasvsd) != 1)
            m_sock.Close(m_pasvsd); //close directly if FreetDataPort() is unimplemented/fails
        m_pasvsd = SOCK_INVALID;    //invalidate the socket desc
        return(0);      //there was an error in listening
    }

        //replace the '.' with ',' in the IP address
    strcpy(bindaddr,m_serverip);
    ptr1 = strchr(bindaddr,'.');
    *ptr1 = ',';
    ptr1++;
    ptr2 = strchr(ptr1,'.');
    *ptr2 = ',';
    ptr2++;
    ptr1 = strchr(ptr2,'.');
    *ptr1 = ',';

        //generate the high and low parts of the port number
    p = atoi(bindport);
    ph = p / 256;
    pl = p % 256;

    WriteToLineBuffer("Entering Passive Mode (%s,%hu,%hu).",bindaddr,ph,pl);
    EventHandler(argc,argv,m_linebuffer,"227",1,1,0);

    return(1);
}

int CFtps::DoPBSZ(int argc, char **argv)
{
    int retval = 0;

    if (m_ftpscmd.GetUseSSL() == 0) {
        EventHandler(argc,argv,"Bad sequence of commands.","503",1,1,0);
    } else {
        if (argc < 2) {
            EventHandler(argc,argv,"'PBSZ': Invalid number of parameters.","500",1,1,0);
        } else {
            EventHandler(argc,argv,"Command ok.  PBSZ=0","200",1,1,0);
            retval = 1;
        }
    }

    return(retval);
}

int CFtps::DoPORT(int argc, char **argv)
{

    if (argc < 2) {
        EventHandler(argc,argv,"'PORT': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

    WriteToLineBuffer("%s",argv[1]);

    if (ParsePORTAddr(m_linebuffer,m_portaddr,sizeof(m_portaddr),m_portport,sizeof(m_portport)) == 0) {
        EventHandler(argc,argv,"Invalid PORT command.","500",1,1,0);
        return(0);
    }

    m_flagpasv = 0;     //set to use active mode

        //if a socket was created for the PASV command, close it.
    if (m_pasvsd != SOCK_INVALID) {
        if (m_psiteinfo->FreetDataPort(m_pasvsd) != 1)
            m_sock.Close(m_pasvsd); //close directly if FreetDataPort() is unimplemented/fails
        m_pasvsd = SOCK_INVALID;
    }

    EventHandler(argc,argv,"PORT command successful.","200",1,1,0);

    return(1);
}

int CFtps::DoPROT(int argc, char **argv)
{
    CStrUtils strutils;
    int retval = 0;

    if (m_ftpscmd.GetUseSSL() == 0) {
        EventHandler(argc,argv,"Bad sequence of commands.","503",1,1,0);
    } else {
        if (argc < 2) {
            EventHandler(argc,argv,"'PROT': Invalid number of parameters.","500",1,1,0);
        } else {
            if (strutils.CaseCmp(argv[1],"C") == 0) {
                EventHandler(argc,argv,"Not Encrypting Data Channel","200",1,1,0);
                m_flagencdata = 0;  //use a clear data channel
                retval = 1;
            } else if (strutils.CaseCmp(argv[1],"S") == 0 ||
                     strutils.CaseCmp(argv[1],"E") == 0 ||
                     strutils.CaseCmp(argv[1],"P") == 0) {
                EventHandler(argc,argv,"Encrypting Data Channel","200",1,1,0);
                m_flagencdata = 1;  //use an encrypted data channel
                retval = 1;
            } else {
                WriteToLineBuffer("'PROT %s': command not understood.",argv[1]);
                EventHandler(argc,argv,m_linebuffer,"500",1,1,0);
            }
        }
    }

    return(retval);
}

int CFtps::DoPWD(int argc, char **argv)
{
    CFSUtils fsutils;

        //check the user's permissions
    if (CheckPermissions("PWD",NULL,"px") == 0)
        return(0);

    WriteToLineBuffer("\"/%s\" is current directory.",m_cwd);
    fsutils.CheckSlashUNIX(m_linebuffer);     //display the path w/UNIX slashes
    EventHandler(argc,argv,m_linebuffer,"257",1,1,0);

    return(1);
}

int CFtps::DoQUIT(int argc, char **argv)
{
    CTimer timer;
    int counter = 0;

        //stop any current transfers
    m_flagabor = 1;         //stop all threads
    while ((m_numlistthreads+m_numdatathreads) > 0) {    //if there are any active transfer threads
        timer.Sleep(50);    //wait until all threads have stopped
        if (counter++ > (FTPS_STDSOCKTIMEOUT*20)) break; //wait up to FTPS_STDSOCKTIMEOUT sec
    }

        //print the goodbye message and logout
    EventHandler(argc,argv,"Exit.","221",1,1,0);
    m_flagquit = 1;  //set flag to indicate the user is quiting

    return(1);
}

int CFtps::DoREIN(int argc, char **argv)
{

    EventHandler(argc,argv,"'REIN': command not implemented.","504",1,1,0);

    return(1);
}

int CFtps::DoREST(int argc, char **argv)
{
    CStrUtils strutils;
    int retval = 0;

    if (argc < 2) {
        EventHandler(argc,argv,"'REST': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

    if (strutils.IsNum(argv[1]) != 0) {
        m_restoffset = atol(argv[1]);
        WriteToLineBuffer("Restarting at %s.  Send STOR or RETR to initiate transfer.",argv[1]);
        EventHandler(argc,argv,m_linebuffer,"350",1,1,0);
        retval = 1;
    } else {
        WriteToLineBuffer("'REST %s': command not understood.",argv[1]);
        EventHandler(argc,argv,m_linebuffer,"500",1,1,0);
    }

    return(retval);
}

int CFtps::DoRETR(int argc, char **argv)
{
    CFSUtils fsutils;
    CCmdLine cmdline(0);
    CThr thr;
    ftpsXferInfo_t *xferinfo;
    char *tmppath;
    int i;

    if (argc < 2) {
        EventHandler(argc,argv,"'RETR': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

        //allocate the transfer information structure
        //this structure is properly free() by ftpsXferThread()
    if ((xferinfo = (ftpsXferInfo_t *)calloc(sizeof(ftpsXferInfo_t),1)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        return(0);
    }

        //allocate memory for the path
    if ((xferinfo->path = cmdline.CombineArgs(argc,argv)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        free(xferinfo);
        return(0);
    }

        //check the user's permissions for the path
    if (CheckPermissions("RETR",xferinfo->path,"dr") == 0) {
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }

        //check if the path is valid
    if ((tmppath = fsutils.BuildPath(m_userroot,m_cwd,xferinfo->path)) == NULL) {
        EventHandler(argc,argv,"ERROR: unable to set directory path.","550",1,1,0);
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }
        //if the path is not a file
    if ((i = fsutils.ValidatePath(tmppath)) != 2) {
        fsutils.FreePath(tmppath);
        fsutils.CheckSlashUNIX(xferinfo->path); //display the path w/UNIX slashes
        if (i == 1)
            WriteToLineBuffer("%s: Not a plain file.",xferinfo->path);     //path is a dir
        else
            WriteToLineBuffer("%s: No such file or directory.",xferinfo->path);    //path DNE
        EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }
    fsutils.FreePath(tmppath);

        //allocate memory for the current working dir
    if ((xferinfo->cwd = (char *)malloc(strlen(m_cwd)+1)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }
    strcpy(xferinfo->cwd,m_cwd);    //set the current working directory

        //allocate memory for the user's root directory
    if ((xferinfo->userroot = (char *)malloc(strlen(m_userroot)+1)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        free(xferinfo->cwd); cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }
    strcpy(xferinfo->userroot,m_userroot);  //set the user's root directory

        //set the remaining values of the transfer info
    xferinfo->command = 'S';    //indicates data should be sent
    xferinfo->flagpasv = m_flagpasv;
    xferinfo->pasvsd = m_pasvsd;
    strcpy(xferinfo->portaddr,m_portaddr);
    strcpy(xferinfo->portport,m_portport);
    xferinfo->type = m_type;
    xferinfo->restoffset = m_restoffset;
    xferinfo->flagencdata = m_flagencdata;
    xferinfo->pftps = this;
    xferinfo->psiteinfo = m_psiteinfo;

    m_flagabor = 0;     //reset the abort flag
    m_numdatathreads++; //increment the number of data transfer threads
    m_restoffset = 0;   //reset the transfer reset offset

        //create the thread to transfer the file
    if (thr.Create(ftpsXferThread,(void *)xferinfo) == 0) {
            //if unable to create the new thread, clean up.
        EventHandler(argc,argv,"ERROR: unable to create a new transfer thread.","530",1,1,1);
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo->cwd);
        free(xferinfo->userroot); free(xferinfo);
        m_numdatathreads--;
        return(0);
    }

    return(1);
}

int CFtps::DoRMD(int argc, char **argv)
{
    CFSUtils fsutils;
    CCmdLine cmdline(0);
    char *dirname, *tmppath;
    int i, retval = 0;

    if (argc < 2) {
        EventHandler(argc,argv,"'RMD': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

    if ((dirname = cmdline.CombineArgs(argc,argv)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        return(0);
    }

        //check the user's permissions for the path
    if (CheckPermissions("RMD",dirname,"wn") == 0) {
        cmdline.FreeCombineArgs(dirname);
        return(0);
    }

    if ((tmppath = fsutils.BuildPath(m_userroot,m_cwd,dirname)) == NULL) {
        EventHandler(argc,argv,"ERROR: unable to set directory path.","550",1,1,0);
        cmdline.FreeCombineArgs(dirname);
        return(0);
    }

        //do not allow the user to delete the root directory
    if (strcmp(tmppath,m_userroot) == 0) {
        EventHandler(argc,argv,"Cannot delete the root directory.","500",1,1,0);
        fsutils.FreePath(tmppath);
        cmdline.FreeCombineArgs(dirname);
        return(0);
    }

    fsutils.CheckSlashUNIX(dirname);    //display the path w/UNIX slashes

    i = fsutils.ValidatePath(tmppath);
    if (i == 0) {   //if the directory does not exist
        WriteToLineBuffer("%s: No such file of directory.",dirname);
        EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
    } else if (i == 2) {    //if path is a file
        WriteToLineBuffer("%s: Not a directory.",dirname);
        EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
    } else {        //the directory exists
            //check if the directory is empty
        if (fsutils.DirIsEmpty(tmppath) != 0) {
            WriteToLineBuffer("%s: The directory is not empty.",dirname);
            EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
        } else {
            if (fsutils.RmDir(tmppath) != 0) {
                EventHandler(argc,argv,"RMD command successful.","250",1,1,0);
                retval = 1;
            } else {
                WriteToLineBuffer("%s: Unable to remove directory.",dirname);
                EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
            }
        }
    }

    fsutils.FreePath(tmppath);
    cmdline.FreeCombineArgs(dirname);
    return(retval);
}

int CFtps::DoRNFR(int argc, char **argv)
{
    CFSUtils fsutils;
    CCmdLine cmdline(0);
    char *filename, *tmppath;
    int i, retval = 0;

    if (argc < 2) {
        EventHandler(argc,argv,"'RNFR': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

    if ((filename = cmdline.CombineArgs(argc,argv)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        return(0);
    }

        //check the user's permissions for the path
    if (CheckPermissions("RNFR",filename,"wa") == 0) {
        cmdline.FreeCombineArgs(filename);
        return(0);
    }

    if ((tmppath = fsutils.BuildPath(m_userroot,m_cwd,filename)) == NULL) {
        EventHandler(argc,argv,"ERROR: unable to set file path.","550",1,1,0);
        cmdline.FreeCombineArgs(filename);
        return(0);
    }

    fsutils.CheckSlashUNIX(filename);    //display the path w/UNIX slashes

    i = fsutils.ValidatePath(tmppath);
    if (i == 0) {   //if the file/directory does not exist
        WriteToLineBuffer("%s: No such file or directory.",filename);
        EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
        fsutils.FreePath(tmppath);
    } else {
        if (m_renbuffer != NULL)
            free(m_renbuffer);
        m_renbuffer = tmppath;
        EventHandler(argc,argv,"File exists, ready for destination name.","350",1,1,0);
        retval = 1;
    }

    cmdline.FreeCombineArgs(filename);
    return(retval);
}

int CFtps::DoRNTO(int argc, char **argv)
{
    CFSUtils fsutils;
    CCmdLine cmdline(0);
    char *filename, *tmppath;
    int i, retval = 0;

    if (argc < 2) {
        EventHandler(argc,argv,"'RNTO': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

    if (m_renbuffer == NULL) {
        EventHandler(argc,argv,"Bad sequence of commands.","503",1,1,0);
        return(0);
    }

    if ((filename = cmdline.CombineArgs(argc,argv)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        return(0);
    }
    fsutils.RemoveSlashEnd(filename);

        //check the user's permissions for the path
    if (CheckPermissions("RNTO",filename,"wa",1) == 0) {
        cmdline.FreeCombineArgs(filename);
        return(0);
    }

    if ((tmppath = fsutils.BuildPath(m_userroot,m_cwd,filename)) == NULL) {
        EventHandler(argc,argv,"ERROR: unable to set file path.","550",1,1,0);
        cmdline.FreeCombineArgs(filename);
        return(0);
    }

    fsutils.CheckSlashUNIX(filename);    //display the path w/UNIX slashes

    i = fsutils.ValidatePath(tmppath);
    if (i == 0) {   //if the file/directory does not exist
        if (fsutils.Rename(m_renbuffer,tmppath) != 0) {
            EventHandler(argc,argv,"RNTO command successful.","250",1,1,0);
            retval = 1;
        } else {
            WriteToLineBuffer("%s: Unable to rename file.",filename);
            EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
        }
    } else {
        WriteToLineBuffer("%s: Cannot create a file when that file already exists.",filename);
        EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
    }

    free(m_renbuffer);
    m_renbuffer = NULL;

    fsutils.FreePath(tmppath);
    cmdline.FreeCombineArgs(filename);
    return(retval);
}

int CFtps::DoSIZE(int argc, char **argv)
{
    CFSUtils fsutils;
    CCmdLine cmdline(0);
    fsutilsFileInfo_t fileinfo;
    char *filename, *path;
    int retval = 0;
    
    if (argc < 2) {
        EventHandler(argc,argv,"'SIZE': Invalid number of parameters.","500",1,1,0);
        return(0);
    }
    
    if ((filename = cmdline.CombineArgs(argc,argv)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        return(0);
    }
    
    if ((path = fsutils.BuildPath(m_userroot,m_cwd,filename)) == NULL) {
        EventHandler(argc,argv,"ERROR: unable to set directory path.","550",1,1,0);
        cmdline.FreeCombineArgs(filename);
        return(0);
    }
    
    if (fsutils.GetFileStats(path,&fileinfo) != 0) {
        if (fileinfo.mode & S_IFDIR) {
            WriteToLineBuffer("%s: not a plain file.",filename);
            EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
        } else {
            WriteToLineBuffer("%u",fileinfo.size);
            EventHandler(argc,argv,m_linebuffer,"213",1,1,0);
            retval = 1;
        }
    } else {
        WriteToLineBuffer("%s: No such file or directory.",filename);
        EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
    }
    
    fsutils.FreePath(path);
    cmdline.FreeCombineArgs(filename);
    
    return(retval);
}

int CFtps::DoSMNT(int argc, char **argv)
{

    EventHandler(argc,argv,"'SMNT': command not implemented.","504",1,1,0);

    return(1);
}

int CFtps::DoSTAT(int argc, char **argv)
{
    char *filename, datestr[32], buffer[64];
    CFSUtils fsutils;
    CStrUtils strutils;
    CCmdLine cmdline(0);
    CFtpsXfer ftpsxfer;
    ftpsXferInfo_t xferinfo;

    *m_linebuffer = '\0';

    if (argc == 1) {
            //check the user's permissions
        if (CheckPermissions("STAT",NULL,"sx") == 0)
            return(0);
        if (*m_servername != '\0')
            strutils.Append(m_linebuffer,m_servername,sizeof(m_linebuffer));
        else
            strutils.Append(m_linebuffer,m_serverip,sizeof(m_linebuffer));
        strutils.Append(m_linebuffer," FTP server status:",sizeof(m_linebuffer));
        m_ftpscmd.ResponseSend(m_linebuffer,"211",1);
        *m_linebuffer = '\0';
        strutils.Append(m_linebuffer,"Version ",sizeof(m_linebuffer));
        strutils.Append(m_linebuffer,m_psiteinfo->GetProgName(buffer,sizeof(buffer)),sizeof(m_linebuffer));
        strutils.Append(m_linebuffer,"-",sizeof(m_linebuffer));
        strutils.Append(m_linebuffer,m_psiteinfo->GetProgVersion(buffer,sizeof(buffer)),sizeof(m_linebuffer));
        strutils.Append(m_linebuffer," (",sizeof(m_linebuffer));
        strutils.Append(m_linebuffer,m_psiteinfo->GetCoreVersion(buffer,sizeof(buffer)),sizeof(m_linebuffer));
        strutils.Append(m_linebuffer,")",sizeof(m_linebuffer));
        m_psiteinfo->GetTimeString(datestr,sizeof(datestr)," %a %b %d %H:%M:%S %Y",time(NULL));
        strutils.Append(m_linebuffer,datestr,sizeof(m_linebuffer));
        m_ftpscmd.ResponseSend(m_linebuffer,"",2);
        *m_linebuffer = '\0';
        strutils.Append(m_linebuffer,"Connected to user-",sizeof(m_linebuffer));
        strutils.Append(m_linebuffer,m_clientname,sizeof(m_linebuffer));
        strutils.Append(m_linebuffer," (",sizeof(m_linebuffer));
        strutils.Append(m_linebuffer,m_clientip,sizeof(m_linebuffer));
        strutils.Append(m_linebuffer,")",sizeof(m_linebuffer));
        m_ftpscmd.ResponseSend(m_linebuffer,"",2);
        *m_linebuffer = '\0';
        strutils.Append(m_linebuffer,"Logged in as ",sizeof(m_linebuffer));
        strutils.Append(m_linebuffer,m_login,sizeof(m_linebuffer));
        m_ftpscmd.ResponseSend(m_linebuffer,"",2);
        *m_linebuffer = '\0';
        strutils.Append(m_linebuffer,"TYPE: ",sizeof(m_linebuffer));
        strutils.Append(m_linebuffer,(m_type == 'A')?"ASCII, FORM: Nonprint; ":"Image; ",sizeof(m_linebuffer));
        strutils.Append(m_linebuffer,"STRUcture: File; transfer MODE: Stream",sizeof(m_linebuffer));
        m_ftpscmd.ResponseSend(m_linebuffer,"",2);
        *m_linebuffer = '\0';
        if ((m_numlistthreads+m_numdatathreads) == 0)   //if there are any active transfer threads
            strutils.Append(m_linebuffer,"No data connection",sizeof(m_linebuffer));
        else
            strutils.Append(m_linebuffer,"Data transfer in progress",sizeof(m_linebuffer));
        m_ftpscmd.ResponseSend(m_linebuffer,"",2);
        EventHandler(argc,argv,"End of status.","211",1,1,0);
    } else {
            //Do dir listing
        if ((filename = cmdline.CombineArgs(argc,argv)) != NULL) {
            fsutils.RemoveSlashEnd(filename);
                //check the user's permissions
            if (CheckPermissions("STAT",filename,"sx",1) == 0) {
                cmdline.FreeCombineArgs(filename);
                return(0);
            }
                //check the user's permissions for the path
            if (CheckPermissions("STAT",filename,"lx") != 0) {
                strutils.Append(m_linebuffer,"status of ",sizeof(m_linebuffer));
                strutils.Append(m_linebuffer,filename,sizeof(m_linebuffer));
                strutils.Append(m_linebuffer,":",sizeof(m_linebuffer));
                m_ftpscmd.ResponseSend(m_linebuffer,"213",1);
                    //send the dir listing over the control connection
                memset(&xferinfo,0,sizeof(ftpsXferInfo_t));
                xferinfo.path = filename;
                xferinfo.cwd = m_cwd;
                xferinfo.userroot = m_userroot;
                xferinfo.flagpasv = m_flagpasv;
                xferinfo.pasvsd = m_pasvsd;
                xferinfo.flagencdata = m_ftpscmd.GetUseSSL();
                xferinfo.pftps = this;
                xferinfo.psiteinfo = m_psiteinfo;
                m_flagabor = 0; //reset the abort flag
                ftpsxfer.SendListCtrl(&xferinfo);
                EventHandler(argc,argv,"End of status.","213",1,1,0);
            }
            cmdline.FreeCombineArgs(filename);
       } else {
            EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
       }
    }

    return(1);
}

    //mode = 0 --> do normal STOR command
    //mode = 1 --> do STOU (store to unique name)
    //mode = 2 --> do APPE (append to a file --or create)
int CFtps::DoSTOR(int argc, char **argv, int mode /*=0*/)
{
    CFSUtils fsutils;
    CCmdLine cmdline(0);
    CThr thr;
    ftpsXferInfo_t *xferinfo;
    char *buffer, *tmppath;
    int i;

    if (argc < 2) {
        EventHandler(argc,argv,"'STOR': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

        //allocate the transfer information structure
        //this structure is properly free() by ftpsXferThread()
    if ((xferinfo = (ftpsXferInfo_t *)calloc(sizeof(ftpsXferInfo_t),1)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        return(0);
    }

        //allocate memory for the path
    if ((xferinfo->path = cmdline.CombineArgs(argc,argv)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        free(xferinfo);
        return(0);
    }
    fsutils.RemoveSlashEnd(xferinfo->path);

        //check the user's permissions for the path
    if (CheckPermissions("STOR",xferinfo->path,"uw",1) == 0) {
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }

        //check if the path is valid
    if ((buffer = fsutils.BuildPath(m_userroot,m_cwd,xferinfo->path)) == NULL) {
        EventHandler(argc,argv,"ERROR: unable to set directory path.","550",1,1,0);
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }
    if ((tmppath = (char *)malloc(strlen(buffer)+1)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        fsutils.FreePath(buffer); cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);

    }
    fsutils.GetDirPath(tmppath,strlen(buffer),buffer);  //get the directory portion of the path
    fsutils.FreePath(buffer);
        //if the path is not a directory
    if ((i = fsutils.ValidatePath(tmppath)) != 1) {
        free(tmppath);
        fsutils.CheckSlashUNIX(xferinfo->path); //display the path w/UNIX slashes
        WriteToLineBuffer("%s: The system cannot find the path specified.",xferinfo->path); //path DNE
        EventHandler(argc,argv,m_linebuffer,"550",1,1,0);
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }
    free(tmppath);

        //allocate memory for the current working dir
    if ((xferinfo->cwd = (char *)malloc(strlen(m_cwd)+1)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }
    strcpy(xferinfo->cwd,m_cwd);    //set the current working directory

        //allocate memory for the user's root directory
    if ((xferinfo->userroot = (char *)malloc(strlen(m_userroot)+1)) == NULL) {
        EventHandler(argc,argv,"ERROR: out of memory.","451",1,1,1);
        free(xferinfo->cwd); cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo);
        return(0);
    }
    strcpy(xferinfo->userroot,m_userroot);  //set the user's root directory

        //set the remaining values of the transfer info
    xferinfo->command = 'R';    //indicates data should be received
    xferinfo->mode = mode;
    xferinfo->flagpasv = m_flagpasv;
    xferinfo->pasvsd = m_pasvsd;
    strcpy(xferinfo->portaddr,m_portaddr);
    strcpy(xferinfo->portport,m_portport);
    xferinfo->type = m_type;
    xferinfo->restoffset = m_restoffset;
    xferinfo->flagencdata = m_flagencdata;
    xferinfo->pftps = this;
    xferinfo->psiteinfo = m_psiteinfo;

    m_flagabor = 0;     //reset the abort flag
    m_numdatathreads++; //increment the number of data transfer threads
    m_restoffset = 0;   //reset the transfer reset offset

        //create the thread to transfer the file
    if (thr.Create(ftpsXferThread,(void *)xferinfo) == 0) {
            //if unable to create the new thread, clean up.
        EventHandler(argc,argv,"ERROR: unable to create a new transfer thread.","530",1,1,1);
        cmdline.FreeCombineArgs(xferinfo->path); free(xferinfo->cwd);
        free(xferinfo->userroot); free(xferinfo);
        m_numdatathreads--;
        return(0);
    }

    return(1);
}

int CFtps::DoSTOU(int argc, char **argv)
{

    if (argc < 2) {
        EventHandler(argc,argv,"'STOU': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

    return(DoSTOR(argc,argv,1));
}

int CFtps::DoSYST(int argc, char **argv)
{

        //check the user's permissions
    if (CheckPermissions("SYST",NULL,"sx") == 0)
        return(0);

      EventHandler(argc,argv,"WINDOWS","215",1,1,0);

    return(1);
}

int CFtps::DoTYPE(int argc, char **argv)
{
    char strtype[2], *args[2];

    if (argc > 1) {
        if (strcmp(argv[1],"e") == 0 || strcmp(argv[1],"E") == 0) {
            EventHandler(argc,argv,"Type E not implemented.","504",1,1,0);
            return(0);
        }
        if (strcmp(argv[1],"a") == 0 || strcmp(argv[1],"A") == 0)
            m_type = 'A';
        else if (strcmp(argv[1],"i") == 0 || strcmp(argv[1],"I") == 0)
            m_type = 'I';
        else if (strcmp(argv[1],"l") == 0 || strcmp(argv[1],"L") == 0)
            m_type = 'I';   //"L 8" or "L" is the same as "I"
    }

        //as a default leave the current setting.
    sprintf(m_linebuffer,"Type is set to %c.",m_type);
    strtype[0] = m_type; strtype[1] = '\0';
    args[0] = argv[0]; args[1] = strtype;
    EventHandler(2,args,m_linebuffer,"200",1,1,0);

    return(1);


}

int CFtps::DoUSER(int argc, char **argv)
{

    if (argc < 2) {
        EventHandler(argc,argv,"'USER': Invalid number of parameters.","500",1,1,0);
        return(0);
    }

        //store the user name
    strncpy(m_login,argv[1],sizeof(m_login)-1);
    m_login[sizeof(m_login)-1] = '\0';

        //always ask for the password (check authentication in DoPASS())
    WriteToLineBuffer("Password required for %s.",argv[1]);
    EventHandler(argc,argv,m_linebuffer,"331",1,1,0);

    return(1);
}

////////////////////////////////////////
// Functions that should be overwritten
// by a class that inherits from CFtps
////////////////////////////////////////

int CFtps::DoSITE(int argc, char **argv)
{

        //check the user's permissions
    if (CheckPermissions("SITE",NULL,"tx") == 0)
        return(0);
    
    EventHandler(argc,argv,"'SITE': command not implemented.","504",1,1,0);
 
    return(1);
}


////////////////////////////////////////
// Functions used to get FTP server
// state
//
// NOTE: Although returning the internal
//       data pointers in functions such
//       as GetLogin() violates proper 
//       information hiding, it saves
//       memory and complexity that
//       would be needed to constantly
//       create/update temp buffers.
//       Although it would be possible
//       to write to the pointers 
//       returned by the functions, 
//       this is never done in this
//       application.
////////////////////////////////////////

SOCKET CFtps::GetSocketDesc()
{
    return(m_sd);
}

    //this is called when establishing an implicit SSL connection
void CFtps::SetSSLInfo(sslsock_t *sslinfo)
{
    if (sslinfo != NULL) {
        memcpy(&m_sslinfo,sslinfo,sizeof(sslsock_t));
        m_ftpscmd.SetSSLInfo(&m_sslinfo);   //set the SSL info
        m_ftpscmd.SetUseSSL(1);             //set to use SSL
        m_flagencdata = 1;  //default to an encrypted data connection
    }
}

sslsock_t *CFtps::GetSSLInfo()
{
    return(&m_sslinfo);
}

int CFtps::GetUseSSL()
{
    return(m_ftpscmd.GetUseSSL());
}

int CFtps::GetFlagEncData()
{
    return(m_flagencdata);
}

char *CFtps::GetServerName()
{
    return(m_servername);
}

char *CFtps::GetServerIP()
{
    return(m_serverip);
}

char *CFtps::GetServerPort()
{
    return(m_serverport);
}

char *CFtps::GetClientName()
{
    return(m_clientname);
}

char *CFtps::GetClientIP()
{
    return(m_clientip);
}

char *CFtps::GetClientPort()
{
    return(m_clientport);
}

CSiteInfo *CFtps::GetSiteInfoPtr()
{
    return(m_psiteinfo);
}

void CFtps::SetLogin(char *login)
{
    if (login != NULL) {
        strncpy(m_login,login,sizeof(m_login)-1);
        m_login[sizeof(m_login)-1] = '\0';
    } else {
        *m_login = '\0';
    }
}

char *CFtps::GetLogin()
{
    return(m_login);
}

void CFtps::SetUserID(int uid)
{
    m_userid = uid;
}

int CFtps::GetUserID()
{
    return(m_userid);
}

int CFtps::GetFlagPASV()
{
    return(m_flagpasv);
}

void CFtps::SetFlagQuit(int flagquit)
{
    m_flagquit = flagquit;
}

int CFtps::GetFlagQuit()
{
    return(m_flagquit);
}

void CFtps::SetFlagAbor(int flagabor)
{
    m_flagabor = flagabor;
}

int CFtps::GetFlagAbor()
{
    return(m_flagabor);
}

void CFtps::SetNumListThreads(int numlistthreads)
{
    m_numlistthreads = numlistthreads;
}

int CFtps::GetNumListThreads()
{
    return(m_numlistthreads);
}

void CFtps::SetNumDataThreads(int numdatathreads)
{
    m_numdatathreads = numdatathreads;
}

int CFtps::GetNumDataThreads()
{
    return(m_numdatathreads);
}

void CFtps::SetLastActive(long lastactive /*=0*/)
{
    if (lastactive == 0)
        m_lastactive = time(NULL);  //if 0, use the current time
    else
        m_lastactive = lastactive;
}

long CFtps::GetLastActive()
{
    return(m_lastactive);
}

////////////////////////////////////////
// Functions used to write to the log
// files and send responses back to the
// FTP client.
////////////////////////////////////////

    //Handles events based on flagclient, flagaccess, and flagprog.
    //flagclient determines if information will be sent back to the client.
    //flagaccess determines if information will be written to the access log.
    //flagprog determines if information will be written to the program log.
    //respmode specifies what kind of response to send to the user (intermediate or final)
int CFtps::EventHandler(int argc, char **argv, const char *mesg, const char *ftpcode, int flagclient, int flagaccess, int flagprog, int respmode /*=0*/)
{
    CCmdLine cmdline(0);
    const char *cmdptr, *argptr; char empty[] = "";

    if (argc == 0 || argv == NULL) {
        argptr = empty;
        cmdptr = empty;
    } else {
        cmdptr = argv[0];
        if ((argptr = cmdline.CombineArgs(argc,argv)) == NULL)
            argptr = empty;
    }

        //write to the access log
    if (flagaccess != 0 && ftpcode != NULL) {
        m_psiteinfo->WriteToAccessLog(m_serverip,m_serverport,m_clientip,m_clientport,m_login,cmdptr,argptr,ftpcode,mesg);
    }

        //write to the program log
    if (flagprog != 0 && mesg != NULL) {
        m_psiteinfo->WriteToProgLog(FTPS_SUBSYSTEMNAME,"\"%s\" occured in '%s %s'",mesg,cmdptr,argptr);
    }

        //send the message back to the client
    if (flagclient != 0 && mesg != NULL && ftpcode != NULL) {
        m_ftpscmd.ResponseSend(mesg,ftpcode,respmode);
    }

    if (argptr != empty)
        cmdline.FreeCombineArgs(const_cast<char*>(argptr));

    return(1);
}

    //Writes to m_linebuffer without exceeding FTPS_LINEBUFSIZE
    //This function works just like sprintf()
int CFtps::WriteToLineBuffer(const char *lineformat, ...)
{
    va_list parg;
    int nchars;

        //get the argument list
    va_start(parg,lineformat);

        //write the arguments into the buffer
        //for Windows
        nchars = _vsnprintf((char*)m_linebuffer,sizeof(m_linebuffer),lineformat,parg);

    va_end(parg);

        //make sure the string is terminated
    m_linebuffer[sizeof(m_linebuffer)-1] = '\0';

    return(nchars);
}

////////////////////////////////////////
// Static Utility Functions
////////////////////////////////////////

    //Parses the address used with the PORT (and PASV) command
    //NOTE: "rawaddr" is modified by this function
int CFtps::ParsePORTAddr(char *rawaddr, char *addr, int maxaddr, char *port, int maxport)
{
    char *ptr1, *ptr2;
    char h1[4],h2[4],h3[4],h4[4];
    unsigned int p1i,p2i;

    if (rawaddr == NULL || addr == NULL || port == NULL)
        return(0);

    if (maxaddr < 16 || maxport < 6)
        return(0);

        //seek to the first digit
    ptr1 = rawaddr;
    while (isdigit(*ptr1) == 0 && *ptr1 != '\0') ptr1++;

        //extract the IP address
    if ((ptr2 = strchr(ptr1,',')) == NULL)
        return(0);
    *ptr2 = '\0';
    strncpy(h1,ptr1,sizeof(h1)-1);
    h1[sizeof(h1)-1] = '\0';
    ptr1 = ptr2 + 1;
    if ((ptr2 = strchr(ptr1,',')) == NULL)
        return(0);
    *ptr2 = '\0';
    strncpy(h2,ptr1,sizeof(h2)-1);
    h2[sizeof(h2)-1] = '\0';
    ptr1 = ptr2 + 1;
    if ((ptr2 = strchr(ptr1,',')) == NULL)
        return(0);
    *ptr2 = '\0';
    strncpy(h3,ptr1,sizeof(h3)-1);
    h3[sizeof(h3)-1] = '\0';
    ptr1 = ptr2 + 1;
    if ((ptr2 = strchr(ptr1,',')) == NULL)
        return(0);
    *ptr2 = '\0';
    strncpy(h4,ptr1,sizeof(h4)-1);
    h4[sizeof(h4)-1] = '\0';

        //extract the port number
    ptr1 = ptr2 + 1;
    if ((ptr2 = strchr(ptr1,',')) == NULL)
        return(0);
    *ptr2 = '\0';
    p1i = atoi(ptr1);
    ptr1 = ptr2 + 1;
    p2i = atoi(ptr1);

    sprintf(addr,"%s.%s.%s.%s",h1,h2,h3,h4);
    sprintf(port,"%u",p1i*256+p2i);
    return(1);
}

//////////////////////////////////////////////////////////////////////
// Protected Methods
//////////////////////////////////////////////////////////////////////

    //Sends the default response to a FTP command that is not understood
void CFtps::DefaultResp(int argc, char **argv)
{
    CCmdLine cmdline;
    char *argptr, empty[] = "";

    if (argc == 0 || argv == NULL) {
        EventHandler(argc,argv,"command not understood","500",1,1,0);
        return;
    }

    if ((argptr = cmdline.CombineArgs(argc,argv)) == NULL)
        argptr = empty;

    WriteToLineBuffer("'%s %s': command not understood",argv[0],argptr);
    EventHandler(argc,argv,m_linebuffer,"500",1,1,0);

    if (argptr != empty)
        cmdline.FreeCombineArgs(argptr);
}

    //checks the user's permissions, given a path, to see if "pflags" are present.
    //if flagdir != 0, the directory part of the path will be checked if the
    //full path does not exist.
    //path = m_userroot + m_cwd + arg
int CFtps::CheckPermissions(const char *cmd, char *arg, const char *pflags, int flagdir /*=0*/)
{
    CFSUtils fsutils;
    char *path, *args[2];
    int type, argc, retval = 0;

    if (cmd == NULL || pflags == NULL) {
        EventHandler(0,NULL,"ERROR: Invalid parameters while checking permissions.","451",1,1,1);
        return(0);
    }

    args[0] = const_cast<char*>(cmd); args[1] = arg;

    if (arg != NULL) {
        if ((path = fsutils.BuildPath(m_userroot,m_cwd,arg)) != NULL) {
            type = fsutils.ValidatePath(path);
            if (type == 2) {        //path is a file
                fsutils.RemoveSlashEnd(path);       //make sure files don't end in a slash
            } else if (type == 1) { //path is directory
                fsutils.CheckSlashEnd(path,strlen(path)+2); //make sure directories end in a slash
            } else {    //path does not exist
                if (flagdir != 0)   //check the directory part
                    fsutils.GetDirPath(path,strlen(path)+1,path); //get the directory part of the path
            }
                //check the user's permissions for the path
            if (fsutils.ValidatePath(path) != 0)
                retval = m_psiteinfo->CheckPermissions(m_login,path,pflags);
            else
                retval = 1; //return success to let the calling function send the response
            fsutils.FreePath(path);
        } else {
            EventHandler(2,args,"ERROR: out of memory.","451",1,1,1);
            return(0);
        }
        argc = 2;
    } else {
            //check the user's permissions (not path specific)
        retval = m_psiteinfo->CheckPermissions(m_login,NULL,pflags);
        argc = 1;
    }

        //print the permission denied message on failure
    if (retval == 0) {
        WriteToLineBuffer("%s: Permission denied.",(arg != NULL) ? arg : "");
        fsutils.CheckSlashUNIX(m_linebuffer);  //display UNIX style paths
        EventHandler(argc,args,m_linebuffer,"550",1,1,0);
    }

    return(retval);
}
