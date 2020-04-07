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
// IndiSiteInfo.cpp: implementation of the CIndiSiteInfo class.
//
// This is the SiteInfo class for the Indi server.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/StrUtils.h"
#include "../core/CmdLine.h"
#include "../core/FSUtils.h"
#include "../core/Sock.h"
#include "../core/SSLSock.h"
#include "IndiFileUtils.h"
#include "IndiSiteInfo.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIndiSiteInfo::CIndiSiteInfo(char *progname, char *progversion, char *coreversion, char *sitedefroot /*=NULL*/)
:CSiteInfo(progname, progversion, coreversion, sitedefroot)
{

    m_userlist = NULL;  //init the user properties list

        //Initialize the logging levels
    m_indiaccessloglevel = INDISITEINFO_LOGLEVELNONE;
    CSiteInfo::SetAccessLoggingLevel(SITEINFO_LOGLEVELNONE);
    CSiteInfo::SetProgLoggingLevel(SITEINFO_LOGLEVELNONE);

        //Initialize the list of PASV ports available
    m_pasvlist = NULL;  //indicates any port can be used
        //Set the PASV port range
    m_pasvstart = 0;
    m_pasvend = 0;      //indicates any port can be used

        //initialize the SSL information
    m_privkey = NULL;    //buffer that stores the private key
    m_privkeysize = 0;   //size of the private key
    m_pubkey = NULL;     //buffer that stores the public key
    m_pubkeysize = 0;    //size of the public key
    m_cert = NULL;       //buffer that stores the certificate
    m_certsize = 0;      //size of the certificate

        //Initialize mutexes
    m_thr.InitializeCritSec(&m_mutexusers);
    m_thr.InitializeCritSec(&m_mutexpasvbind);
}

CIndiSiteInfo::~CIndiSiteInfo()
{
    CSSLSock sslsock;

        //free the user information list if necessary
    if (m_userlist != NULL)
        m_dll.Destroy(m_userlist,1);

        //free the list of PASV ports if necessary
    if (m_pasvlist != NULL)
        free(m_pasvlist);

        //free the SSL information if necessary
    if (m_privkey != NULL)
        sslsock.FreeKeyMem(m_privkey);
    if (m_pubkey != NULL)
        sslsock.FreeKeyMem(m_pubkey);
    if (m_cert != NULL)
        sslsock.FreeX509Mem(m_cert);

        //destroy mutexes
    m_thr.DestroyCritSec(&m_mutexusers);
    m_thr.DestroyCritSec(&m_mutexpasvbind);
}


//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

////////////////////////////////////////
// Initialize the certificate and keys
// for the SSL connection
////////////////////////////////////////

int CIndiSiteInfo::InitSSLInfo()
{
    CSock sock;
    CSSLSock sslsock;
    CStrUtils strutils;
    sslsockCertInfo_t certinfo;
    
    if (m_cert != NULL || m_privkey != NULL || m_pubkey != NULL)
        return(1);  //InitSSLInfo() was already called

        //load the information for the x509 certificate
    sock.GetMyIPAddress(certinfo.commonname,sizeof(certinfo.commonname));
    strutils.SNPrintf(certinfo.country,sizeof(certinfo.country),"XX");
    strutils.SNPrintf(certinfo.emailaddr,sizeof(certinfo.emailaddr),"none@none.com");
    strutils.SNPrintf(certinfo.locality,sizeof(certinfo.locality),"None");
    strutils.SNPrintf(certinfo.organization,sizeof(certinfo.organization),"IndiFTPD");
    strutils.SNPrintf(certinfo.orgunit,sizeof(certinfo.orgunit),"None");
    strutils.SNPrintf(certinfo.state,sizeof(certinfo.state),"None");

        //Generate the private and public keys
    sslsock.GenKeysMem(INDISITEINFO_TEMPPRIVPW,&m_privkey,&m_privkeysize,&m_pubkey,&m_pubkeysize);

        //Generate the x509 certificate
    m_cert = sslsock.GenX509Mem(&m_certsize,m_privkey,m_privkeysize,INDISITEINFO_TEMPPRIVPW,&certinfo);

    if (m_cert == NULL || m_privkey == NULL || m_pubkey == NULL)
        return(0);
    else
        return(1);
}

////////////////////////////////////////
// Functions used to load/read the user
// information
////////////////////////////////////////

    //Loads all the users from the file "filepath" into the user list
    //in memory.
int CIndiSiteInfo::LoadUsers(const char *filepath)
{
    CCmdLine cmdline;
    CStrUtils strutils;
    CIndiFileUtils fileutils;
    indisiteinfoUserProp_t puserprop;
    int argc, nusers = 0;
    char **argv;
    FILE *fdr;

    if ((fdr = fopen(filepath,"r")) == NULL) {
        WriteToProgLog("MAIN","WARNING: unable to open file \"%s\" for reading.",filepath);
        return(0);
    }

    while (fgets(cmdline.m_cmdline,cmdline.GetCmdLineSize(),fdr) != NULL) {
            //strip off the '\r' and/or '\n'
        strutils.RemoveEOL(cmdline.m_cmdline);
            //remove the commented parts of the line
        strutils.RemoveComments(cmdline.m_cmdline,INDIFILEUTILS_COMMENTSTRING);
            //parse the command line
        argv = cmdline.ParseCmdLine(&argc,INDIFILEUTILS_USERFILEDELIMITER);
            //Parse the line of the user information file
        if (fileutils.ParseUserInfo(argc,argv,&puserprop) != 0) {
            if (AddUser(&puserprop) != 0)
                nusers++;   //increment the number of users added to memory
        }
    }
    
    fclose(fdr);

    return(nusers);
}

    //Clears the user list in memory
void CIndiSiteInfo::ClearUsers()
{

    m_thr.P(&m_mutexusers); //enter the critical section for the user list

    if (m_userlist != NULL) {
        m_dll.Destroy(m_userlist,1);
        m_userlist = NULL;
    }

    m_thr.V(&m_mutexusers); //exit the critical section
}

    //Adds a user to the user list in memory
int CIndiSiteInfo::AddUser(indisiteinfoUserProp_t *puser)
{
    int retval = 0;
    indisiteinfoUserProp_t *puserprop;
    unsigned key;

    m_thr.P(&m_mutexusers); //enter the critical section for the user list

        //create a new user list if necessary
    if (m_userlist == NULL) {
        if ((m_userlist = m_dll.Create(NULL,0,"User List Header")) == NULL) {
            m_thr.V(&m_mutexusers); //exit the critical section
            return(0);
        }
    }

        //initialize the linked list index (key)
    key = m_dll.GetGreatestKey(m_userlist) + 1;

        //check if the user has already been added
    if (m_dll.Search(m_userlist,(unsigned)-1,puser->pwinfo.username,1) == NULL) {
        if ((puserprop = (indisiteinfoUserProp_t *)malloc(sizeof(indisiteinfoUserProp_t))) != NULL) {
            memcpy(puserprop,puser,sizeof(indisiteinfoUserProp_t));
                //add the user to the userlist
            if (m_dll.Create(m_userlist,key,puser->pwinfo.username,(void *)puserprop) != NULL) {
                retval = 1;     //return success
            } else {
                free(puserprop);
            }
        }
    }
    
    m_thr.V(&m_mutexusers); //exit the critical section

    return(retval);
}

int CIndiSiteInfo::CheckUserProp(char *username)
{
    int len, retval = 0;
    dll_t *node;

    m_thr.P(&m_mutexusers); //enter the critical section for the user list
    
    if (m_userlist == NULL) {
        m_thr.V(&m_mutexusers); //exit the critical section
        return(0);
    }

    if ((node = m_dll.Search(m_userlist,(unsigned)-1,username,1)) != NULL) {
            //make sure the "username" matches the name in memory
        len = strlen(username);
        strncpy(username,node->name,len);
        retval = 1;     //the user was found on the userlist
    }

    m_thr.V(&m_mutexusers); //exit the critical section

    return(retval);
}

////////////////////////////////////////
// Functions used to access the password
// information
////////////////////////////////////////

    //Get the password information for the user.
    //Rteurn NULL on error
siteinfoPWInfo_t *CIndiSiteInfo::GetPWInfo(char *username)
{
    siteinfoPWInfo_t *pwinfo = NULL;
    dll_t *node;

    m_thr.P(&m_mutexusers); //enter the critical section for the user list

    if (m_userlist == NULL) {
        m_thr.V(&m_mutexusers); //exit the critical section
        return(0);
    }
    
    if ((node = m_dll.Search(m_userlist,(unsigned)-1,username,1)) != NULL) {
            //the user was found on the userlist
        if ((pwinfo = (siteinfoPWInfo_t *)malloc(sizeof(siteinfoPWInfo_t))) != NULL) {
            memcpy(pwinfo,node->genptr,sizeof(siteinfoPWInfo_t));
        }
    }
    
    m_thr.V(&m_mutexusers); //exit the critical section

    return(pwinfo);
}

////////////////////////////////////////
// Functions used to check the user's
// permissions.
////////////////////////////////////////

    //Returns 1 if any of the "flags" are present in the user's permissions
int CIndiSiteInfo::CheckPermissions(char *username, char *path, char *flags)
{
    indisiteinfoUserProp_t *puserprop;
    char *ptr;
    dll_t *node;
    int retval = 0;

    m_thr.P(&m_mutexusers); //enter the critical section for the user list

    if (username == NULL || flags == NULL || m_userlist == NULL) {
        m_thr.V(&m_mutexusers); //exit the critical section
        return(0);
    }
    
    if ((node = m_dll.Search(m_userlist,(unsigned)-1,username,1)) != NULL) {
        puserprop = (indisiteinfoUserProp_t *)(node->genptr);
        for (ptr = flags; *ptr != '\0'; ptr++) {
            if (strchr(puserprop->permissions,*ptr) != NULL) {
                retval = 1;
                break;
            }
        }
    }

    m_thr.V(&m_mutexusers); //exit the critical section

    return(retval);
}

////////////////////////////////////////
// Functions used for logging
////////////////////////////////////////

    //Translate between the INDISITEINFO and SITEINFO log levels for the program log.
    //There is no need to store the logging level within this class because the
    //program log is never filtered.
void CIndiSiteInfo::SetProgLoggingLevel(int level)
{

    if (level == INDISITEINFO_LOGLEVELNONE) {
        CSiteInfo::SetProgLoggingLevel(SITEINFO_LOGLEVELNONE);
    } else if (level == INDISITEINFO_LOGLEVELDISPLAY || level == INDISITEINFO_LOGLEVELREDDISPLAY) {
        CSiteInfo::SetProgLoggingLevel(SITEINFO_LOGLEVELDISPLAY);
    } else if (level == INDISITEINFO_LOGLEVELREDUCED || level == INDISITEINFO_LOGLEVELNORMAL) {
        CSiteInfo::SetProgLoggingLevel(SITEINFO_LOGLEVELNORMAL);
    } else if (level == INDISITEINFO_LOGLEVELREDDEBUG || level == INDISITEINFO_LOGLEVELDEBUG) {
        CSiteInfo::SetProgLoggingLevel(SITEINFO_LOGLEVELDEBUG);
    }
}

    //Translate between the INDISITEINFO and SITEINFO log levels for the access log.
    //The current access log level is also stored to apply the filtering needed
    //for the "reduced" logging levels.
void CIndiSiteInfo::SetAccessLoggingLevel(int level)
{

    if (level == INDISITEINFO_LOGLEVELNONE) {
        CSiteInfo::SetAccessLoggingLevel(SITEINFO_LOGLEVELNONE);
        m_indiaccessloglevel = level;
    } else if (level == INDISITEINFO_LOGLEVELDISPLAY || level == INDISITEINFO_LOGLEVELREDDISPLAY) {
        CSiteInfo::SetAccessLoggingLevel(SITEINFO_LOGLEVELDISPLAY);
        m_indiaccessloglevel = level;
    } else if (level == INDISITEINFO_LOGLEVELREDUCED || level == INDISITEINFO_LOGLEVELNORMAL) {
        CSiteInfo::SetAccessLoggingLevel(SITEINFO_LOGLEVELNORMAL);
        m_indiaccessloglevel = level;
    } else if (level == INDISITEINFO_LOGLEVELREDDEBUG || level == INDISITEINFO_LOGLEVELDEBUG) {
        CSiteInfo::SetAccessLoggingLevel(SITEINFO_LOGLEVELDEBUG);
        m_indiaccessloglevel = level;
    }
}

    //Apply the filtering for the "reduced" logging levels
int CIndiSiteInfo::WriteToAccessLog(char *sip, char *sp, char *cip, char *cp, char *user, char *cmd, char *arg, char *status, char *text)
{
    CStrUtils strutils;
    int i, retval = 0;
    char reducedcmds[][20] = {"CONNECT","DISCONNECT","USER","PASS","RETR","STOR","ABOR","QUIT",""};

        //Add the filtering for the reduced log formats
    if (m_indiaccessloglevel == INDISITEINFO_LOGLEVELREDDISPLAY ||
        m_indiaccessloglevel == INDISITEINFO_LOGLEVELREDUCED    ||
        m_indiaccessloglevel == INDISITEINFO_LOGLEVELREDDEBUG) {
        for (i = 0; *reducedcmds[i] != '\0'; i++) {
            if (strutils.CaseCmp(cmd,reducedcmds[i]) == 0) {
                retval = CSiteInfo::WriteToAccessLog(sip,sp,cip,cp,user,cmd,arg,status,text);
                break;
            }
        }
    } else {
        retval = CSiteInfo::WriteToAccessLog(sip,sp,cip,cp,user,cmd,arg,status,text);
    }
    
    return(retval);
}

////////////////////////////////////////
// Functions used to specify/limit the
// range of ports used in PASV mode
////////////////////////////////////////

    //Used to specify the range of ports that can be used for data connections
    //when the client is using PASV mode.
int CIndiSiteInfo::SetDataPortRange(unsigned short startport, unsigned short endport)
{
    int i, nports;

    if (startport > endport)
        return(0);  //invalid range

    m_thr.P(&m_mutexpasvbind);  //enter the critical section for PASV port binding

    m_pasvstart = startport;
    m_pasvend = endport;

    nports = endport - startport + 1;

        //free the old list
    if (m_pasvlist != NULL)
        free(m_pasvlist);

        //allocate the new list
    if ((m_pasvlist = (indisiteinfoPasvPort_t *)malloc(nports*sizeof(indisiteinfoPasvPort_t))) == NULL) {
        m_thr.V(&m_mutexpasvbind);  //exit the critical section
        return(0);
    }

        //load the port numbers backwards so the highest ports will be tried first
    for (i = 0; i < nports; i++) {
        m_pasvlist[i].port = endport - i;
        m_pasvlist[i].sockdesc = SOCK_INVALID;  //not in use
    }

    m_thr.V(&m_mutexpasvbind);  //exit the critical section

    return(1);
}

    //Gets an available port from the list of PASV ports 
    //Returns 1 when an available port was successfully found
    //Returns 0 when an available port could not be found
int CIndiSiteInfo::GetDataPort(char *bindport, int maxportlen, int *sdptr, char *bindip)
{
    int i, retval = 0, nattempts = 0, nports;
    CSock sock;

    if (bindport == NULL || maxportlen == 0 || sdptr == NULL)
        return(0);

    *bindport = '\0';
    *sdptr = SOCK_INVALID;

    if (m_pasvlist == NULL) {
            //Let the OS pick the port
        if ((*sdptr = sock.BindServer(bindport,maxportlen,bindip)) == SOCK_INVALID)
            return(0);
        else
            return(1);
    }

    m_thr.P(&m_mutexpasvbind);  //enter the critical section for PASV port binding

    nports = m_pasvend - m_pasvstart + 1;

    for (i = 0; i < nports; i++) {
        if (m_pasvlist[i].sockdesc == SOCK_INVALID) {   //if the port is not in use
            if (nattempts < INDISITEINFO_MAXBINDATTEMPTS) {
                if ((*sdptr = sock.BindServer(bindport,maxportlen,bindip,m_pasvlist[i].port)) == SOCK_INVALID) {
                    nattempts++;    //increment the number of attempted bindings
                } else {
                    retval = 1; //a port was successfully binded to
                    m_pasvlist[i].sockdesc = *sdptr;
                    break;
                }
            } else {
                break;
            }
        }
    }

    m_thr.V(&m_mutexpasvbind);  //exit the critical section

    return(retval);
}

    //Frees the data port that is allocated for "sd" in GetDataPort()
int CIndiSiteInfo::FreetDataPort(int sd)
{
    int i, nports, retval = 0;
    CSock sock;

    if (m_pasvlist == NULL) {
        sock.Close(sd); //close the socket desc
        return(1);
    }

    m_thr.P(&m_mutexpasvbind);  //enter the critical section for PASV port binding

    nports = m_pasvend - m_pasvstart + 1;

    for (i = 0; i < nports; i++) {
        if (m_pasvlist[i].sockdesc == sd) {
            sock.Close(sd); //close the socket desc
            m_pasvlist[i].sockdesc = SOCK_INVALID; //free the port
            retval = 1;
            break;
        }
    }

    m_thr.V(&m_mutexpasvbind);  //exit the critical section
    
    return(retval);
}

////////////////////////////////////////
// SSL key/certificate functions
////////////////////////////////////////

    //returns a buffer containing the PEM encrypted private key
char *CIndiSiteInfo::GetPrivKey(int *privkeysize)
{
    char *privkeybuf;

    if (m_privkey == NULL)
        return(NULL);

    if ((privkeybuf = (char *)calloc(m_privkeysize+1,1)) == NULL)
        return(NULL);
    memcpy(privkeybuf,m_privkey,m_privkeysize);

    if (privkeysize != NULL)
        *privkeysize = m_privkeysize;

    return(privkeybuf);
}

    //returns a buffer containing the PEM x509 certificate
char *CIndiSiteInfo::GetCert(int *certsize)
{
    char *certbuf;

    if (m_cert == NULL)
        return(NULL);

    if ((certbuf = (char *)calloc(m_certsize+1,1)) == NULL)
        return(NULL);
    memcpy(certbuf,m_cert,m_certsize);

    if (certsize != NULL)
        *certsize = m_certsize;

    return(certbuf);
}

    //gets the password to use with the private key
int CIndiSiteInfo::GetPrivKeyPW(char *pwbuff, int maxpwbuff)
{

    if (pwbuff == NULL || maxpwbuff == 0)
        return(0);
    *pwbuff = '\0';

    strncpy(pwbuff,INDISITEINFO_TEMPPRIVPW,maxpwbuff-1);
    pwbuff[maxpwbuff-1] = '\0';

    return(1);
}


//////////////////////////////////////////////////////////////////////
// Private Methods
//////////////////////////////////////////////////////////////////////
