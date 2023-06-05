#!/bin/sh

# add arguments to the invocation of make. e.g. different REGEX_PACKAGE,
# or for a specific mingw compiler - add CC=<mingw-compiler>

n=$(nproc 2>/dev/null)
[ -z "$n" ] || set -- -j$n "$@"

targets=
for t in $(grep "^all:" < Makefile.wng); do
    case $t in all:|clean|*[!0-9A-Za-z.]*);; *) targets="$targets $t"; esac
done
: ${targets:?cannot identify build targets}

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


make -f Makefile.wng clean
rm -f $targets  # if the compiler doesn't add .exe

base=$(git merge-base master HEAD)
id_git=$(git describe --tags $base)+$(git rev-list --count $base..HEAD)@avih
printf '"(%s)"' "$id_git" > ver_extra.cstr  # quoted without pre/post spacing

make -f Makefile.wng REGEX_PACKAGE=regcomp-local $targets "$@" || exit

for t in $targets; do
  [ -x $t ] && mv $t ${t%.exe}.exe || :  # if the compiler doesn't add .exe
done
