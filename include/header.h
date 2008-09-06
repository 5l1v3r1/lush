/***********************************************************************
 * 
 *  PSU Lush
 *    Copyright (C) 2005 Ralf Juengling.
 *  Derived from LUSH Lisp Universal Shell
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
 * $Id: header.h,v 1.68 2007/04/02 21:58:48 leonb Exp $
 **********************************************************************/

#ifndef HEADER_H
#define HEADER_H

#ifndef DEFINE_H
#include "define.h"
#endif

#ifndef FLTLIB_H
#include "fltlib.h"
#endif

#ifdef __cplusplus
extern "C" {
#ifndef __cplusplus
}
#endif
#endif

/* VERSION.H --------------------------------------------------- */

#define LUSH_MAJOR  50
#define LUSH_MINOR  00
#define TLOPEN_MAJOR  LUSH_MAJOR
#define TLOPEN_MINOR  LUSH_MINOR


/* MISC.H ------------------------------------------------------ */


#define ifn(s)          if(!(s))
#define forever         for(;;)
#define until(s)        while(!(s))

#if defined(__GNUC__)
#define no_return __attribute__((__noreturn__))
#endif
#ifndef no_return
#define no_return /**/
#endif

/* Actually defined in dh.h */
typedef struct dhclassdoc_s dhclassdoc_t;
typedef struct dhdoc_s dhdoc_t;

/* From main */
extern LUSHAPI int lush_argc;
extern LUSHAPI char** lush_argv;


/* Large integers:
   Lush storages and matrices on 64 bit platforms
   are limited by the size of integers (often 32 bits).
   We are fixing this in two steps:
   1- Replace all relevant occurences of 'int' by 'intg'.
   2- Change the definition of intx from 'int' to 'long'.
*/

#ifdef INTG_IS_LONG
typedef long intg;
#else
typedef int intg;
#endif

/* Dealing with errors. */

/* lifted from TOPLEVEL.H; find better solution */
typedef struct at at;
LUSHAPI void error(const char *prefix, char *text, at *suffix) no_return;

LUSHAPI char *api_translate_c2lisp(const char*);
LUSHAPI char *api_translate_lisp2c(const char*);

#define RAISE(caller, msg, p) {                    \
  if (msg) error(caller, msg, p);                  \
}
#define RAISEF(msg, p) {                           \
  if (msg)                                         \
    error(api_translate_c2lisp(__func__), msg, p); \
}
#define RAISEFX(msg, p) {                           \
  if (msg)                                         \
    error(api_translate_c2lisp(__func__)+1, msg, p); \
}

/* OS.H ---------------------------------------------------------- */


#ifdef UNIX
/* interruptions */
extern LUSHAPI int break_attempt;
LUSHAPI void lastchance(char *s) no_return;
/* unix hooks */
void init_unix(void);
void fini_unix(void);
void toplevel_unix(void);
void console_getline(char *prompt, char *buf, int size);
/* openTL entry points */
void init_user(void);
int  init_user_dll(int major, int minor);
/* replacement functions */
LUSHAPI void  filteropen(const char *cmd, FILE **pfw, FILE **pfr);
LUSHAPI void  filteropenpty(const char *cmd, FILE **pfw, FILE **pfr);
LUSHAPI FILE* unix_popen(const char *cmd, const char *mode);
LUSHAPI int   unix_pclose(FILE *f);
LUSHAPI int   unix_setenv(const char *name, const char *value);
/* cygwin */
# ifdef __CYGWIN32__
LUSHAPI void cygwin_fmode_text(FILE *f);
LUSHAPI void cygwin_fmode_binary(FILE *f);
# endif
#endif

#ifdef WIN32
/* interruptions */
extern LUSHAPI int break_attempt;
extern LUSHAPI int kill_attempt;
LUSHAPI void lastchance(char *s) no_return;
/* system override */
LUSHAPI void  win32_exit(int);
LUSHAPI int   win32_isatty(int);
LUSHAPI void  win32_user_break(char *s);
LUSHAPI FILE* win32_popen(char *, char *);
LUSHAPI int   win32_pclose(FILE *);
LUSHAPI void  win32_fmode_text(FILE *f);
LUSHAPI void  win32_fmode_binary(FILE *f);
LUSHAPI int   win32_waitproc(void *wproc);
/* console management */
LUSHAPI void console_getline(char *prompt, char *buf, int size);
/* openTL entry points */
void init_user(void);
DLLEXPORT int init_user_dll(int major, int minor);
#endif



/* AT.H -------------------------------------------------------- */

typedef struct class class_t;

extern LUSHAPI class_t class_class;
extern LUSHAPI class_t *object_class;
extern LUSHAPI class_t null_class;
extern LUSHAPI class_t cons_class;
extern LUSHAPI class_t number_class;
extern LUSHAPI class_t gptr_class;
extern LUSHAPI class_t zombie_class;

#define NUM_PTRBITS       3
#define PTRBITS(p)        ((intptr_t)(p) & ((1<<NUM_PTRBITS) - 1))
#define SET_PTRBIT(p, b)  ((void *)((intptr_t)(p) | b))
#define CLEAR_PTR(p)      ((void *)((intptr_t)(p)&~((1<<NUM_PTRBITS) - 1)))

struct at {
   struct class *class;
   int    flags;
   union {
      double  r;
      void   *p;
      struct {
         struct at *car;
         struct at *cdr;
      } cons;
   } payload;
};

#define Car     payload.cons.car
#define Cdr     payload.cons.cdr
#define Object  payload.p
#define Class   class
#define Gptr    payload.p
#define Number  payload.r


/* flags */

#define C_MARK          (1<<6)  /* Temp (bwrite) */
#define C_MULTIPLE      (1<<7)  /* Temp (bwrite) */


/* Some useful macros */

//#define LOCK(x)         { if (x) (x)->count++; }
//#define UNLOCK(x)       { if ( (x) && --((x)->count)==0 )  purge(x); }
#define ANY_CLASS       NULL

#define CONSP(x)        ((x)&&((x)->Class == &cons_class))
#define FUNCTIONP(x)    ((x)&&((x)->Class->super==&function_class))
#define LASTCONSP(x)    (CONSP(x) && !CONSP((x)->Cdr))
#define LISTP(x)        (!(x)||((x)->Class == &cons_class))
#define NUMBERP(x)	((x)&&((x)->Class == &number_class))
#define GPTRP(x)	((x)&&((x)->Class == &gptr_class))
#define OBJECTP(x)      ((x)&&((x)->Class->dispose == object_class->dispose))
#define CLASSP(x)       ((x)&&((x)->Class == &class_class))
#define SYMBOLP(x)      ((x)&&((x)->Class == &symbol_class))
#define STORAGEP(x)     ((x)&&((x)->Class->super == &abstract_storage_class))
#define INDEXP(x)       ((x)&&((x)->Class == &index_class))
#define STRINGP(x)      ((x)&&((x)->Class == &string_class))
#define ZOMBIEP(x)      ((x)&&((x)->Class == &null_class))
#define EXTERNP(x)	((x)&&!(((x)->Class == &cons_class) || \
                                ((x)->Class == &number_class) || \
                                ((x)->Class == &gptr_class)) )
#define WINDOWP(x)  ((x)&&((x)->Class == &window_class))
                         

extern LUSHAPI at *(*eval_ptr) (at*);
extern LUSHAPI at *(*argeval_ptr) (at*);

#define NEW_NUMBER(x)   new_number((real)(x))
#define NEW_GPTR(x)     new_gptr((gptr)(x))
#define eval(q)         (*eval_ptr)(q)

/*
 * The class structure defines the behavior of
 * each type of object.
 */

typedef void *dispose_func_t(void *);
struct class {
   /* class vectors */
   void*          (*dispose)      (void *);
   void           (*action)       (at*, void (*f)(at*));
   char*          (*name)         (at*);
   at*            (*selfeval)     (at*);
   at*            (*listeval)     (at*, at*);
   void           (*serialize)    (at**, int);
   int            (*compare)      (at*, at*, int);
   unsigned long  (*hash)         (at*);
   at*		  (*getslot)      (at*, at*);
   void           (*setslot)      (at*, at*, at*);
   /* class information */
   at*              classname;   /* class name */
   at*              priminame;   /* class name for binary files */
   at*              backptr;     /* back pointer to class object */
   at*              atsuper;     /* superclass object */
   struct class*    super;	 /* link to superclass */
   struct class*    subclasses;	 /* link to subclasses */
   struct class*    nextclass;	 /* next subclass of the same superclass */
   int              slotssofar;  /* number of fields */  
   at*              keylist;     /* field names */
   at*              defaults;    /* default field values */
   at*              methods;     /* alist of methods */
   struct hashelem* hashtable;   /* buckets for hashed methods */
   int              hashsize;    /* number of buckets */
   bool	    	    hashok;      /* is the hash table up-to-date */
   bool             managed;     /* non-static class (heap object) */
   bool 	    dontdelete;  /* instances should not be deleted */
   bool             live;        /* true if class is current */
   /* additional info for dhclasses */
   dhclassdoc_t    *classdoc;  
   char            *kname;
};

struct hashelem {
   at *symbol;
   at *function;
   int sofar;
};

LUSHAPI at *new_gptr(gptr x);
LUSHAPI at *new_number(double x);
LUSHAPI at *new_extern(class_t*, void *);
LUSHAPI at *new_cons(at *car, at *cdr);
LUSHAPI void zombify(at *p);
LUSHAPI void class_init(class_t *, bool);

/* LIST.H ----------------------------------------------------- */

LUSHAPI at *car(at *q);
LUSHAPI at *caar(at *q);
LUSHAPI at *cadr(at *q);
LUSHAPI at *cdr(at *q);
LUSHAPI at *cdar(at *q);
LUSHAPI at *cddr(at *q);
LUSHAPI at *rplaca(at *q, at *p);
LUSHAPI at *rplacd(at *q, at *p);
LUSHAPI at *displace(at *q, at *p);
LUSHAPI at *make_list(int n, at *v);
LUSHAPI at *deepcopy_list(at *p);
LUSHAPI int length(at *p);
LUSHAPI at *member(at *elem, at *list);
LUSHAPI at *nfirst(int n, at *l);
LUSHAPI at *nth(at *l, int n);
LUSHAPI at *nthcdr(at *l, int n);
LUSHAPI at *lasta(at *list);
LUSHAPI at *lastcdr(at *list);
LUSHAPI at *flatten(at *l);
LUSHAPI at *append(at *l1, at *l2);
LUSHAPI at *reverse(at *l);
LUSHAPI at *assoc(at *k, at *l);



/* EVAL.H ----------------------------------------------------- */

LUSHAPI at *eval_std(at *p);
LUSHAPI at *eval_debug(at *q);
LUSHAPI at *eval_nothing(at *q);
LUSHAPI at *apply(at *q, at *p);
LUSHAPI at *let(at *vardecls, at *body);
LUSHAPI at *letS(at *vardecls, at *body);
LUSHAPI at *progn(at *p);
LUSHAPI at *prog1(at *p);
LUSHAPI at *mapc(at *f, at *lists);
LUSHAPI at *mapcar(at *f, at *lists);
LUSHAPI at *mapcan(at *f, at *lists);


/* weakref.h */

typedef void wr_notify_func_t(void *, void *);
LUSHAPI void add_notifier(void *, wr_notify_func_t *, void *);
LUSHAPI void run_notifiers(void *);
LUSHAPI void del_notifiers_with_context(void *);
LUSHAPI void del_notifiers_for_target(void *);
LUSHAPI void protect(at *q);
LUSHAPI void unprotect(at *q);


/* SYMBOL.H ---------------------------------------------------- */

extern LUSHAPI class_t symbol_class;

typedef struct symbol { 	/* each symbol is an external AT which */
   short mode;		        /* points to this structure */
   struct symbol *next;
   struct hash_name *hn;
   at *value;
   at **valueptr;
} symbol_t;

#define SYMBOL_UNUSED   0
#define SYMBOL_LOCKED   2
#define SYMBOL_UNLOCKED 1

/* symbol creation */
LUSHAPI at *new_symbol(char *s);
LUSHAPI at *named(char *s);
LUSHAPI at *namedclean(char *s);
extern at *at_t; 
#define t()           at_t

LUSHAPI char *nameof(at *p);
LUSHAPI symbol_t *symbol_push(symbol_t *sym, at *q);
LUSHAPI symbol_t *symbol_pop(symbol_t *sym);
#define SYMBOL_PUSH(p, q) { at *__p__ = p; __p__->Object = symbol_push((symbol_t*)__p__->Object, q); }
#define SYMBOL_POP(p) { at *__p__ = p; __p__->Object = symbol_pop((symbol_t*)__p__->Object); }

LUSHAPI at *setq(at *p, at *q);	/* Warning: Never use the result. */
LUSHAPI at *global_names(void); 
LUSHAPI at *global_defs(void);
LUSHAPI at *oblist(void);
LUSHAPI void purge_names(void);
LUSHAPI void reset_symbols(void);
LUSHAPI void sym_set(symbol_t *s, at *q, bool in_global_scope); 
LUSHAPI void var_set(at *p, at *q);
LUSHAPI void var_SET(at *p, at *q); /* Set variable regardless of lock mode */
LUSHAPI void var_lock(at *p);
LUSHAPI at *sym_get(symbol_t *s, bool in_global_scope);
LUSHAPI at *var_get(at *p);
LUSHAPI at *var_define(char *s);



/* TOPLEVEL.H ------------------------------------------------- */

struct recur_elt { 
   struct recur_elt *next; 
   void *call; 
   at *p;
};

extern LUSHAPI struct recur_doc {
   /* hash table for detecting infinite recursion */
   int hsize;
   int hbuckets;
   struct recur_elt **htable;
} recur_doc;


extern LUSHAPI struct error_doc {	   
   /* contains info for printing error messages */
   at *this_call;
   at *error_call;
   char *error_prefix;
   char *error_text;
   at *error_suffix;
   short debug_tab;
   short ready_to_an_error;
   short debug_toplevel;
   short script_mode;
   FILE *script_file;
} error_doc;

#define ED_PUSH_CALL(p) {\
  error_doc.this_call = new_cons(p, error_doc.this_call); \
}

#define ED_POP_CALL()   {\
  at *q = error_doc.this_call; \
  error_doc.this_call = q->Cdr; \
  q->Cdr = NIL; \
}

#define SCRIPT_OFF      0
#define SCRIPT_INPUT    1
#define SCRIPT_PROMPT   2
#define SCRIPT_OUTPUT   3

/*
 * This structure is used to handle exception in the C code.
 */

extern LUSHAPI struct context {
  struct context *next;
  sigjmp_buf error_jump;
  char *input_string;
  FILE *input_file;
  short input_tab;
  short input_case_sensitive;
  FILE *output_file;
  short output_tab;
} *context;

LUSHAPI int  recur_push_ok(struct recur_elt *elt, void *call, at *p);
LUSHAPI void recur_pop(struct recur_elt *elt);
LUSHAPI void context_push(struct context *newc);
LUSHAPI void context_pop(void);
LUSHAPI void toplevel(char *in, char *out, char *new_prompt);
//LUSHAPI void error(char *prefix, char *text, at *suffix) no_return;
LUSHAPI void user_break(char *s);
LUSHAPI void init_lush (char *program_name);
LUSHAPI void start_lisp(int argc, char **argv, int quiet);
LUSHAPI void clean_up(void);
LUSHAPI void abort (char *s) no_return;

/* STRING.H ---------------------------------------------------- */

extern LUSHAPI class_t string_class;

#define SADD(obj)         ((char *)(obj))
LUSHAPI at *new_string(const char *s);
LUSHAPI at *new_string_bylen(int n);
LUSHAPI int str_index(char *s1, char *s2, int start);
LUSHAPI at *str_val(char *s);
LUSHAPI char *str_number(double x);
LUSHAPI char *str_number_hex(double x);
LUSHAPI char *str_gptr(gptr x);

LUSHAPI char *regex_compile(char *pattern, short int *bufstart, short int *bufend,
			  int strict, int *rnum);
LUSHAPI int regex_exec(short int *buffer, char *string, 
		     char **regptr, int *reglen, int nregs);
LUSHAPI int regex_seek(short int *buffer, char *string, char *seekstart, 
		     char **regptr, int *reglen, int nregs, 
		     char **start, char **end);

extern LUSHAPI char string_buffer[];
extern LUSHAPI at  *null_string; 

typedef struct large_string {
  char *p;
  char buffer[1024];
  at *backup;
  at **where;
} large_string_t;

LUSHAPI void large_string_init(large_string_t *ls);
LUSHAPI void large_string_add(large_string_t *ls, char *s, int len);
LUSHAPI at * large_string_collect(large_string_t *ls);

LUSHAPI at* str_mb_to_utf8(const char *s);
LUSHAPI at* str_utf8_to_mb(const char *s);


/* FUNCTION.H -------------------------------------------------- */

/*
 * function are implemented as external objects 
 * pointing to this structure:
 */

struct cfunction {
   int used;
   at *name;
   void *call;
   void *info;
   char *kname;
};

typedef struct cfunction cfunction_t;

struct lfunction {
   int used;
   at *formal_args;
   at *body;
};

typedef struct lfunction lfunction_t;

extern LUSHAPI class_t function_class;          /* parent of all function classes */
extern LUSHAPI class_t de_class;
extern LUSHAPI class_t df_class;
extern LUSHAPI class_t dm_class;
extern LUSHAPI class_t dx_class;		/* dx functions are external C_function */
extern LUSHAPI class_t dy_class;		/* dy functions have unflattened args. */

LUSHAPI at *new_de(at *formal, at *evaluable);
LUSHAPI at *new_df(at *formal, at *evaluable);
LUSHAPI at *new_dm(at *formal, at *evaluable);
LUSHAPI at *new_dx(at *name, at *(*addr)(int,at**));
LUSHAPI at *new_dy(at *name, at *(*addr)(at *));
LUSHAPI at *funcdef(at *f);
LUSHAPI at *eval_a_list(at *p);
LUSHAPI gptr need_error(int i, int j, at **arg_array_ptr);
LUSHAPI void arg_eval(at **arg_array, int i);
LUSHAPI void all_args_eval(at **arg_array, int i);

/* This is the interface header builder */

#define DX(Xname)  static at *Xname(int arg_number, at **arg_array)
#define DY(Yname)  static at *Yname(at *ARG_LIST)

/* Macros and functions used in argument transmission in DX functions */

#define ISNUMBER(i)	(NUMBERP(arg_array[i]))
#define ISGPTR(i)	(GPTRP(arg_array[i]))
#define ISLIST(i)	(LISTP(arg_array[i]))
#define ISCONS(i)	(CONSP(arg_array[i]))
#define ISSTRING(i)	(STRINGP(arg_array[i]))
#define ISSTORAGE(i)	(STORAGEP(arg_array[i]))
#define ISINDEX(i)	(INDEXP(arg_array[i]))
#define ISSYMBOL(i)     (SYMBOLP(arg_array[i]))
#define ISEXTERN(i) 	(EXTERNP(arg_array[i]))
//#define ISOBJECT(i)     (OBJECTP(arg_array[i]))
#define ISCLASS(i)      (CLASSP(arg_array[i]))
#define DX_ERROR(i,j)   (need_error(i,j,arg_array))

#define APOINTER(i)     ( arg_array[i] )
#define AREAL(i)        ( ISNUMBER(i) ? APOINTER(i)->Number:(long)DX_ERROR(1,i))
#define AGPTR(i)        ( ISGPTR(i) ? APOINTER(i)->Gptr:(gptr)DX_ERROR(9,i))
#define AINTEGER(i)     ( (intg) AREAL(i) )
#define AFLT(i)         ( rtoF(AREAL(i)) )
#define ALIST(i)        ( ISLIST(i) ? APOINTER(i):(at*)DX_ERROR(2,i) )
#define ACONS(i)        ( ISCONS(i) ? APOINTER(i):(at*)DX_ERROR(3,i) )
#define ASTRING(i)      ( ISSTRING(i) ? SADD( AEXTERN(i)):(char*)DX_ERROR(4,i) )
#define ASYMBOL(i)      ( ISSYMBOL(i) ? APOINTER(i)->Object:DX_ERROR(7,i) )
#define AEXTERN(i)      ( ISEXTERN(i) ? APOINTER(i)->Object:DX_ERROR(8,i) )
#define ASTORAGE(i)     ( ISSTORAGE(i) ? APOINTER(i)->Object:DX_ERROR(10,i) )
#define AINDEX(i)       ( ISINDEX(i) ? APOINTER(i)->Object:DX_ERROR(11,i) )
#define ACLASS(i)       ( ISCLASS(i) ? APOINTER(i)->Object:DX_ERROR(12,i) )

#define ARG_NUMBER(i)	if (arg_number != i)  DX_ERROR(0,i);
#define ARG_EVAL(i)	arg_eval(arg_array,i)
#define ALL_ARGS_EVAL	all_args_eval(arg_array,arg_number)


/* FILEIO.H ------------------------------------------------- */

extern LUSHAPI class_t file_R_class, file_W_class;
extern LUSHAPI char lushdir_name[];
extern LUSHAPI char file_name[];

#define OPEN_READ(f,s)  new_extern(&file_R_class,open_read(f,s))
#define OPEN_WRITE(f,s) new_extern(&file_W_class,open_write(f,s))

LUSHAPI char *cwd(char *s);
LUSHAPI at *files(char *s);
LUSHAPI bool dirp(char *s);
LUSHAPI bool filep(char *s);
LUSHAPI char *dirname(char *fname);
LUSHAPI char *basename(char *fname, char *suffix);
LUSHAPI char *concat_fname(char *from, char *fname);
LUSHAPI char *relative_fname(char *from, char *fname);
LUSHAPI void unlink_tmp_files(void);
LUSHAPI char *tmpname(char *s, char *suffix);
LUSHAPI char *search_file(char *s, char *suffixes);
LUSHAPI void test_file_error(FILE *f);
LUSHAPI FILE *open_read(char *s, char *suffixes);
LUSHAPI FILE *open_write(char *s, char *suffixes);
LUSHAPI FILE *open_append(char *s, char *suffixes);
LUSHAPI FILE *attempt_open_read(char *s, char *suffixes);
LUSHAPI FILE *attempt_open_write(char *s, char *suffixes);
LUSHAPI FILE *attempt_open_append(char *s, char *suffixes);
LUSHAPI void file_close(FILE *f);
LUSHAPI void set_script(char *s);
LUSHAPI int read4(FILE *f);
LUSHAPI int write4(FILE *f, unsigned int l);
LUSHAPI off_t file_size(FILE *f);
#ifndef HAVE_STRERROR
LUSHAPI char *strerror(int errno);
#endif

#define RFILEP(x) ((x)&&((x)->Class == &file_R_class))
#define WFILEP(x) ((x)&&((x)->Class == &file_W_class))


/* IO.H ----------------------------------------------------- */


extern LUSHAPI char *line_pos;
extern LUSHAPI char *line_buffer;
extern LUSHAPI char *prompt_string;

LUSHAPI void print_char (char c);
LUSHAPI void print_string(char *s);
LUSHAPI void print_list(at *list);
LUSHAPI void print_tab(int n);
LUSHAPI char *pname(at *l);
LUSHAPI char *first_line(at *l);
LUSHAPI char read_char(void);
LUSHAPI char next_char(void);
LUSHAPI int  ask (char *t);
LUSHAPI char *dmc(char *s, at *l);
LUSHAPI char skip_char(char *s);
LUSHAPI char skip_to_expr(void);
LUSHAPI at *read_list(void);



/* HTABLE.H ------------------------------------------------- */

extern LUSHAPI class_t htable_class;
#define HTABLEP(x)   ((x)&&((x)->Class == &htable_class))

LUSHAPI unsigned long hash_value(at *);
LUSHAPI unsigned long hash_pointer(at *);
LUSHAPI at  *new_htable(int, bool, bool);
LUSHAPI void htable_set(at *htable, at *key, at *value);
LUSHAPI at  *htable_get(at *htable, at *key);


/* CALLS.H ----------------------------------------------------- */

LUSHAPI int comp_test(at *p, at *q);
LUSHAPI int eq_test (at *p, at *q);


/* ARITH.H ----------------------------------------------------- */

extern LUSHAPI class_t complex_class;
#ifdef HAVE_COMPLEXREAL
LUSHAPI at *new_complex(complexreal z);
LUSHAPI int complexp(at*);
LUSHAPI complexreal get_complex(at*);
#endif

/* OOSTRUCT.H ----------------------------------------------------- */

typedef struct oostruct object_t;

struct oostruct {
   class_t *cl;
   int     size;
   at      *backptr;
   void    *cptr;
   struct oostructitem {at *symb, *val;} slots[];
};

LUSHAPI bool builtin_class_p(class_t *cl);
LUSHAPI at *new_ooclass(at *classname, at *superclass, at *keylist, at *defaults);
LUSHAPI void putmethod(class_t *cl, at *name, at *fun);
LUSHAPI at *new_object(class_t *cl);
LUSHAPI at *with_object(at *obj, at *f, at *q, int howmuch);
LUSHAPI at *checksend(class_t *cl, at *prop);
LUSHAPI at *send_message(at *classname, at *obj, at *method, at *args);
LUSHAPI class_t *classof(at *p);
LUSHAPI bool is_of_class(at *p, class_t *cl);
LUSHAPI void delete(at *p, bool);
LUSHAPI at *getslot(at*, at*);
LUSHAPI void setslot(at**, at*, at*);


/* MODULE.H --------------------------------------------------- */

extern LUSHAPI class_t module_class;
#define MODULEP(x)  ((x)&&(x)->Class == &module_class)

LUSHAPI void class_define(char *name, class_t *cl);
LUSHAPI void dx_define(char *name, at *(*addr) (int, at **));
LUSHAPI void dy_define(char *name, at *(*addr) (at *));
LUSHAPI void dxmethod_define(class_t *cl, char *name, at *(*addr) (int, at **));
LUSHAPI void dymethod_define(class_t *cl, char *name, at *(*addr) (at *));

LUSHAPI void dhclass_define(char *name, dhclassdoc_t *kclass);
LUSHAPI void dh_define(char *name, dhdoc_t *kname);
LUSHAPI void dhmethod_define(dhclassdoc_t *kclass, char *name, dhdoc_t *kname);

LUSHAPI void check_primitive(at *prim, void *info);
LUSHAPI at *find_primitive(at *module, at *name);
LUSHAPI at *module_list(void);
LUSHAPI at *module_load(char *filename, at *hook);
LUSHAPI void module_unload(at *atmodule);


/* DATE.H ----------------------------------------------------- */

#define DATE_YEAR       0
#define DATE_MONTH      1
#define DATE_DAY        2
#define DATE_HOUR       3
#define DATE_MINUTE     4
#define DATE_SECOND     5

extern class_t date_class;

LUSHAPI char *str_date( at *p, int *pfrom, int *pto );
LUSHAPI at *new_date( char *s, int from, int to );
LUSHAPI at *new_date_from_time( void *clock, int from, int to );

/* BINARY.H ----------------------------------------------------- */

enum serialize_action {
  SRZ_SETFL, 
  SRZ_CLRFL,
  SRZ_WRITE,
  SRZ_READ
};

LUSHAPI int bwrite(at *p, FILE *f, int opt);
LUSHAPI at *bread(FILE *f, int opt);

/* serialization functions */
LUSHAPI void serialize_char(char *data, int code);
LUSHAPI void serialize_short(short int *data, int code);
LUSHAPI void serialize_int(int *data, int code);
LUSHAPI void serialize_size(size_t *data, int code);
LUSHAPI void serialize_offset(ptrdiff_t *data, int code);
LUSHAPI void serialize_string(char **data, int code, int maxlen);
LUSHAPI void serialize_chars(void **data, int code, int len);
LUSHAPI void serialize_flt(flt *data, int code);
LUSHAPI void serialize_real(real *data, int code);
LUSHAPI void serialize_float(float *data, int code);
LUSHAPI void serialize_double(double *data, int code);
LUSHAPI int  serialize_atstar(at **data, int code);
LUSHAPI FILE *serialization_file_descriptor(int code);

/* NAN.H -------------------------------------------------------- */

LUSHAPI flt getnanF (void);
LUSHAPI int isnanF(flt x);
LUSHAPI flt infinityF (void);
LUSHAPI int isinfF(flt x);
LUSHAPI real getnanD (void);
LUSHAPI int  isnanD(real x);
LUSHAPI real infinityD (void);
LUSHAPI int  isinfD(real x);


/* STORAGE.H --------------------------------------------------- */

/* 
 * The field 'type' of a storage defines the type of the elements.
 * Integer type ST_ID should be used for indexing.
 */
  
typedef enum storage_type {
   ST_AT,
   ST_F, ST_D,
   ST_I32, ST_I16, ST_I8, ST_U8,
   ST_GPTR,
   /* TAG */
   ST_LAST
} storage_type_t;

#define ST_FIRST      ST_AT
#define ST_ID         ST_I32
#define ST_FLOAT      ST_F
#define ST_DOUBLE     ST_D
#define ST_INT        ST_I32
#define ST_SHORT      ST_I16
#define ST_BYTE       ST_I8
#define ST_UBYTE      ST_U8
#define id_t          int

/*
 * The other flags define the
 * nature of the storage (STS)
 */

#define STS_MALLOC    (1<<1)	/* in memory via malloc */
#define STS_MMAP      (1<<2)	/* mapped via mmap */
#define STS_STATIC    (1<<5)	/* in data segment */
#define STF_RDONLY    (1<<15)	/* read only storage */

#define SRG_FIELDS \
   short  flags;   \
   short  type;    \
   size_t size;    \
   gptr   data

struct srg {
   SRG_FIELDS;
};

struct storage {
   SRG_FIELDS;
   at     *backptr;        /* pointer to the at referencing this srg*/
   void   *cptr;           /* pointer to cside representation */
#ifdef HAVE_MMAP
   gptr   mmap_addr;
   size_t mmap_len;
#ifdef WIN32
   gptr   mmap_xtra;
#endif
#endif
};

typedef struct storage  storage_t;

extern LUSHAPI class_t abstract_storage_class;
extern LUSHAPI class_t storage_class[ST_LAST];
extern LUSHAPI size_t  storage_sizeof[ST_LAST];
extern LUSHAPI flt   (*storage_getf[ST_LAST])(gptr, size_t);
extern LUSHAPI void  (*storage_setf[ST_LAST])(gptr, size_t, flt);
extern LUSHAPI real  (*storage_getr[ST_LAST])(gptr, size_t);
extern LUSHAPI void  (*storage_setr[ST_LAST])(gptr, size_t, real);
extern LUSHAPI at *  (*storage_getat[ST_LAST])(storage_t *, size_t);
extern LUSHAPI void  (*storage_setat[ST_LAST])(storage_t *, size_t, at *);

/* storage creation */
LUSHAPI storage_t *new_storage(storage_type_t);
LUSHAPI storage_t *make_storage(storage_type_t, size_t, at*);
#define NEW_STORAGE(t)        (new_storage(t)->backptr)
#define MAKE_STORAGE(t, n, i) (make_storage(t, n, i)->backptr)

/* storage properties */
LUSHAPI bool   storage_classp(const at*);
LUSHAPI bool   storage_readonlyp(const storage_t *);
LUSHAPI size_t storage_nelems(const storage_t *);
LUSHAPI size_t storage_nbytes(const storage_t *);

/* storage manipulation */
LUSHAPI void storage_malloc(storage_t*, size_t, at*);
LUSHAPI void storage_realloc(storage_t*, size_t, at*);
LUSHAPI void storage_clear(storage_t*, at*, size_t);
LUSHAPI void storage_mmap(storage_t*, FILE*, size_t);
LUSHAPI void storage_load(storage_t*, FILE*);
LUSHAPI void storage_save(storage_t*, FILE*);

/* INDEX.H ---------------------------------------------- */

#define MAXDIMS 7

extern LUSHAPI class_t index_class;

/* The "light" idx structure */

struct idx {	
   int ndim;
   size_t *dim;
   ptrdiff_t *mod;
   ptrdiff_t offset;	
   struct srg *srg;
};

#define IDX_BASE(idx)   (gptr) ((char *) (idx)->srg->data + \
	                (idx)->offset * storage_sizeof[(idx)->srg->type])
#define IDX_BASE_TYPED(idx, Type) \
                        (((Type *)((idx)->srg->data)) + (idx)->offset)
#define IDX_DATA_PTR  IDX_BASE

/* The "heavy" index structure */

typedef struct index {			
   /* Field names are similar to those of the  idx structure. */
   /* IDX macros work on index structures! */
   
   int ndim;			/* number of dimensions */
   size_t dim[MAXDIMS];		/* array size for each dimension */
   ptrdiff_t mod[MAXDIMS];      /* stride for each dimension */
   ptrdiff_t offset;		/* in element size */
   storage_t  *st;		/* a pointer to the storage */
   struct idx *cptr;            /* struct idx for the C side (lisp_c) */
   at *backptr;                 /* back reference to at */
} index_t;

/* shape_t and subscript_t are used as argument types in the index 
   C API. shape_t coincides with the head of the index structure. */

typedef struct shape {
  int ndims;
  size_t  dim[MAXDIMS];
} shape_t;

typedef struct subscript {
  int ndims;
  ptrdiff_t dim[MAXDIMS];
} subscript_t;

#define IND_ST(ind)        (((index_t *)ind)->st)
#define IND_ATST(ind)      (((index_t *)ind)->st->backptr)
#define IND_STTYPE(ind)    (IND_ST(ind)->type)
#define IND_STNELEMS(ind)  (IND_ST(ind)->size)
#define IND_SHAPE(ind)     ((shape_t *)ind)
#define IND_NDIMS(ind)     ((ind)->ndim)
#define IND_DIM(ind, n)   ((ind)->dim[n])
#define IND_MOD(ind, n)   ((ind)->mod[n])
#define IND_BASE(ind)      (gptr) ((char *) IND_ST(ind)->data + \
                           (ind)->offset * storage_sizeof[IND_STTYPE(ind)])
#define IND_BASE_TYPED(ind, T) ((T *)(IND_ST(ind)->data) + (ind)->offset)
#define IND_UNSIZEDP(ind) (IND_NDIMS(ind)==-1)
#define IDX_UNSIZEDP(idx) IND_UNSIZEDP(idx)
LUSHAPI size_t index_nelems(const index_t*);

/* Function related to <struct index> objects */

/* index and array creation */
LUSHAPI index_t *new_index(storage_t*, shape_t*);
LUSHAPI index_t *new_index_for_cdata(storage_type_t, shape_t *, void*);
LUSHAPI index_t *make_array(storage_type_t, shape_t*, at*);
LUSHAPI index_t *clone_array(index_t*);
LUSHAPI index_t *copy_index(index_t*);
LUSHAPI index_t *copy_array(index_t*);

#define NEW_INDEX(st, shp) (new_index(st, shp)->backptr)
#define MAKE_ARRAY(t, shp, i) (make_array(t, shp, i)->backptr)
#define CLONE_ARRAY(ind)  MAKE_ARRAY(IND_STTYPE(ind), IND_SHAPE(ind), NIL)
#define COPY_INDEX(ind) (copy_index(ind)->backptr)
#define COPY_ARRAY(ind) (copy_array(ind)->backptr)

/* argument processing */
#define DOUBLE_ARRAY_P(p) (INDEXP(p) && IND_STTYPE(p->Object)==ST_DOUBLE)
#define INT_ARRAY_P(p)  (INDEXP(p) && IND_STTYPE(p->Object)==ST_INT)
LUSHAPI index_t *as_contiguous_array(index_t *ind);
LUSHAPI index_t *as_double_array(at *arg);
LUSHAPI index_t *as_int_array(at *arg);

/* index predicates */
LUSHAPI bool index_numericp(const index_t *);
LUSHAPI bool index_readonlyp(const index_t *);
LUSHAPI bool index_emptyp(const index_t *);
LUSHAPI bool index_contiguousp(const index_t*);
LUSHAPI bool index_broadcastable_p(index_t*, index_t*);

/* dealing with shapes */
LUSHAPI bool   shape_equalp(shape_t *, shape_t *);
LUSHAPI size_t shape_nelems(shape_t*);
LUSHAPI shape_t *shape_copy(shape_t*, shape_t*);
LUSHAPI shape_t* shape_set(shape_t*, int, size_t, size_t, size_t, size_t);
#define SHAPE0D                 shape_set(NIL, 0, 0, 0, 0, 0)
#define SHAPE1D(d1)             shape_set(NIL, 1, d1, 0, 0, 0)
#define SHAPE2D(d1, d2)         shape_set(NIL, 2, d1, d2, 0, 0)
#define SHAPE3D(d1, d2, d3)     shape_set(NIL, 3, d1, d2, d3, 0)
#define SHAPE4D(d1, d2, d3, d4) shape_set(NIL, 4, d1, d2, d3, d4)

/* index manipulation */
LUSHAPI index_t *index_reshape(index_t*, shape_t*);
LUSHAPI index_t *index_ravel(index_t *);
LUSHAPI index_t *index_trim(index_t*, int d, ptrdiff_t nz, size_t ne);
LUSHAPI index_t *index_trim_to_shape(index_t*, shape_t*);
LUSHAPI index_t *index_extend(index_t*, int d, ptrdiff_t ne);
LUSHAPI index_t *index_extendS(index_t*, subscript_t*);
LUSHAPI index_t *index_expand(index_t*, int d, size_t ne);
LUSHAPI index_t *index_shift(index_t*, int d, ptrdiff_t ne);
LUSHAPI index_t *index_shiftS(index_t*, subscript_t*);
LUSHAPI index_t *index_select(index_t*, int d, ptrdiff_t n);
LUSHAPI index_t *index_transpose(index_t*, shape_t*);
LUSHAPI index_t *index_reverse(index_t*, int d);
LUSHAPI shape_t *index_broadcast2(index_t*, index_t*, index_t**, index_t**);

/* in-place index manipulation */
LUSHAPI index_t *index_reshapeD(index_t*, shape_t *);
LUSHAPI index_t *index_trimD(index_t*, int d, ptrdiff_t nz, size_t ne);
LUSHAPI index_t *index_trim_to_shapeD(index_t*, shape_t*);
LUSHAPI index_t *index_extendD(index_t*, int d, ptrdiff_t ne);
LUSHAPI index_t *index_extendSD(index_t*, subscript_t*);
LUSHAPI index_t *index_expandD(index_t*, int d, size_t ne);
LUSHAPI index_t *index_shiftD(index_t*, int d, ptrdiff_t ne);
LUSHAPI index_t *index_shiftSD(index_t*, subscript_t*);
LUSHAPI index_t *index_reverseD(index_t*, int d);

LUSHAPI void easy_index_check(index_t*, shape_t*);
//LUSHAPI real easy_index_get(index_t*, size_t*);
//LUSHAPI void easy_index_set(index_t*, size_t*, real);

/* Functions related to <struct idx> objects */
LUSHAPI void index_read_idx(index_t*, struct idx *);
LUSHAPI void index_write_idx(index_t*, struct idx *);
LUSHAPI void index_rls_idx(index_t*, struct idx *);

/* Other functions */
LUSHAPI index_t *index_copy(index_t *, index_t *);
LUSHAPI index_t *array_copy(index_t *, index_t *);
LUSHAPI void     array_swap(index_t*, index_t*);
LUSHAPI index_t *array_extend(index_t*, int d, ptrdiff_t ne, at* init);
LUSHAPI index_t *array_select(index_t*, int d, ptrdiff_t n);

LUSHAPI void import_raw_matrix(index_t*, FILE*, size_t);
LUSHAPI void import_text_matrix(index_t*, FILE*);
LUSHAPI int  save_matrix_len (index_t*);
LUSHAPI void save_matrix(index_t*, FILE*);
LUSHAPI void export_matrix(index_t*, FILE*);
LUSHAPI void array_export(index_t*, FILE*);
LUSHAPI void save_ascii_matrix(index_t*, FILE*);
LUSHAPI void export_ascii_matrix(index_t*, FILE*);
LUSHAPI at  *load_matrix(FILE*);


/* 
 * Loops over all elements of idx <idx>
 * The variable <ptr> is an offset
 * referencing each element of <idx>.
 * It is incremented by the loop, over all the idx.
 */

#define begin_idx_aloop1(idx,ptr) { 					     \
  ptrdiff_t _d_[MAXDIMS]; 						     \
  ptrdiff_t ptr = 0;							     \
  int _j_;                                                                   \
  bool emptyp = false;                                                       \
  for (_j_=0;_j_<(idx)->ndim; _j_++ ) 					     \
    { _d_[_j_]=0; emptyp = emptyp || (idx)->dim[_j_] == 0; }                 \
  _j_ = emptyp ? -1 : (idx)->ndim;					     \
  while (_j_>=0) {

#define end_idx_aloop1(idx,ptr) 					     \
    _j_--; 								     \
    do { 								     \
      if (_j_<0) break; 						     \
      if (++_d_[_j_] < (idx)->dim[_j_]) {				     \
	ptr+=(idx)->mod[_j_];						     \
	_j_++;								     \
      } else { 								     \
	ptr-=(idx)->dim[_j_]*(idx)->mod[_j_]; 				     \
	_d_[_j_--] = -1; 						     \
      } 								     \
    } while (_j_<(idx)->ndim);  					     \
  } 									     \
}

/* 
 * Independently loops over all elements of both idxs <idx1> and <idx2>
 * The variables <ptr1> and <ptr2> are offsets
 * referencing the element of <idx1> and <idx2>.
 * Idxs <idx1> and <idx2> don't need to have the same structure,
 * but they must have the same number of elements.
 */

#define begin_idx_aloop2(idx1, idx2, ptr1, ptr2) { 			     \
  ptrdiff_t _d1_[MAXDIMS]; 						     \
  ptrdiff_t _d2_[MAXDIMS];						     \
  ptrdiff_t ptr1 = 0;							     \
  ptrdiff_t ptr2 = 0;							     \
  int _j1_, _j2_;                                                            \
  bool emptyp1 = false;                                                      \
  bool emptyp2 = false;                                                      \
  for (_j1_=0;_j1_<(idx1)->ndim; _j1_++ ) 				     \
    { _d1_[_j1_]=0; emptyp1 = emptyp1 || (idx1)->dim[_j1_] == 0; }           \
  for (_j2_=0;_j2_<(idx2)->ndim; _j2_++ ) 				     \
    { _d2_[_j2_]=0; emptyp2 = emptyp2 || (idx2)->dim[_j2_] == 0; }           \
  _j1_ = emptyp1 ? -1 : (idx1)->ndim;					     \
  _j2_ = emptyp2 ? -1 : (idx2)->ndim;					     \
  while (_j1_>=0 && _j2_>=0) {
    
#define end_idx_aloop2(idx1, idx2, ptr1,ptr2) 				     \
    _j1_--;								     \
    _j2_--; 								     \
    do { 								     \
      if (_j1_<0) 							     \
	break; 								     \
      if (++_d1_[_j1_] < (idx1)->dim[_j1_]) {				     \
	ptr1 += (idx1)->mod[_j1_];					     \
	_j1_++;								     \
      } else { 								     \
	ptr1 -= (idx1)->dim[_j1_]*(idx1)->mod[_j1_];			     \
	_d1_[_j1_--] = -1; 						     \
      } 								     \
    } while (_j1_<(idx1)->ndim); 					     \
    do { 								     \
      if (_j2_<0) break; 						     \
      if (++_d2_[_j2_] < (idx2)->dim[_j2_]) {				     \
	ptr2 += (idx2)->mod[_j2_];					     \
	_j2_++;								     \
      } else { 								     \
	ptr2 -= (idx2)->dim[_j2_]*(idx2)->mod[_j2_]; 			     \
	_d2_[_j2_--] = -1; 						     \
      } 								     \
    } while (_j2_<(idx2)->ndim); 					     \
  } 									     \
}


/* 
 * Independently loops over all elements of both idxs <idx0>,<idx1> and <idx2>
 * The variables <ptr0><ptr1> and <ptr2> are offsets
 * referencing the elements of <idx0><idx1> and <idx2>.
 * Idxs <idx0><idx1> and <idx2> don't need to have the same structure,
 * but they must have the same number of elements.
 */

#define begin_idx_aloop3(idx0, idx1, idx2, ptr0, ptr1, ptr2) { 	             \
  ptrdiff_t _d0_[MAXDIMS]; 						     \
  ptrdiff_t _d1_[MAXDIMS]; 						     \
  ptrdiff_t _d2_[MAXDIMS]; 						     \
  ptrdiff_t ptr0 = 0;						     	     \
  ptrdiff_t ptr1 = 0;						     	     \
  ptrdiff_t ptr2 = 0;							     \
  int _j0_, _j1_, _j2_;                                                      \
  bool emptyp0 = false;                                                      \
  bool emptyp1 = false;                                                      \
  bool emptyp2 = false;                                                      \
  for (_j0_=0;_j0_<(idx0)->ndim; _j0_++ ) 				     \
    { _d0_[_j0_]=0; emptyp0 = emptyp0 || (idx0)->dim[_j0_] == 0; }           \
  for (_j1_=0;_j1_<(idx1)->ndim; _j1_++ ) 				     \
    { _d1_[_j1_]=0; emptyp1 = emptyp1 || (idx1)->dim[_j1_] == 0; }           \
  for (_j2_=0;_j2_<(idx2)->ndim; _j2_++ ) 				     \
    { _d2_[_j2_]=0; emptyp2 = emptyp2 || (idx2)->dim[_j2_] == 0; }           \
  _j1_ = emptyp0 ? -1 : (idx0)->ndim;					     \
  _j1_ = emptyp1 ? -1 : (idx1)->ndim;					     \
  _j2_ = emptyp2 ? -1 : (idx2)->ndim;					     \
  while (_j0_>=0 && _j1_>=0 && _j2_>=0) {
    
#define end_idx_aloop3(idx0, idx1, idx2, ptr0, ptr1,ptr2) 		     \
    _j0_--;								     \
    _j1_--;								     \
    _j2_--; 								     \
    do { 								     \
      if (_j0_<0) 							     \
	break; 								     \
      if (++_d0_[_j0_] < (idx0)->dim[_j0_]) {				     \
	ptr0 += (idx0)->mod[_j0_];					     \
	_j0_++;								     \
      } else { 								     \
	ptr0 -= (idx0)->dim[_j0_]*(idx0)->mod[_j0_];			     \
	_d0_[_j0_--] = -1; 						     \
      } 								     \
    } while (_j0_<(idx0)->ndim); 					     \
    do {                                                                     \
      if (_j1_<0) 							     \
	break; 								     \
      if (++_d1_[_j1_] < (idx1)->dim[_j1_]) {				     \
	ptr1 += (idx1)->mod[_j1_];					     \
	_j1_++;								     \
      } else { 								     \
	ptr1 -= (idx1)->dim[_j1_]*(idx1)->mod[_j1_];			     \
	_d1_[_j1_--] = -1; 						     \
      } 								     \
    } while (_j1_<(idx1)->ndim); 					     \
    do { 								     \
      if (_j2_<0) break; 						     \
      if (++_d2_[_j2_] < (idx2)->dim[_j2_]) {				     \
	ptr2 += (idx2)->mod[_j2_];					     \
	_j2_++;								     \
      } else { 								     \
	ptr2 -= (idx2)->dim[_j2_]*(idx2)->mod[_j2_]; 			     \
	_d2_[_j2_--] = -1; 						     \
      } 								     \
    } while (_j2_<(idx2)->ndim); 					     \
  } 									     \
}




/* CHECK_FUNC.H ---------------------------------------------- */

#ifndef CHECK_FUNC_H
#include "check_func.h"
#endif



/* DH.H -------------------------------------------------- */

/*
 * DH are C functions working on matrices and numbers.
 * They may be compiled using 'dh-compile'.
 */

extern LUSHAPI class_t dh_class;

LUSHAPI at *new_dh(at *name, dhdoc_t *kdata);
LUSHAPI at *new_dhclass(at *name, dhclassdoc_t *kdata);



/* LISP_C.H ---------------------------------------------- */

LUSHAPI int  lside_mark_unlinked(gptr);
LUSHAPI void lside_destroy_item(gptr);

LUSHAPI void cside_create_str(void *cptr);
LUSHAPI void cside_create_str_gc(void *cptr);
LUSHAPI void cside_create_idx(void *cptr);
LUSHAPI void cside_create_srg(void *cptr);
LUSHAPI void cside_create_obj(void *cptr, dhclassdoc_t *);
LUSHAPI void cside_destroy_item(void *cptr);
LUSHAPI void cside_destroy_range(void *from, void *to);
LUSHAPI at * cside_find_litem(void *cptr);

LUSHAPI bool lisp_owns_p(void *cptr);  /* true when interpreter owns the object */

extern LUSHAPI int run_time_error_flag;
extern LUSHAPI jmp_buf run_time_error_jump;
LUSHAPI void run_time_error(char *s);


/* EVENT.H ----------------------------------------------------- */


/* Event sources */
LUSHAPI void  block_async_poll(void);
LUSHAPI void  unblock_async_poll(void);
LUSHAPI void  unregister_poll_functions(void *handle);
LUSHAPI void *register_poll_functions(int  (*spoll)(void), 
                                      void (*apoll)(void),
                                      void (*bwait)(void), 
                                      void (*ewait)(void), int fd );

/* Event queues */ 
LUSHAPI void *timer_add(at *handler, int delay, int period);
LUSHAPI void *timer_abs(at *handler, real date);
LUSHAPI void  timer_del(void *handle);
LUSHAPI int   timer_fire(void);
LUSHAPI void  event_add(at *handler, at *event);
LUSHAPI at   *event_peek(void);
LUSHAPI at   *event_get(void *handler, bool remove);
LUSHAPI at   *event_wait(int console);
LUSHAPI void  process_pending_events(void);

/* Compatible event queue functions */
LUSHAPI void  enqueue_event(at*, int event, int, int, int, int);
LUSHAPI void  enqueue_eventdesc(at*, int event, int, int, int, int, char*);
#define EVENT_NONE        (-1L)
#define EVENT_ASCII_MIN   (0L)
#define EVENT_ASCII_MAX   (255L)
#define EVENT_MOUSE_DOWN  (1001L)
#define EVENT_MOUSE_UP    (1002L)
#define EVENT_MOUSE_DRAG  (1003L)
#define EVENT_ARROW_UP    (1011L)
#define EVENT_ARROW_RIGHT (1012L)
#define EVENT_ARROW_DOWN  (1013L)
#define EVENT_ARROW_LEFT  (1014L)
#define EVENT_FKEY        (1015L)
#define EVENT_RESIZE      (2001L)
#define EVENT_HELP        (2002L)
#define EVENT_DELETE      (2003L)
#define EVENT_SENDEVENT   (2004L)
#define EVENT_ALARM       (3001L)
#define EVENT_EXPOSE      (4001L)
#define EVENT_GLEXPOSE    (4002L)



/* CPLUSPLUS --------------------------------------------------- */

#ifdef __cplusplus
}
#ifdef class
#undef class
#endif
#ifdef true
#undef true
#endif
#endif

#endif /* HEADER_H */


/* -------------------------------------------------------------
   Local Variables:
   c-font-lock-extra-types: (
     "FILE" "\\sw+_t" "at" "gptr" "real" "flt" "intg" )
   c-file-style: k&r
   c-basic-offset: 3
   End:
   ------------------------------------------------------------- */