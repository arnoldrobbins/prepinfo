## History of the `prepinfo` program

### Introduction

This directory tracks the history of `prepinfo`, as best as I can
reconstruct it from various versions in my filesystem and from RCS
files.

### The C Version

There are three generations of `prepinfo.c`, the original C program
that I first wrote to do this job.  After rewriting it in AWK, I
abandoned the C version.

### The AWK Version

I found 12(!) different versions of the AWK script on my hard disk.
The dates of the files are a little confusing and committing them
based just on the file dates would have been a mistake. Instead,
I have tried to commit the files in the order that changes were
made, based on content comparisons and file dates.

The contents forked for a while (which is truly weird). It looks
something like this:

	Version 1
	    |
	    V
	Version 2
	    |
	    V
	Version 3 ------> (3a) Added copyright, released in texinfo/contrib.
	    |\
	    | \
	    |  +--------> (3b) Added skipping @ignore/@end ignore, no copyright.
	    |                          |
	    |                          V
	    |             (3c) Added handling of @detailmenu.
	    V
	Version 4
	    |
	    V
	Version 5
	    |
	    V
	Version 6
	    |
	    V
	Version 7	Copyright statement folded in.
	    |
	    V
	Version 8	@ignore/@end ignore and @detailmenu folded in.
	    |
	    V
	Version 9	Documented in 2001, but not published.

The `alternate-timeline` branch contains the right-hand side of
the fork; `master` has the left-hand side.

##### Last Updated

Tue Nov  7 06:29:10 IST 2017
