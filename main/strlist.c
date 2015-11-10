/*
*   Copyright (c) 1999-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions managing resizable string lists.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <string.h>
#include <fnmatch.h>

#include "debug.h"
#include "read.h"
#include "routines.h"
#include "strlist.h"

/*
*   FUNCTION DEFINITIONS
*/

extern stringList *stringListNew (void)
{
	stringList* const result = xMalloc (1, stringList);
	result->max   = 0;
	result->count = 0;
	result->list  = NULL;
	return result;
}

extern void stringListAdd (stringList *const current, vString *string)
{
	enum { incrementalIncrease = 10 };
	Assert (current != NULL);
	if (current->list == NULL)
	{
		Assert (current->max == 0);
		current->count = 0;
		current->max   = incrementalIncrease;
		current->list  = xMalloc (current->max, vString*);
	}
	else if (current->count == current->max)
	{
		current->max += incrementalIncrease;
		current->list = xRealloc (current->list, current->max, vString*);
	}
	current->list [current->count++] = string;
}

extern void stringListRemoveLast (stringList *const current)
{
	Assert (current != NULL);
	Assert (current->count > 0);
	--current->count;
	current->list [current->count] = NULL;
}

/* Combine list `from' into `current', deleting `from' */
extern void stringListCombine (
		stringList *const current, stringList *const from)
{
	unsigned int i;
	Assert (current != NULL);
	Assert (from != NULL);
	for (i = 0  ;  i < from->count  ;  ++i)
	{
		stringListAdd (current, from->list [i]);
		from->list [i] = NULL;
	}
	stringListDelete (from);
}

extern stringList* stringListNewFromArgv (const char* const* const argv)
{
	stringList* const result = stringListNew ();
	const char *const *p;
	Assert (argv != NULL);
	for (p = argv  ;  *p != NULL  ;  ++p)
		stringListAdd (result, vStringNewInit (*p));
	return result;
}

extern stringList* stringListNewFromFile (const char* const fileName)
{
	stringList* result = NULL;
	FILE* const fp = fopen (fileName, "r");
	if (fp != NULL)
	{
		result = stringListNew ();
		while (! feof (fp))
		{
			vString* const str = vStringNew ();
			readLine (str, fp);
			vStringStripTrailing (str);
			if (vStringLength (str) > 0)
				stringListAdd (result, str);
			else
				vStringDelete (str);
		}
	}
	return result;
}

extern unsigned int stringListCount (const stringList *const current)
{
	Assert (current != NULL);
	return current->count;
}

extern vString* stringListItem (
		const stringList *const current, const unsigned int indx)
{
	Assert (current != NULL);
	return current->list [indx];
}

extern vString* stringListLast (const stringList *const current)
{
	Assert (current != NULL);
	Assert (current->count > 0);
	return current->list [current->count - 1];
}

extern void stringListClear (stringList *const current)
{
	unsigned int i;
	Assert (current != NULL);
	for (i = 0  ;  i < current->count  ;  ++i)
	{
		vStringDelete (current->list [i]);
		current->list [i] = NULL;
	}
	current->count = 0;
}

extern void stringListDelete (stringList *const current)
{
	if (current != NULL)
	{
		if (current->list != NULL)
		{
			stringListClear (current);
			eFree (current->list);
			current->list = NULL;
		}
		current->max   = 0;
		current->count = 0;
		eFree (current);
	}
}

static boolean compareString (
		const char *const string, vString *const itm)
{
	return (boolean) (strcmp (string, vStringValue (itm)) == 0);
}

static boolean compareStringInsensitive (
		const char *const string, vString *const itm)
{
	return (boolean) (strcasecmp (string, vStringValue (itm)) == 0);
}

static int stringListIndex (
		const stringList *const current,
		const char *const string,
		boolean (*test)(const char *s, vString *const vs))
{
	int result = -1;
	unsigned int i;
	Assert (current != NULL);
	Assert (string != NULL);
	Assert (test != NULL);
	for (i = 0  ;  result == -1  &&  i < current->count  ;  ++i)
		if ((*test)(string, current->list [i]))
			result = i;
	return result;
}

extern boolean stringListHas (
		const stringList *const current, const char *const string)
{
	boolean result = FALSE;
	Assert (current != NULL);
	result = stringListIndex (current, string, compareString) != -1;
	return result;
}

static vString* stringListFinds (
		const stringList *const current, const char *const string,
		boolean (*test)(const char *s, vString *const vs))
{
	int i;

	Assert (current != NULL);
	Assert (string != NULL);

	i = stringListIndex (current, string, test);
	if (i == -1)
		return NULL;
	else
		return stringListItem(current, i);
}

extern boolean stringListHasInsensitive (
		const stringList *const current, const char *const string)
{
	boolean result = FALSE;
	Assert (current != NULL);
	Assert (string != NULL);
	result = stringListIndex (current, string, compareStringInsensitive) != -1;
	return result;
}

extern boolean stringListHasTest (const stringList *const current,
				  boolean (*test)(const char *s, void *userData),
				  void *userData)
{
	boolean result = FALSE;
	unsigned int i;
	Assert (current != NULL);
	for (i = 0  ;  ! result  &&  i < current->count  ;  ++i)
		result = (*test)(vStringValue (current->list [i]), userData);
	return result;
}

extern boolean stringListDeleteItemExtension (stringList* const current, const char* const extension)

{
	boolean result = FALSE;
	int where;
#ifdef CASE_INSENSITIVE_FILENAMES
	where = stringListIndex (current, extension, compareStringInsensitive);
#else
	where = stringListIndex (current, extension, compareString);
#endif
	if (where != -1)
	{
		vStringDelete (current->list [where]);
		memmove (current->list + where, current->list + where + 1,
				(current->count - where) * sizeof (*current->list));
		current->list [current->count - 1] = NULL;
		--current->count;
		result = TRUE;
	}
	return result;
}

extern boolean stringListExtensionMatched (
		const stringList* const current, const char* const extension)
{
#ifdef CASE_INSENSITIVE_FILENAMES
	return stringListHasInsensitive (current, extension);
#else
	return stringListHas (current, extension);
#endif
}

extern vString* stringListExtensionFinds (
		const stringList* const current, const char* const extension)
{
#ifdef CASE_INSENSITIVE_FILENAMES
	return stringListFinds (current, extension, compareStringInsensitive);
#else
	return stringListFinds (current, extension, compareString);
#endif
}

static boolean fileNameMatched (
		const vString* const vpattern, const char* const fileName)
{
	const char* const pattern = vStringValue (vpattern);
	return (boolean) (fnmatch (pattern, fileName, 0) == 0);
}

extern boolean stringListFileMatched (
			const stringList* const current, const char* const fileName)
{
	return stringListFileFinds (current, fileName)? TRUE: FALSE;
}

extern vString* stringListFileFinds (
		const stringList* const current, const char* const fileName)
{
	vString* vstr = NULL;
	boolean matched = FALSE;
	unsigned int i;
	for (i = 0  ;  ! matched  &&  i < stringListCount (current)  ;  ++i)
	{
		vstr = stringListItem (current, i);
		matched = fileNameMatched (vstr, fileName);
	}
	return matched? vstr: NULL;
}

extern void stringListPrint (const stringList *const current, FILE *fp)
{
	unsigned int i;
	Assert (current != NULL);
	for (i = 0  ;  i < current->count  ;  ++i)
		fprintf (fp, "%s%s", (i > 0) ? ", " : "", vStringValue (current->list [i]));
}

extern void stringListReverse (const stringList *const current)
{
	unsigned int i, j;
	vString *tmp;

	Assert (current != NULL);
	for (i = 0, j = current->count - 1 ; i <  (current->count / 2); ++i, --j)
	{
		tmp = current->list[i];
		current->list[i] = current->list[j];
		current->list[j] = tmp;
	}
}

/* vi:set tabstop=4 shiftwidth=4: */
