/*
 * stdio.h
 *
 * Ullrich von Bassewitz, 30.05.1998
 *
 */



#ifndef _STDIO_H
#define _STDIO_H



#ifndef _STDDEF_H
#  include <stddef.h>
#endif
#ifndef _STDARG_H
#  include <stdarg.h>
#endif



/* Types */
typedef struct _FILE FILE;
typedef unsigned long fpos_t;

/* Standard file descriptors */
extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

/* Standard defines */
#define _IOFBF		0
#define _IOLBF		1
#define _IONBF		2
#define BUFSIZ		256
#define EOF 	      	-1
#define FILENAME_MAX	16
#define FOPEN_MAX	8
#define L_tmpnam	(FILENAME_MAX + 1)
#define SEEK_CUR	0
#define SEEK_END	1
#define SEEK_SET	2
#define TMP_MAX		256



/* Functions */
void __fastcall__ clearerr (FILE* f);
int fclose (FILE* f);
int __fastcall__ feof (FILE* f);
int __fastcall__ ferror (FILE* f);
int __fastcall__ fflush (FILE* f);
int fgetc (FILE* f);
char* fgets (char* buf, size_t size, FILE* f);
FILE* fopen (const char* name, const char* mode);
int fprintf (FILE* f, const char* format, ...);
int fputc (int c, FILE* f);
int fputs (const char* s, FILE* f);
size_t fread (void* buf, size_t size, size_t count, FILE* f);
FILE* freopen (const char* name, const char* mode, FILE* f);
size_t fwrite (const void* buf, size_t size, size_t count, FILE* f);
int getchar (void);
char* gets (char* s);
void perror (const char* s);
int printf (const char* format, ...);
int putchar (int c);
int puts (const char* s);
int remove (const char* name);
int rename (const char* old, const char* new);
int sprintf (char* buf, const char* format, ...);
int vfprintf (FILE* f, const char* format, va_list ap);
int vprintf (const char* format, va_list ap);
int vsprintf (char* buf, const char* format, va_list ap);

#ifndef __STRICT_ANSI__
FILE* fdopen (int fd, const char* mode); 	/* Unix */
int __fastcall__ fileno (FILE* f);		/* Unix */
#endif


/* Masking macros for some functions */
#define getchar()   	fgetc (stdin)		/* ANSI */
#define putchar(c)  	fputc (c, stdout)	/* ANSI */
#define getc(f)	       	fgetc (f)     		/* ANSI */
#define putc(c, f)     	fputc (c, f)  		/* ANSI */

/* Non-standard function like macros */
#ifndef __STRICT_ANSI__
#define flushall()     		      		/* Unix */
#define unlink(name)   	remove (name) 		/* Unix */
#endif



/* End of stdio.h */
#endif



