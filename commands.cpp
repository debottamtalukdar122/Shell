#include<windows.h>
#include<iostream>
#include"commands.h"
#include<filesystem>
#include<vector>
#include<cstdlib>

using namespace std;
namespace fs = std::filesystem;
using namespace fs;

int cd(string path)
{
    if (path == "..")
    {
        string path = current_path().string();
        int n = path.length();
        int last_index = -1;

        for (int i = n - 1; i >= 0; i--)
        {
            if (path[i] == '\\')
            {
                last_index = i - 1;
                break;
            }
        }

        if (last_index == -1)
        {
            cout << "No parent dir exists!" << endl;
        }

        else
        {
            string newpath = path.substr(0, last_index + 1);
            current_path(newpath);
        }
    }

    else
    {
        try
        {
            string newpath = path.substr(2);
            current_path(newpath);
        }
        catch (const std::exception &e)
        {
            cerr << "cd: " << "Invalid path!" << endl;
            return 1;
        }

    }

    return 0;

    cout << "New Path: " << current_path() << endl;

}

int pwd()
{
    cout << current_path() << endl;

    return 0;
}

int status(int code,string error)
{

    if (code == -1)
    {
        cout << "Last exit code: " <<  code << endl;
    }

    else
    {
        cout << "Last exit code: " << code << endl;
    }

    return 0;
}

void clear()
{
    system("cls");
}

void help()
{
    cout << endl;
    cout << "MySH - Available Commands" << endl << endl;
    cout << "  " << "cd <path>           Changes the current working directory." << endl;
    cout << "  " << "pwd                 Print the current working directory." << endl;
    cout << "  " << "status              Display the last exit status." << endl;
    cout << "  " << "clear               Clear the console screen." << endl;
    cout << "  " << "help                Display available commands." << endl;
    cout << "  " << "exit                Exit the shell";
    cout << endl << endl;
}

int get(string name)
{
    DWORD size = GetEnvironmentVariableA(name.c_str(),nullptr,0);

    if(size == 0)
    {
        cout << "Variable not found!" << endl;
        return 1;
    }
    
    string buffer(size,'\0');

    DWORD copied = GetEnvironmentVariableA(name.c_str(),&buffer[0],size);

    if(copied == 0)
    {
        cout << "Failed to retrieve " << name << endl;
        return 1;
    }

    buffer.resize(copied);

    cout << name << " = " << buffer << endl;
    return 0;
}

int set(string name, string value)
{
    if(name == "" || value == "")
    {
        cout << "Invalid syntax of set" << endl;
        return 1;
    }

    if(SetEnvironmentVariableA(name.c_str(),value.c_str()) != 0)
    {
        cout << name << " = " << value << " set successfully!" << endl;
        return 0;
    }

    else
    {
        cout << "Failed to set " << name << " = " << value << endl;
        return 1;
    }
}

int unset(string name)
{
    if (name == "")
    {
        cout << "Invalid syntax of unset" << endl;
        return 1;
    }

    if(SetEnvironmentVariableA(name.c_str(),NULL) != 0)
    {
        cout << "Environment variable unset successfully!" << endl;
        return 0;
    }

    else
    {
        cout << "Variable not found!" << endl;
        return 1;
    }
}

int env()
{
    LPWCH envStrings = GetEnvironmentStringsW();
    if (envStrings == NULL)
    {
        std::wcerr << L"Failed to get environment strings\n";
        return 1;
    }

    LPWCH var = envStrings;
    while (*var)
    {
        std::wcout << var << L"\n";
        var += wcslen(var) + 1; // move to next string
    }

    FreeEnvironmentStringsW(envStrings);
    return 0;
}