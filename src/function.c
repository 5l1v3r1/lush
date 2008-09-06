/***********************************************************************
 * 
 *  PSU Lush
 *    Copyright (C) 2005 Ralf Juengling.
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
 * $Id: function.c,v 1.16 2004/07/20 18:51:06 leonb Exp $
 **********************************************************************/

#include "header.h"
#include "mm.h"

void clear_cfunction(cfunction_t *f)
{
   f->name = NULL;
   f->kname = NULL;
}

void mark_cfunction(cfunction_t *f)
{
   MM_MARK(f->name);
   MM_MARK(f->kname);
}

mt_t mt_cfunction = mt_undefined;

char *cfunc_name(at *p) {
   
   cfunction_t *func = p->Object;
   at *name = func->name;
   at *clname = NIL;
  
   if (CONSP(name) && MODULEP(name->Car))
      name = name->Cdr;

  if (CONSP(name) && SYMBOLP(name->Car)) {
     clname = name->Car;
     name = name->Cdr;
  }
  if (SYMBOLP(name)) {
     sprintf(string_buffer, "::%s:", nameof(p->Class->classname));
     if (SYMBOLP(clname)) {
        strcat(string_buffer, nameof(clname));
        strcat(string_buffer, ".");
     }
     strcat(string_buffer, nameof(name));
     return string_buffer;
  }
  /* Kesako? */
  assert(p->Class->name);
  return (p->Class->name)(p);
}

/* General lfunc routines -----------------------------	 */

void clear_lfunction(lfunction_t *f)
{
   f->formal_args = NULL;
   f->body = NULL;
}

void mark_lfunction(lfunction_t *f)
{
   MM_MARK(f->formal_args);
   MM_MARK(f->body);
}

static mt_t mt_lfunction = mt_undefined;

static at *at_optional, *at_rest, *at_define_hook;
static void parse_optional_stuff(at *formal_list, at *real_list);


/* static void */
/* pop_args(at *formal_list) */
/* { */
/*   while (CONSP(formal_list)) { */
/*     pop_args(formal_list->Car); */
/*     formal_list = formal_list->Cdr; */
/*   } */
/*   if SYMBOLP(formal_list) */
/*     SYMBOL_POP(formal_list); */
/* } */

static void pop_args(at *formal_list) {
   
   while (CONSP(formal_list)) {
      pop_args(formal_list->Car);
      formal_list = formal_list->Cdr;
   }
   if (SYMBOLP(formal_list)) {
      symbol_t *symb = formal_list->Object;
      assert(symb);
      if (symb->next)
         formal_list->Object = symb->next;
   }
}

static void push_args(at *formal_list, at *real_list) {

   /* fast non tail-recursive loop for parsing the trees */
   while (CONSP(formal_list) && CONSP(real_list) && 
          formal_list->Car!=at_rest &&
          formal_list->Car!=at_optional) {
      push_args(formal_list->Car, real_list->Car);
      real_list = real_list->Cdr;
      formal_list = formal_list->Cdr;
   };
   
   /* parsing a single symbol in a tree */
   if (SYMBOLP(formal_list)) {
      SYMBOL_PUSH(formal_list, real_list);
   
   } else if (CONSP(formal_list)) {
      parse_optional_stuff(formal_list, real_list);

   } else if (formal_list && !real_list) {
      error(NIL, "missing arguments", formal_list);

   } else if (real_list && !formal_list)
      error(NIL, "too many arguments", real_list);
   
   return;
}


static void  parse_optional_stuff(at *formal_list, at *real_list) {

   if (CONSP(formal_list) && formal_list->Car == at_optional) {
      push_args(at_optional, NIL);
      formal_list = formal_list->Cdr;
      
      while (CONSP(formal_list)) {
         at *flcar = formal_list->Car;
         if (flcar==at_rest) break;
         
         if (SYMBOLP(flcar)) {
            if (CONSP(real_list)) {
               push_args(flcar, real_list->Car);
               real_list = real_list->Cdr;
            } else
               push_args(flcar, NIL);
            
         } else if (CONSP(flcar) && SYMBOLP(flcar->Car) && LASTCONSP(flcar->Cdr)) {
            if (CONSP(real_list)) {
               push_args(flcar->Car, real_list->Car);
               real_list = real_list->Cdr;
            } else 
               push_args(flcar->Car, flcar->Cdr->Car);
	
         } else
            error(NIL, "error in &optional syntax", formal_list);
         
         formal_list = formal_list->Cdr;
      } 
   }
   /* &rest */
   if (CONSP(formal_list) && formal_list->Car==at_rest) {
      push_args(at_rest, NIL);
      formal_list=formal_list->Cdr;
      if (LASTCONSP(formal_list) && SYMBOLP(formal_list->Car)) {
         push_args(formal_list->Car, real_list);
         return;
      } else
         error(NIL, "error in &rest syntax", formal_list);
   }
   
   if (formal_list)
      error(NIL, "error in &optional syntax", formal_list);		
   if (real_list)
      error(NIL, "too many arguments", real_list);
}


at *eval_a_list(at *p)
{
   MM_ENTER;

   at *list = p;
   at **now = &p;
   p = NIL;
   
   while (CONSP(list)) {
      *now = new_cons((*argeval_ptr) (list->Car), NIL);
      now = &((*now)->Cdr);
      list = list->Cdr;
   }
   if (list)
      *now = eval(list);

   MM_RETURN(p);
}


/* DX class -------------------------------------------	 */

at **dx_stack, **dx_sp;

at *dx_listeval(at *p, at *q2)
{
   MM_ENTER;

   cfunction_t *f = p->Object;
   if (CONSP(f->name))
      check_primitive(f->name, f->info);
   
   at *q = q2;
   at **spbuff = dx_sp;
   at **arg_pos = dx_sp;
   int arg_num = 0;
   q = q->Cdr;
   while (CONSP(q)) {
      arg_num++;
      if (++spbuff >= dx_stack + DXSTACKSIZE)
         error(NIL, "sorry, stack full (Merci Yann)", NIL);
      *spbuff = q->Car;
      q = q->Cdr;
   }
   if (q)
      RAISEF("bad argument list", q2);

   at *(*call)(int, at**) = f->call;
   dx_sp = spbuff;
   at *ans = call(arg_num, arg_pos);
   while (arg_pos < spbuff)
      spbuff--;
   dx_sp = spbuff;

   MM_RETURN(ans);
}

at *new_dx(at *name, at *(*addr)(int, at **))
{
   cfunction_t *f = mm_alloc(mt_cfunction);
   f->call = f->info = addr;
   f->name = name;
   f->kname = 0;
   return new_extern(&dx_class, f);
}


/* DY class -------------------------------------------	 */

at *dy_listeval(at *p, at *q)
{
   MM_ENTER;
   
   cfunction_t *f = p->Object;
   if (CONSP(f->name))
      check_primitive(f->name, f->info);
   
   at *(*call)(at*) = f->call;
   at *ans = call(q->Cdr);
   
   MM_RETURN(ZOMBIEP(ans) ? NIL : ans);
}


at *new_dy(at *name, at *(*addr)(at *))
{
   cfunction_t *f = mm_alloc(mt_cfunction);
   f->call = f->info = addr;
   f->name = name;
   f->kname = 0;
   return new_extern(&dy_class, f);
}


/* Macros for making DYs for de, df, and dm ----------------------- */

/* hook for code transform at definition */
#define TRANSFORM_DEF(q, sdx)  \
  at *define_hook = eval(at_define_hook); \
  if (define_hook) { \
    at *def = new_cons(named(sdx), q); \
    at *tdef = apply(define_hook, def); \
    q = tdef->Cdr; \
  }

#define DY_DEF(NAME) \
DY(name2(y,NAME)) \
{ \
  at *q = ARG_LIST; \
  TRANSFORM_DEF(q, enclose_in_string(NAME)); \
  ifn (CONSP(q) && CONSP(q->Cdr)) \
    RAISEF("syntax error", new_cons(named(enclose_in_string(NAME)), q)); \
  ifn (SYMBOLP(q->Car)) \
    RAISEF("syntax error in definition: not a symbol", q->Car); \
  at *symbol = q->Car; \
  at *func = name2(new_,NAME)(q->Cdr->Car, q->Cdr->Cdr); \
  sym_set((symbol_t*)symbol->Object, func, true); \
  return symbol; \
}


/* DE class -------------------------------------------	 */

at *de_listeval(at *p, at *q)
{
   MM_ENTER;

   lfunction_t *f = p->Object;
   q = eval_a_list(q->Cdr);
   push_args(f->formal_args, q);
   at *ans = progn(f->body);
   pop_args(f->formal_args);

   MM_RETURN(ans);
}

at *new_de(at *formal, at *body)
{
   lfunction_t *f = mm_alloc(mt_lfunction);
   f->formal_args = formal;
   f->body = body;
   return new_extern(&de_class, f);
}

DY(ylambda)
{
  at *q = ARG_LIST;
  ifn (CONSP(q) && CONSP(q->Cdr)) 
     RAISEFX("syntax error in function definition", NIL);
  return new_de(q->Car, q->Cdr);
}

DY_DEF(de);


/* DF class -------------------------------------------	 */

at *df_listeval(at *p, at *q)
{
   MM_ENTER;

   lfunction_t *f = p->Object;
   push_args(f->formal_args, q->Cdr);
   at *ans = progn(f->body);
   pop_args(f->formal_args);

   MM_RETURN(ans);
}

at *new_df(at *formal, at *body)
{
  lfunction_t *f = mm_alloc(mt_lfunction);
  f->formal_args = formal;
  f->body = body;
  return new_extern(&df_class, f);
}

DY(yflambda)
{
   at *q = ARG_LIST;
   ifn (CONSP(q) && CONSP(q->Cdr))
      RAISEF("illegal definition of function", NIL);
   return new_df(q->Car, q->Cdr);
}

DY_DEF(df);

/* DM class -------------------------------------------	 */


at *dm_listeval(at *p, at *q)
{
   MM_ENTER;

   lfunction_t *f = p->Object;
   push_args(f->formal_args, q);
   at *m = progn(f->body);
   pop_args(f->formal_args);

   MM_RETURN(eval(m));
}

at *new_dm(at *formal, at *body)
{
   lfunction_t *f = mm_alloc(mt_lfunction);
   f->formal_args = formal;
   f->body = body;
   return new_extern(&dm_class, f);
}

DY(ymlambda)
{
   at *q = ARG_LIST;
   ifn(CONSP(q) && CONSP(q->Cdr))
      RAISEFX("illegal definition of function", NIL);
   return new_dm(q->Car, q->Cdr);
}

DY_DEF(dm);

static at *macroexpand(at *p, at *q)
{
   ifn (p && p->Class == &dm_class)
      RAISEF("not a macro", p);

   lfunction_t *f = p->Object;
   push_args(f->formal_args, q);
   at *ans = progn(f->body);
   pop_args(f->formal_args);
   return ans;
}

DY(ymacroexpand)
{
   ifn (CONSP(ARG_LIST) && CONSP(ARG_LIST->Car) && (!ARG_LIST->Cdr))
      RAISEFX("syntax error", NIL);
   at *p = eval(ARG_LIST->Car->Car);
   return macroexpand(p, ARG_LIST->Car);
}


/* General purpose routines -------------------	 */

/*
 * function definition extraction given a lisp function, builds the list  (X
 * ARG  {..LST..} ) where  X is   'lambda', 'flambda', 'mlambda', or   'de
 * XX' , 'df XX',   'dm XX', if a name is found, ARG is the formal arguments
 * list, LST is the definition of the function.
 */

at *funcdef(at *p)
{
   char *s = NIL;
   
   if (p->Class == &de_class)
      s = "lambda";
   if (p->Class == &df_class)
      s = "flambda";
   if (p->Class == &dm_class)
      s = "mlambda";
   if (!s)
      return NIL;

   lfunction_t *f = p->Object;
   return new_cons(named(s), new_cons(f->formal_args, f->body));
}

DX(xfuncdef)
{
   ARG_NUMBER(1);
   ARG_EVAL(1);
   return funcdef(APOINTER(1));
}

DX(xfunctionp)
{
   ARG_NUMBER(1);
   ARG_EVAL(1);
   at *p = APOINTER(1);
   return FUNCTIONP(p) ? p : NIL;
}

void pre_init_function(void)
{
   if (mt_cfunction == mt_undefined)
      mt_cfunction = 
         MM_REGTYPE("cfunction", sizeof(cfunction_t),
                    clear_cfunction, mark_cfunction, 0);
   if (mt_lfunction == mt_undefined)
      mt_lfunction =
         MM_REGTYPE("lfunction", sizeof(lfunction_t),
                    clear_lfunction, mark_lfunction, 0);
}

class_t function_class;
class_t dx_class, dy_class, de_class, df_class, dm_class;

void init_function(void)
{
   if (! (dx_stack = malloc(sizeof(at *) * (DXSTACKSIZE+8))))
      abort("Not enough memory");
   pre_init_function();

   /* set up function classes */
   class_init(&function_class, false);
   class_define("FUNCTION", &function_class);
   
   class_init(&dx_class, false);
   dx_class.name = cfunc_name;
   dx_class.listeval = dx_listeval;
   dx_class.super = &function_class;
   dx_class.atsuper = function_class.backptr;
   class_define("DX",&dx_class);
   
   class_init(&dy_class, false);
   dy_class.name = cfunc_name;
   dy_class.listeval = dy_listeval;
   dy_class.super = &function_class;
   dy_class.atsuper = function_class.backptr;
   class_define("DY",&dy_class);
   
   class_init(&de_class, false);
   de_class.listeval = de_listeval;
   de_class.super = &function_class;
   de_class.atsuper = function_class.backptr;
   class_define("DE",&de_class);

   class_init(&df_class, false);
   df_class.listeval = df_listeval;
   df_class.super = &function_class;
   df_class.atsuper = function_class.backptr;
   class_define("DF",&df_class);

   class_init(&dm_class, false);
   dm_class.listeval = dm_listeval;
   dm_class.super = &function_class;
   dm_class.atsuper = function_class.backptr;
   class_define("DM",&dm_class);

   at_optional = var_define("&optional");
   at_rest = var_define("&rest");
   at_define_hook = var_define("define-hook");
   
   dy_define("lambda", ylambda);
   dy_define("de", yde);
   dy_define("flambda", yflambda);
   dy_define("df", ydf);
   dy_define("mlambda", ymlambda);
   dy_define("dm", ydm);
   dy_define("macroexpand", ymacroexpand);
   dx_define("funcdef", xfuncdef);
   dx_define("functionp", xfunctionp);
}


/* -------------------------------------------------------------
   Local Variables:
   c-file-style: "k&r"
   c-basic-offset: 3
   End:
   ------------------------------------------------------------- */