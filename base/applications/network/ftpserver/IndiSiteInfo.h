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
// IndiSiteInfo.h: interface for the CIndiSiteInfo class.
//
//////////////////////////////////////////////////////////////////////
#ifndef INDISITEINFO_H
#define INDISITEINFO_H

#include "../core/SiteInfo.h"

#include "../core/Dll.h"

#define INDISITEINFO_MAXBINDATTEMPTS    3  //max number of ports to attempt to bind to

#define INDISITEINFO_LOGLEVELNONE       0  //do not log anything
#define INDISITEINFO_LOGLEVELREDDISPLAY 1  //display the reduced logs to the screen only
#define INDISITEINFO_LOGLEVELDISPLAY    2  //only display the logs on the screen
#define INDISITEINFO_LOGLEVELREDUCED    3  //only write logins and transfers to the log file
#define INDISITEINFO_LOGLEVELNORMAL     4  //only write to the log file
#define INDISITEINFO_LOGLEVELREDDEBUG   5  //write reduced logs to the screen also
#define INDISITEINFO_LOGLEVELDEBUG      6  //write to the log and the screen

#define INDISITEINFO_TEMPPRIVPW      "tmppass"

typedef struct {
    siteinfoPWInfo_t pwinfo;    //password info struct for the user
    char permissions[16];       //user's permissions (as a string of flags)
} indisiteinfoUserProp_t;

typedef struct {
    unsigned short port;    //port number
    int sockdesc;           //SOCK_INVALID -> this port is not in use
} indisiteinfoPasvPort_t;


class CIndiSiteInfo : public CSiteInfo
{
public:
    CIndiSiteInfo(char *progname, char *progversion, char *coreversion, char *sitedefroot = NULL);
    virtual ~CIndiSiteInfo();

    int InitSSLInfo(); //create certificate and keys

    int LoadUsers(const char *filepath);
    void ClearUsers();
    int AddUser(indisiteinfoUserProp_t *puser);
    int CheckUserProp(char *username);

    siteinfoPWInfo_t *GetPWInfo(char *username);
    //use SiteInfo::FreePWInfo(siteinfoPWInfo_t *pwinfo);

    int CheckPermissions(char *username, char *path, char *flags);

    void SetProgLoggingLevel(int level);
    void SetAccessLoggingLevel(int level);
    int WriteToAccessLog(char *sip, char *sp, char *cip, char *cp, char *user, char *cmd, char *arg, char *status, char *text);

    int SetDataPortRange(unsigned short startport, unsigned short endport);
    int GetDataPort(char *bindport, int maxportlen, int *sdptr, char *bindip);
    int FreetDataPort(int sd);

    char *GetPrivKey(int *privkeysize);
    //use SiteInfo::FreePrivKey(char *privkeybuf);
    char *GetCert(int *certsize);
    //use SiteInfo::FreeCert(char *certbuf);
    int GetPrivKeyPW(char *pwbuff, int maxpwbuff);

private:
        //mutex for the user properties list (m_userlist)
    thrSync_t m_mutexusers;
        //Mutex for PASV port binding list (m_pasvlist)
    thrSync_t m_mutexpasvbind;

    CDll m_dll;         //contains doubly-linked list functions

    dll_t *m_userlist;  //header of the linked list of user properties

    int m_indiaccessloglevel;   //access log level

        //list of PASV ports available
    indisiteinfoPasvPort_t *m_pasvlist;
        //used to specify the range of ports that can be used in passive mode
    unsigned short m_pasvstart;
    unsigned short m_pasvend;

        //stores the keys/certificates used for SSL connections
    char *m_privkey;    //buffer that stores the private key
    int m_privkeysize;  //size of the private key
    char *m_pubkey;     //buffer that stores the public key
    int m_pubkeysize;   //size of the public key
    char *m_cert;       //buffer that stores the certificate
    int m_certsize;     //size of the certificate
};

#endif //INDISITEINFO_H
