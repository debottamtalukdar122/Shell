#ifndef COMMANDS_H
#define COMMANDS_H

using  namespace std;

int cd(string path);
int pwd();
int status(int code, string error);
void clear();
void help();
int get(string name);
int set(string name,string value);
int unset(string name);
int env();
#endif