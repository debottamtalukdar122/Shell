#include<iostream>
#include<unordered_set>
#include<sstream>
#include<vector>
#include<windows.h>
#include"commands.h"
#include"handles.h"
#include"globals.h"

using namespace std;

unordered_set<string> built_in_commands = {"exit","cd","pwd","status","clear","help","get","set","unset","env"};

DWORD last_exit_code = 0;
string last_error = "";

bool createChild(char *command, string output_destination, string input_source, string behaviour, HANDLE pipeInput = nullptr,HANDLE pipeOutput = nullptr,PROCESS_INFORMATION* process_info = nullptr)
{
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));

    bool redirect_output = TRUE;
    bool redirect_input = TRUE;

    if (output_destination == "")
    {
        redirect_output = FALSE;
    }

    if(input_source == "")
    {
        redirect_input = FALSE;
    }

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HANDLE hFile_output;
    HANDLE hFile_input;

    if (redirect_output == TRUE)
    {
        int mode = GENERIC_WRITE;
        int creation_disposition = CREATE_ALWAYS;

        if(behaviour == "append")
        {
            mode = FILE_APPEND_DATA;
            creation_disposition = OPEN_ALWAYS;
        }

        hFile_output = CreateFileA(
            output_destination.c_str(),
            mode,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa,
            creation_disposition,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        if (hFile_output == INVALID_HANDLE_VALUE)
        {
            cerr << "Failed to create file\n" << output_destination << endl;
            return false;
        }

        SetHandleInformation(hFile_output, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        
        si.hStdOutput = hFile_output;
        si.hStdError = hFile_output;
    }

    else
    {
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    }

    if (pipeOutput != nullptr)
    {
        si.hStdOutput = pipeOutput;
    }

    if (redirect_input == TRUE)
    {
        hFile_input = CreateFileA(
            input_source.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            &sa,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        if (hFile_input == INVALID_HANDLE_VALUE)
        {
            cerr << "Failed to open input file\n"
                 << input_source << endl;
            return false;
        }

        SetHandleInformation(hFile_input, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        si.hStdInput = hFile_input;
    }

    else
    {
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }

    if (pipeInput != nullptr)
    {
        si.hStdInput = pipeInput;
    }

    if (redirect_output && pipeOutput == nullptr)
    {
        si.hStdError = hFile_output;
    }

    else
    {
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    }

    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;

    BOOL status = CreateProcessA(nullptr,command,nullptr,nullptr,TRUE,CREATE_NEW_PROCESS_GROUP,nullptr,nullptr,&si,&pi);

    if(!status)
    {
        cout << "Invalid Command!" << endl;
        last_exit_code = -1;
        return false;
    }

    else
    {
        DWORD exit_code = 0;
        group_id = pi.dwProcessId;

        // Pipeline mode: return process handles to the caller.
        if (process_info != nullptr)
        {
            *process_info = pi;

            if (redirect_output)
            {
                CloseHandle(hFile_output);
            }

            if (redirect_input)
            {
                CloseHandle(hFile_input);
            }

            // Do NOT wait or close pi.hProcess / pi.hThread here.
            // The caller owns these handles in pipeline mode.
            return true;
        }

        // Normal command mode: wait for the child.
        WaitForSingleObject(pi.hProcess, INFINITE);

        GetExitCodeProcess(pi.hProcess, &exit_code);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (redirect_output)
        {
            CloseHandle(hFile_output);
        }

        if (redirect_input)
        {
            CloseHandle(hFile_input);
        }

        group_id = 2;
        last_exit_code = exit_code;

        cout << "Process finished with exit code: "
             << exit_code << endl;

        return true;
    }
}

void parseInput(string input, string& command, vector<string> &arguments)
{

    string token;
    istringstream iss(input);

    iss >> command;

    while(iss >> token)
    {
        arguments.push_back(token);
    }
}

void parseCommands(vector<string> arguments, vector<vector<string>> &subCommands)
{
    int n = arguments.size();
    vector<string> subCommand;

    for(int i = 0; i < n; i++)
    {
        if(arguments[i] != "|")
        {
            subCommand.push_back(arguments[i]);
        }

        else
        {
            subCommands.push_back(subCommand);
            subCommand.clear();
        }
    }

    subCommands.push_back(subCommand);
}

void routeCommand(string input, string command, vector<string> arguments)
{
    if (built_in_commands.find(command) == built_in_commands.end())
    {
        int n = arguments.size();
        vector<int> pipePositions;

        for (int i = 0; i < n; i++)
        {
            if (arguments[i] == "|")
            {
                pipePositions.push_back(i);
            }
        }

        if (!pipePositions.empty())
        {
                vector<vector<string>> subCommands;
                parseCommands(arguments, subCommands);

                // Add the first command, which is stored separately
                subCommands[0].insert(
                    subCommands[0].begin(),
                    command);

                // Validate pipeline syntax
                for (const auto &subCommand : subCommands)
                {
                    if (subCommand.empty())
                    {
                        cout << "Invalid pipe syntax!" << endl;
                        last_exit_code = 1;
                        return;
                    }
                }

                if (subCommands.size() != pipePositions.size() + 1)
                {
                    cout << "Invalid pipe syntax!" << endl;
                    last_exit_code = 1;
                    return;
                }

                // Reject redirection combined with pipes for now
                for (const auto &subCommand : subCommands)
                {
                    for (const string &arg : subCommand)
                    {
                        if (arg == "<" || arg == ">" || arg == ">>")
                        {
                            cout << "Redirection with pipes is not supported yet."
                                 << endl;
                            last_exit_code = 1;
                            return;
                        }
                    }
                }

                const size_t commandCount = subCommands.size();
                const size_t pipeCount = commandCount - 1;

                // Store all pipes
                vector<HANDLE> readPipes(pipeCount, nullptr);
                vector<HANDLE> writePipes(pipeCount, nullptr);

                // Store all child processes
                vector<PROCESS_INFORMATION> processes(
                    commandCount,
                    PROCESS_INFORMATION{});

                size_t launched = 0;

                // Close parent pipe handles
                auto closePipes = [&]()
                {
                    for (HANDLE &h : readPipes)
                    {
                        if (h != nullptr && h != INVALID_HANDLE_VALUE)
                        {
                            CloseHandle(h);
                            h = nullptr;
                        }
                    }

                    for (HANDLE &h : writePipes)
                    {
                        if (h != nullptr && h != INVALID_HANDLE_VALUE)
                        {
                            CloseHandle(h);
                            h = nullptr;
                        }
                    }
                };

                // Clean up if a later process fails to launch
                auto cleanupProcesses = [&]()
                {
                    closePipes();

                    for (size_t i = 0; i < launched; i++)
                    {
                        TerminateProcess(processes[i].hProcess, 1);
                    }

                    for (size_t i = 0; i < launched; i++)
                    {
                        WaitForSingleObject(
                            processes[i].hProcess,
                            INFINITE);

                        CloseHandle(processes[i].hThread);
                        CloseHandle(processes[i].hProcess);
                    }
                };

                // Create all pipes before launching any children
                SECURITY_ATTRIBUTES pipeSA{};

                pipeSA.nLength = sizeof(pipeSA);
                pipeSA.lpSecurityDescriptor = nullptr;
                pipeSA.bInheritHandle = TRUE;

                for (size_t i = 0; i < pipeCount; i++)
                {
                    if (!CreatePipe(
                            &readPipes[i],
                            &writePipes[i],
                            &pipeSA,
                            0))
                    {
                        cerr << "CreatePipe failed: "
                             << GetLastError() << endl;

                        closePipes();
                        last_exit_code = 1;
                        return;
                    }
                }

                // Build each command line
                vector<string> commandLines(commandCount);

                for (size_t i = 0; i < commandCount; i++)
                {
                    for (const string &arg : subCommands[i])
                    {
                        if (!commandLines[i].empty())
                        {
                            commandLines[i] += " ";
                        }

                        commandLines[i] += arg;
                    }
                }

                // Launch every process without waiting
                for (size_t i = 0; i < commandCount; i++)
                {
                    bool handlesReady = true;

                    // Make every pipe handle non-inheritable first
                    for (size_t j = 0; j < pipeCount; j++)
                    {
                        if (!SetHandleInformation(
                                readPipes[j],
                                HANDLE_FLAG_INHERIT,
                                0) ||
                            !SetHandleInformation(
                                writePipes[j],
                                HANDLE_FLAG_INHERIT,
                                0))
                        {
                            handlesReady = false;
                            break;
                        }
                    }

                    // This process reads from the preceding pipe
                    if (handlesReady && i > 0)
                    {
                        handlesReady = SetHandleInformation(
                            readPipes[i - 1],
                            HANDLE_FLAG_INHERIT,
                            HANDLE_FLAG_INHERIT);
                    }

                    // This process writes to the following pipe
                    if (handlesReady && i < pipeCount)
                    {
                        handlesReady = SetHandleInformation(
                            writePipes[i],
                            HANDLE_FLAG_INHERIT,
                            HANDLE_FLAG_INHERIT);
                    }

                    if (!handlesReady)
                    {
                        cerr << "Failed to configure pipe handles: "
                             << GetLastError() << endl;

                        cleanupProcesses();
                        last_exit_code = 1;
                        return;
                    }

                    HANDLE inputPipe = nullptr;
                    HANDLE outputPipe = nullptr;

                    if (i > 0)
                    {
                        inputPipe = readPipes[i - 1];
                    }

                    if (i < pipeCount)
                    {
                        outputPipe = writePipes[i];
                    }

                    bool started = createChild(
                        &commandLines[i][0],
                        "",
                        "",
                        "write",
                        inputPipe,
                        outputPipe,
                        &processes[i]);

                    if (!started)
                    {
                        cleanupProcesses();
                        last_exit_code = 1;
                        return;
                    }

                    launched++;
                }

                closePipes();

                for (size_t i = 0; i < commandCount; i++)
                {
                    DWORD result = WaitForSingleObject(
                        processes[i].hProcess,
                        5000);

                    
                }

                DWORD exitCode = 1;

                if (GetExitCodeProcess(
                        processes.back().hProcess,
                        &exitCode))
                {
                    last_exit_code = exitCode;
                }
                else
                {
                    last_exit_code = 1;
                }

                for (size_t i = 0; i < commandCount; i++)
                {
                    CloseHandle(processes[i].hThread);
                    CloseHandle(processes[i].hProcess);
                }

                return;
            

            return;
        }


        char *commandP = &input[0];

        string source = "";
        string destination = "";
        string behaviour = "write";

        for (int i = 0; i < n; i++)
        {
            if (arguments[i] == "<")
            {
                if (i + 1 >= n)
                {
                    cout << "Invalid Syntax!" << endl;
                    last_exit_code = 1;
                    return;
                }

                source = arguments[i + 1];
            }
            else if (arguments[i] == ">" ||
                     arguments[i] == ">>")
            {
                if (i + 1 >= n)
                {
                    cout << "Invalid Syntax!" << endl;
                    last_exit_code = 1;
                    return;
                }

                destination = arguments[i + 1];

                if (arguments[i] == ">>")
                {
                    behaviour = "append";
                }
            }
        }

        createChild(
            commandP,
            destination,
            source,
            behaviour);

        return;
    }

    else if (command == "exit")
    {
        cout << "Process finishes with exit code: " << 0 << endl;
        exit(0);
    }

    else if (command == "cd")
    {
        last_exit_code = cd(arguments[0]);
    }

    else if (command == "pwd")
    {
        last_exit_code = pwd();
    }

    else if (command == "status")
    {
        status(last_exit_code, "");
    }

    else if (command == "clear")
    {
        clear();
    }

    else if (command == "help")
    {
        help();
    }

    else if (command == "get")
    {
        last_exit_code = get(arguments[0]);
    }

    else if (command == "set")
    {
        string value = "";
        int n = arguments.size();

        if (arguments.size() == 0)
        {
            cout << "User error! Variable name is missing." << endl;
            last_exit_code = 1;
            return;
        }

        for (int i = 1; i < n; i++)
        {
            value.append(arguments[i]);
            value.append(" ");
        }

        if (value == "")
        {
            cout << "User error! Variable value is missing." << endl;
            last_exit_code = 1;
            return;
        }

        value.pop_back();
        last_exit_code = set(arguments[0], value);
    }

    else if (command == "unset")
    {
        if (arguments.size() == 0)
        {
            cout << "User error! Variable name is missing." << endl;
            last_exit_code = 1;
            return;
        }

        last_exit_code = unset(arguments[0]);
    }

    else if (command == "env")
    {
        last_exit_code = env();
    }
}

int main()
{

    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE))
    {
        cerr << "Failed to set handler" << endl;
        return 1;
    }

    string input;   

    while(true)
    {
        cout << "mysh>";
        getline(cin,input);

        if(!cin)
        {
            cin.clear();
            cout << endl;
            continue;
        }

        if(input == "")
        {
            continue;
        }

        string command = "";
        vector<string> arguments;

        parseInput(input,command,arguments);
        routeCommand(input,command,arguments);
    } 
    
    return 0;
}

