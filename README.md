Welcome to the LC-3 Simulator Simulator. This is a full simulator of the LC-3 microarchitecture, with a interface built around the Ncurses library.

This is a collaborative project between myself, Michael Fulton, Enoch Chan, and Sam Brendel, created for a TCSS 372 course at the University of Washington Tacoma. I've cloned this repository for personal reference and to revisit the source code without affecting the original repository in any way.

For more information on the LC-3 Computer and the machine's microarchitecture, feel free to browse the Wikipedia article on the LC-3:
https://en.wikipedia.org/wiki/LC-3

## Getting Started - Compilation and Execution

The Ncurses library is required to compile and run this program. For further information or download instructions, please visit:

```
https://www.gnu.org/software/ncurses/
```
for further information.

For Ubuntu and macOS machines, you can run the following command to build and run the program:

```
make && ./a.out
```

For Windows-based machines, as long as you have Make and a current GCC compiler installed, you can run the following command to build and run the program:

```
make && ./a.exe
```

Trying to run this program outside of these environments, or without neccesary dependencies, may result in errors, unexpected behavior, and/or other incompatibilities. This program was built and tested on macOS High Sierra (version 10.3.4) and Ubuntu 16.04 LTS, and the developers cannot guarantee program behavior outside of these conditions.

## Debugging

### with VSCode

VSCode is really nice for debugging C. With the C/C++ Extensions (the one from Microsoft with 7.6 million downloads) you can create a debug configuration in the debug menu (Ctrl+Shift+D). Select C/C++ (GDB/LLDB), give it path of your executable (probably ./a.out)

For reference, the configuration file looks like this
```
"configurations": [
    
    {
        "name": "(gdb) Launch",
        "type": "cppdbg",
        "request": "launch",
        "program": "${workspaceFolder}/a.out",
        "args": [],
        "stopAtEntry": false,
        "cwd": "${workspaceFolder}",
        "environment": [],
        "externalConsole": true,
        "MIMode": "gdb",
        "setupCommands": [
            {
                "description": "Enable pretty-printing for gdb",
                "text": "-enable-pretty-printing",
                "ignoreFailures": true
            }
        ]
    }
]
```
### with Valgrind

If you're dealing with a segfault, it's faster to first check with Valgrind what line of code is causing the segfault before setting breakpoints and stepping through code. After you install valgrind you can run the program with the following command to have valgrind create a report on the programs status during runtime:

```
make && valgrind --log-file="valgrind_report.txt" --tool=memcheck --leak-check=yes --show-reachable=yes --num-callers=20 --track-fds=yes --track-origins=yes -v ./a.out
```
