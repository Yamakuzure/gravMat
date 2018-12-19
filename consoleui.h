#ifndef PWX_GRAVMAT_CONSOLEUI_H_INCLUDED
#define PWX_GRAVMAT_CONSOLEUI_H_INCLUDED 1
#pragma once

#include "main.h"

int32_t checkOutFileFmt (ENVIRONMENT *env);
int32_t processArguments(ENVIRONMENT *env, int argc, char * argv[]);
void    showHelp        (ENVIRONMENT *env);
void    showVersion     (ENVIRONMENT *env);
void    showVerDash     (ENVIRONMENT *env);


#endif // PWX_GRAVMAT_CONSOLEUI_H_INCLUDED

