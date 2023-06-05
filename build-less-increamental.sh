#!/bin/sh

# add arguments to the invocation of make. e.g. different REGEX_PACKAGE,
# or for a specific mingw compiler - add CC=<mingw-compiler>

n=$(nproc 2>/dev/null)
[ -z "$n" ] || set -- -j$n "$@"

path_prepend() {
    case $PATH in *";"*) PATH="$1;$PATH";;
                      *) PATH="$1:$PATH";; esac
}

if which cmd.exe >/dev/null; then  # on windows
    path_prepend "$PWD/winperl"
else
    path_prepend "$PWD/wincmd"  # alternatively, add SHELL=sh if supported
    case " $* " in *" CC="*);; *)
        cc=${CC-$(IFS=:; find $PATH -name "*mingw*-gcc" | head -n1)}
        [ -z "$cc" ] || set -- "CC=$cc" "$@"
    esac
fi


rm -f less less.exe  # if the compiler doesn't add .exe

# these headers depend on all the source (object) files, so any change
# in any source files triggers a full rebuild (rightfully). however
# they only matter when global defines or functions change, so to only
# build less incrementally, ignore them. But if some global function
# changed then this build will fail due to not rebuilding these headers
grep -v "less.h defines.h funcs.h"< Makefile.wng > Makefile.tmp

make -f Makefile.tmp REGEX_PACKAGE=regcomp-local less "$@" || exit

[ -x less ] && mv less less.exe || :  # if the compiler doesn't add .exe
