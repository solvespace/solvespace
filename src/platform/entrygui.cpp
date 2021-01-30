//-----------------------------------------------------------------------------
// Our main() function for the graphical interface.
//
// Copyright 2018 <whitequark@whitequark.org>
//-----------------------------------------------------------------------------
#include "solvespace.h"
#if defined(WIN32)
#   include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>

bool RedirectIOToConsole()
{
    bool result = true;

    // attach to parent process's console
    AttachConsole(ATTACH_PARENT_PROCESS);

    // Redirect STDIN if the console has an input handle
    HANDLE ConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
    int SystemInput = _open_osfhandle(intptr_t(ConsoleInput), _O_TEXT);
    FILE *CInputHandle = _fdopen(SystemInput, "r");
    if (freopen_s(&CInputHandle, "CONIN$", "r", stdin) != 0)
        result = false;
    else
        setvbuf(stdin, NULL, _IONBF, 0);

    // Redirect STDOUT if the console has an output handle
    HANDLE ConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    int SystemOutput = _open_osfhandle(intptr_t(ConsoleOutput), _O_TEXT);
    FILE *COutputHandle = _fdopen(SystemOutput, "w");
    if (freopen_s(&COutputHandle, "CONOUT$", "w", stdout) != 0)
        result = false;
    else
        setvbuf(stdout, NULL, _IONBF, 0);

    // Redirect STDERR if the console has an error handle
    HANDLE ConsoleError = GetStdHandle(STD_ERROR_HANDLE);
    int SystemError = _open_osfhandle(intptr_t(ConsoleError), _O_TEXT);
    FILE *CErrorHandle = _fdopen(SystemError, "w");
    if (freopen_s(&CErrorHandle, "CONOUT$", "w", stderr) != 0)
        result = false;
    else
        setvbuf(stderr, NULL, _IONBF, 0);

    // Clear the error state for each of the C++ standard streams.
    std::wcout.clear();
    std::cout.clear();
    std::wcerr.clear();
    std::cerr.clear();
    std::wcin.clear();
    std::cin.clear();

    return result;
}

#endif

using namespace SolveSpace;

int main(int argc, char** argv) {
    std::vector<std::string> args = Platform::InitCli(argc, argv);
    if(args.size() > 2 || args[1][0] == '-') {
#if defined(WIN32)
        // Windows requires setting up the standard IO handles for RunCommand
        // This didn't work from WinMain on mingw
        RedirectIOToConsole();
#endif
        if(!SolveSpace::RunCommand(args)) {
            return 1;
        }
    } else {
        Platform::InitGui(argc, argv);

        Platform::Open3DConnexion();
        SS.Init();

        if(args.size() >= 2) {
            if(args.size() > 2) {
                dbp("Only the first file passed on command line will be opened.");
            }

            SS.Load(Platform::Path::From(args.back()));
        }

        Platform::RunGui();

        Platform::Close3DConnexion();
        SS.Clear();
        SK.Clear();
        Platform::ClearGui();
    }

    return 0;
}

#if defined(WIN32)

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, INT nCmdShow) {
    return main(0, NULL);
}

#endif
