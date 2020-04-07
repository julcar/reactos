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
// IndiFtps.h: interface for the CIndiFtps class.
//
//////////////////////////////////////////////////////////////////////
#ifndef INDIFTPS_H
#define INDIFTPS_H

#include "../core/Ftps.h"

class CIndiSiteInfo;


///////////////////////////////////////////////////////////////////////////////
// IndiFTPD specific implementations/extensions
///////////////////////////////////////////////////////////////////////////////

class CIndiFtps : public CFtps
{
public:
    CIndiFtps(SOCKET sd, CSiteInfo *psiteinfo, char *serverip = NULL, int resolvedns = 1);
    virtual ~CIndiFtps();

        //Overridded methods from CFtps
    virtual int DoSITE(int argc, char **argv);

        //SITE commands
    virtual int DoSiteVERS(int argc, char **argv);

protected:
        //pointer to the class containing the IndiFTPD site information
    CIndiSiteInfo *m_pindisiteinfo;
};

#endif //INDIFTPS_H
