#pragma once
#ifndef PWX_GRAVMAT_SFMLUI_H_INCLUDED
#define PWX_GRAVMAT_SFMLUI_H_INCLUDED 1

// Font settings:
#if defined(_WIN32) || defined(_WIN64)
#  if !defined(FONT_PATH)
#    define FONT_PATH "C:\\WINDOWS\\Fonts"
#  endif // font path
#  if !defined(FONT_NAME)
#    define FONT_NAME "COUR.TTF"
#  endif // font
#  define FONT_SEP "\\"
#else
#  if !defined(FONT_PATH)
#    define FONT_PATH "/usr/share/fonts/freefont-ttf"
#  endif // font path
#  if !defined(FONT_NAME)
#    define FONT_NAME "FreeMono.ttf"
#  endif // font
#  define FONT_SEP "/"
#endif // Windows versus linux

#include "main.h"

void    cleanup  ();
void    doEvents (ENVIRONMENT * env);
double  getSimOff(double x, double y, double z, double zoom);
int32_t initSFML (ENVIRONMENT * env);
int32_t running  (ENVIRONMENT * env, int32_t *progress);
int32_t save     (ENVIRONMENT * env);
void    setSleep (float pOld, float pCur, float pMax, int32_t * toSleep, int32_t * partSleep);
void    showMsg  (ENVIRONMENT * env, const char * fmt, ...);
int32_t sorting  (ENVIRONMENT * env, int32_t *progress);
void    thrdCheck(void *xEnv);
void    thrdDraw (void *xEnv);
void    thrdGrav (void *xEnv);
void    thrdInit (void *xEnv);
void    thrdImpu (void *xEnv);
void    thrdLoad (void *xEnv);
void    thrdMove (void *xEnv);
void    thrdProj (void *xEnv);
void    thrdSort (void *xEnv);
int32_t workLoop (ENVIRONMENT * env);
void    waitLoad (ENVIRONMENT * env, const char *fmt, int32_t maxNr);
void    waitSort (ENVIRONMENT * env);
void    waitThrd (ENVIRONMENT * env, const char *msg, int32_t maxNr = 0);


#endif // PWX_GRAVMAT_SFMLUI_H_INCLUDED

