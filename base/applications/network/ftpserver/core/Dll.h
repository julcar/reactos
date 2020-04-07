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
// Dll.h: interface for the CDll class.
//
// DLL:  ADT for Doubly-Linked List 
//
// Note: Assumption that "key" is a unique non-negative number.
//       The "key" of a header node must always be 0.
//
//////////////////////////////////////////////////////////////////////
#ifndef DLL_H
#define DLL_H

	//Define Structure of Node
typedef struct _dll_t {
    struct _dll_t *prev;    //pointer to previous element in list
    struct _dll_t *next;    //pointer to next element in list */
    struct _dll_t *list;    //pointer to header node of list
    unsigned key;           //unique key
    char *name;             //symbolic name for node
    void *genptr;           //pointer to generic data
} dll_t;


class CDll  
{
public:
    CDll();
    virtual ~CDll();

        //Add/Remove nodes or lists
    dll_t *Create(dll_t *list, unsigned key, const char *symname, void *genptr = NULL);
    void Destroy(dll_t *node, int freegenptr = 0);
    void Reset(dll_t *list, int freegenptr = 0);
    void Insert(dll_t *list, dll_t *node); 			
    void Remove(dll_t *node);

        //Functions for searching the liked list.
    dll_t *Search(dll_t *list, unsigned key, const char *name, int flagcase = 0);

        //Print the key and symname of each element in list
    dll_t *ListPrint(dll_t *list);    //don't need (FOR DEBUG)

        //Misc utility functions
    dll_t *NodeNext(dll_t *node);
    dll_t *NodePrev(dll_t *node);
    unsigned GetGreatestKey(dll_t *list);
    unsigned GetListLength(dll_t *list);
};

#endif //DLL_H
