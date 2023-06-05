#!/bin/sh
set -eu

# arch (for the archive name) is from env ARCH, or env CC, or cc or gcc

# getopt-like, we only support [-k] [--]
[ "${1-}" = -k ] && keep=x && shift || keep=  # the tmp work dir
[ "${1-}" != -- ] || shift

merge_base=$(git merge-base master HEAD)
id_date=$(date +%Y-%m-%d)

id_git_date=$(git show -s --format=%ci $merge_base)
id_git_date=${id_git_date%% *}  # keep only yyyy-mm-dd

id_git=$(git describe --tags $merge_base)+$(git rev-list --count $merge_base..HEAD)@avih
# id_git=$(git rev-parse --short $merge_base)+$(git rev-list --count $merge_base..HEAD)@avih

arch=${ARCH:=$(${CC:-cc} -dumpmachine || gcc -dumpmachine) 2>/dev/null}; arch=${arch%%-*}

id=less-mingw.$id_git_date.$id_git${1+.$*}.$arch[$id_date]


rm -rf -- "$id"
mkdir -p -- "$id"/patches-avih
cp -a -- ./*.exe "$id"/
strip "$id"/*.exe

git format-patch --quiet -o "$id"/patches-avih/ $merge_base

PATH=d:/run/groff/bin\;$PATH
if which groff >/dev/null; then
    mkdoc() {
        # https://freeshell.de/~mk/blog/1497550032.html
        for x; do
            groff -m man -Tutf8 -P -cbdu < "$x".nro.VER > "$id"/docs/"$x".man.txt
            groff -m man -Tutf8          < "$x".nro.VER > "$id"/docs/"$x".man
        done
    }

    PATH=$(dirname "$(readlink -f `which groff`)")\;$PATH
    mkdir "$id"/docs

    mkdoc less lesskey lessecho
fi

out=$id.tar.gz
tar c -- "$id" | gzip > "$out"

echo "created: $out from:"
ls -tl -- "$id"
[ "$keep" ] && echo keeping "$id"/ || rm -rf -- "$id"
