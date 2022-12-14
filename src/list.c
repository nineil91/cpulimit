/**
 *
 * cpulimit - a CPU limiter for Linux
 *
 * Copyright (C) 2005-2012, by:  Angelo Marletta <angelo dot marletta at gmail dot com> 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include <string.h>

#include "cpulimit.h"

#define EMPTYLIST NULL

void init_list(struct list *l,int keysize) {
   l->first=l->last=NULL;
   l->keysize=keysize;
   l->count=0;
}

struct list_node *add_elem(struct list *l,void *elem) {
   struct list_node *newnode=(struct list_node*)malloc(sizeof(struct list_node));
   newnode->data=elem;
   newnode->previous=l->last;
   newnode->next=NULL;
   if (l->count==0) {
      l->first=l->last=newnode;
   }
   else {
      l->last->next=newnode;
      l->last=newnode;
   }
   l->count++;
   return newnode;
}

void delete_node(struct list *l,struct list_node *node) {
   if (l->count==1) {
      l->first=l->last=NULL;
   }
   else if (node==l->first) {
      node->next->previous=NULL;
      l->first=node->next;
   }
   else if (node==l->last) {
      node->previous->next=NULL;
      l->last=node->previous;
   }
   else {
      node->previous->next=node->next;
      node->next->previous=node->previous;
   }
   l->count--;
   free(node);
}

void destroy_node(struct list *l,struct list_node *node) {
   free(node->data);
   node->data=NULL;
   delete_node(l,node);
}

int is_empty_list(struct list *l) {
   return (l->count==0?TRUE:FALSE);
}

int get_list_count(struct list *l) {
   return l->count;
}

void *first_elem(struct list *l) {
   return l->first->data;
}

struct list_node *first_node(struct list *l) {
   return l->first;
}

void *last_elem(struct list *l) {
   return l->last->data;
}

struct list_node *last_node(struct list *l) {
   return l->last;
}

struct list_node *xlocate_node(struct list *l,void *elem,int offset,int length) {
   struct list_node *tmp;
   tmp=l->first;
   while(tmp!=NULL) {
      if(!memcmp((char*)tmp->data+offset,elem,length==0?l->keysize:length)) return (tmp);
      tmp=tmp->next;
   }
   return EMPTYLIST;
}

struct list_node *locate_node(struct list *l,void *elem) {
   return(xlocate_node(l,elem,0,0));
}

void *xlocate_elem(struct list *l,void *elem,int offset,int length) {
   struct list_node *node=xlocate_node(l,elem,offset,length);
   return(node==NULL?NULL:node->data);
}

void *locate_elem(struct list *l,void *elem) {
   return(xlocate_elem(l,elem,0,0));
}

void clear_list(struct list *l) {
   while(l->first!=EMPTYLIST) {
      struct list_node *tmp;
      tmp=l->first;
      l->first=l->first->next;
      free(tmp);
      tmp=NULL;
   }
   l->last=EMPTYLIST;
   l->count=0;
}

void destroy_list(struct list *l) {
   while(l->first!=EMPTYLIST) {
      struct list_node *tmp;
      tmp=l->first;
      l->first=l->first->next;
      free(tmp->data);
      tmp->data=NULL;
      free(tmp);
      tmp=NULL;
   }
   l->last=EMPTYLIST;
   l->count=0;
}
void printProcList( proc_list *list )
{
   process *current = list->head;
   printf("\nList:");
   while( current != NULL )
   {
      printf("\n\t%d", current->pid);
      current = current->next;
   }
}

// Add "newproc" to head of "list".
void addToProcList( proc_list *list, process *newproc )
{
   newproc->next = list->head;
   list->head = newproc;
   
   return;
}

proc_list *newProcList()
{
   proc_list *ret = malloc(sizeof(proc_list));
   if( ret == NULL )
      errorQuit("Memory allocation failed.");
      
   ret->head = NULL;
   return ret;
}

OptionList *newOptList()
{
   OptionList *ret = malloc(sizeof(OptionList));
   if( ret == NULL )
      errorQuit("Memory allocation failed.");
      
   ret->head = NULL;
   return ret;
}

OptionBlock *newOptBlock()
{
   OptionBlock *ret = malloc(sizeof(OptionBlock));
   if( ret == NULL )
      errorQuit("Memory allocation failed.");
      
   memset( ret, 0x00, sizeof(OptionBlock) );
   return ret;
}

process *newProc()
{
   process *ret = malloc(sizeof(process));
   if( ret == NULL )
      errorQuit("Memory allocation failed.");
      
   memset( ret, 0x00, sizeof(process) );
   return ret;
}

void addToOptList( OptionList *list, OptionBlock *optBlock )
{
   optBlock->next = list->head;
   list->head = optBlock;
   
   return;
}

void removeFromProcList( proc_list *list, int pid )
{
   process *current = list->head, *last = NULL;
   current = list->head;
   last = NULL;

   while( current != NULL )
   {
      // Remove from list if we found the right pid.
      if( current->pid == pid )
      {
         if( last == NULL )
            list->head = current->next;
         else
            last->next = current->next;
         free(current);
      }

      last = current;
      current = current->next;
   }
}

void removeFromOptionList( OptionList *list, OptionBlock *item )
{
   OptionBlock *current = list->head, *last = NULL;
   
   while( current != NULL )
   {
      if( current == item )
      {
         if( last == NULL )
            list->head = current->next;
         else
            last->next = current->next;
            
         free(current);
      }
      
      last = current;
      current = current->next;
   }
}

// Returns address of process in "list" with pid "pid". NULL if non-existent pid.
process *searchProcList( proc_list *list, int pid )
{
   process *current = list->head;

   while( current != NULL )
   {
      if( current->pid == pid )
         return current;

      current = current->next;
   }

   return NULL;
}