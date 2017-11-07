## The `prepinfo` Program

### Introduction

This program processes [Texinfo](https://www.gnu.org/software/texinfo)
files, updating `@node` lines and keeping menus organized.
It is written in `awk`, using the
[TexiWebJr.](https://github.com/arnoldrobbins/texiwebjr)
literate programming system.

### Contents

The `aux` directory contains auxiliary files; primarily the
`jrweave` and `jrtangle` programs from TexiWebJr., and the
list of valid words that `spell` doesn't recognize.

The `history` directory tracks the previous versions of this
program and the original plain Texinfo documentation thereof.

The files `COPYING`, `ChangeLog`, `Makefile`, and `texinfo.tex`
are all what you think they are.

### Building

You will need version 4.0 or later of `gawk` in `/usr/bin`,
and to have a recent version of Texinfo available. If both of
these conditions are met, you should be able to just run `make`.

Otherwise, you may need to edit the path in the `jrweave` and
`jrtangle` files, and/or install Texinfo.

### Feedback!

Comments and input about this program are welcomed.  Contact me
via email.

##### Last updated:

Tue Nov  7 21:47:59 IST 2017

Arnold Robbins
[arnold at skeeve.com](mailto:arnold@skeeve.com)
