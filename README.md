# TCP Chat
## Building instructions
TCP Chat uses the CMake (https://cmake.org/) tool to generate the appropriate
makefiles for building, so be sure that you have it installed.

Clone the repo and then position yourself in a directory in which you want to
build the project (using the source directory for building is possible, but not
recommended), and run the following:
```
$ cmake -G "Unix Makefiles" <path/to/project>
$ make
```
If everything completes without errors, you should have two executables
available: `server` and `client`.
