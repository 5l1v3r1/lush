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

/***********************************************************************
 * $Id: lisp_c.c,v 1.36 2005/11/14 15:35:06 leonb Exp $
 **********************************************************************/


#include "header.h"
#include "mm.h"
#include "check_func.h"
#include "dh.h"

/* Flags */
static bool dont_track_cside = false;
static bool dont_warn_zombie = false;
static bool dont_warn = false;


/* -----------------------------------------
   STORAGE TYPE <-> DHT TYPE
   ----------------------------------------- */

static int storage_to_dht[ST_LAST];
static int dht_to_storage[DHT_LAST];

static void init_storage_to_dht(void) 
{
  for(int i=0;i<ST_LAST;i++)
    storage_to_dht[i] = -1;
  for(int i=0;i<DHT_LAST;i++)
    dht_to_storage[i] = -1;
  storage_to_dht[ST_F] = DHT_FLT;
  storage_to_dht[ST_D] = DHT_REAL;
  storage_to_dht[ST_I32] = DHT_INT;
  storage_to_dht[ST_I16] = DHT_SHORT;
  storage_to_dht[ST_I8] = DHT_BYTE;
  storage_to_dht[ST_U8] = DHT_UBYTE;
  storage_to_dht[ST_GPTR] = DHT_GPTR;
  dht_to_storage[DHT_FLT] = ST_F;
  dht_to_storage[DHT_REAL] = ST_D;
  dht_to_storage[DHT_INT] = ST_I32;
  dht_to_storage[DHT_BYTE] = ST_I8;
  dht_to_storage[DHT_UBYTE] = ST_U8;
  dht_to_storage[DHT_SHORT] = ST_I16;
  dht_to_storage[DHT_GPTR] = ST_GPTR;
}


#include "avltree.c"


/* -----------------------------------------------------
   ROUTINES MANAGING THE AVL TREE FROM INTERPRETER CODE
   ----------------------------------------------------- */

/* link containing node that we may update */
static avlnode_t dummy_upds = { &dummy_upds, &dummy_upds };
/* link containing temporary nodes */
static avlnode_t dummy_tmps = { &dummy_tmps, &dummy_tmps };

inline static void mark_temporary (avlnode_t *n)
{
  avlchain_set(n, &dummy_tmps);
}

/* #define mark_temporary(n) {      \ */
/*   printf("%s : mark_temporary(%lx,%d);\n",__func__,(long)(n->citem),n->cinfo); \ */
/*   avlchain_set(n, &dummy_tmps); \ */
/*   } */

inline static void mark_for_update(avlnode_t *n)
{
  avlchain_set(n, &dummy_upds);
}




/* alloc_srg -- allocate space for a SRG */

static avlnode_t *alloc_srg(storage_type_t type)
{
  /* A Lisp owned SRG associated to a Lisp object directly points into the
   * data area of the Lisp object.
   *
   * A Lisp owned SRG not associated to a Lisp storage (e.g. a temporary)
   * owns its data block. It must be freed when the object is freed or
   * disowned when we associate a lisp storage (make_lisp_from_c)
   *
   * For your information, a C owned SRG manages completely its data
   * block. These SRG are not allocated with this function and are not freed
   * by any function in this file. The data must be copied back and forth when 
   * synchronizing the L and C sides.
   */
  struct srg *cptr = malloc(sizeof(struct srg));
  if (!cptr) 
    error(NIL,"Out of memory",NIL);
  cptr->size = 0;
  cptr->type = (char)type;
  cptr->data = 0;
  cptr->flags = STS_MALLOC;

  avlnode_t *n = avl_add(cptr);
  if (n==0)
    error(NIL,"lisp_c internal: cannot add element to item map",NIL);
  n->cinfo = CINFO_SRG;
  n->belong = BELONG_LISP;
  n->cmoreinfo = 0;
  return n;
}


/* alloc_idx -- allocate space for an IDX */

static avlnode_t *alloc_idx(int ndim)
{
  /* We allocate the IDX structure and the DIM and MOD arrays
   * in a single memory block.
   */
  struct idx *cptr = malloc(sizeof(struct idx) + 2*ndim*sizeof(ptrdiff_t) );
  if (!cptr) 
    error(NIL,"Out of memory",NIL);
  assert(ndim<MAXDIMS);
  cptr->ndim = ndim;
  cptr->dim = (size_t *)(cptr+1);
  cptr->mod = (ptrdiff_t *)cptr->dim + ndim;

  avlnode_t *n = avl_add(cptr);
  if (n==0)
    error(NIL,"lisp_c internal: cannot add element to item map",NIL);
  n->cinfo = CINFO_IDX;
  n->belong = BELONG_LISP;
  n->cmoreinfo = 0;
  return n;
}

/* alloc_obj -- allocate an object */

static avlnode_t *alloc_obj(dhclassdoc_t *classdoc)
{
  /* Object allocation is plain vanilla :-)
   */
  void *cptr = malloc(classdoc->lispdata.size);
  if (!cptr) 
    error(NIL,"Out of memory",NIL);
  memset(cptr,0,classdoc->lispdata.size);
  *((void**)cptr) = (void*)(classdoc->lispdata.vtable);

  avlnode_t *n = avl_add(cptr);
  if (n==0)
    error(NIL,"lisp_c internal: cannot add element to item map",NIL);
  n->belong = BELONG_LISP;
  n->cinfo = CINFO_OBJ;
  n->cmoreinfo = classdoc;
  return n;
}


/* alloc_str -- allocate a string */

static avlnode_t *alloc_str(char *data)
{
  /* A string is represented as a ubyte storage.
   * The storage points to the same data as the
   * lisp side string.
   */
  avlnode_t *n = avl_add(data);
  ifn (n)
    error(NIL, "lisp_c internal: cannot add element to item map", NIL);
  n->cinfo = CINFO_STR;
  n->cmoreinfo = 0;
  n->belong = BELONG_LISP;
  n->citem = data;
  return n;
}


/* alloc_list -- allocate a list */

static avlnode_t *alloc_list(int len)
{
  /* LIST size is known from the LIST type.
   * Space for this copy is allocated using srg_resize. 
   * We must free it with srg_free. 
   */ 
  avlnode_t *n = alloc_srg(ST_U8);
  n->cinfo = CINFO_LIST;

  struct srg *s = n->citem;
  srg_resize(s, len*sizeof(dharg),__FILE__,__LINE__);
  memset(s->data, 0, len*sizeof(dharg));
  return n;
}


/* Forward */
static void update_c_from_lisp();
static void update_lisp_from_c();


/* lside_create_srg -- call this for creating an SRG from interpreted code */

static avlnode_t *lside_create_srg(at *p)
{
  ifn (STORAGEP(p))
    error(NIL,"not a storage", p);
  storage_t *st = p->Object;

  if (st->cptr) {
    avlnode_t *n = avl_find(st->cptr);
    if (n==0)
      error(NIL, "lisp_c internal: cannot find srg", st->backptr);
    return n;
    
  } else {
    /* allocate */
    avlnode_t *n = alloc_srg(st->type);
    n->litem = p;
    st->cptr = n->citem;
    mark_for_update(n);
    
    /* special marker for read-only storages */
    if (st->flags & STF_RDONLY)
      n->cmoreinfo = st;
    /* update c object */
    update_c_from_lisp(n);
    return n;
  }
}


/* lside_create_idx -- call this for creating an IDX from interpreted code */
  
static avlnode_t *lside_create_idx(at *p)
{
  ifn (INDEXP(p))
    error(NIL,"Not an index",p);
  index_t *ind = p->Object;
  
  if (ind->cptr) {
    avlnode_t *n = avl_find(ind->cptr);
    if (n==0)
      error(NIL,"lisp_c internal: cannot find idx",p);
    return n;
    
  } else {
    avlnode_t *n = alloc_idx(ind->ndim);
    n->litem = p;
    ind->cptr = n->citem;
    mark_for_update(n);
    update_c_from_lisp(n);
    return n;
  }
}

  /* lside_create_obj -- call this for creating an object from interpreted code */
  
static avlnode_t *lside_create_obj(at *p)
{
  /* check type */
  ifn (OBJECTP(p))
    error(NIL,"Object expected",p);
  struct oostruct *obj = p->Object;
  
  if (obj && obj->cptr) {
    avlnode_t *n = avl_find(obj->cptr);
    if (n==0)
      error(NIL,"lisp_c internal: cannot find object",p);
    if (n->cinfo == CINFO_UNLINKED)
      error(NIL,"found object with unlinked class", p);
    return n;
    
  } else {
    /* get compiled class */
    class_t *cl = p->Class;
    while (cl && !cl->classdoc)
      cl = cl->super;
    ifn (cl)
      error(NIL,"Not an instance of a compiled class",p);        
    
    /* check classdoc */
    if (CONSP(cl->priminame))
      check_primitive(cl->priminame, cl->classdoc);
    
    avlnode_t *n = alloc_obj(cl->classdoc);
    if (obj) {
      n->litem = p;
      obj->cptr = n->citem;
      mark_for_update(n);
      update_c_from_lisp(n);
      
    } else
      mark_temporary(n);

    return n;
  }
}
  
  
/* lside_create_str -- creates a string */

static avlnode_t *lside_create_str(at *p)
{
  ifn (STRINGP(p))
    error(NIL,"String expected",p);
  char *str = SADD(p->Object);
  assert(str);
  avlnode_t *n = avl_find(str);
  
  if (n==0) {
     /* create storage pointing to the string */
     avlnode_t *n = alloc_str(str);
     n->litem = p;
  }
  return n;
}


/* lisp_owns_p -- check that object belongs to lisp */
  
bool lisp_owns_p(void *cptr)
{
  if (cptr) {
    avlnode_t *n = avl_find(cptr);
    if (n && (n->belong != BELONG_LISP))
      return false;
  }
  return true;
}


/* lside_destroy_item -- called when a mirrored lisp object is purged */

void lside_destroy_item(void *cptr)
{
  if (cptr) {
    avlnode_t *n = avl_find(cptr);        
    if (n) {
      avlchain_set(n, 0);

      switch (n->belong) {

      case BELONG_C:
	/* The lisp counterpart of this C object is being destroyed.
	 * Nevertheless the object is still there until C calls
	 * 'cside-destroy-item'. We must update it using lisp information.
	 */
	update_c_from_lisp(n);
	n->litem = 0;
	break;

      case BELONG_LISP:
	/* The lisp counterpart of this C object is being destroyed.
	 * We must destroy the object. We do not need to destroy
	 * the data blocks associated to this object (e.g. storage data)
	 * because this data block was owned by the lisp object...
	 */
	assert(n->citem==cptr);
	if (n->litem) {
	  at *p = n->litem;
	  switch (n->cinfo) {

	  case CINFO_SRG:
	    ((storage_t *)p->Object)->cptr = NULL;
	    break;
	    
	  case CINFO_OBJ:
	    ((object_t *)p->Object)->cptr = NULL;
	    break;

	  case CINFO_IDX:
            ((index_t *)p->Object)->cptr = NULL;
	    break;

	  case CINFO_STR:
             //n->litem->Object = NULL;
            n->citem = NULL;
	    break;
	  }
	}
        if (n->citem)
           free(n->citem);
	avl_del(cptr);
	break;

      default:
	error(NIL,"lisp_c internal : corrupted data structure",NIL);
      }
    }
  }
}



/* lside_dld_partial -- called from DLD.C when objects become non executable */

int lside_mark_unlinked(void *cdoc)
{
  int count = 0;
  bool again = true;
  while (again) {
    avlnode_t *m = avl_first(0);
    again = false;
    while (m) {
      avlnode_t *n = m;
      m = avl_succ(m);

      if (n->cmoreinfo != cdoc)
	continue;

      if (n->belong == BELONG_LISP) {
	lside_destroy_item(n->citem);
	count++;
	again = true;
      
      } else if (n->cinfo == CINFO_OBJ)	{
	n->cinfo = CINFO_UNLINKED;
	n->cmoreinfo = 0;
	count++;
      }
    }
  }
  return count;
}



/* -------------------------------------------------
   ROUTINES MANAGING THE AVL TREE FROM COMPILED CODE
   ------------------------------------------------- */


/* cside_create_idx -- call this when creating an IDX from C code */

void cside_create_idx(void *cptr)
{
  if (!dont_track_cside) {
    avlnode_t *n = avl_add(cptr);
    if (n) {
      n->cinfo = CINFO_IDX;
      n->belong = BELONG_C;
    } else
      error(NIL,"lisp_c internal : idx created twice",NIL);
  }
}

/* cside_create_srg -- call this when creating an SRG from C code */

void cside_create_srg(void *cptr)
{
  if (!dont_track_cside) {
    avlnode_t *n = avl_add(cptr);
    if (n) {
      n->cinfo = CINFO_SRG;
      n->belong = BELONG_C;
    } else
      error(NIL,"lisp_c internal : srg created twice",NIL);
  }
}

/* cside_create_obj -- call this when creating an object from C code */

void cside_create_obj(void *cptr, dhclassdoc_t *kname)
{
  if (!dont_track_cside) {
    avlnode_t *n = avl_add(cptr);
    if (n) {
      n->cinfo = CINFO_OBJ;
      n->cmoreinfo = kname;
      n->belong = BELONG_C;
    } else
      error(NIL,"lisp_c internal : obj created twice",NIL);
  }
}

/* cside_create_str -- call this when creating an STR from C code */

void cside_create_str(void *cptr)
{
  if (!dont_track_cside) {
    avlnode_t *n = avl_add(cptr);
    if (n) {
      n->cinfo = CINFO_STR;
      n->belong = BELONG_C;
    } else
      error(NIL,"lisp_c internal : str created twice",NIL);
  }
}


/* cside_create_str_gc -- create an STR from C code but it *
 *                        shall belong to LISP             */

void cside_create_str_gc(void *cptr)
{
  if (!dont_track_cside) {
    avlnode_t *n = avl_add(cptr);
    if (n) {
      n->cinfo = CINFO_STR;
      n->belong = BELONG_LISP;
    } else
      error(NIL,"lisp_c internal : str created twice",NIL);
    mark_temporary(n); // string might be just temporary
  }
}

/* transmute_object_into_gptr -- 
   trick a lisp object into a gptr */

static void transmute_object_into_gptr(at *p, void *px)
{
  /* This is bad practice */
  if (p && (EXTERNP(p))) {
    /* clean object up */
    (*p->Class->dispose)(p);
    /* disguise it as a gptr */
    p->Class = &gptr_class;
    p->Gptr = px;
  }
}


/* cside_destroy_node -- destroys an avlnode_t */

static void cside_destroy_node(avlnode_t *n)
{
  /* There is no need to free this object.
   * This is the reponsibility of the compiled code who effectively
   * owns this object and told us that the object was being deleted.
   */
  if (n->belong != BELONG_C) 
    return;

  /* Delete lisp object as well */
  if (n->litem) {
    avlchain_set(n, 0);
    at *p = n->litem;
    switch (n->cinfo) {

    case CINFO_SRG:
      ((struct storage *)(p->Object))->cptr = 0;
      update_lisp_from_c(n);
      break;

    case CINFO_OBJ:
      ((struct oostruct*)(p->Object))->cptr = 0;
      transmute_object_into_gptr(n->litem, n->citem);
      break;

    case CINFO_IDX:
      ((struct index *)(p->Object))->cptr = 0;
      transmute_object_into_gptr(n->litem, n->citem);
      break;

    case CINFO_STR:
      p->Object = 0;
      transmute_object_into_gptr(n->litem, n->citem);
      break;
      
    default:
      error(NIL,"lisp_c internal: cinfo field invalid",n->litem);
      break;
    }
  }
  /* Apply destructors */
  if (n->citem) {
    switch (n->cinfo) {
      
    case CINFO_SRG:
      srg_free(n->citem);
      break;
      
    case CINFO_OBJ:
      if (n->cmoreinfo) {
	dhclassdoc_t *cdoc = n->cmoreinfo;
	struct VClass_object *vtbl = cdoc->lispdata.vtable;
	if (vtbl && vtbl->Cdoc == cdoc && vtbl->Cdestroy)
	  (*vtbl->Cdestroy)(n->citem);
      }
      break;
    }
  }
  /* delete avl map entry */
  avl_del(n->citem);
}
  

/* cside_destroy_item -- call this before destroying an item from C code */

void cside_destroy_item(void *cptr)
{
  avlnode_t *n = avl_find(cptr);
  if (n)
    cside_destroy_node(n);
}


/* cside_destroy_range -- call this before destroying several items from C code */

void cside_destroy_range(void *from, void *to)
{
  avlnode_t *n = avl_first(from);
  while (n && n->citem < to) {
    avlnode_t *m = avl_succ(n);
    if (n->belong==BELONG_C) 
      cside_destroy_node(n);
    n = m;
  }
}


/* cside_find_litem -- returns lisp object associated with cptr */

at *cside_find_litem(void *cptr)
{
  avlnode_t *n = avl_find(cptr);
  if (n && n->litem) {
    return n->litem;
  }
  return NIL;
}



/* -----------------------------------------
   THE KILL LIST
   ----------------------------------------- */

/*
 * When we udpate the lisp objects with information stored in the C code, we
 * must replace the contents of the object slots with the new content. In the
 * process, we must reduce the counter of the old value. This may call a
 * destructor and subsequently call a DH. This a problem because DH_LISTEVAL
 * is not a nice reentrant function.
 *
 * Instead of unlocking these objects we store them in a list by stealing
 * their lock. At the end of listeval, we just UNLOCK the list.
 */

static at *delayed_kill_list = 0;

#define DELAYED_UNLOCK(new,old) \
   if (new!=old) { delayed_kill_list = new_cons(old,delayed_kill_list); }




/* -----------------------------------------
   GET A LISP OBJECT FOR A C OBJECT
   ----------------------------------------- */


/* lisp2c_warning -- prints a warning message */

static void lisp2c_warning(char *s, at *errctx)
{
  if (dont_warn)
    return;
  fprintf(stderr,"+++ Warning(lisp_c): %s\n", s);
  if (errctx) 
    fprintf(stderr,"+++ in object %s\n",pname(errctx));
}



/* -----------------------------------------
   DHARG MANAGEMENT
   ----------------------------------------- */


/* dharg_to_address -- store dharg value at a specific location */

static inline void dharg_to_address(dharg *arg, char *addr, dhrecord *drec)
{
  switch(drec->op) {
  case DHT_BYTE:
  case DHT_BOOL:
  case DHT_NIL:
    *((char *) addr) = arg->dh_char;
    break;
  case DHT_UBYTE:
    *((unsigned char *) addr) = arg->dh_uchar;
    break;
  case DHT_SHORT:
    *((short *) addr) = arg->dh_short;
    break;
  case DHT_INT:
    *((int *) addr) = arg->dh_int;
    break;
  case DHT_FLT:
    *((flt *) addr) = arg->dh_flt;
    break;
  case DHT_REAL:
    *((real *) addr) = arg->dh_real;
    break;
  case DHT_GPTR:
    *((gptr *) addr) = arg->dh_gptr;
    break;
  default:	/* Well, its probably a pointer */
    *((gptr *) addr) = arg->dh_gptr;
    break;
  }
}


/* address_to_dharg -- copy contents of specific location into dharg */

static inline void address_to_dharg(dharg *arg, char *addr, dhrecord *drec)
{
  switch(drec->op) {
  case DHT_BYTE:
  case DHT_BOOL:
  case DHT_NIL:
    arg->dh_char = *((char *) addr);
    break;
  case DHT_UBYTE:
    arg->dh_uchar = *((unsigned char *) addr);
    break;
  case DHT_SHORT:
    arg->dh_short = *((short *) addr);
    break;
  case DHT_INT:
    arg->dh_int = *((int *) addr);
    break;
  case DHT_FLT:
    arg->dh_flt = *((flt *) addr);
    break;
  case DHT_REAL:
    arg->dh_real = *((real *) addr);
    break;
  default:
    arg->dh_gptr = *((gptr *) addr);
  }
}


/* lisp2c_error -- richer error message */

static void lisp2c_error(char *s, at *errctx, at *p)
{
  char errmsg[512];
  if (errctx == 0)
    sprintf(errmsg,"(lisp_c) %c%s", toupper(s[0]), s+1 );
  else
    sprintf(errmsg,"(lisp_c) %c%s\n***    in object %s", 
            toupper(s[0]), s+1, first_line(errctx) );
  error(NIL,errmsg,p);
}


/* at_to_dharg -- fills dharg with the c equivalent of a lisp object */

static void _at_to_dharg();

static inline void at_to_dharg(at *at_obj, dharg *arg, dhrecord *drec, at *errctx)
{
  /* deal with special case at_obj==NIL separately */
  /* happens in constructors */
  if (at_obj==NIL) {
    switch(drec->op) {
    case DHT_FLT: 
      arg->dh_flt = 0; break;
    case DHT_REAL: 
      arg->dh_real = 0; break;
    default:
      arg->dh_gptr = 0; break;
    }

  } else
    _at_to_dharg(at_obj, arg, drec, errctx);
}

/* don't call _at_to_dharg directly */
static void _at_to_dharg(at *at_obj, dharg *arg, dhrecord *drec, at *errctx)
{
  switch(drec->op) {
  
  case DHT_FLT:
    if (NUMBERP(at_obj))
      arg->dh_flt = (flt) at_obj->Number; 
    else
      lisp2c_error("FLT expected",errctx,at_obj);
    break;

  case DHT_REAL:
    if (NUMBERP(at_obj))
      arg->dh_real = (real) at_obj->Number; 
    else
      lisp2c_error("REAL expected",errctx,at_obj);
    break;
  
  case DHT_INT:
    if (NUMBERP(at_obj) &&
	(at_obj->Number == (int)(at_obj->Number)) )
      arg->dh_int = (int) at_obj->Number; 
    else
      lisp2c_error("INT expected",errctx,at_obj);
    break;
  
  case DHT_SHORT:
    if (NUMBERP(at_obj) &&
	(at_obj->Number == (short)(at_obj->Number)) )
      arg->dh_short = (short) at_obj->Number; 
    else
      lisp2c_error("SHORT expected",errctx,at_obj);
    break;
  
  case DHT_BYTE:
    if (NUMBERP(at_obj) &&
	(at_obj->Number == (char)(at_obj->Number)) )
      arg->dh_char = (char) at_obj->Number; 
    else
      lisp2c_error("BYTE expected",errctx,at_obj);
    break;
  
  case DHT_UBYTE:
    if (NUMBERP(at_obj) &&
	(at_obj->Number == (unsigned char)(at_obj->Number)) )
      arg->dh_uchar = (unsigned char) at_obj->Number; 
    else
      lisp2c_error("UBYTE expected",errctx,at_obj);
    break;
  
  case DHT_GPTR:
    if (GPTRP(at_obj))
      arg->dh_gptr = (gptr) at_obj->Gptr;             
    else
      lisp2c_error("GPTR expected",errctx,at_obj);
    break;
  
  case DHT_NIL:
    lisp2c_error("NIL expected",errctx,at_obj);
    break;
  
  case DHT_BOOL:
    if (at_obj == at_t)
      arg->dh_char = 1;
    else
      lisp2c_error("BOOL expected",errctx,at_obj);
    break;
  
  case DHT_SRG:
    if (GPTRP(at_obj)) {
      if (!dont_track_cside) 
	lisp2c_warning("(in): found GPTR instead of SRG", errctx);
      arg->dh_srg_ptr = at_obj->Gptr;
      
    } else if (ZOMBIEP(at_obj) && dont_warn_zombie) {
      arg->dh_srg_ptr = 0;
      
    } else if (STORAGEP(at_obj)) {
      /* check type and access */
      storage_t *st = at_obj->Object;
      if (storage_to_dht[st->type] != (drec+1)->op)
	lisp2c_error("STORAGE has illegal type",errctx,at_obj);
      if ((st->flags & STF_RDONLY) && (drec->access == DHT_WRITE))
	lisp2c_error("STORAGE is read only",errctx,at_obj);                
      
      /* create object */
      avlnode_t *n = lside_create_srg(at_obj);
      (arg->dh_srg_ptr) = (struct srg *)(n->citem);

    } else
      lisp2c_error("STORAGE expected",errctx,at_obj);
    break;
  
  case DHT_IDX:
    if (GPTRP(at_obj)) {
      if (!dont_track_cside) 
        lisp2c_warning("(in): found GPTR instead of IDX", errctx);
      arg->dh_idx_ptr = at_obj->Gptr;
      
    } else if (ZOMBIEP(at_obj) && dont_warn_zombie) {
      arg->dh_idx_ptr = 0;

    } else if (INDEXP(at_obj)) {
      /* check type and access */
      index_t *ind = at_obj->Object;
      if (ind->ndim != drec->ndim)
	lisp2c_error("INDEX has wrong number of dimensions",
		     errctx, at_obj);
      if (storage_to_dht[ind->st->type] != (drec+2)->op)
	lisp2c_error("INDEX is based on a STORAGE with illegal type",
		     errctx, at_obj);
      if ((ind->st->flags & STF_RDONLY) && (drec->access == DHT_WRITE))
	lisp2c_error("INDEX is read only", errctx, at_obj);            
      
      /* create object */
      avlnode_t *n = lside_create_idx(at_obj);
      (arg->dh_idx_ptr) = (struct idx *)(n->citem);
      
    } else
      lisp2c_error("IDX expected",errctx,at_obj);
    break;
        
  case DHT_OBJ:
    if (GPTRP(at_obj)) {
      if (!dont_track_cside)
        lisp2c_warning("(in): found GPTR instead of OBJ", errctx);
      arg->dh_obj_ptr = at_obj->Gptr;

    } else if (ZOMBIEP(at_obj)) {
      if (!dont_track_cside && !dont_warn_zombie)
        lisp2c_warning("(in): found ZOMBIE instead of OBJ", errctx);
      arg->dh_obj_ptr = 0;

    } else if (OBJECTP(at_obj)) {
      /* check type */
      class_t *cl = at_obj->Class;
      for (; cl; cl=cl->super) {
	dhclassdoc_t *cdoc = cl->classdoc;
	if ((cdoc) && (cdoc == (dhclassdoc_t*)(drec->arg)))
	  break;
      }
      if (cl==NULL)
	lisp2c_error("OBJECT has illegal type",errctx,at_obj);
      /* create object */
      avlnode_t *n = lside_create_obj(at_obj);
      (arg->dh_obj_ptr) = n->citem;
    } else
      lisp2c_error("OBJECT expected", errctx, at_obj);
    break;
    
  case DHT_STR:
    if (GPTRP(at_obj)) {
      if (!dont_track_cside)
        lisp2c_warning("(in): found GPTR instead of STR", errctx);
      arg->dh_str_ptr = at_obj->Gptr;

    } else if (ZOMBIEP(at_obj) && dont_warn_zombie) {
      arg->dh_str_ptr = 0;

    } else if (STRINGP(at_obj)) {
       //avlnode_t *n = lside_create_str(at_obj);
      avlnode_t *n = alloc_str(at_obj->Object);
      arg->dh_str_ptr = n->citem;
      
    } else
      lisp2c_error("STRING expected",errctx,at_obj);
    break;

  case DHT_LIST:
    /* Create and *initialize* the SRG:
     * UPDATE_C_FROM_LISP does nothing. 
     * This is an exception to the general rule
     * because STRs and compiled LISTs are immutable.
     */
    if (GPTRP(at_obj)) {
      if (!dont_track_cside) 
        lisp2c_warning("(in): found GPTR instead of LIST", errctx);
      arg->dh_srg_ptr = at_obj->Gptr;

    } else if (CONSP(at_obj)) {
      if (length(at_obj) != drec->ndim)
	lisp2c_error("LIST lengths do not match",errctx,at_obj);

      /* create storage for the list */
      avlnode_t *n = alloc_list(drec->ndim);
      n->cmoreinfo = drec;
      mark_temporary(n);

      /* fill the list elements */
      struct srg *srg = n->citem;
      dharg *larg = srg->data;
      at *p = at_obj;
      int i = drec->ndim;
      drec++;
      while (--i >= 0) {
	at_to_dharg(p->Car, larg, drec, at_obj);
	drec = drec->end;
	larg += 1;
	p = p->Cdr;
      }
      (arg->dh_srg_ptr) = srg;
      
    } else
      lisp2c_error("LIST expected", errctx, at_obj);
    break;
	        
    default:
      lisp2c_error("Unknown DHDOC type",errctx,at_obj);
    }
}

/* dharg_to_at -- builds at object from dharg */

static at *_dharg_to_at();
static at *make_lisp_from_c(avlnode_t *n, void *px);

static inline at *dharg_to_at(dharg *arg, dhrecord *drec, at *errctx)
{
  switch(drec->op) {
    
  case DHT_NIL:
    return NIL;

  case DHT_INT:
    return NEW_NUMBER(arg->dh_int);

  case DHT_REAL:
    return NEW_NUMBER(arg->dh_real);

  case DHT_FLT:
    return NEW_NUMBER(arg->dh_flt);

  case DHT_GPTR:
    if (!arg->dh_gptr)
      return NIL;
    return NEW_GPTR(arg->dh_gptr);

  case DHT_BOOL:
    return (arg->dh_char==0) ? NIL : t();

  case DHT_BYTE:
    return NEW_NUMBER(arg->dh_char);

  case DHT_UBYTE:
    return NEW_NUMBER(arg->dh_uchar);

  case DHT_SHORT:
    return NEW_NUMBER(arg->dh_short);

  default:
    return _dharg_to_at(arg, drec, errctx);
  }
}

/* don't call _dharg_to_at directly */
static at *_dharg_to_at(dharg *arg, dhrecord *drec, at *errctx)
{
  avlnode_t *n;

  switch(drec->op) {

  case DHT_SRG:
    if (arg->dh_srg_ptr==0) 
      return NIL;
    n = avl_find(arg->dh_srg_ptr);
    if (n)
      return make_lisp_from_c(n,arg->dh_srg_ptr);
    if (!dont_track_cside)
      lisp2c_warning("(out): Dangling pointer instead of SRG", errctx);
    return NEW_GPTR(arg->dh_srg_ptr);
        
  case DHT_IDX:
    if (arg->dh_idx_ptr==0) 
      return NIL;
    n = avl_find(arg->dh_idx_ptr);
    if (n)
      return make_lisp_from_c(n,arg->dh_idx_ptr);
    if (!dont_track_cside) 
      lisp2c_warning("(out): Dangling pointer instead of IDX", errctx);
    return NEW_GPTR(arg->dh_idx_ptr);

  case DHT_OBJ:
    if (arg->dh_obj_ptr==0) 
      return NIL;
    n = avl_find(arg->dh_obj_ptr);
    if (n)
      return make_lisp_from_c(n,arg->dh_obj_ptr);
    if (!dont_track_cside)
      lisp2c_warning("(out): Dangling pointer instead of OBJ", errctx);
    return NEW_GPTR(arg->dh_obj_ptr);
        
  case DHT_STR:
    if (arg->dh_str_ptr==0) 
      return NIL;
    n = avl_find(arg->dh_str_ptr);
    if (n)
      return make_lisp_from_c(n,arg->dh_str_ptr);
    if (!dont_track_cside) 
      lisp2c_warning("(out): Dangling pointer instead of STR", errctx);
    return NEW_GPTR(arg->dh_str_ptr);
        
  case DHT_LIST:
    if (arg->dh_srg_ptr==0)
      return NIL;
    n = avl_find(arg->dh_srg_ptr);
    if (n && n->cmoreinfo==0)
      n->cmoreinfo = drec;
    if (n)
      return make_lisp_from_c(n,arg->dh_srg_ptr);
    if (!dont_track_cside)
      lisp2c_warning("(out): Dangling pointer instead of LIST", errctx);
    return NEW_GPTR(arg->dh_srg_ptr);
        
  default:
    error(NIL,"lisp_c internal: unknown op in dhrecord",NIL);
  }
}


/* -----------------------------------------
   SYNCHRONIZE LISP AND C SIDES
   ----------------------------------------- */

/* make_lisp_from_c -- get a lisp object for a c object (no synchronization) */

static at *make_lisp_from_c(avlnode_t *n, void *px)
{
  /* Complain if pointer is invalid */
  if (n==0) {
    lisp2c_warning("(out): Found dangling pointer",0);
    return NEW_GPTR((unsigned long)px);
  }
  if (n->cinfo == CINFO_UNLINKED) {
    lisp2c_warning("(out): Found pointer to unlinked object",0);
    return NEW_GPTR((unsigned long)px);
  }
  if (px != n->citem) {
    printf("*** lisp_c internal problem in 'make_lisp_from_c'");
  }
    
  /* Return existing object if any */
  if (n->litem) {
    /* object should be up to date */
    return n->litem;
  }
    
  /* Create lisp object when it does not exist */
  switch (n->cinfo) {
    
  case CINFO_IDX: {
    struct idx *idx = n->citem;
    avlnode_t *nst = avl_find(idx->srg);
    at *atst = make_lisp_from_c(nst, idx->srg);
    index_t *ind = new_index(atst->Object, NIL);
    ind->cptr = idx;
    n->litem = ind->backptr;
    //DELAYED_UNLOCK(n->litem, NIL);
    mark_for_update(n);
    update_lisp_from_c(n);
    return n->litem;
  }

  case CINFO_SRG: {
    struct srg *srg = n->citem;
    storage_t *st = new_storage(srg->type);

    if (n->belong == BELONG_LISP) {
      /* If this object belong to LISP and was not previously
       * associated to a lisp storage, we must transfer the ownership
       * of the data block of the C object to the Lisp storage.
       */
       *(struct srg *)st = *srg;

    } else {
      /* We patch the flag STS_MALLOC in order to allow
       * function srg_resize (update_lisp_from_c) to work.
       */
      if (st->data || st->size)
	error(NIL,"lisp_c internal: new storage non zero size", st->backptr);
      assert(!mm_ismanaged(st->data));
      st->flags = STS_MALLOC;
    }
    /* We can now update the avl tree */
    st->cptr = srg;
    n->litem = st->backptr;
    //DELAYED_UNLOCK(n->litem, NIL);
    mark_for_update(n);
    update_lisp_from_c(n);
    return n->litem;
  }

  case CINFO_OBJ: {
    dhclassdoc_t *classdoc = n->cmoreinfo;
    ifn (classdoc->lispdata.atclass)
      error(NIL,"lisp_c internal: "
	    "classdoc appears to be uninitialized",NIL);

    /* Update avlnode_t */
    n->litem = new_object(classdoc->lispdata.atclass->Object);
    //DELAYED_UNLOCK(n->litem, NIL);
    ((struct oostruct*)(n->litem->Object))->cptr = n->citem;
    mark_for_update(n);
    /* Update object */
    update_lisp_from_c(n);
    return n->litem;
  }

  case CINFO_STR: {
    char *cs = n->citem;
    ifn (cs)
      lisp2c_warning("(out): found uninitialized string",0);

    at *p = new_string(cs);
    n->litem = p;
    //DELAYED_UNLOCK(n->litem, NIL);
    //mark_for_update(n);
    return n->litem;
  }

  case CINFO_LIST: {
    /* We do not update the LITEM field
     * for these partially supported objects.
     * They do not last between DH calls.
     */
    struct srg *srg = n->citem;
    dharg *arg = srg->data;
    dhrecord *drec = n->cmoreinfo;
    if (drec==0 || drec->op!=DHT_LIST)
      error(NIL,"lisp_c internal: "
	    "untyped list made it to make-lisp-from-c",NIL);
    int ndim = drec->ndim;
    if (ndim * sizeof(dharg) != srg->size)
      error(NIL,"lisp_c internal: "
	    "list changed size",NIL);

    at *p = NIL;
    at **where = &p;
    drec++;
    for (int i=0; i<ndim; i++) {
      *where = new_cons(dharg_to_at(arg,drec,NIL),NIL);
      arg += 1;
      drec = drec->end;
      where = &((*where)->Cdr);
    }
    //DELAYED_UNLOCK(p, NIL);
    return p;
  }
  default:
    error(NIL,"lisp_c internal: "
	  "invalid cinfo value in avl map",NIL);
  }
}


/* update_c_from_lisp -- copy lisp to c for a specific entry of the map */

extern void get_write_permit(storage_t *);

static void update_c_from_lisp(avlnode_t *n)
{
  n->need_update = 0;
  if (n->litem == NIL)
    return;    /* nothing to synchronize */

  switch (n->cinfo) {
      
  case CINFO_SRG: {

    /* Possibly broken code */        
    storage_t *st = n->litem->Object;
    if (n->cmoreinfo)
       get_write_permit(st);
            
    /* Synchronize compiled object */
    struct srg *cptr = n->citem;
    if (n->belong==BELONG_C) {
      /* C allocated SRG manage their own data block */
      ifn (st->flags & STS_MALLOC)
	error(NIL,"lisp_c internal: expected ram storage",n->litem);

      if (cptr->size < st->size) 
        srg_resize(cptr,st->size,__FILE__,__LINE__);
      size_t bytes = cptr->size * storage_sizeof[cptr->type];
      if (bytes>0 && cptr->data && st->data)
	memcpy(cptr->data, st->data, bytes);

    } else {
       /* Other SRG point to the same data area */
       *cptr = *(struct srg *)st;
    } 
    break;
  }
    
  case CINFO_IDX: {

    struct idx *idx = n->citem;
    struct index *ind = n->litem->Object;
    
    /* setup storage */
    avlnode_t *nst = lside_create_srg(IND_ATST(ind));
    idx->srg = nst->citem;
    /* copy index structure:
     * we cannot use index_to_idx because it overwrites
     * the DIM and MOD pointers instead of copying the values
     */
    idx->ndim = ind->ndim;
    idx->offset = ind->offset;
    for (int i=0; i<ind->ndim; i++) {
      idx->dim[i] = ind->dim[i];
      idx->mod[i] = ind->mod[i];
    }
    break;
  }
    
  case CINFO_UNLINKED:
#if LISP_C_VERBOSE
    lisp2c_warning("(in): Found object with unlinked class",0);
#endif
    break;
    
  case CINFO_OBJ: {

    void *cptr = n->citem;
    at *p = n->litem;
    struct oostruct *obj = p->Object;

    if (obj) {
       // if (p->flags & C_GARBAGE)
       // dont_warn_zombie = true;
      
      dhclassdoc_t *cdoc = n->cmoreinfo;
      if (cdoc==0)
	error(NIL,"lisp_c internal: corrupted class information",NIL);

      int k = 0;
      int sl = 0;
      dhclassdoc_t *class_list[1024];
      dhclassdoc_t *super = cdoc;
      while (super) {
	class_list[k++] = super;
	sl += super->argdata->ndim;
	super = super->lispdata.ksuper;
      }
      
      if (sl > obj->size)
	error(NIL, "lisp_c internal: class slot mismatch", p);
      sl = obj->size;

      while (k--) {
	super = class_list[k];
	int nsl = sl - super->argdata->ndim;
	dhrecord *drec = super->argdata + 1;
	for (int j=nsl; j<sl; j++) {
	  /* quick check of slot name */
	  //symbol_t *symb = obj->slots[j].symb->Object;
	  //if (symb->hn->name[0] != drec->name[0])
	  //  error(NIL, "lisp_c internal : object slot mismatch", p);
	
	  /* copy field described by current record */
	  dharg tmparg;
	  at_to_dharg(obj->slots[j].val, &tmparg, drec+1, p);
	  char *pos = (char*)cptr + (unsigned long)(drec->arg);
	  dharg_to_address(&tmparg, pos, drec+1);
	  
	  drec = drec->end;
	}
	sl = nsl;
      }
    }
    break;
  }
  case CINFO_STR:
    /* Strings are never modified by interpreted code */
    break;
  case CINFO_LIST:
    /* Partial support: lists do not last between DH calls */
    break;
  default:
    error(NIL, "(lisp_c) invalid cinfo field in update_c_from_lisp", NIL);
    break;
  }
}


/* update_lisp_from_c -- copy c to lisp for a specific entry of the map */

static void update_lisp_from_c(avlnode_t *n)
{
  n->need_update = 0;
  if (n->litem == 0) 
    /* nothing to synchronize */
    return;

  switch (n->cinfo) {

  case CINFO_SRG: {
    struct srg *cptr = n->citem;
    struct storage *st = n->litem->Object;

    if (n->belong == BELONG_C) {
      /* C allocated SRG manage their own data block */
      ifn (st->flags & STS_MALLOC)
	error(NIL,"lisp_c internal: expected ram storage",n->litem);

      if (st->size < cptr->size)
         srg_resize((struct srg *)st, cptr->size,__FILE__,__LINE__);
      size_t nbytes = st->size * storage_sizeof[st->type];
      if (nbytes>0)
	memcpy(st->data, cptr->data, nbytes);

    } else {
      /* Other SRG share their data block */
       *(struct srg *)st = *cptr;
       if (st->flags & STF_RDONLY)
          n->cmoreinfo = st;
    } 
    /* possibly broken code */ 
    //storage_rls_srg(st); 
    break;
  }

  case CINFO_IDX: {

    struct idx *idx = n->citem;
    struct index *ind = n->litem->Object;

    /* copy index structure */
    ind->ndim = idx->ndim;
    ind->offset = idx->offset;
    for(int i=0;i<idx->ndim;i++) {
      ind->mod[i] = idx->mod[i];
      ind->dim[i] = idx->dim[i];
    }
    /* find the storage */
    avlnode_t *nst = avl_find(idx->srg);
    if (nst==0) {
      /* danger: storage has been deallocated! */
      lisp2c_warning("(out) : Found idx with dangling storage!",0);
      delete(n->litem, true);
      n->litem = NIL;
      return;
    } 
    /* plug the storage into lisp object */
    at *origatst = IND_ATST(ind);
    at *atst = make_lisp_from_c(nst, idx->srg);
    IND_ST(ind) = atst->Object;
    DELAYED_UNLOCK(atst, origatst);
    break;
  }

  case CINFO_UNLINKED:
#if LISP_C_VERBOSE
    lisp2c_warning("(out) : Found C object with unlinked class",0);
#endif
    break;

  case CINFO_OBJ: {

    void *cptr = n->citem;
    at* p = n->litem;
    struct oostruct *obj = p->Object;
    
    if (obj) {
       //if (p->flags & C_GARBAGE)
       //dont_warn_zombie = true;

      dhclassdoc_t *cdoc = n->cmoreinfo;
      if (cdoc==0)
	error(NIL,"lisp_c internal: corrupted class information",NIL);
            
      int k = 0;
      int sl = 0;
      dhclassdoc_t *super = cdoc;
      dhclassdoc_t *class_list[1024];
      while (super) {
	class_list[k++] = super;
	sl += super->argdata->ndim;
	super = super->lispdata.ksuper;
      }

      if (sl > obj->size)
	error(NIL,"lisp_c internal: class slot mismatch",n->litem);
      sl = obj->size;

      while (k--) {
	super = class_list[k];
	int nsl = sl - super->argdata->ndim;
	dhrecord *drec = super->argdata + 1;

	for (int j=nsl; j<sl; j++) {
	  /* quick check of slot name */
	  //symbol_t *symb = obj->slots[j].symb->Object;
	  //if (symb->hn->name[0] != drec->name[0])
	  //  error(NIL,"lisp_c internal : object slot mismatch",p);

	  /* copy field described by current record */
	  char *pos = (char*)cptr + (unsigned long)(drec->arg);
	  dharg tmparg;
	  address_to_dharg(&tmparg, pos, drec+1);
	  at *orig = obj->slots[j].val;
	  at *new = dharg_to_at(&tmparg, drec+1, p);
	  obj->slots[j].val = new;
	  DELAYED_UNLOCK(new, orig);

	  drec = drec->end;
	}
	sl = nsl;
      }
    }
    break;
  }
        
  case CINFO_STR:
    /* Strings are never modified by compiled code */
    break;
  case CINFO_LIST:
    /* Lists are never modified by compiled code */
    break;
  default:
    error(NIL, "(lisp_c) invalid cinfo field in update_lisp_from_c", NIL);
    break;
  }
}


/* -----------------------------------------
   CALL A DH FUNCTION
   ----------------------------------------- */

/* build_at_temporary -- build an AT using the temporary style of dhrecord */

static void build_at_temporary(dhrecord *drec, dharg *arg)
{
  avlnode_t *n;

  switch (drec->op) {
  
  case DHT_SRG: {
    n = alloc_srg(dht_to_storage[(drec+1)->op]);
    arg->dh_srg_ptr = n->citem;
    break;
  }
  case DHT_IDX: {
    n = alloc_idx(drec->ndim);
    arg->dh_idx_ptr = n->citem;
    break;
  }
  case DHT_OBJ: {
    dhclassdoc_t *classdoc = (dhclassdoc_t*)(drec->arg);
    n = alloc_obj(classdoc);
    arg->dh_obj_ptr = n->citem;
    break;
  }
  case DHT_STR: {
    RAISEF("(lisp_c) string temporary (should never get here)",NIL);
    break;
  }
  case DHT_LIST: {
    n = alloc_list(drec->ndim);
    n->cmoreinfo = 0;
    arg->dh_srg_ptr = n->citem;
    break;
  }
  default:
    error(NIL,"lisp_c internal: unknown temporary style",NIL);
  }
  mark_temporary(n);
}


/* set the need_update field */

static void set_update_flag(void)
{
  avlnode_t *n = dummy_upds.chnxt;
  while (n != &dummy_upds) {
    n->need_update = 1;
    n = n->chnxt;
  }
}


/* full_update_c_from_lisp -- copy lisp to c */

static void full_update_c_from_lisp(void)
{
  avlnode_t *n = dummy_upds.chnxt;
  while (n != &dummy_upds) {
    if (n->litem == NIL)
      error(NIL,"internal error in UPDS chain",NIL);
    if (n->need_update) {
       //printf("updating C of '%s'\n", pname(n->litem));
       update_c_from_lisp(n);
    }
    n = n->chnxt;
  }
}

/* full_update_lisp_from_c -- copy c to lisp */

static void full_update_lisp_from_c(void)
{
  avlnode_t *n = dummy_upds.chnxt;
  while (n != &dummy_upds) {
    if (n->litem == 0)
      error(NIL,"internal error in UPDS chain",NIL);
    if (n->need_update) {
       //printf("updating lisp of '%s'\n", pname(n->litem));
      update_lisp_from_c(n);
    }
    n = n->chnxt;
  }
}


/* wipe_out_temps -- remove temporary LISP objects */

static void wipe_out_temps(void)
{
  while (dummy_tmps.chnxt != &dummy_tmps) {

    avlnode_t *n = dummy_tmps.chnxt;

    if (n->belong!=BELONG_LISP)
      error(NIL,"internal error in TMPS chain",NIL);
    
    if (n->litem) {
/*       printf("ignoring temp %s (count %d)\n",  */
/* 	     pname(n->litem), n->litem->count); */
      avlchain_set(n, 0);
      
    } else {
	     
      void *cptr = n->citem;
      switch(n->cinfo) {
      case CINFO_LIST:
      case CINFO_SRG:
         //srg_free(cptr);
	break;
	
      case CINFO_OBJ: {
	void (*cdestroy)(gptr) = ((struct CClass_object*)cptr)->Vtbl->Cdestroy;
	if (cdestroy)
	  (*cdestroy)(cptr); /* call destructor */
	break;
	
      case CINFO_STR:
        /* printf("(lisp_c) wiping out temporary string\n"); */
        cptr = NULL;
	break;
	
      case CINFO_IDX:
        /* printf("(lisp_c) wiping out temporary index\n"); */
        break;

      default:
	printf("(lisp_c) wiping out unknown temporay (%lx)\n",(long)cptr);
	break;
      }
      }
      avlchain_set(n, 0);
      avl_del(n->citem);
      if (cptr) free(cptr);
    }
  }
}


/* run_time_error -- called by compiled code when an error occurs */

int     run_time_error_flag;
jmp_buf run_time_error_jump;

void run_time_error(char *s)
{
  if (run_time_error_flag) {
    printf("\n\n*** lisp_c runtime error: %s\007\007\n",s);
    print_dh_trace_stack();
    dh_trace_root = 0;
    longjmp(run_time_error_jump,-1);

  } else {
    dh_trace_root = 0;
    error(NIL, s, NIL);
  }
}


/* dh_listeval -- calls a compiled function */
    
at *dh_listeval(at *p, at *q)
{
#define MAXARGS 1024
  at *atgs[MAXARGS];
  dharg args[MAXARGS];
  
  // printf("dh_listeval: %s\n", pname(q));
  /* Find and check the DHDOC */
  struct cfunction *cfunc = p->Object;
  if (CONSP(cfunc->name))
    check_primitive(cfunc->name, cfunc->info);

  dhdoc_t *kname = cfunc->info;
  dhrecord *drec = kname->argdata;
  if(drec->op != DHT_FUNC)
    error(NIL, "(lisp_c) a function dhdoc was expected", NIL);
  dont_warn_zombie = false;
  
  /* Count the arguments */
  int nargs = drec->ndim;
  int ntemps = ((dhrecord *) drec->name)->ndim;
  //printf("dh_listeval(%d [%d])...\n", nargs, ntemps);
  if (nargs + ntemps > MAXARGS)
    error(NIL,"(lisp_c) too many arguments and temporaries",NIL);
  
  /* Copy and evaluate arguments list */
  for(int i=0; i<nargs; i++) {
    q = q->Cdr;
    ifn (CONSP(q))
      need_error(0,nargs,NIL);
    atgs[i] = (*argeval_ptr)(q->Car);
  }
  if (q->Cdr)	
    need_error(0,nargs,NIL);

  /* Make compiled version of the arguments */
  drec++;
  for (int i=0; i<nargs; i++) {
    if (!atgs[i] && 
        drec->op!=DHT_NIL && 
        drec->op!=DHT_GPTR && 
        drec->op!=DHT_BOOL)
      error(NIL,"(lisp_c) illegal nil argument",NIL);
    at_to_dharg(atgs[i], &args[i], drec, NIL);
    drec = drec->end;
  }
  
  /* Prepare temporaries */
  if (ntemps != 0) {
    drec++;
    for(int i=nargs; i<nargs+ntemps; i++) {
      build_at_temporary(drec, &args[i]);
      drec = drec->end;
    }
    drec++;
  }
    
  /* Synchronize all compiled objects */
  set_update_flag();
  full_update_c_from_lisp();

  /* Prepare delayed_kill_list and environment */
  if (run_time_error_flag)
    lisp2c_warning("reentrant call to compiled code",0);
  delayed_kill_list = NIL;
  int errflag = setjmp(run_time_error_jump);
  dh_trace_root = 0;
    
  /* Call compiled code */
  dharg funcret;
  if (!errflag) {
    run_time_error_flag = 1;
    /* Call the test function if it exists */
    if (kname->lispdata.dhtest)
      (*kname->lispdata.dhtest->lispdata.call)(args-1);
    /* Call to the function */
    funcret = (*kname->lispdata.call)(args-1);
  }
    
  /* Prepare for the update */
  run_time_error_flag = 0;
  dh_trace_root = 0;
  set_update_flag();
    
  /* Build return value */
  at *atfuncret = NIL;
  if (!errflag) {
    if (drec->op != DHT_RETURN)
      error(NIL,"lisp_c internal: cannot find DHT_RETURN in DHDOC",NIL);
    atfuncret = dharg_to_at(&funcret, drec+1, NIL);
  }
  
  /* Synchronize all compiled objects */
  full_update_lisp_from_c();
  /* Remove objects owned by LISP and not associated to LISP object */
  wipe_out_temps();
  /* Execute pending deletions */
  //printf("kill-list: %s\n", pname(delayed_kill_list));
  delayed_kill_list = NIL;

  /* return */
  dont_warn_zombie = false;
  if (errflag)
    error(NIL,"Run-time error in compiled code",NIL);
  return atfuncret;
}



at *lisp_c_map(void *p)
{
  if (p==(void*)0) {
    if (root)
      avl_display(root,0); 
    return NEW_NUMBER(nobjects);
    
  } else if (p==(void*)1) {
      int no = 0;
      for (avlnode_t *n = dummy_upds.chnxt; 
	   n != &dummy_upds; n=n->chnxt, no++)
        avl_pnode(n);
      return NEW_NUMBER(no);

  } else if (p==(void*)2) {
      int no = 0;
      for (avlnode_t *n = dummy_tmps.chnxt;
	   n != &dummy_tmps; n=n->chnxt, no++)
        avl_pnode(n);
      return NEW_NUMBER(no);

  } else {
    avlnode_t *n = avl_find(p);
    if (n) {
      avl_pnode(n);
      return n->litem;
    }
    return NIL;
  }
}

DX(xlisp_c_map)
{
  at *p = 0;
  void *cptr = 0;
  if (arg_number == 0)
    return NEW_NUMBER(nobjects);
  ARG_NUMBER(1);
  ARG_EVAL(1);
  p = APOINTER(1);
  if (p==0)
    return lisp_c_map(0);
  else if (NUMBERP(p))
    return lisp_c_map((void*)(unsigned long)(p->Number));
  else if (OBJECTP(p))
    cptr = ((struct oostruct *)(p->Object))->cptr;
  else if (INDEXP(p))
    cptr = ((struct index *)(p->Object))->cptr;
  else if (STORAGEP(p))
    cptr = ((struct storage *)(p->Object))->cptr;
  else if (GPTRP(p))
    cptr = p->Gptr;
  if (cptr) {
    avlnode_t *n = avl_find(cptr);
    if (n) {
      avl_pnode(n); 
      return p;
    }
  }
  return NIL;
}

/* lisp-c-dont-track-cside -- stopstracking cside item (like old SN) */

DX(xlisp_c_dont_track_cside)
{
  ARG_NUMBER(0);
  dont_track_cside = true;

  int nitem = 0;
  avlnode_t *n = avl_first(0);
  while (n) {
    avlnode_t *m = avl_succ(n);
    if (n->belong==BELONG_C) {
      cside_destroy_node(n);
      nitem ++;
    }
    n = m;
  }
  return NEW_NUMBER(nitem);
}

/* lisp-c-no-warnings -- revert to old mode */

DY(ylisp_c_no_warnings)
{
  bool sav_dont_warn = dont_warn;
  struct context mycontext;
  context_push(&mycontext);
  if (setjmp(context->error_jump)) {
    context_pop();
    dont_warn = sav_dont_warn;
    longjmp(context->error_jump, -1L);
  }
  dont_warn = true;
  at *ans = progn(ARG_LIST);
  dont_warn = sav_dont_warn;
  context_pop();
  return ans;
}



/* -----------------------------------------
   INTERPRETED CASTS
   ----------------------------------------- */


/* (to-int <arg>) */
DX(xto_int)
{
  ARG_NUMBER(1);
  ARG_EVAL(1);
  return NEW_NUMBER( AINTEGER(1) );
}

/* (to-byte <arg>) */
DX(xto_byte)
{
  ARG_NUMBER(1);
  ARG_EVAL(1);
  return NEW_NUMBER( (char)AINTEGER(1) );
}

/* (to-ubyte <arg>) */
DX(xto_ubyte)
{
  ARG_NUMBER(1);
  ARG_EVAL(1);
  return NEW_NUMBER( (unsigned char)AINTEGER(1) );
}

/* (to-flt <arg>) */
DX(xto_flt)
{
  ARG_NUMBER(1);
  ARG_EVAL(1);
  return NEW_NUMBER( Ftor( AFLT(1) ) );
}

/* (to-real <arg>) */
DX(xto_real)
{
  ARG_NUMBER(1);
  ARG_EVAL(1);
  return NEW_NUMBER( AREAL(1) );
}

/* (to-number <arg>) */
DX(xto_number)
{
  ARG_NUMBER(1);
  ARG_EVAL(1);
  return NEW_NUMBER( AREAL(1) );
}

/* (to-bool <arg>) */
DX(xto_bool)
{
  at *p;
  ARG_NUMBER(1);
  ARG_EVAL(1);
  p = APOINTER(1);
  if (! p)
    return NIL;
  if (NUMBERP(p))
    if (p->Number == 0)
      return NIL; /* compiled semantic */
  return t();
}

/* (to-obj [<class>] <gptr|obj>)  */
DX(xto_obj)
{
  ALL_ARGS_EVAL;

  at *p = NIL;
  class_t *cl = NULL;

  switch (arg_number) {
  case 1:
    p = APOINTER(1);
    break;

  case 2:
    p = APOINTER(1);
    ifn (CLASSP(p))
      error(NIL,"not a class", p);
    cl = p->Object;
    p = APOINTER(2);
    break;

  default:
      ARG_NUMBER(-1);
  }
  if (!p) {
    return p;

  }  else if (OBJECTP(p)) {
     //LOCK(p);

  } else if (GPTRP(p)) {
    /* search object */
    avlnode_t *n = avl_find(p->Gptr);
    ifn (n)
      error(NIL,"Object pointed to by this GPTR has been deallocated",p);

    /* make lisp object */
    p = make_lisp_from_c(n, p->Gptr);
    delayed_kill_list = NIL;

  } else
    error(NIL,"Expecting GPTR or OBJECT",p);

  /* check class */
  if (cl) {
    ifn (OBJECTP(p))
      error(NIL, "GPTR does not point to an object", APOINTER(2));

    class_t *clm = p->Class;
    while (clm && (clm != cl))
      clm = clm->super;
    if (clm != cl)
      error(NIL, "GPTR does not point to an object of this class", APOINTER(1));
  }
  /* return */
  return p;
}

DX(xto_str)
{
  ARG_NUMBER(1);
  ARG_EVAL(1);
  
  at *p = APOINTER(1);
  
  if (STRINGP(p)) {
     //LOCK(p);
    return p;

  } else if (GPTRP(p)) {
    avlnode_t *n = avl_find(p->Gptr);
    ifn (n)
      RAISEF("Object pointed to by this GPTR has been deallocated",p);

    /* make lisp object */
    at *q = make_lisp_from_c(n, p->Gptr);
    delayed_kill_list = NIL;

    ifn (STRINGP(q)) {
       //UNLOCK(q);
      RAISEF("not a pointer to a string",p);
    }
    return q;

  } else
    RAISEF("cannot interpret as string",p);

  return NIL;
}
  
/* defined in module.c */
struct module;
extern void *dynlink_symbol(struct module *, char *, int, int);

/* (to-gptr <obj>) */
DX(xto_gptr)
{
  ARG_NUMBER(1);
  ARG_EVAL(1);

  at *p = APOINTER(1);

  if (p==NIL)
    return NIL;

  else if (GPTRP(p)) {
     //LOCK(p);
    return p;
    
  } else if (INDEXP(p)) {
    avlnode_t *n = lside_create_idx(p);
    return NEW_GPTR(n->citem);

  } else if (OBJECTP(p)) {
    avlnode_t *n = lside_create_obj(p);
    return NEW_GPTR(n->citem);

  } else if (STORAGEP(p)) {
    avlnode_t *n = lside_create_srg(p);
    return NEW_GPTR(n->citem);

  } else if (STRINGP(p)) {
    avlnode_t *n = lside_create_str(p);
    return NEW_GPTR(n->citem);

  } else if (p && (p->class == &dh_class)) {
    struct cfunction *cfunc = p->Object;
    if (CONSP(cfunc->name))
      check_primitive(cfunc->name, cfunc->info);
    
    assert(MODULEP(cfunc->name->Car));
    struct module *m = cfunc->name->Car->Object;
    dhdoc_t *dhdoc;
    if (( dhdoc = (dhdoc_t*)(cfunc->info) )) {
      void *q = dynlink_symbol(m, dhdoc->lispdata.c_name, 1, 1);
      ifn (q)
	RAISEF("could not find function pointer\n", p);
      return NEW_GPTR(q);
    }
  }
  error(NIL,"Cannot make a compiled version of this lisp object",p);
}




/* -----------------------------------------
   INITIALIZATION
   ----------------------------------------- */


void init_lisp_c(void)
{
   MM_ROOT(delayed_kill_list);

  init_storage_to_dht();
  dx_define("lisp-c-map", xlisp_c_map);
  dx_define("lisp-c-dont-track-cside", xlisp_c_dont_track_cside);
  dy_define("lisp-c-no-warnings", ylisp_c_no_warnings);
  dx_define("to-int", xto_int);
  dx_define("to-byte", xto_byte);
  dx_define("to-ubyte", xto_ubyte);
  dx_define("to-flt", xto_flt);
  dx_define("to-real", xto_real);
  dx_define("to-number", xto_number);
  dx_define("to-bool", xto_bool);
  dx_define("to-str", xto_str);
  dx_define("to-gptr", xto_gptr);
  dx_define("to-obj", xto_obj);
}
