/*
 * Prepinfo --- handle node and menu ordering in a texinfo document.
 * Copyright (c) 1989, Arnold David Robbins, arnold@skeeve.atl.ga.us
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 1, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHATABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675 Mass
 * Ave, Cambridage, MA 02139, USA.
 */ 

/*
 * This is a preliminary version of an unpublished program.  Please do
 * not give this code to anyone else.
 */

/*
 * Purpose --- to automate arranging of @node lines by intuiting structure
 * from @chapter, @section, etc. lines and their ordering in a file, and
 * generate correct menus.
 * 
 * This program allows an author to make massive rearrangements of the nodes
 * in a texinfo doucment. As long as each node has a corresponding @chapter,
 * @section, and so on, the generated file will be a copy of the input with
 * the pointers in all the @node's pointing to the correct places.
 * 
 * Strategy: Make two passes over the input. First, build a structure of
 * linked lists. Each node on the same level has a "next" pointer to the
 * following nodes on the same level, e.g. all chapters point to each other
 * with next pointers. Each node has a "child" pointer, e.g. a chapter node
 * to the first section in the chapter.
 *
 * There are some special cases for the first and second nodes in terms of
 * how their next and prev pointers are set up.  The first node is the Top,
 * and is a level above the chapters.
 * 
 * Then, build an array of pointers to all the nodes, and sort it by node
 * name for easy binay searching.  Since the next and child pointers of
 * the first two nodes aren't completely the way they should be, we thread
 * every node to the previous one, as it's allocated.  This list is what's
 * used to build the array before sorting.
 * 
 * In the second pass over the input, for each node, look it up by name,
 * and then follow the pointers to generate correct @node statemens.
 * 
 * Notes: The array could just be sorted by line number, which makes the
 * second pass looking-up easeier. However, as an extension, prepinfo could
 * scan for @xref and @pxref, and fill in the additional textual arguents
 * using the title info from the @chapter and @section.
 *
 * For this program to work, with the execption of the first node, EVERY
 * title must have a node associated with it.  For TeX's purposes, the nodes
 * are not necessary.  So, if any title does not have a real node, it should
 * be preceeded by ``@c fakenode - for prepinfo''. Prepinfo will then ignore
 * this title.
 *
 * For best results, the @node should precede the @chapter, @section, etc.
 *
 * MENUS:
 *
 * Prepinfo can only do so much.  To generate correct menus, it has to know
 * where to put them.  Therefore, while prepinfo will produce correct menus,
 * each chapter, section, etc. must have at least an empty @menu, @end menu
 * pair to indicate for the second pass where the menu should go.  This is
 * not an unreasonable restriction.
 *
 * Menues are handled as follows.  On the first pass, slurp up the entire
 * text of the menu into a buffer.  Pull it apart as follows: Any leading
 * comment is associated with the current node.  The menu item name, node
 * name and description are then isolated, null terminated, and a menu
 * structure is set up to point at the text of each.  The menu items are
 * all put on one big linked list.  We have to special case empty menus.
 *
 * Before the second pass, sort the list into an array, and cross link
 * menus and nodes.
 *
 * On the second pass, the existing menus are ignored, and new menus are
 * produced from scratch.  The existing menus merely act as a place holder
 * to signal where the menus go.
 *
 * TODO:
 *	Switch to GNU readline.
 *	If stdin is pipe, save input in a temp file.
 *	Add an option to leave the menus alone.
 *	Add an option for an output file.
 *	Read input from multiple command line files.
 */

#include <stdio.h>
#include <ctype.h>

/* the "Top" node is special-cased to be level 1, so chapter is 2, etc. */

struct title {		/* texinfo headings, chapter, section, etc. */
	char *t_text;	/* text of heading */
	short t_level;	/* level */
} titles[] = {				/* this array MUST be sorted */
	"appendix",			2,
	"appendixsec",			3,
	"appendixsubsec",		4,
	"appendixsubsubsec",		5,
	"chapter",			2,
	"heading",			3,
	"majorheading",			2,
	"section",			3,
	"subheading",			4,
	"subsection",			4,
	"subsubheading",		5,
	"subsubsection",		5,
	"unnumbered",			2,
	"unnumberedsec",		3,
	"unnumberedsubsec",		4,
	"unnumberedsubsubsec",		5,
};

/*
 * structure of a texinfo node is
 *
 * @node node-name, next, previous, up
 */

typedef struct texinode {
	char	*n_name;		/* from @node */
	char	*n_title;		/* from @chapter, @section */
	int	n_level;
	long	n_lineno;
	short	n_isfake;		/* this is a fake node */
	char	*n_mencom;		/* leading comment in a menu */
	struct texinode *n_next;
	struct texinode *n_prev;
	struct texinode *n_up;
	struct texinode *n_child;
	struct texinode *n_thread;
	struct menu	*n_menu;
} NODE;

/* a menu item */

typedef struct menu {
	char	*m_item;		/* menu item */
	char	*m_node;		/* @node name that for this item */
	char	*m_desc;		/* description of the item */
	short	m_dumped;		/* was this printed? */
	int	m_lineno;		/* line where seen */
	struct menu *m_next;		/* next menu item */
	NODE	*m_texinode;
} MENU;

MENU *firstmen;		/* head of list */
MENU *curmen;		/* most recent menu item */
MENU **menus;		/* sorted array */
int num_menus;

NODE top = {
	"(dir)",	/* n_name */
	NULL,		/* n_title */
	0,		/* n_level */
	0,		/* n_lineno */
	0,		/* n_isfake */
	NULL,		/* n_mencom */
	NULL,		/* n_next */
	NULL,		/* n_prev */
	& top,		/* n_up */
	NULL,		/* n_child */
	NULL,		/* n_thread */
	NULL,		/* n_menu */
};

NODE *curnode = &top;

int have_title = 0;	/* saw @chapter, @section, etc. */
int have_node = 0;	/* saw @node */

long lineno = 0;
long num_nodes = 0;

char *new_title;	/* info extracted from @chapter, @section, etc. */
int save_title_len;

NODE newnode;		/* info extracted from @node */
struct title *cur_title;

NODE **nodes;		/* for array of all nodes */

NODE *np1, *np2;	/* temps */
int i;
char *cp1, *cp2;

/*
 * should eventually use gnu readline() for infinite length lines
 */
char line[BUFSIZ * 2];
#define getline()	(fgets(line, sizeof line, stdin) != NULL)

char *xmalloc(), *xrealloc();
char *strsave();
extern int node_cmp();	/* for qsort(3) */
extern int menu_cmp();
extern NODE *getnode();
extern struct title *get_title();

extern char *strchr();

#ifdef DEBUG
#define exit	myexit
#endif

main (argc, argv)
int argc;
char **argv;
{
	char save;

	/* pass 1 */
	while (getline ()) {
		lineno++;
		if (line[0] != '@')
			continue;
		if (strncmp (line, "@menu", 5) == 0) {
			menu();
			continue;
		} else if (strncmp (line, "@node", 5) == 0
		    || strncmp (line, "@c fakenode", 11) == 0) {
			num_nodes++;
			if (num_nodes == 1) {	/* first node is special */
				save_node();
				combine(0);
				curnode -> n_prev = & top;	/* special */
				have_node = 0;
				continue;
			}
			if (have_node) {
				/* insert previous node in tree with no title */
				fprintf (stderr,
					"line %ld: new @node but %s\n", lineno,
					"no title for previous @node");
				fprintf (stderr, "text = %s\n", line);
				combine(0);
			} else
				have_node = 1;
			save_node();
		} else if ((cur_title = get_title())) {
			if (have_title) {
				fprintf (stderr,
					"line %ld: new title but %s\n", lineno,
					"no @node for previous title");
				fprintf (stderr, "text = %s\n", line);
			} else
				have_title = 1;
			save_title();
		}
		if (have_title && have_node) {
			combine(1);
			have_title = have_node = 0;
		}
	}

	rewind(stdin);

	/* now set up the array */

	/* total nodes = num_nodes + our special top node */
	num_nodes++;

	/* allocate */
	nodes = (NODE **) xmalloc(num_nodes * sizeof(NODE *));

	/* fill the array */
	flatten (&top);

	/* sort it */
	qsort (nodes, num_nodes, sizeof (NODE *), node_cmp);

	/* now do menus */
	menus = (MENU **) xmalloc(num_menus * sizeof(MENU *));
	for (i = 0, curmen = firstmen; curmen; curmen = curmen -> m_next, i++)
		menus[i] = curmen;

	qsort (menus, num_menus, sizeof (MENU *), menu_cmp);
	dupmenu();

	link_menu();	/* link menus and nodes */

	/* pass 2 */
	lineno = 0;
	while (getline ()) {
		lineno++;
		if (line[0] != '@') {
#ifdef notdef
			/* add @xref and @pxref here */
#endif
			fputs (line, stdout);
			continue;
		} else if (strncmp (line, "@menu", 5) == 0) {
			do {
				getline();
			} while (! end_menu());
			if (! np1) {
				fprintf (stderr,
					"line %d: menu before a node\n",
					lineno);
				exit(1);	/* throw up hands */
			} else if (! np1 -> n_child) {
				fprintf (stderr,
		"line %d: preceding node '%s' has no inferior nodes\n",
					lineno, np1 -> n_name);
				exit(1);
			} else
				dump_menu(np1->n_child);
			continue;
		} else if (strncmp (line, "@node", 5) != 0) {
			fputs (line, stdout);
			continue;
		}

		cp1 = line + 5;
		while (*cp1 && isspace (*cp1))
			cp1++;
		cp2 = cp1;
		/* node names CAN have spaces in them */
		while (*cp2 && *cp2 != ',' && *cp2 != '\n')
			cp2++;
		if (*cp2 == ',') {
			cp2--;
			while (isspace(*cp2))
				cp2--;
			if (! isspace(*cp2))
				cp2++;
		}
		save = *cp2;
		*cp2 = '\0';

		/* cp1 now points at node name */
		np1 = getnode(cp1);
		printnode(np1);
	}

	for (i = 0; i < num_menus; i++) {
		if (! menus[i]->m_dumped) {
			fprintf (stderr, "no @menu ");
			if (menus[i]->m_item)
				fprintf(stderr, "for item '%s' ",
					menus[i]->m_item);
			fprintf (stderr, "for node '%s', ending line %d\n",
				menus[i]->m_node, menus[i]->m_lineno);
		}
	}

	for (i = 0; i < num_nodes; i++) {
		if (nodes[i]->n_menu == NULL && nodes[i]->n_level >= 2)
			fprintf (stderr, "no menu item for node '%s' - %s\n",
				nodes[i]->n_name,
				"one will be generated if possible");
	}
	exit(0);
	/* NOTREACHED */
}

/* node_cmp --- compare two nodes by name */

int node_cmp (p1, p2)
NODE **p1, **p2;
{
	return strcmp ((*p1)->n_name, (*p2)->n_name);
}

/* flatten --- walk the linked lists, filling the nodes array */

flatten (np)
NODE *np;
{
	register int i = 0;

	while (np) {
		nodes[i] = np;
		np = np -> n_thread;
		i++;
	}
}

/* getnode --- search the nodes array */

NODE *
getnode (n)
char *n;
{
	register int l, m, h, r;

	l = 0;
	h = num_nodes;

	while (l <= h) {
		m = (h + l) / 2;
		r = strcmp (n, nodes[m]->n_name);
		if (r == 0)	/* got it */
			return nodes[m];
		else if (r < 0)	/* n < nodes[m] */
			h = m - 1;
		else		/* n > nodes[m] */
			l = m + 1;
	}
	return NULL;
}

/* get_title --- search the title array */

struct title *
get_title ()
{
	register int l, m, h, r;
	char save;
	struct title *ret = NULL;

	cp1 = line + 1;
	for (cp2 = cp1; ! isspace(*cp2); cp2++)
		/* null */ ;
	save = *cp2;
	*cp2 = '\0';

	l = 0;
	h = sizeof(titles) / sizeof(titles[0]);

	while (l <= h) {
		m = (h + l) / 2;
		r = strcmp (cp1, titles[m].t_text);
		if (r == 0) {	/* got it */
			ret = &titles[m];
			break;
		} else if (r < 0)	/* n < titles[m] */
			h = m - 1;
		else		/* n > titles[m] */
			l = m + 1;
	}
	*cp2 = save;
	return ret;
}

/* save_node --- save the node name and line number */

save_node ()
{
	if (line[1] == 'c')	/* @c fakenode ... */
		cp1 = line + 11;
	else
		cp1 = line + 5;

	while (isspace(*cp1))
		cp1++;
	cp2 = cp1;
	while (*cp2 && *cp2 != ',' && *cp2 != '\n')
		cp2++;
	if (*cp2 == ',') {
		cp2--;
		while (isspace(*cp2))
			cp2--;
		if (! isspace(*cp2))
			cp2++;
	}
	*cp2 = '\0';

	newnode.n_lineno = lineno;
	newnode.n_isfake = (line[1] == 'c');
	if (! newnode.n_isfake)
		newnode.n_name = strsave(cp1);
	else
		num_nodes--;	/* fake nodes are not saved */
}

/* save_title --- save the title info */

save_title ()
{
	int i;

	cp1 = line;

	while (! isspace(*cp1))
		cp1++;
	while (isspace(*cp1))
		cp1++;

	cp2 = cp1;
	while (*cp2 && *cp2 != '\n')
		cp2++;
	if (*cp2)
		*cp2 = '\0';	/* clobber newline */

	i = strlen(cp1);
	if (i > save_title_len) {
		new_title = xrealloc (new_title, i + 1);
		save_title_len = i;
	}
	strcpy (new_title, cp1);
}

/* combine --- merge node and title info, link in at appropriate place */

combine (have_title)
int have_title;
{
	NODE *np, *np2;

	if (newnode.n_isfake)
		return;

	if (have_title) {
		/* merge to one structure */
		newnode.n_title = strsave(new_title);
		newnode.n_level = cur_title -> t_level;
	} else {
		newnode.n_title = NULL;
		if (curnode -> n_level)
			newnode.n_level = curnode -> n_level;	/* best guess */
		else
			newnode.n_level = 1;
	}

	/* alloc new node and copy */
	np = (NODE *) xmalloc(sizeof(NODE));
	np -> n_title = newnode.n_title;
	np -> n_level = newnode.n_level;
	np -> n_name = newnode.n_name;
	np -> n_lineno = newnode.n_lineno;

	curnode -> n_thread = np;

	/* insert */

	if (np -> n_level == curnode -> n_level) {	/* sibling */
		curnode -> n_next = np;
		np -> n_prev = curnode;
		np -> n_up = curnode -> n_up;
	} else if (np -> n_level > curnode -> n_level) {	/* child */
		curnode -> n_child = np;
		np -> n_up = curnode;
		if (num_nodes == 2) {	/* another special case, sigh */
			curnode -> n_next = np;
			np -> n_prev = curnode;
		}
		if (np -> n_level != (curnode -> n_level + 1)) {
			fprintf (stderr,
		"warning: node %s, at line %d is %d levels too far down\n",
				np -> n_name, np-> n_lineno,
				np -> n_level - (curnode -> n_level + 1));
		}

	} else {	/* np->n_level < curnode->n_level: ancestor's sibling */
		np2 = curnode -> n_up;

		/* go to correct ancestor's level, e.g. subsection to chapter */
		while (np2 -> n_level > np -> n_level)
			np2 = np2 -> n_up;

		/* now to end of list of siblings */
		while (np2 -> n_next)
			np2 = np2 -> n_next;
		np2 -> n_next = np;
		np -> n_prev = np2;
		np -> n_up = np2 -> n_up;
	}

	curnode = np;
}

/* printnode --- actually print an @node statement */

printnode (np)
NODE *np;
{
	static char blank[] = " ";

	if (! np) {
		fprintf (stderr, "printnode: can't happen: np == NULL\n");
		exit(1);
	}
	printf ("@node %s, ", np -> n_name);
	printf ("%s, ", np -> n_next ? np -> n_next -> n_name : blank);
	/*
	 * It's not clear in the manual, but makeinfo wants the UP node
	 * for the PREV field if there is no PREV node.
	 */
	printf ("%s, ", np -> n_prev ? np -> n_prev -> n_name :
				np -> n_up ? np -> n_up -> n_name :
				blank);
	printf ("%s\n", np -> n_up ? np -> n_up -> n_name : blank);
}

/* menu_cmp --- compare two menus by name */

int menu_cmp (p1, p2)
MENU **p1, **p2;
{
	int i = strcmp ((*p1)->m_node, (*p2)->m_node);

	if (i)
		return i;
	else
		return ((*p1)->m_lineno - (*p2)->m_lineno);
}

/* link_menu --- link the nodes and the menus */

link_menu()
{
	register int i, j, k;

	i = j = 0;

	do {
		k = strcmp (nodes[i]->n_name, menus[j]->m_node);
		if (k == 0) {	/* got one! */
			nodes[i]->n_menu = menus[j];
			menus[j]->m_texinode = nodes[i];
			i++;
			j++;
		} else if (k > 0)	/* node > menu */
			j++;
		else			/* node < menu */
			i++;
	} while (i < num_nodes && j < num_menus);
}

/* end_menu --- decide if we've seen an ``@end menu'' statement */

int
end_menu()
{
	char *cp;

	if (strncmp(line, "@end", 4) == 0) {
		cp = line + 4;
		while (*cp && isspace(*cp))
			cp++;
		return (strncmp (cp, "menu", 4) == 0);
	}
	return 0;
}

/* menu --- slurp up a whole menu, pull it apart */

menu()
{
	char *menbuf = NULL;
	int menbufsize = 0;
	int menbuflen = 0;
	int l = 0;
	char *cp;

	/* first, read the whole menu into a buffer */
	while (1) {
		if (! getline()) {
			fprintf (stderr, "Unexpected EOF inside menu at line %d\n",
				lineno);
			exit(1);
		}
		lineno++;
		if (end_menu()) {
			if (menbufsize == 0)
				return;
			else
				break;
		}
		l = strlen(line);
		if (menbuflen + l > menbufsize) {
			menbuf = xrealloc(menbuf, menbufsize + BUFSIZ);
			menbufsize += BUFSIZ;
		}
		strcpy (& menbuf[menbuflen], line);
		menbuflen += l;
	}

	if (l == 0)
		return;		/* an empty menu */

	if (menbuf[menbuflen-1] == '\n')	/* should be true... */
		menbuf[--menbuflen] = '\0'; 	/* clobber newline */

	/* next, extract any leading comment */
	cp = menbuf;
	while (isspace(*cp))
		cp++;
	if (*cp != '*') {
		curnode -> n_mencom = menbuf;
		while (*cp && *cp != '*')
			cp++;
		if (! *cp) {	/* only comment, geez */
			*--cp = '\0';	/* nuke newline */
			return;
		}
		while (*cp != '\n')
			cp--;
		*cp++ = '\0';	/* clobber newline */
		while (isspace(*cp))
			cp++;
	}

loop:
	if (! *cp)
		return;

	/* at this point, cp had better be a '*' */
	if (*cp != '*') {
		fprintf (stderr, "badly formed menu ending line %d\n", lineno);
		exit(1);
	 } else
		cp++;

	num_menus++;

	while (isspace(*cp))
		cp++;

	if (curmen == NULL) {	/* first time */
		curmen = firstmen = (MENU *) xmalloc(sizeof(MENU));
	} else {
		curmen -> m_next = (MENU *) xmalloc(sizeof(MENU));
		curmen = curmen -> m_next;
	}

	curmen -> m_lineno = lineno;

	curmen -> m_item = cp;
	while (*cp != ':')
		cp++;
	*cp++ = '\0';
	if (*cp == ':')	{	/* no item, just a node name */
		cp++;
		curmen -> m_node = curmen -> m_item;
		curmen -> m_item = NULL;
	} else {
		while (*cp && isspace(*cp))
			cp++;
		curmen -> m_node = cp++;
		while (*cp && strchr (".,\t\n", *cp) == NULL)
			cp++;
		*cp++ = '\0';
	}
	while (*cp && isspace(*cp) && *cp != '*')
		cp++;

	if (! *cp)
		return;

	if (*cp != '*') {	/* comment text */
		curmen -> m_desc = cp;
		while (*cp && *cp != '*')
			cp++;
		if (*cp == '*') {
			for (cp--; *cp != '\n'; cp--)
				/* null */ ;
			*cp++ = '\0';
			while (*cp != '*')
				cp++;
		}
	}
	goto loop;
}

/* dump_menu --- print out a menu */

/* note: incoming node is first interior node, comment is associated with
   parent node */

dump_menu(np)
NODE *np;
{
	MENU *mp;

	fputs ("@menu\n", stdout);
	if (np -> n_up -> n_mencom)
		printf ("%s\n", np -> n_up -> n_mencom);
	for (; np; np = np -> n_next) {
		mp = np -> n_menu;
		if (! mp) {
			printf ("* %s::\t%s.\n", np -> n_name, np -> n_title);
			continue;
		}
		mp -> m_dumped = 1;
		if (mp -> m_item)
			printf ("* %s: %s.", mp -> m_item, np -> n_name);
		else
			printf ("* %s::",  np -> n_name);
		if (mp -> m_desc)
			printf ("\t%s", mp -> m_desc);
		putchar('\n');
	}
	fputs ("@end menu\n", stdout);
}

/* dupmenu --- see if there are any duplicate node references */

dupmenu()
{
	int i;

	for (i = 0; i < num_menus - 1; i++)
		if (strcmp(menus[i]->m_node, menus[i+1]->m_node) == 0) {
			fprintf (stderr,
		"duplicate menu entries for node '%s', near lines %d and %d\n",
				menus[i]->m_node,
				menus[i]->m_lineno,
				menus[i+1]->m_lineno);
		}
}

/* xmalloc --- safety checking malloc */

extern char *calloc();		/* used because it zero-fills */
extern char *realloc();

char *
xmalloc (size)
unsigned int size;
{
	char *cp;

	if ((cp = calloc(1, size)) == NULL) {
		fprintf (stderr, "out of memory!\n");
		exit(1);
	}

	return cp;
}

/* xrealloc --- safety checking realloc */

char *
xrealloc (ptr, size)
char *ptr;
unsigned int size;
{
	char *p;

	if (ptr == NULL)
		return xmalloc (size);
	else if ((p = realloc (ptr, size)) == NULL) {
		fprintf (stderr, "out of memory!\n");
		exit(1);
	}

	return p;
}

/* strsave --- malloc space for a string */

char *
strsave (s)
char *s;
{
	char *p;
	int i;

	i = strlen(s);
	p = xmalloc(i + 1);
	strcpy(p, s);
	return p;
}

dumpit()
{
	int i;
	static char nil[] = { '\0' };

	fprintf (stderr, "\nnum_nodes = %d\n", num_nodes);
	for (i = 0; i < num_nodes; i++)
		fprintf(stderr, "node[%d] <%s><%s><%s><%s>\n", i,
			nodes[i] -> n_name,
			nodes[i] -> n_next ? nodes[i] -> n_next -> n_name : nil,
			nodes[i] -> n_prev ? nodes[i] -> n_prev -> n_name : nil,
			nodes[i] -> n_up ? nodes[i] -> n_up -> n_name : nil);
	fprintf (stderr, "\n");
}

#ifdef DEBUG
#undef exit

myexit(val)
int val;
{
	if (val == 0)
		exit(0);
	else
		abort();	/* drop core */
}
#endif
