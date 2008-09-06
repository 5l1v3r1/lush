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
 * $Id: fileio.c,v 1.27 2007/04/02 21:58:49 leonb Exp $
 **********************************************************************/


#ifdef HAVE_CONFIG_H
# include "lushconf.h"
#endif

#ifdef WIN32
# include <errno.h>
# include <windows.h>
# include <direct.h>
# include <io.h>
# include <time.h>
# include <process.h>
# include <fcntl.h>
# include <sys/types.h>
# include <sys/stat.h>
# define access _access
# define R_OK 04
# define W_OK 02
#endif

#ifdef UNIX
# include <errno.h>
# include <sys/types.h>
# include <sys/stat.h>
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
# include <fcntl.h>
# include <stdio.h>
# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
# ifdef HAVE_DIRENT_H
#  include <dirent.h>
#  define NAMLEN(dirent) strlen((dirent)->d_name)
# else
#  define dirent direct
#  define NAMLEN(dirent) (dirent)->d_namlen
#  if HAVE_SYS_NDIR_H
#   include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#   include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#   include <ndir.h>
#  endif
# endif
#endif

#include "header.h"
#include "mm.h"

/* --------- VARIABLES --------- */

static at *at_path;
static at *at_lushdir;

char file_name[FILELEN];
char lushdir_name[FILELEN];


/* --------- FILE OPERATIONS --------- */

char *cwd(char *s)
{
#ifdef UNIX
   if (s) {
      if (chdir(s)==-1)
         test_file_error(NULL);
   }
#ifdef HAVE_GETCWD
   return getcwd(string_buffer,STRING_BUFFER);
#else
   return getwd(string_buffer);
#endif
#endif
#ifdef WIN32
   char drv[2];
   if (s)
      if (_chdir(s)==-1)
         test_file_error(NULL);
   drv[0]='.'; drv[1]=0;
   GetFullPathName(drv, STRING_BUFFER, string_buffer, &s);
   return string_buffer;
#endif
}

DX(xchdir)
{
   if (arg_number!=0) {
      ARG_NUMBER(1);
      ARG_EVAL(1);
      return new_string(cwd(ASTRING(1)));
   } else
      return new_string(cwd(NULL));
}



/** files **/

at *files(char *s)
{
   at *ans = NIL;
   at **where = &ans;

#ifdef UNIX
   DIR *dirp = opendir(s);
   if (dirp) {
      struct dirent *d;
      while ((d = readdir(dirp))) {
         int n = NAMLEN(d);
         at *ats = new_string_bylen(n);
         char *s = SADD(ats->Object);
         strncpy(s, d->d_name, n); s[n] = 0;
         *where = new_cons(ats,NIL);
         where = &(*where)->Cdr;
      }
      closedir(dirp);
   }
#endif

#ifdef WIN32

   struct _finddata_t info;

   if ((s[0]=='/' || s[0]=='\\') && 
       (s[1]=='/' || s[1]=='\\') && !s[2]) {
      long hfind = GetLogicalDrives();
      strcpy(info.name,"A:\\");
      for (info.name[0]='A'; info.name[0]<='Z'; info.name[0]++)
         if (hfind & (1<<(info.name[0]-'A'))) {
            *where = new_cons(new_string(info.name),NIL);
            where = &(*where)->Cdr;
         }
   } else if (dirp(s)) {
      *where = new_cons(new_string(".."),NIL);
      where = &(*where)->Cdr;
   }
   strcpy(string_buffer,s);
   char *last = string_buffer + strlen(string_buffer);
   if (last>string_buffer && last[-1]!='/' && last[-1]!='\\')
      strcpy(last,"\\*.*");
   else 
      strcpy(last,"*.*");
   long hfind = _findfirst(string_buffer, &info);
   if (hfind != -1) {
      do {
         if (strcmp(".",info.name) && strcmp("..",info.name)) {
            *where = new_cons(new_string(info.name),NIL);
            where = &(*where)->Cdr;
         }
      } while ( _findnext(hfind, &info) != -1 );
      _findclose(hfind);
   }
#endif
   return ans;
}

DX(xfiles)
{
   if (arg_number==0)
      return files(".");
   ARG_NUMBER(1);
   ARG_EVAL(1);
   return files(ASTRING(1));
}


static int makedir(char *s)
{
#ifdef UNIX
   return mkdir(s,0777);
#endif
#ifdef WIN32
   return _mkdir(s);
#endif
}

DX(xmkdir)
{
   ARG_NUMBER(1);
   ARG_EVAL(1);
   if (makedir(ASTRING(1))!=0) 
      test_file_error(NULL);
   return NIL;
}


static int deletefile(char *s)
{
#ifdef WIN32
   if (dirp(s))
      return _rmdir(s);
   else
      return _unlink(s);
#else
   if (dirp(s))
      return rmdir(s);
   else
      return unlink(s);
#endif
}

DX(xunlink)
{
   ARG_NUMBER(1);
   ARG_EVAL(1);
   char *s = ASTRING(1);
   if (deletefile(s))
      test_file_error(NULL);
   return NIL;
}


DX(xrename)
{
   ARG_NUMBER(2);
   ALL_ARGS_EVAL;
   if (rename(ASTRING(1),ASTRING(2))<0)
      test_file_error(NULL);
   return NIL;
}


DX(xcopyfile)
{
   char buffer[4096];

   /* parse arguments */
   ARG_NUMBER(2);
   ALL_ARGS_EVAL;

   at *atfin = APOINTER(1);
   if (RFILEP(atfin)) {
      // ok
   } else if (STRINGP(atfin)) {
      atfin = OPEN_READ(SADD(atfin->Object), NULL);
   } else
      RAISEFX("not a string or file descriptor", atfin);

   at *atfout = APOINTER(2);
   if (WFILEP(atfout)) {
      // ok
   } else if (STRINGP(atfout)) {
      atfout = OPEN_WRITE(SADD(atfout->Object), NULL);
   } else
      RAISEFX("not a string or file descriptor", atfout);

   /* copy */
   FILE *fin = atfin->Object;
   FILE *fout = atfout->Object;
   int nread = 0;
   for (;;) {
      int bread = fread(buffer, 1, sizeof(buffer), fin);
      if (bread==0)
         break;
      nread += bread;
      while (bread > 0)
         bread -= fwrite(buffer, 1, bread, fout);
   }
   /* return size */
   return NEW_NUMBER(nread);
}


/* create a locking file using O_EXCL */
   
static int lockfile(char *filename)
{
#ifdef WIN32
   int fd = _open(filename, _O_RDWR|_O_CREAT|_O_EXCL, 0644);
#else
   int fd = open(filename, O_RDWR|O_CREAT|O_EXCL, 0644);
#endif
   if (fd<0) {
      if (errno==EEXIST)
         return 0;
      else
         test_file_error(NULL);
   }
   time_t tl;
   time(&tl);
#ifdef UNIX
   {
      char hname[80];
      char *user;
      gethostname(hname,79);
      if (! (user=getenv("USER")))
      if (! (user=getenv("LOGNAME")))
         user="<unknown>";  
      sprintf(string_buffer,"created by %s@%s (pid=%d)\non %s", 
              user, hname, (int)getpid(), ctime(&tl));
  }
#endif
#ifdef WIN32
   {
      char user[80];
      char computer[80];
      int size = sizeof(user);
      if (! (GetUserName(user,&size)))
         strcpy(user,"<unknown>");
      size = sizeof(computer);
      if (! (GetComputerName(computer,&size)))
         strcpy(computer,"<unknown>");
      sprintf(string_buffer,"created by %s@%s on %s", 
              user, computer, time(&tl));
  }
#endif
   write(fd, string_buffer, strlen(string_buffer));
   close(fd);
   return 1;
}

DX(xlockfile)
{
   ARG_NUMBER(1);
   ARG_EVAL(1);
   if (lockfile(ASTRING(1)))
      return t();
   else
      return NIL;
}


bool dirp(char *s)
{
#ifdef UNIX
   struct stat buf;
   if (stat(s,&buf)==0)
      if (buf.st_mode & S_IFDIR)
         return true;
#endif
   
#ifdef WIN32

   char buffer[FILELEN];
   struct _stat buf;
   if ((s[0]=='/' || s[0]=='\\') && 
       (s[1]=='/' || s[1]=='\\') && !s[2]) 
      return false;
   if (strlen(s) > sizeof(buffer) - 4)
      RAISEF(NIL, "filename too long",NIL);
   strcpy(buffer,s);
   char *last = buffer + strlen(buffer) - 1;
   if (*last=='/' || *last=='\\' || *last==':')
      strcat(last,".");
   if (_stat(buffer,&buf)==0)
      if (buf.st_mode & S_IFDIR)
         return false;
#endif
   return false;
}

DX(xdirp)
{
   ARG_NUMBER(1);
   ARG_EVAL(1);
   return dirp(ASTRING(1)) ? t() : NIL;
}


bool filep(char *s)
{
#ifdef UNIX
   struct stat buf;
   if (stat(s,&buf)==-1)
      return false;
   if (buf.st_mode & S_IFDIR) 
      return false;
#endif
#ifdef WIN32
   struct _stat buf;
   if (_stat(s,&buf)==-1)
      return false;
   if (buf.st_mode & S_IFDIR) 
      return false;
#endif
   return true;
}

DX(xfilep)
{
   ARG_NUMBER(1);
   ARG_EVAL(1);
   return filep(ASTRING(1)) ? t() : NIL;
}


DX(xfileinfo)
{

#ifdef UNIX
  struct stat buf;
  ARG_NUMBER(1);
  ARG_EVAL(1);
  if (stat(ASTRING(1),&buf)==-1)
     return NIL;
#endif
#ifdef WIN32
  struct _stat buf;
  ARG_NUMBER(1);
  ARG_EVAL(1);
  if (_stat(ASTRING(1),&buf)==-1)
     return NIL;
#endif

  at *ans = NIL;
  ans = new_cons(new_cons(named("ctime"), 
                          new_date_from_time(&buf.st_ctime, DATE_YEAR, DATE_SECOND)), 
                 ans);
  ans = new_cons(new_cons(named("mtime"), 
                          new_date_from_time(&buf.st_mtime, DATE_YEAR, DATE_SECOND)), 
                 ans);
  ans = new_cons(new_cons(named("atime"), 
                          new_date_from_time(&buf.st_atime, DATE_YEAR, DATE_SECOND)), 
                 ans);
#ifdef UNIX
  ans = new_cons(new_cons(named("gid"),   NEW_NUMBER(buf.st_gid)), ans);
  ans = new_cons(new_cons(named("uid"),   NEW_NUMBER(buf.st_uid)), ans);
  ans = new_cons(new_cons(named("nlink"), NEW_NUMBER(buf.st_nlink)), ans);
  ans = new_cons(new_cons(named("ino"),   NEW_NUMBER(buf.st_ino)), ans);
  ans = new_cons(new_cons(named("dev"),   NEW_NUMBER(buf.st_dev)), ans);
#endif  
  ans = new_cons(new_cons(named("mode"),  NEW_NUMBER(buf.st_mode)), ans);
  ans = new_cons(new_cons(named("size"),  NEW_NUMBER(buf.st_size)), ans);

  at *type = NIL;
#ifdef S_ISREG
  if (!type && S_ISREG(buf.st_mode))
     type = named("reg");
#endif
#ifdef S_ISDIR
  if (! type && S_ISDIR(buf.st_mode))
     type = named("dir");
#endif
#ifdef S_ISCHR
  if (! type && S_ISCHR(buf.st_mode))
     type = named("chr");
#endif
#ifdef S_ISBLK
  if (! type && S_ISBLK(buf.st_mode))
     type = named("blk");
#endif
#ifdef S_ISFIFO
  if (! type && S_ISFIFO(buf.st_mode))
     type = named("fifo");
#endif
#ifdef S_ISLNK
  if (! type && S_ISLNK(buf.st_mode))
    type = named("lnk");
#endif
#ifdef S_ISSOCK
  if (! type && S_ISSOCK(buf.st_mode))
     type = named("sock");
#endif
  if (type)
     ans = new_cons(new_cons(named("type"), type), ans);
  return ans;
}


/* --------- FILENAME MANIPULATION --------- */


static char *strcpyif(char *d, const char *s)
{
   if (d != s)
      return strcpy(d,s);
   return d;
}

char *dirname(char *fname)
{
#ifdef UNIX
   char *s = fname;
   char *p = 0;
   char *q = string_buffer;
   while (*s) {
      if (s[0]=='/' && s[1])
         p = s;
      s++;
   }
   if (!p) {
      if (fname[0]=='/')
         return fname;
      else
         return ".";
   }
   s = fname;
   if (p-s > STRING_BUFFER-1)
      RAISEF("filename is too long",NIL);
   do {
      *q++ = *s++;
   } while (s<p);
   *q = 0;
   return string_buffer;
#endif

#ifdef WIN32
   char *s, *p;
   char *q = string_buffer;
   /* Handle leading drive specifier */
   if (fname[0] && fname[1]==':') {
      *q++ = *fname++;
      *q++ = *fname++;
   }
   /* Search last non terminal / or \ */
   p = 0;
   s = fname;
   while (*s) {
      if (s[0]=='\\' || s[0]=='/')
         if (s[1] && s[1]!='/' && s[1]!='\\')
            p = s;
      s++;
   }
   /* Cannot find non terminal / or \ */
   if (p == 0) {
      if (q>string_buffer) {
         if (fname[0]==0 || fname[0]=='/' || fname[0]=='\\')
            return "\\\\";
         *q = 0;
         return string_buffer;
      } else {
         if (fname[0]=='/' || fname[0]=='\\')
            return "\\\\";
         else
            return ".";
      }
   }
   /* Single leading slash */
   if (p == fname) {
      strcpy(q,"\\");
      return string_buffer;
   }
   /* Backtrack all slashes */
   while (p>fname && (p[-1]=='/' || p[-1]=='\\'))
      p--;
   /* Multiple leading slashes */
   if (p == fname)
      return "\\\\";
   /* Regular case */
   s = fname;
   if (p-s > STRING_BUFFER-4)
      error(NIL,"filename is too long",NIL);
   do {
      *q++ = *s++;
   } while (s<p);
   *q = 0;
   return string_buffer;
#endif
}


DX(xdirname)
{
   ARG_NUMBER(1);
   ARG_EVAL(1);
   return new_string(dirname(ASTRING(1)));
}


char *basename(char *fname, char *suffix)
{
#ifdef UNIX
   if (strlen(fname) > STRING_BUFFER-4)
      RAISEF("filename is too long",NIL);
   char *s = strrchr(fname,'/');
   if (s)
      fname = s+1;
   /* Process suffix */
   if (suffix==0 || suffix[0]==0)
      return fname;
   if (suffix[0]=='.')
      suffix += 1;
   if (suffix[0]==0)
      return fname;
   strcpyif(string_buffer,fname);
   int sl = strlen(suffix);
   s = string_buffer + strlen(string_buffer);
   if (s > string_buffer + sl) {
      s =  s - (sl + 1);
      if (s[0]=='.' && strcmp(s+1,suffix)==0)
         *s = 0;
   }
   return string_buffer;
#endif
  
#ifdef WIN32
   char *p = fname;
   char *s = fname;
   /* Special cases */
   if (fname[0] && fname[1]==':') {
      strcpyif(string_buffer,fname);
      if (fname[2]==0)
         return string_buffer;
      string_buffer[2] = '\\'; 
      if (fname[3]==0 && (fname[2]=='/' || fname[2]=='\\'))
         return string_buffer;
   }
   /* Position p after last slash */
   while (*s) {
      if (s[0]=='\\' || s[0]=='/')
         p = s + 1;
      s++;
   }
   /* Copy into buffer */
   if (strlen(p) > STRING_BUFFER-10)
      RAISEF("filename too long",NIL);
   s = string_buffer;
   while (*p && *p!='/' && *p!='\\')
      *s++ = *p++;
   *s = 0;
   /* Process suffix */
   if (suffix==0 || suffix[0]==0)
      return string_buffer;
   if (suffix[0]=='.')
      suffix += 1;
   if (suffix[0]==0)
      return string_buffer;    
   int sl = strlen(suffix);
   if (s > string_buffer + sl) {
      s = s - (sl + 1);
      if (s[0]=='.' && stricmp(s+1,suffix)==0)
         *s = 0;
   }
   return string_buffer;
#endif
}

DX(xbasename)
{
   ALL_ARGS_EVAL;
   if (arg_number!=1) {
      ARG_NUMBER(2)
         return new_string(basename(ASTRING(1),ASTRING(2)));
   } else {
      ARG_NUMBER(1);
      return new_string(basename(ASTRING(1),NULL));
   }
}


char *concat_fname(char *from, char *fname)
{
#ifdef UNIX
   if (fname && fname[0]=='/') 
      strcpyif(string_buffer,"/");
   else if (from)
      strcpyif(string_buffer,concat_fname(NULL,from));
   else
      strcpyif(string_buffer,cwd(NULL));
   char *s = string_buffer + strlen(string_buffer);
   for (;;) {
      while (fname && fname[0]=='/')
         fname++;
      if (!fname || !fname[0]) {
         while (s>string_buffer+1 && s[-1]=='/')
            s--;
         *s = 0;
         return string_buffer;
      }
      if (fname[0]=='.') {
         if (fname[1]=='/' || fname[1]==0) {
            fname +=1;
            continue;
         }
         if (fname[1]=='.')
            if (fname[2]=='/' || fname[2]==0) {
               fname +=2;
               while (s>string_buffer+1 && s[-1]=='/')
                  s--;
               while (s>string_buffer+1 && s[-1]!='/')
                  s--;
               continue;
            }
      }
      if (s==string_buffer || s[-1]!='/')
         *s++ = '/';
      while (*fname!=0 && *fname!='/') {
         *s++ = *fname++;
         if (s-string_buffer > STRING_BUFFER-10)
            RAISEF("filename is too long",NIL);
      }
      *s = 0;
   }
#endif
   
#ifdef WIN32
   char  drv[4];
   /* Handle base */
   if (from)
      strcpyif(string_buffer, concat_fname(NULL,from));
   else
      strcpyif(string_buffer, cwd(NULL));
   char *s = string_buffer;
   if (fname==0)
      return s;
   /* Handle absolute part of fname */
   if (fname[0]=='/' || fname[0]=='\\') {
      if (fname[1]=='/' || fname[1]=='\\') {	    /* Case "//abcd" */
         s[0]=s[1]='\\'; s[2]=0;
      } else {					    /* Case "/abcd" */
         if (s[0]==0 || s[1]!=':')
            s[0] = _getdrive() + 'A' - 1;
         s[1]=':'; s[2]='\\'; s[3]=0;
      }
   } else if (fname[0] && fname[1]==':') {
      if (fname[2]!='/' && fname[2]!='\\') {	    /* Case "x:abcd"   */
         if ( toupper((unsigned char)s[0])!=toupper((unsigned char)fname[0]) || s[1]!=':') {
            drv[0]=fname[0]; drv[1]=':'; drv[2]='.'; drv[3]=0;
            GetFullPathName(drv, STRING_BUFFER, string_buffer, &s);
            s = string_buffer;
         }
         fname += 2;
      } else if (fname[3]!='/' && fname[3]!='\\') {   /* Case "x:/abcd"  */
         s[0]=toupper((unsigned char)fname[0]); s[1]=':'; s[2]='\\'; s[3]=0;
         fname += 2;
      } else {					    /* Case "x://abcd" */
         s[0]=s[1]='\\'; s[2]=0;
         fname += 2;
      }
   }
   /* Process path components */
   for (;;) {
      while (*fname=='/' || *fname=='\\')
         fname ++;
      if (*fname == 0)
         break;
      if (fname[0]=='.') {
         if (fname[1]=='/' || fname[1]=='\\' || fname[1]==0) {
            fname += 1;
            continue;
         }
         if (fname[1]=='.')
            if (fname[2]=='/' || fname[2]=='\\' || fname[2]==0) {
               fname += 2;
               strcpyif(string_buffer, dirname(string_buffer));
               s = string_buffer;
               continue;
            }
      }
      while (*s) 
         s++;
      if (s[-1]!='/' && s[-1]!='\\')
         *s++ = '\\';
      while (*fname && *fname!='/' && *fname!='\\')
         if (s-string_buffer > STRING_BUFFER-10)
            RAISEF("filename is too long",NIL);
         else
            *s++ = *fname++;
      *s = 0;
   }
   return string_buffer;
#endif
}
    
DX(xconcat_fname)
{
   ALL_ARGS_EVAL;
   if (arg_number==1)
      return new_string(concat_fname(NULL,ASTRING(1)));
   ARG_NUMBER(2);
   return new_string(concat_fname(ASTRING(1),ASTRING(2)));
}


/** relative_fname **/

char *relative_fname(char *from, char *fname)
{
   from = concat_fname(NULL,from);
   int fromlen = strlen(from);
   if (fromlen > FILELEN-1)
      return 0;
   strcpyif(file_name, from);
   from = file_name;
   fname = concat_fname(NULL,fname);
#ifdef UNIX
   if (fromlen>0 && !strncmp(from, fname, fromlen)) {
      if ( fname[fromlen]==0 )
         return ".";
      if (fname[fromlen]=='/')
         return fname + fromlen + 1;
      if (fname[fromlen-1]=='/')
         return fname + fromlen;
   }
#endif
#ifdef WIN32
   if (fromlen>3 && !strncmp(from, fname, fromlen)) {
      if ( fname[fromlen]==0 )
         return ".";
      if (fname[fromlen]=='/' || fname[fromlen]=='\\')
         return fname + fromlen + 1;
      if (fname[fromlen-1]=='/' || fname[fromlen-1]=='\\')
         return fname + fromlen;
   }
#endif
   return 0;
}

DX(xrelative_fname)
{
   ARG_NUMBER(2);
   ARG_EVAL(1);
   ARG_EVAL(2);
   char *s = relative_fname(ASTRING(1), ASTRING(2));
   if (s)
      return new_string(s);
   return NIL;
}




/* -------------- TMP FILES ------------- */


static struct tmpname {
   struct tmpname *next;
   char file[];
} *tmpnames = 0;

void clear_tmpname(struct tmpname *tn)
{
   tn->next = NULL;
}

void mark_tmpname(struct tmpname *tn)
{
   MM_MARK(tn->next);
}

bool finalize_tmpname(struct tmpname *tn, void *_)
{
   if (filep(tn->file)) {
      if (unlink(tn->file) == 0)
         tn->file[0] = '\0';
      else
         return false;
   }
   return true;
}

static mt_t mt_tmpname = mt_undefined;

void unlink_tmp_files(void)
{
   tmpnames = NULL;
   mm_collect_now();
}

char *tmpname(char *dir, char *suffix)
{
   static int uniq = 0;
   
   /* check temp directory */
   char *dot;
   if (! dirp(dir)) {
      RAISEF("invalid directory", new_string(dir));
   }
   if (! suffix)
      dot = suffix = "";
   else if (strlen(suffix)>32) {
      RAISEF("suffix is too long", new_string(suffix));
   } else
      dot = ".";
   
   /* searches free filename */
   char buffer[256];
   char *tmp;
   int fd;
   do {
#ifdef WIN32
      sprintf(buffer,"lush%d%s%s", ++uniq, dot, suffix);
      tmp = concat_fname(dir, buffer);
      fd = _open(tmp, _O_RDWR|_O_CREAT|_O_EXCL, 0644);
#else
      sprintf(buffer,"lush.%d.%d%s%s", (int)getpid(), ++uniq, dot, suffix);
      tmp = concat_fname(dir, buffer);
      fd = open(tmp, O_RDWR|O_CREAT|O_EXCL, 0644);
#endif
   } while (fd<0 && errno==EEXIST);

   /* test for error and close file */
   if (fd<0)
      test_file_error(NULL);
   close(fd);

   /* record temp file name */
   size_t size = sizeof(struct tmpname)+strlen(tmp)+1;
   struct tmpname *tn = mm_allocv(mt_tmpname, size);
   if (!tn)
      RAISEF("memory exhausted",NIL);
   tn->next = tmpnames;
   strcpy(tn->file, tmp);
   tmpnames = tn;
   return tn->file;
}


DX(xtmpname)
{
   char tempdir[256];
#ifdef WIN32
   GetTempPath(sizeof(tempdir),tempdir);
#else
   strcpy(tempdir,"/tmp");
#endif
   ALL_ARGS_EVAL;
   switch (arg_number) {
   case 0:
      return new_string(tmpname(tempdir, NULL));
   case 1:
      return new_string(tmpname(ASTRING(1), NULL));
   case 2:
      return new_string(tmpname( (APOINTER(1) ? ASTRING(1) : tempdir),
                                 (APOINTER(2) ? ASTRING(2) : NULL) ));
   default:
      ARG_NUMBER(-1);
      return NIL;
   }
}


/* --------- AUTOMATIC DIRECTORY--------- */

static char *search_lushdir(char *progname)
{
#ifdef UNIX
   assert(progname);
   if (progname[0]=='/') {
      strcpy(file_name,progname);

   } else if (strchr(progname,'/')) {
      strcpy(file_name,concat_fname(NULL,progname));

   } else { /* search along path */
      char *s1 = getenv("PATH");
      for (;;) {
         if (! (s1 && *s1))
            return 0;
         char *s2 = file_name;
         while (*s1 && *s1!=':')
            *s2++ = *s1++;
         if (*s1==':')
            s1++;
         *s2 = 0;
         strcpy(file_name, concat_fname(file_name,progname));
#ifdef DEBUG_DIRSEARCH
         printf("P %s\n",file_name);
#endif
         if (filep(file_name) && access(file_name,X_OK)==0)
            break;
      }
   }
#endif

#ifdef WIN32
   if (GetModuleFileName(GetModuleHandle(NULL), file_name, FILELEN-1)==0)
      return 0;
#ifdef DEBUG_DIRSEARCH
   printf("P %s\n",file_name);
#endif
#endif
   
   /* Tests for symlink */
#ifdef S_IFLNK
   { 
      int len;
      char buffer[FILELEN];
      while ((len=readlink(file_name,buffer,FILELEN)) > 0) {
         buffer[len]=0;
         strcpy(file_name, dirname(file_name));
         strcpy(file_name, concat_fname(file_name,buffer));
#ifdef DEBUG_DIRSEARCH
         printf("L %s\n",file_name);
#endif
      }
   }
#endif
   
   /* Searches auxilliary files */
   {
      static char *trials[] = {
         "stdenv.dump",
         "sys/stdenv.dump",
         "sys/stdenv.lshc",
         "sys/stdenv.lsh",
         "../sys/stdenv.dump",
         "../sys/stdenv.lshc",
         "../sys/stdenv.lsh",
         "../share/" PACKAGE_TARNAME "/sys/stdenv.dump",
         "../share/" PACKAGE_TARNAME "/sys/stdenv.lshc",
         "../share/" PACKAGE_TARNAME "/sys/stdenv.lsh",
         "../../sys/stdenv.dump",
         "../../sys/stdenv.lshc",
         "../../sys/stdenv.lsh",
         "../../share/" PACKAGE_TARNAME "/sys/stdenv.dump",
         "../../share/" PACKAGE_TARNAME "/sys/stdenv.lshc",
         "../../share/" PACKAGE_TARNAME "/sys/stdenv.lsh",
#ifdef DIR_DATADIR
         DIR_DATADIR "/" PACKAGE_TARNAME "/sys/stdenv.dump",
         DIR_DATADIR "/" PACKAGE_TARNAME "/sys/stdenv.lshc",
         DIR_DATADIR "/" PACKAGE_TARNAME "/sys/stdenv.lsh",
#endif
         0L,
      };
      char **st = trials;
      strcpy(file_name,dirname(file_name));
      while (*st) 
      {
         char *s = concat_fname(file_name,*st++);
#ifdef DEBUG_DIRSEARCH
         printf("D %s\n",s);
#endif
         if (filep(s))
            if (access(s,R_OK)!=-1) {
               s = dirname(s);
               s = dirname(s);
               strcpy(lushdir_name, s);
               return lushdir_name;
	    }
      }
   }
   /* Failure */
   return NULL;
}

/* --------- SEARCH PATH ROUTINES --------- */

/*
 * add_suffix
 * - adds a suffix to variable unless
 *   variable already contains a suffix.
 *   
 */

static char *add_suffix(char *q, char *suffixes)
{
   /* Trivial suffixes */
   if (!suffixes)
      return q;
   if (suffixes[0]==0 || suffixes[0]=='|')
      return q;
   
   /* Test if there is already a suffix */
   char *s = strrchr(q,'.');
   if (s)
      if (!strchr(s,'/'))
#ifdef WIN32
         if (!strchr(s,'\\'))
#endif
            return q;

   /* Test if this is a old style suffix */
   if (suffixes[0]!='.') {
      s = suffixes + strlen(suffixes);
      /* handle old macintosh syntax */
      if (strlen(suffixes)>5 && suffixes[4]=='@')
         suffixes += 5;
   } else {
      s = suffixes = suffixes + 1;
      while (*s && *s!='|')
         s++;
   }

   /* add suffix to file_name */
   if (strlen(q) + (s - suffixes) > FILELEN - 4)
      error(NIL,"Filename is too long",NIL);
   strcpy(file_name, q);
   q = file_name + strlen(file_name);
   *q++ = '.';
   strncpy(q, suffixes, s - suffixes);
   q[s-suffixes] = 0;
   return file_name;
}




/* suffixed_file
 * Tests whether the name obtained by adding suffices
 * to file_name is an existing file. The first completed
 * filename of an existing file is returned and the
 * suffix completion is written to file_name. If no
 * file is found, suffixed_file return NULL.
 */

static char *suffixed_file(char *suffices)
{
   if (!suffices) {
      /* No suffix */
      return filep(file_name) ? file_name : NULL;

   } else {
      char *s = suffices;
      char *q = file_name + strlen(file_name);
      /* -- loop over suffix string */
      while (s) {
         char *r = s;
         if (*r && *r!='|' && *r!='.')
            error(NIL,"Illegal suffix specification",
                  new_string(suffices));
         while (*r && *r!='|')
            r++;
         if (q + (r - s) + 1 > file_name + FILELEN)
            error(NIL,"File name is too long",NIL);
         strncpy(q, s, (r - s));
         *(q + (r-s)) = 0;
         s = (*r ? r+1 : NULL);
         if (filep(file_name))
            return file_name;
      }
      /* -- not found */
      return NULL;  
   }
}


/*
 * Search a file
 * - first in the current directory, 
 * - then along the path. 
 * - with the specified suffixes.
 * Returns the full filename in a static area.
 */

extern char *pname_buffer;
char *search_file(char *ss, char *suffices)
{
   char s[FILELEN];
  
   if (strlen(ss)+1 > FILELEN)
      error(NIL,"File name is too long",NIL);
   strcpy(s,ss);

   /* -- search along path */
   char *c = 0;
#ifdef UNIX
   if (*s != '/')
#endif
      if (SYMBOLP(at_path)) {
         at *q = sym_get((symbol_t *)at_path->Object, true);
         
         while (CONSP(q)) {
            if (STRINGP(q->Car)) {
               c = concat_fname(SADD(q->Car->Object), s);
               if (strlen(c)+1 > FILELEN)
                  error(NIL,"File name is too long",NIL);
               strcpy(file_name, c);
               if (suffixed_file(suffices))
                  return file_name;
            }
            q = q->Cdr;
         }
      }
   /* -- absolute filename or broken path */
   if (!c) {
      c = concat_fname(NULL, s);
      if (strlen(c)+1 > FILELEN)
         error(NIL,"File name is too long",NIL);
      strcpy(file_name,c);
      if (suffixed_file(suffices))
         return file_name;
   }
   /* -- fail */
   return NIL;
}


DX(xfilepath)
{
   char *suf = "|.lshc|.snc|.tlc|.lsh|.sn|.tl";
   ALL_ARGS_EVAL;
   if (arg_number!=1){
     ARG_NUMBER(2);
     suf = (APOINTER(2) ? ASTRING(2) : NULL);
  }
   char *ans = search_file(ASTRING(1),suf);
   if (ans)
      return new_string(ans);
   else
      return NIL;
}


/* --------------- ERROR CHECK ---------------- */

/*
 * Print the error according to variable ERRNO.
 * If a file is specified, return if there is no error on this file.
 */

int stdin_errors = 0;
int stdout_errors = 0;

void test_file_error(FILE *f)
{
   char *s = NIL;
   char buffer[200];
   
   if (f && !ferror(f)) {
      if (f == stdin)
         stdin_errors = 0;
      if (f==stdout || f==stderr)
         stdout_errors = 0;
      return;
   }
   if (f && f == error_doc.script_file) {
      file_close(f);
      error_doc.script_file = NIL;
      set_script(NIL);
      s = "SCRIPT";
   }
   if (f==stdin) {
      if (stdin_errors > 8)
         abort("ABORT -- STDIN failure");
      else {
         clearerr(stdin);
         errno = 0;
         stdin_errors++;
         return;
      }
   } else if (f==stdout || f==stderr) {
      if (stdout_errors > 8)
         abort("ABORT -- STDOUT failure");
      else {
         clearerr(stdout);      
         clearerr(stderr);
         errno = 0;
         stdout_errors++;
         return;
      }
   }
   if (errno) {
      sprintf(buffer,"%s (errno=%d)",strerror(errno),errno);
      error(s,buffer,NIL);
   }
}


/* --------- LOW LEVEL FILE FUNCTIONS --------- */

/*
 * open_read( filename, regular_suffix )
 * opens a file for reading
 */

FILE *attempt_open_read(char *s, char *suffixes)
{
  /*** spaces in name ***/
  while (isspace((int)(unsigned char)*s))
     s += 1;
  strcpyif(file_name, s);
  
  if (! strcmp(s, "$stdin"))
     return stdin;
  
  /*** pipes ***/
  if (*s == '|') {
     errno = 0;
     FILE *f = popen(s + 1, "r");
     if (f) {
        FMODE_BINARY(f);
        return f;
     } else
        return NIL;
  }
  
  /*** search and open ***/
  FILE *f;
  char *name = search_file(s, suffixes);
  if (name && ((f = fopen(name, "rb")))) {
     FMODE_BINARY(f);
     return f;
  } else
     return NULL;
}


FILE *open_read(char *s, char *suffixes)
{
   FILE *f = attempt_open_read(s,suffixes);
   if (! f) {
      test_file_error(NIL);
      RAISEF("cannot open file", new_string(s));
   }
   return f;
}


/*
 * open_write(filename,regular_suffix) 
 * opens a file for writing
 */

FILE *attempt_open_write(char *s, char *suffixes)
{
   /*** spaces in name ***/
   while (isspace((int)(unsigned char)*s))
      s += 1;
   strcpyif(file_name, s);
   
   if (! strcmp(s, "$stdout"))
      return stdout;
   if (! strcmp(s, "$stderr"))
      return stderr;

   /*** pipes ***/
   if (*s == '|') {
      errno = 0;
      FILE *f = popen(s + 1, "w");
      if (f) {
         FMODE_BINARY(f);
         return f;
      } else
         return NIL;
   }

   /*** suffix ***/
   if (access(s, W_OK) == -1) {
      s = add_suffix(s, suffixes);
      strcpy(file_name, s);
   }

   /*** open ***/
   FILE *f = fopen(s, "w"); 
   if (f) {
      FMODE_BINARY(f);
      return f;
   } else
      return NIL;
}


FILE *open_write(char *s, char *suffixes)
{
   FILE *f = attempt_open_write(s, suffixes);
   if (! f) {
      test_file_error(NIL);
      RAISEF("cannot open file", new_string(s));
   }
   return f;
}


/*
 * open_append( s,suffix ) 
 * opens a file for appending 
 * this file must exist before
 */

FILE *attempt_open_append(char *s, char *suffixes)
{
   /*** spaces in name ***/
   while (isspace((int)(unsigned char)*s))
      s += 1;
   strcpy(file_name, s);

   if (!strcmp(s, "$stdout"))
      return stdout;
   if (!strcmp(s, "$stderr"))
      return stderr;
  
   /*** pipes ***/
   if (*s == '|') {
      errno = 0;
      FILE *f = popen(s + 1, "w");
      if (f) {
         FMODE_BINARY(f);
         return f;
      } else
         return NIL;
   }

   /*** suffix ***/
   if (access(s, W_OK) == -1) {
      s = add_suffix(s, suffixes);
      strcpy(file_name, s);
   }
  
   /*** open ***/
   FILE *f = fopen(s, "a");
   if (f) {
      FMODE_BINARY(f);
      return f;
   } else
      return NIL;
}


FILE *open_append(char *s, char *suffixes)
{
   FILE *f = attempt_open_append(s,suffixes);
   if (! f) {
      test_file_error(NIL);
      error(NIL,"Cannot open file",new_string(s));
   }
   return f;
}


void file_close(FILE *f)
{
   if (f!=stdin && f!=stdout && f!=stderr && f) {
      if (pclose(f)<0 && fclose(f)<0)
         test_file_error(f);
   }
}


/* read4(f), write4(f,x) 
 * Low level function to read or write 4-bytes integers, 
 * Assuming sizeof(int)==4
 */
int read4(FILE *f)
{
   int i;
   int status = fread(&i, sizeof(int), 1, f);
   if (status != 1)
      test_file_error(f);
   return i;
}

int write4(FILE *f, unsigned int l)
{
   int status = fwrite(&l, sizeof(int), 1, f);
   if (status != 1)
      test_file_error(f);
   return l;
}

/* file_size returns the remaining length of the file
 * It causes an error when the file is not exhaustable
 */
off_t file_size(FILE *f)
{
#if HAVE_FSEEKO
   off_t x = ftello(f);
   if (fseeko(f,(off_t)0,SEEK_END))
      RAISEF("non exhaustable file (pipe or terminal ?)",NIL);
   off_t e = ftello(f);
   if (fseeko(f,(off_t)x,SEEK_SET))
      RAISEF("non rewindable file (pipe or terminal ?)",NIL);
   return e - x;
#else
   off_t x = (off_t)ftell(f);
   if(fseek(f,(long)0,SEEK_END))
      RAISEF("non exhaustable file (pipe or terminal ?)",NIL);
   off_t e = (off_t)ftell(f);
   if(fseek(f, (long)x,SEEK_SET))
      RAISEF("non rewindable file (pipe or terminal ?)",NIL);
   return e - x;
#endif
}


/* --------- FILE CLASS FUNCTIONS --------- */


static FILE *file_dispose(FILE *f)
{
   if (f != stdin && f != stdout && f) {
      if (pclose(f) < 0)
         fclose(f);
   }
   return NULL;
}

/* 
 * When an AT referencing a file is reclaimed,
 * call the file_dispose.
 */

void at_file_notify(at *p, void *_)
{
   FILE *f = (FILE *)p->Object;
   if (f) {
      p->Object = file_dispose(f);
   }
} 

/* --------- LISP LEVEL FILE MANIPULATION FUNCTIONS --------- */

/*
 * script( filename)
 * - sets the script file as 'filename'.
 */

void set_script(char *s)
{
   if (error_doc.script_file) {
      fputs("\n\n *** End of script ***\n", error_doc.script_file);
      file_close(error_doc.script_file);
   }
   error_doc.script_file = NIL;
   error_doc.script_mode = SCRIPT_OFF;
   if (s) {
      error_doc.script_file = open_write(s, "script");
      fputs("\n\n *** Start of script ***\n", error_doc.script_file);
   }
}

DX(xscript)
{
   if (!arg_number) {
      set_script(NIL);
      return NIL;

   } else {
      ARG_NUMBER(1);
      ARG_EVAL(1);
      set_script(ASTRING(1));
      return APOINTER(1);
   }
}


/*
 * (open_read	STRING)	   returns a file descriptor for reading 
 * (open_write  STRING)				     for writing 
 * (open_append STRING)				     for appending
 */

DX(xopen_read)
{
   ALL_ARGS_EVAL;
   FILE *f;
   switch (arg_number) {
   case 1:
      f = attempt_open_read(ASTRING(1), NIL);
      break;
   case 2:
      f = attempt_open_read(ASTRING(1), ASTRING(2));
      break;
   default:
      ARG_NUMBER(-1);
      return NIL;
   }

   if (f) {
      at *p = new_extern(&file_R_class, f);
      add_notifier(p, (wr_notify_func_t *)at_file_notify, NULL);
      return p;
   } else
      return NIL;
}

DX(xopen_write)
{
   FILE *f;
   
   ALL_ARGS_EVAL;
   switch (arg_number) {
   case 1:
      f = attempt_open_write(ASTRING(1), NIL);
      break;
   case 2:
      f = attempt_open_write(ASTRING(1), ASTRING(2));
      break;
   default:
      ARG_NUMBER(-1);
      return NIL;
   }

   if (f) {
      at *p = new_extern(&file_W_class, f);
      add_notifier(p, (wr_notify_func_t *)at_file_notify, NULL);
      return p;
   } else
      return NIL;
}

DX(xopen_append)
{
   FILE *f;
   
   ALL_ARGS_EVAL;
   switch (arg_number) {
   case 1:
      f = attempt_open_append(ASTRING(1), NIL);
      break;
   case 2:
      f = attempt_open_append(ASTRING(1), ASTRING(2));
      break;
   default:
      ARG_NUMBER(-1);
      return NIL;
   }
   if (f) {
      at *p = new_extern(&file_W_class, f);
      add_notifier(p, (wr_notify_func_t *)at_file_notify, NULL);
      return p;
   }
   else
      return NIL;
}




/*
 * writing - reading - appending LISP I/O FUNCTIONS
 * (reading "filename" | filedesc ( ..l1.. ) .......ln.. ) )	
 * (writing "filename" | filedesc ( ..l1.. ) ........... ) )
 */

DY(yreading)
{
   if (! (CONSP(ARG_LIST) && CONSP(ARG_LIST->Cdr)))
      RAISEFX("syntax error", NIL);
   
   at *fdesc = eval(ARG_LIST->Car);

   FILE *f;
   if (STRINGP(fdesc))
      f = open_read(SADD(fdesc->Object), NIL);
   else if (RFILEP(fdesc))
      f = fdesc->Object;
   else
      RAISEFX("file name or read descriptor expected", fdesc);

   struct context mycontext;
   context_push(&mycontext);
   context->input_tab = 0;
   context->input_case_sensitive = 0;
   context->input_string = 0;
   context->input_file = f;

   if (sigsetjmp(context->error_jump, 1)) {
      f = context->input_file;
      if (STRINGP(fdesc)) {
         if (f != stdin && f != stdout && f)
            if (pclose(f) < 0)
               fclose(f);
      }
      context->input_tab = -1;
      context->input_string = NULL;
      context_pop();
      siglongjmp(context->error_jump, -1L);
   }

   at *answer = progn(ARG_LIST->Cdr);
   if (STRINGP(fdesc))
      file_close(context->input_file);
   
   context->input_tab = -1;
   context->input_string = NULL;
   context_pop();
   return answer;
}


DY(ywriting)
{
   if (! (CONSP(ARG_LIST) && CONSP(ARG_LIST->Cdr)))
      RAISEFX("syntax error", NIL);
   
   at *fdesc = eval(ARG_LIST->Car);

   FILE *f;
   if (STRINGP(fdesc))
      f = open_write(SADD(fdesc->Object), NIL);
   else if (WFILEP(fdesc))
      f = fdesc->Object;
   else
      RAISEFX("file name or write descriptor expected", fdesc);

   struct context mycontext;
   context_push(&mycontext);
   context->output_tab = 0;
   context->output_file = f;
   
   if (sigsetjmp(context->error_jump, 1)) {
      f = context->output_file;
      if (STRINGP(fdesc)) {
         if (f != stdin && f != stdout && f)
            if (pclose(f) < 0)
               fclose(f);
      } else
         fflush(f);
      context->output_tab = -1;
      context_pop();
      siglongjmp(context->error_jump, -1L);
   }

   at *answer = progn(ARG_LIST->Cdr);
   if (STRINGP(fdesc))
      file_close(context->output_file);
   else
      fflush(context->output_file);
   context->output_tab = -1;
   context_pop();
   return answer;
}

/*
 * writing - reading - appending LISP I/O FUNCTIONS
 * (reading "filename" | filedesc ( ..l1.. ) .......ln.. ) )	
 * (writing "filename" | filedesc ( ..l1.. ) ........... ) )
 */

DY(yreading_string)
{
   ifn (CONSP(ARG_LIST) && CONSP(ARG_LIST->Cdr))
      RAISEFX("syntax error", NIL);

   at *str = eval(ARG_LIST->Car);
   
   ifn (STRINGP(str))
      RAISEFX("string expected", str);

   struct context mycontext;
   context_push(&mycontext);
   context->input_tab = 0;
   context->input_case_sensitive = 0;
   context->input_string = SADD(str->Object);
   if (sigsetjmp(context->error_jump, 1)) {
      context->input_tab = -1;
      context->input_string = NULL;
      context_pop();
      siglongjmp(context->error_jump, -1L);
   }

   at *answer = progn(ARG_LIST->Cdr);
   context->input_tab = -1;
   context->input_string = NULL;
   context_pop();
   return answer;
}



DX(xread8)
{
   ARG_NUMBER(1);
   ARG_EVAL(1);
   at *fdesc = APOINTER(1);
   ifn (RFILEP(fdesc))
      RAISEFX("read file descriptor expected", fdesc);
   FILE *f = fdesc->Object;
   return NEW_NUMBER(fgetc(f));
}





DX(xwrite8)
{
   ARG_NUMBER(2);
   ARG_EVAL(1);
   ARG_EVAL(2);
   at *fdesc = APOINTER(1);
   int x = AINTEGER(2);

   ifn (WFILEP(fdesc))
     RAISEFX("write file descriptor expected", fdesc);
   FILE *f = fdesc->Object;
   return NEW_NUMBER(fputc(x,f));
}



DX(xfsize)
{
   ARG_NUMBER(1);
   ARG_EVAL(1);
   
   at *p;
   if (ISSTRING(1)) {
      p = OPEN_READ(ASTRING(1), NULL);
   } else {
      p = APOINTER(1);
      ifn (RFILEP(p))
         RAISEFX("not a string or read descriptor", p);
   }
   return NEW_NUMBER(file_size(p->Object));
}




/* --------- INITIALISATION CODE --------- */


class_t file_R_class;
class_t file_W_class;

void init_fileio(char *program_name)
{
   mt_tmpname = 
      MM_REGTYPE("tmpname", sizeof(struct tmpname),
                 clear_tmpname, mark_tmpname, finalize_tmpname);
   MM_ROOT(tmpnames);
   
   /** SETUP PATH */
   at_path = var_define("*PATH");
   at_lushdir = var_define("lushdir");

   char *s;
   if (!(s=search_lushdir(program_name)))
      if (!(s=search_lushdir("lush")))
         abort("cannot locate library files");
#ifdef UNIX
   unix_setenv("LUSHDIR",s);
#endif
   at *q = new_string(s);
   var_set(at_lushdir, q);
   var_lock(at_lushdir);
   s = concat_fname(SADD(q->Object),"sys");
   q = new_cons(new_string(s),NIL);
   var_set(at_path, q);
   
   /* setting up classes */
   class_init(&file_R_class, false);
   file_R_class.dispose = (dispose_func_t *)file_dispose;
   class_define("FILE_RO", &file_R_class);
   
   class_init(&file_W_class, false);
   file_W_class.dispose = (dispose_func_t *)file_dispose;
   class_define("FILE_WO", &file_W_class);
   
   /* DECLARE THE FUNCTIONS */
   dx_define("chdir", xchdir);
   dx_define("dirp", xdirp);
   dx_define("filep", xfilep);
   dx_define("fileinfo", xfileinfo);
   dx_define("files", xfiles);
   dx_define("mkdir", xmkdir);
   dx_define("unlink", xunlink);
   dx_define("rename", xrename);
   dx_define("copyfile", xcopyfile);
   dx_define("lockfile", xlockfile);
   dx_define("basename",xbasename);
   dx_define("dirname", xdirname);
   dx_define("concat-fname", xconcat_fname);
   dx_define("relative-fname", xrelative_fname);
   dx_define("tmpname", xtmpname);
   dx_define("filepath", xfilepath);
   dx_define("script", xscript);
   dx_define("open-read", xopen_read);
   dx_define("open-write", xopen_write);
   dx_define("open-append", xopen_append);
   dy_define("reading", yreading);
   dy_define("writing", ywriting);
   dy_define("reading-string", yreading_string);
   dx_define("read8", xread8);
   dx_define("write8", xwrite8);
   dx_define("fsize", xfsize);
}


/* -------------------------------------------------------------
   Local Variables:
   c-file-style: "k&r"
   c-basic-offset: 3
   End:
   ------------------------------------------------------------- */