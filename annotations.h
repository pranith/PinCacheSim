#ifndef _ANNOTATIONS_H
#define _ANNOTATIONS_H

static int flag_annot;

#include <stdio.h>

static void ANNOTATE_SITE_BEGIN_WKR(const char *name, void *siteObj)
{
  printf("ANNOTATION SITE BEGIN\n");
  flag_annot = 0;
}

static void ANNOTATE_SITE_END_WKR(const void *siteObj)
{
  printf("ANNOTATION SITE END\n");
  flag_annot = 1;
}

static void ANNOTATE_TASK_BEGIN_WKR(char* task)
{
  flag_annot = 2;
}

static void ANNOTATE_TASK_END_WKR(char* task)
{
  flag_annot = 3;
}

void (* volatile ANNOTATE_SIMPLE_SITE_BEGIN)(const char* name, void *siteObj) = ANNOTATE_SITE_BEGIN_WKR;
void (* volatile ANNOTATE_SIMPLE_SITE_END)(const void *siteObj) = ANNOTATE_SITE_END_WKR;
void (* volatile ANNOTATE_TASK_BEGIN)(char* name) = ANNOTATE_TASK_BEGIN_WKR;
void (* volatile ANNOTATE_TASK_END)(char* name) = ANNOTATE_TASK_END_WKR;

/*
void ANNOTATE_INITIALIZE() {}
void ANNOTATE_FINALIZE(char *file_name){ printf("flag is %d\n", flag_annot);}
void ANNOTATE_LOCK_ACQUIRE(void *p) {}
void ANNOTATE_LOCK_RELEASE(void *p) {}
*/

#endif
