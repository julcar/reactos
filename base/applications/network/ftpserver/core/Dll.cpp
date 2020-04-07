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
// Dll.cpp: implementation of the CDll class.
//
// ADT for Doubly-Linked List 
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>  //for toupper

#include "Dll.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDll::CDll()
{

}

CDll::~CDll()
{

}

//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

////////////////////////////////////////
// Add/Remove nodes or lists.
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Create a new node or list.  If "list" != NULL, add node to "list".
//
// [in] list    : Pointer to the list to add a node to.  If NULL
//                create a new list.
// [in] key     : Key (ID) to assign to the new node.  The key for the
//                header node in a new list must be 0 ("key" must be 0
//                when "list" = NULL).
// [in] symname : Symbolic name for the new node.
// [in] genptr  : Data pointer for the node.  This will usually be a
//                pointer to a previously malloc() structure.
//
// Return : On success a pointer to the new node or list is returned.
//          On failure NULL is returned.
//
dll_t *CDll::Create(dll_t *list, unsigned key, const char *symname, void *genptr /*=NULL*/)
{
    dll_t *node;
    
    if ((node = (dll_t *)malloc(sizeof(dll_t))) == NULL)
        return(NULL);

    node->key = key;    //store key

    if (symname != NULL) {
        if ((node->name = (char *)malloc(strlen(symname)+1)) == NULL) {
            free(node);
            return(NULL);
        }
        strcpy(node->name,symname);     //store symbolic name
    } else {
        node->name = NULL;
    }

    node->genptr = genptr;          //store pointer to data

    if (list == NULL) {
            //not on any list yet, so this node is a list of its own
        node->list = node; 
        node->prev = node;
        node->next = node;
    } else {
        Insert(list,node);
    }

    return(node);
}

//////////////////////////////////////////////////////////////////////
// Destroy a node (free memory used by it too).
// If "freegenptr" != 0, the "genptr" will be free() if not NULL
//
// [in] node       : Pointer to the list/node to destroy.
// [in] freegenptr : If != 0, free() will be called for 
//                   "node->genptr".
//
// Return : VOID
//
void CDll::Destroy(dll_t *node, int freegenptr /*=0*/)
{

    if (node->list == node) {
        dll_t *head = node;
            //destroy entire list
        node = node->next;
        while (node != head) {
                //free the data pointer if it is not NULL
            if (freegenptr != 0 && node->genptr != NULL)
                free(node->genptr);
                //move the the next node and free the current node
            if (node->name != NULL) free(node->name);   //free the symbolic name
            node = node->next;
            free(node->prev);
        }
            //free the head data pointer if it is not NULL
        if (freegenptr != 0 && head->genptr != NULL)
            free(head->genptr);
        if (head->name != NULL) free(head->name);
        free(head);
    } else {
            //destroy node only; first removing it from list
        Remove(node);
            //free the data pointer if it is not NULL
        if (freegenptr != 0 && node->genptr != NULL)
            free(node->genptr);
        if (node->name != NULL) free(node->name);   //free the symbolic name
        free(node);         //free the current node
    }
}

//////////////////////////////////////////////////////////////////////
// Reset the list (i.e. destroy all nodes except the list header 
// node).  If "freegenptr" != 0, the "genptr" will be free() if not
// NULL.
//
// [in] list       : Pointer to the list to reset.
// [in] freegenptr : If != 0, free() will be called for "node->genptr"
//                   for each node in the list.
//
// Return : VOID
//
void CDll::Reset(dll_t *list, int freegenptr /*=0*/)
{
    dll_t *node;
    dll_t *head = list->list; //make sure we start at the head of the list

    node = head->next;
    while (node != head) {
        node = node->next;
        Destroy(node->prev,freegenptr);
    }
}

//////////////////////////////////////////////////////////////////////
// Insert a node into the specified list.
//
// [in] list : Pointer to the list to insert the node into.
// [in] node : Pointer to the node to insert into the list.
//
// Return : VOID
//
void CDll::Insert(dll_t *list, dll_t *node) 
{
    dll_t *d = list->prev;

        //now insert after 'd'
    node->next = d->next;
    node->prev = d;
    node->next->prev = node;
    d->next = node;

    node->list = list;
}

//////////////////////////////////////////////////////////////////////
// Remove a node from whatever list it is in.
//
// [in] node : Pointer to the node to remove from the list.
//
// Return : VOID
//
void CDll::Remove(dll_t *node) 
{
        //only do it if node is indeed part of a list
    if (node->next != node) {
            //remove node from list
        node->prev->next = node->next;    
        node->next->prev = node->prev;
        node->next = node;
        node->prev = node;
            //update the list pointer; node is now a one-element list
        node->list = node;
    } 
}

////////////////////////////////////////
// Functions for searching the liked
// list.
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Search for a node by key or symbolic name. If "flagcase" != 0, 
// the string compares using "name" will be case insensitive.
//
// [in] list     : Pointer to the list to search.
// [in] key      : The key (ID) of the node being searched for.
// [in] name     : Symbolic name of the node being searched for.  
//                 If != NULL, "name" will be used.  If == NULL, 
//                 "key" is used.
// [in] flagcase : If != 0, the symbolic name search will be case
//                 insensitive.
//
// Return : If the node is found, a pointer to the node is returned.
//          If the node is not found, NULL is returned.
//
dll_t *CDll::Search(dll_t *list, unsigned key, const char *name, int flagcase /*=0*/)
{

    if (name == NULL) {
        dll_t *node = list->prev;

            //Search by key
        while ((node->key != key) && (node->key != 0))
            node = node->prev;

        if (key == node->key)
            return(node);	//key found in list, so return node
        else
            return(NULL);	//key not in list
    } else {
        dll_t *node = list->next;

            //Search by symbolic name
            //Farthest we go is to the header node.
            //Note that when comparing strings, we first test just the
            //first character.  We only test the rest of the string if
            //the first character matches.  This will on average reduce
            //the search time for this routine.
        while (node != list) {
            if (node->name != NULL) {
                if (toupper(*(node->name)) == toupper(*name)) {
                        //first letter matches, so test rest of name
                    if (flagcase == 0) {
                        if (strcmp(node->name,name) == 0)   //case sensitive
                            return(node);   //match found
                        else
                            node = node->next;  //check next node
                    } else {
                        #ifdef WIN32
                          if (_strcmpi(node->name,name) == 0)    //case insensitive
                        #else
                          if (strcasecmp(node->name,name) == 0) //case insensitive
                        #endif
                            return(node);   //match found
                        else
                            node = node->next;  //check next node
                    }
                } else {
                    node = node->next;  //check next node 
                }
            } else {
                node = node->next;  //check next node 
            }
        }

        return(NULL);    //name not found
    }
}

////////////////////////////////////////
// Print the key and symname of each
// element in list. (for DEBUGGING)
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Print the key and name of each element in list
//
// [in] list : Pointer to the list to print.
//
// Return : NULL
//
dll_t *CDll::ListPrint(dll_t *list)
{
    dll_t *node = list->next;

    if (list->name != NULL)
        printf("List '%s':\n",list->name);
    else
        printf("List 'NULL':\n");

    while (node != list) {
    	printf("> Key=%2d, Data=0x%08x, prev=0x%08x, next=0x%08x, list=0x%08x\n",
	            (unsigned int)node->key, (unsigned int)node->genptr,
                (unsigned int)node->prev, (unsigned int)node->next,
                (unsigned int)node->list);
        if (node->name != NULL)
            printf("> Name=%s\n\n",node->name);
        else
            printf("> Name=NULL\n\n");
        node = node->next;
    }
    printf("> Key=%2d, Data=0x%08x, prev=0x%08x, next=0x%08x, list=0x%08x\n",
            (unsigned int)node->key, (unsigned int)node->genptr,
            (unsigned int)node->prev, (unsigned int)node->next,
            (unsigned int)node->list);
    if (node->name != NULL)
        printf("> Name=%s\n\n",node->name);
    else
        printf("> Name=NULL\n\n");

    return(NULL);
}

////////////////////////////////////////
// Misc utility functions
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Get the next node in list.  This is needed to manually traverse
// the list.
//
// [in] node : Pointer to the current node in the list.
//
// Return : Pointer to the next node in the list.
//
dll_t *CDll::NodeNext(dll_t *node)
{
    return(node->next);
}

//////////////////////////////////////////////////////////////////////
// Get the previous node in list.  This is needed to manually traverse
// the list.
//
// [in] node : Pointer to the current node in the list.
//
// Return : Pointer to the previous node in the list.
//
dll_t *CDll::NodePrev(dll_t *node)
{
    return(node->prev);
}

//////////////////////////////////////////////////////////////////////
// Get the greatest key in the list.  This is useful when adding a
// new node to the list.
//
// [in] list : Pointer to the list to get the greatest key for.
//
// Return : The value of the greatest key in the list.
//
unsigned CDll::GetGreatestKey(dll_t *list)
{
    dll_t *node;
    unsigned maxkey = 0;

    node = list->next;
    while (node != list) {
        maxkey = (maxkey < node->key) ? node->key : maxkey;
        node = node->next;
    }

    return(maxkey);
}

//////////////////////////////////////////////////////////////////////
// Get the number of nodes on the list.
//
// [in] list : Pointer to the list.
//
// Return : The number of nodes on the list
//
unsigned CDll::GetListLength(dll_t *list)
{
    dll_t *node;
    unsigned nnodes = 0;

    node = list->next;
    while (node != list) {
        nnodes++;
        node = node->next;
    }

    return(nnodes);
}

