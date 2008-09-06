/***********************************************************************
 * 
 *  LUSH Lisp Universal Shell
 *    Copyright (C) 2002 Leon Bottou, Yann Le Cun, AT&T Corp, NECI.
 *  Includes parts of TL3:
 *    Copyright (C) 1987-1999 Leon Bottou and Neuristique.
 *  Includes selected parts of SN3.2:
 *    Copyright (C) 1991-2001 AT&T Corp.
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA
 * 
 ***********************************************************************/

#include "header.h"
#include "mm.h"

/*
 * Objects holding weak references to another object may 
 * register a "notifier function" for the referent object.
 * When the memory manager reclaims the referent object,
 * the notifier function gets called.
 */

typedef struct notifier {
   struct notifier *next;
   void *target;
   void *context;
   wr_notify_func_t *notify;
} notifier_t;

static notifier_t **notifiers;

static void clear_notifier(notifier_t *n)
{
   n->next = NULL;
   n->context = NULL;
}

static void mark_notifier(notifier_t *n)
{
   MM_MARK(n->next);
   MM_MARK(n->context);
}

static mt_t mt_notifier = mt_undefined;


void add_notifier(void *t, wr_notify_func_t *notify, void *c)
{
   if (!t) return;
          
   unsigned int h = hash_pointer(t) % HASHTABLESIZE;
   /* Already there? */
   for(notifier_t *n = notifiers[h]; n; n = n->next)
      if (n->target==t && n->notify==notify && n->context==c)
         return;
   /* Add a new one */
   notifier_t *n = mm_alloc(mt_notifier);
   n->next = notifiers[h];
   n->target = t;
   n->context = c;
   n->notify = notify;
   notifiers[h] = n;
   mm_notify(t, true);
}

void del_notifiers_with_context(void *c)
{
   for (int h = 0; h < HASHTABLESIZE; h++) {
      notifier_t **pn = &notifiers[h];
      notifier_t *n = *pn;
      while (n) {
         if (n->context == c)
            *pn = n->next;
         else
            pn = &(n->next);
         n = *pn;
      }
   }
}

void del_notifiers_for_target(void *t)
{
   for (int h = 0; h < HASHTABLESIZE; h++) {
      notifier_t **pn = &notifiers[h];
      notifier_t *n = *pn;
      while (n) {
         if (n->target == t)
            *pn = n->next;
         else
            pn = &(n->next);
         n = *pn;
      }
   }
}
void run_notifiers(void *t)
{
   unsigned int h = hash_pointer(t) % HASHTABLESIZE;
   notifier_t **pn = &notifiers[h];
   notifier_t *todo = NULL;

   /* collect */
   while (*pn) {
      notifier_t *n = *pn;
      if (n->target != t)
         pn = &n->next;
      else {
         /* unlink */
         *pn = n->next;
         n->next = todo;
         todo = n;
      }
   }

   /* execute */
   while (todo) {
      notifier_t *n = todo;
      todo = n->next;
      (*n->notify)(t, n->context);
   }
}

void dbg_notify(void *p, void *q)
{
   printf("0x%x (context 0x%x) was reclaimed\n",
          (uintptr_t)p, (uintptr_t) q);
}


/* put some roots in a list */

static at *protected = NIL;

void protect(at *q)
{
   at *p = protected;

   while (CONSP(p)) {
      if (p->Car==q)
         return;
      p = p->Cdr;
   }
   protected = new_cons(q, protected);
}

void unprotect(at *q)
{
   at **p = &protected;
   
   while(CONSP((*p))) {
      if ((*p)->Car==q) {
         q = *p;
         *p = (*p)->Cdr;
         q->Cdr = NIL;
      } else
         p = &((*p)->Cdr);
   }
}

void init_weakref(void)
{
   if (mt_notifier == mt_undefined)
      mt_notifier = MM_REGTYPE("notifier", sizeof(notifier_t), 
                               clear_notifier, mark_notifier, 0);
   notifiers = mm_allocv(mt_refs, HASHTABLESIZE * sizeof(notifier_t *));
   MM_ROOT(notifiers);
   MM_ROOT(protected);
}


/* -------------------------------------------------------------
   Local Variables:
   c-file-style: "k&r"
   c-basic-offset: 3
   End:
   ------------------------------------------------------------- */