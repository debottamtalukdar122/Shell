#include<iostream>
#include"handles.h"
#include"globals.h"

using namespace std;

BOOL WINAPI CtrlHandler(DWORD ctrlType)
{
    if (ctrlType == CTRL_C_EVENT)
    {
         return TRUE;
    }

    return FALSE;
}