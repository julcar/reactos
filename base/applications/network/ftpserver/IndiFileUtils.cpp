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
// IndiFileUtils.cpp: implementation of the CIndiFileUtils class.
//
// This is the SiteInfo class for the Indi server.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>

#include "../core/FSUtils.h"
#include "IndiFileUtils.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIndiFileUtils::CIndiFileUtils()
{
}

CIndiFileUtils::~CIndiFileUtils()
{
}


//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

    //Adds a user line to the user information file
int CIndiFileUtils::AddUserLine(const char *filepath, const indisiteinfoUserProp_t *puserprop)
{
    FILE *fda;
    char d = INDIFILEUTILS_USERFILEDELIMITER;

    if (filepath == NULL || puserprop == NULL)
        return(0);

    if ((fda = fopen(filepath,"at+")) == NULL) {
        printf("WARNING: unable to write to file %s.\n",filepath);
        return(0);
    }

    fprintf(fda,"%s%c%s%c%s%c%s\n",puserprop->pwinfo.username,d,puserprop->pwinfo.passwordraw
                                  ,d,puserprop->pwinfo.homedir,d,puserprop->permissions);

    fclose(fda);

    return(1);
}

    //parses the user line into the indisiteinfoUserProp_t structure
int CIndiFileUtils::ParseUserInfo(int argc, char **argv, indisiteinfoUserProp_t *puserprop)
{
    CFSUtils fsutils;

    if (argv == NULL || puserprop == NULL)
        return(0);

    if (argc >= 4) {
        memset(puserprop,0,sizeof(indisiteinfoUserProp_t));
            //set the username
        strncpy(puserprop->pwinfo.username,argv[0],sizeof(puserprop->pwinfo.username)-1);
        puserprop->pwinfo.username[sizeof(puserprop->pwinfo.username)-1] = '\0';
            //set the password
        strncpy(puserprop->pwinfo.passwordraw,argv[1],sizeof(puserprop->pwinfo.passwordraw)-1);
        puserprop->pwinfo.passwordraw[sizeof(puserprop->pwinfo.passwordraw)-1] = '\0';
            //set the home directory for the user
        strncpy(puserprop->pwinfo.homedir,argv[2],sizeof(puserprop->pwinfo.homedir)-2);
        puserprop->pwinfo.homedir[sizeof(puserprop->pwinfo.homedir)-2] = '\0';
        fsutils.CheckSlash(puserprop->pwinfo.homedir);
        fsutils.CheckSlashEnd(puserprop->pwinfo.homedir,sizeof(puserprop->pwinfo.homedir));
            //make sure the specified homedir is a valid directory
        if (fsutils.ValidatePath(puserprop->pwinfo.homedir) != 1) {
            printf("WARNING: invalid home directory \"%s\" for user %s.\n",
                    puserprop->pwinfo.homedir,puserprop->pwinfo.username);
            return(0);
        }
            //set the permissions
        strncpy(puserprop->permissions,argv[3],sizeof(puserprop->permissions)-1);
        puserprop->permissions[sizeof(puserprop->permissions)-1] = '\0';
        return(1);
    } else {
        return(0);
    }
}
