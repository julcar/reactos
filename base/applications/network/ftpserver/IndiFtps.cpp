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
// IndiFtps.cpp: implementation of the CIndiFtps class.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../core/StrUtils.h"
#include "IndiFtps.h"
#include "IndiSiteInfo.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIndiFtps::CIndiFtps(SOCKET sd, CSiteInfo *psiteinfo, char *serverip /*=NULL*/, int resolvedns /*=1*/)
:CFtps(sd,psiteinfo,serverip,resolvedns)
{

    m_pindisiteinfo = (CIndiSiteInfo *)psiteinfo;
}

CIndiFtps::~CIndiFtps()
{

}

//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////


////////////////////////////////////////
// Overridded methods from CFtps
////////////////////////////////////////

int CIndiFtps::DoSITE(int argc, char **argv)
{
    int retval = 0;     //Stores the return value of the FTP function
    CStrUtils strutils; //String utilities class

        //check the user's permissions
    if (CheckPermissions("SITE",NULL,"tx") == 0)
        return(0);

    if (argc <= 1) {
        EventHandler(argc,argv,"'SITE': command not understood.","500",1,1,0);
        return(1);
    }

    switch (*(argv[1])) {

        case 'V': case 'v': {
            if (strutils.CaseCmp(argv[1],"VERS") == 0) {
                retval = DoSiteVERS(argc,argv);
            } else {
                DefaultResp(argc,argv);
            }
        } break;

        default: {
            DefaultResp(argc,argv);
        } break;

    } // end switch

    return(retval);
}

////////////////////////////////////////
// SITE functions
//
// Implementations of the SITE commands
////////////////////////////////////////

int CIndiFtps::DoSiteVERS(int argc, char **argv)
{
    char progname[32], progver[32], corever[32];
    #ifdef WIN32
      char platform[] = "WINDOWS";
    #else
      char platform[] = "UNIX";
    #endif

        //build the response line (stored in m_linebuffer)
    WriteToLineBuffer("%s %s (%s) %s.",m_pindisiteinfo->GetProgName(progname,sizeof(progname)),
                                      m_pindisiteinfo->GetProgVersion(progver,sizeof(progver)),
                                      m_pindisiteinfo->GetCoreVersion(corever,sizeof(corever)),
                                      platform);

        //send the response to the client
    EventHandler(argc,argv,m_linebuffer,"200",1,1,0);

    return(1);
}


//////////////////////////////////////////////////////////////////////
// Protected Methods
//////////////////////////////////////////////////////////////////////
