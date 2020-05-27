# topcmd

This tool is similar to well known `watch` but it executes given command every time it receives any data from standard input. The idea is to use it together with [`entr`](http://entrproject.org) or [`inotify-tools`](https://github.com/inotify-tools/inotify-tools) tools to see the changes only when files are modified.

For example, one can watch the output of `git diff` command every time some file in repository changes:
```sh
git ls-files | entr echo | topcmd git diff
```
My motivation for this utility was to see the git tree of huge repositories (linux kernel or gentoo overlay for example), where it takes tens of seconds to print it. It does not make sense to print it every 2 seconds (default timeout of `watch`) but only when some reference changes. For this reason the repository contains a script `gitwatch` to simplify usage a little bit. The script basically watches all changes in branch references and spawns git subcommand every time something changes. Therefore to fulfill my motivation one can simply write:
```sh
gitwatch log --graph --all --decorate --oneline
```

## build

To build `topcmd` simply type

```sh
make
```

There is a Gentoo [ebuild](https://github.com/arkamar/overlay/tree/master/app-misc/topcmd) in my local overlay.
