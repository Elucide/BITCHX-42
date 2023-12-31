/*
 * ircaux.c: some extra routines... not specific to irc... that I needed 
 *
 * Written By Michael Sandrof
 *
 * Copyright(c) 1990, 1991 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include "irc.h"
static char cvsrevision[] = "$Id$";
CVS_REVISION(ircaux_c)
#include "struct.h"

#include "alias.h"
#include "log.h"
#include "misc.h"
#include "vars.h"
#include "screen.h"

#include <pwd.h>

#include <sys/stat.h>

#include "ircaux.h"
#include "output.h"
#include "ircterm.h"
#define MAIN_SOURCE
#include "modval.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN PATHLEN
#endif

/*
 * These are used by the malloc routines.  We actually ask for an int-size
 * more of memory, and in that extra int we store the malloc size.  Very
 * handy for debugging and other hackeries.
 */

/*#define MEM_DEBUG 1*/

#ifdef MEM_DEBUG
#include <dmalloc.h>
#endif

/* This union is used to ensure that the resulting address
 * will be aligned correctly for all types we use. */
union alloc_info {
	size_t size;

	void *dummy1;
	long dummy2;
	double dummy3;
};

#define alloc_start(ptr) ((void *)((char *)(ptr) - sizeof(union alloc_info)))
#define alloc_size(ptr) (((union alloc_info *)alloc_start(ptr))->size)

#define FREE_DEBUG 1
#define FREED_VAL (size_t)-3
#define ALLOC_MAGIC 0xafbdce70

char compress_buffer[10000];

void start_memdebug(void)
{
#ifdef MEM_DEBUG
	dmalloc_debug(/*0x2202*/0x14f41d83);
#endif
}

/*
 * really_new_malloc is the general interface to the malloc(3) call.
 * It is only called by way of the ``new_malloc'' #define.
 * It won't ever return NULL.
 */

/*
 * Malloc allocator with size caching.
 */
void * n_malloc (size_t size, const char *module, const char *file, const int line)
{
	char *ptr;

	if (size < FREED_VAL - sizeof(union alloc_info))
	{
#ifdef MEM_DEBUG
		ptr = _calloc_leap(file, line, 1, size + sizeof(union alloc_info));
#else
		ptr = calloc(1, size + sizeof(union alloc_info));
#endif
	}
	else
	{
		ptr = NULL;
	}

	if (!ptr)
	{
		yell("Malloc() failed, giving up!");
		putlog(LOG_ALL, "*", "*** failed calloc %s %s (%d)", module?module:empty_string, file, line);
		term_reset();
		exit(1);
	}
	/* Store the size of the allocation in the buffer. */
	ptr += sizeof(union alloc_info);
	alloc_size(ptr) = size;
	return ptr;
}

/*
 * new_free:  Why do this?  Why not?  Saves me a bit of trouble here and there 
 */
void *	n_free(void *ptr, const char *module, const char *file, const int line)
{
	if (ptr)
	{
#ifdef FREE_DEBUG
		/* Check to make sure its not been freed before */
		if (alloc_size(ptr) == FREED_VAL)
		{
			yell("free()ing a already free'd pointer, giving up!");
			putlog(LOG_ALL, "*", "*** failed free %s %s (%d)", module?module:empty_string, file, line);
			term_reset();
			exit(1);
		}
#endif
		alloc_size(ptr) = FREED_VAL;

#ifdef MEM_DEBUG
		_free_leap(file, line, alloc_start(ptr));
#else
		free(alloc_start(ptr));
#endif
		ptr = NULL;
	}
	return ptr;
}

void * n_realloc (void *ptr, size_t size, const char *module, const char *file, const int line)
{
	char *ptr2 = NULL;

	if (ptr)
	{
		if (size)
		{
			size_t msize = alloc_size(ptr);

			if (msize >= size)
				return ptr;

			ptr2 = n_malloc(size, module, file, line);
			memmove(ptr2, ptr, msize);
			n_free(ptr, module, file, line);
			return ptr2;
		}
		return n_free(ptr, module, file, line);
	} 
	else if (size)
		ptr2 = n_malloc(size, module, file, line);
	return ptr2;
}

/*
 * malloc_strcpy:  Mallocs enough space for src to be copied in to where
 * ptr points to.
 *
 * Never call this with ptr pointinng to an uninitialised string, as the
 * call to new_free() might crash the client... - phone, jan, 1993.
 */
char *	n_malloc_strcpy (char **ptr, const char *src, const char *module, const char *file, const int line)
{
	if (!src)
		return *ptr = n_free(*ptr, module, file, line);
	if (*ptr)
	{
		if (*ptr == src)
			return *ptr;
		if (alloc_size(*ptr) > strlen(src))
			return strcpy(*ptr, src);
		*ptr = n_free(*ptr, module, file, line);
	}
	*ptr = n_malloc(strlen(src) + 1, module, file, line);

	return strcpy(*ptr, src);
}

/* malloc_strcat: Yeah, right */
char *	n_malloc_strcat (char **ptr, const char *src, const char *module, const char *file, const int line)
{
	size_t  msize;

	if (*ptr)
	{
		if (!src)
			return *ptr;
		msize = strlen(*ptr) + strlen(src) + 1;
		*ptr = n_realloc(*ptr, sizeof(char)*msize, module, file, line);
		return strcat(*ptr, src);
	}
	return (*ptr = n_m_strdup(src, module, file, line));
}

char *BX_malloc_str2cpy(char **ptr, const char *src1, const char *src2)
{
	if (!src1 && !src2)
		return new_free(ptr);

	if (*ptr)
	{
		if (alloc_size(*ptr) > strlen(src1) + strlen(src2))
			return strcat(strcpy(*ptr, src1), src2);
		new_free(ptr);
	}

	*ptr = new_malloc(strlen(src1) + strlen(src2) + 1);
	return strcat(strcpy(*ptr, src1), src2);
}

char *BX_m_3dup (const char *str1, const char *str2, const char *str3)
{
	size_t msize = strlen(str1) + strlen(str2) + strlen(str3) + 1;
	return strcat(strcat(strcpy((char *)new_malloc(msize), str1), str2), str3);
}

char *BX_m_opendup (const char *str1, ...)
{
	va_list args;
	int size;
	char *this_arg = NULL;
	char *retval = NULL;

	size = strlen(str1);
	va_start(args, str1);
	while ((this_arg = va_arg(args, char *)))
		size += strlen(this_arg);

	retval = (char *)new_malloc(size + 1);

	strcpy(retval, str1);
	va_start(args, str1);
	while ((this_arg = va_arg(args, char *)))
		strcat(retval, this_arg);

	va_end(args);
	return retval;
}

char	*n_m_strdup (const char *str, const char *module, const char *file, const int line)
{
	char *ptr;
	
	if (!str)
		str = empty_string;
	ptr = (char *)n_malloc(strlen(str) + 1, module, file, line);
	return strcpy(ptr, str);
}

char	*BX_m_s3cat (char **one, const char *maybe, const char *definitely)
{
	if (*one && **one)
		return m_3cat(one, maybe, definitely);
	return malloc_strcpy(one, definitely);
}

char *BX_m_s3cat_s (char **one, const char *maybe, const char *ifthere)
{
	if (ifthere && *ifthere)
		return m_3cat(one, maybe, ifthere);
	return *one;
}

char	*BX_m_3cat(char **one, const char *two, const char *three)
{
	size_t one_len = 0, two_len = 0, three_len = 0;
	char *str;

	if (*one)
		one_len = strlen(*one);
	if (two)
		two_len = strlen(two);
	if (three)
		three_len = strlen(three);

	str = new_malloc(one_len + two_len + three_len + 1);
	if (*one)
		memcpy(str, *one, one_len);
	if (two)
		memcpy(str + one_len, two, two_len);
	if (three)
		memcpy(str + one_len + two_len, three, three_len);
	str[one_len + two_len + three_len] = 0;

	new_free(one);
	return (*one = str);
}

/* upper()
 *
 * Convert in-place every lower-case character in a string to upper-case.
 */
char *BX_upper(char *str)
{
	char *ptr;

	if (str)
	{
		for (ptr = str; *ptr; ptr++)
		{
			unsigned char c = *ptr;

			if (islower(c))
				*ptr = toupper(c);
		}
	}

	return str;
}

/* lower()
 *
 * Convert in-place every upper-case character in a string to lower-case.
 */
char *BX_lower(char *str)
{
	char *ptr;

	if (str)
	{
		for (ptr = str; *ptr; ptr++)
		{
			unsigned char c = *ptr;

			if (isupper(c))
				*ptr = tolower(c);
		}
	}

	return str;
}

char *BX_malloc_sprintf (char **to, const char *pattern, ...)
{
	char booya[BIG_BUFFER_SIZE*3+1];
	*booya = 0;
	
	if (pattern)
	{
		va_list args;
		va_start (args, pattern);
		vsnprintf(booya, BIG_BUFFER_SIZE * 3, pattern, args);
		va_end(args);
	}
	malloc_strcpy(to, booya);
	return *to;
}

/* same thing, different variation */
char *BX_m_sprintf (const char *pattern, ...)
{
	char booya[BIG_BUFFER_SIZE * 3 + 1];
	*booya = 0;
	
	if (pattern)
	{
		va_list args;
		va_start (args, pattern);
		vsnprintf(booya, BIG_BUFFER_SIZE * 3, pattern, args);
		va_end(args);
	}
	return m_strdup(booya);
}

/* case insensitive string searching */
char	*BX_stristr (const char *source, const char *search)
{
        int     x = 0;

        if (!source || !*source || !search || !*search || strlen(source) < strlen(search))
		return NULL;

        while (*source)
        {
                if (source[x] && toupper(source[x]) == toupper(search[x]))
			x++;
                else if (search[x])
			source++, x = 0;
		else
			return (char *)source;
        }
	return NULL;
}

/* case insensitive string searching from the end */
char	*BX_rstristr (char *source, char *search)
{
	char *ptr;
	int x = 0;

        if (!source || !*source || !search || !*search || strlen(source) < strlen(search))
		return NULL;

	ptr = source + strlen(source) - strlen(search);

	while (ptr >= source)
        {
		if (!search[x])
			return ptr;

		if (toupper(ptr[x]) == toupper(search[x]))
			x++;
		else
			ptr--, x = 0;
	}
	return NULL;
}

/* 
 * word_count:  Efficient way to find out how many words are in
 * a given string.  Relies on isspace() not being broken.
 */
int     BX_word_count (char *str)
{
        int cocs = 0;
        int isv = 1;
register char *foo = str;

        if (!foo)
                return 0;

        while (*foo)
        {
		if (*foo == '"' && isv)
		{
			while (*(foo+1) && *++foo != '"')
				;
			isv = 0;
			cocs++;
		}
                if (!my_isspace(*foo) != !isv)
                {
                        isv = my_isspace(*foo);
                        cocs++;
                }
                foo++;
        }
        return (cocs + 1) / 2;
}

#if 0
extern  int     word_scount (char *str)
{
        int cocs = 0;
        char *foo = str;
	int isv = 1;
	
        if (!foo)
                return 0;

        while (*foo)
        {
                if (my_isspace(*foo) != !isv)
                {
                        isv = my_isspace(*foo);
                        cocs++;
                }
                foo++;
        }
        return (cocs + 1) / 2;
}
#endif

char *BX_next_arg(char *str, char **new_ptr)
{
	char *ptr;

	if (!str)
		return NULL;

	if ((ptr = inv_strpbrk(str, " ")) != NULL)
	{
		if ((str = strchr(ptr, ' ')) != NULL)
			*str++ = 0;
		else
			str = empty_string;
	}
	else
	{
		str = empty_string;
	}

	if (new_ptr)
		*new_ptr = str;

	return ptr;
}

extern char *BX_remove_trailing_spaces (char *foo)
{
	char *end;
	if (!*foo)
		return foo;

	end = foo + strlen(foo) - 1;
	while (end > foo && my_isspace(*end))
		end--;
	end[1] = 0;
	return foo;
}

/*
 * yanks off the last word from 'src'
 * kinda the opposite of next_arg
 */
char *BX_last_arg (char **src)
{
	char *ptr;

	if (!src || !*src)
		return NULL;

	remove_trailing_spaces(*src);
	ptr = *src + strlen(*src) - 1;

	if (*ptr == '"')
	{
		for (ptr--;;ptr--)
		{
			if (*ptr == '"')
			{
				if (ptr == *src)
					break;
				if (ptr[-1] == ' ')
				{
					ptr--;
					break;
				}
			}
			if (ptr == *src)
				break;
		}
	}
	else
	{
		for (;;ptr--)
		{
			if (*ptr == ' ')
				break;
			if (ptr == *src)
				break;
		}
	}

	if (ptr == *src)
	{
		ptr = *src;
		*src = empty_string;
	}
	else
	{
		*ptr++ = 0;
		remove_trailing_spaces(*src);
	}
	return ptr;

}

#define risspace(c) (c == ' ')
char	*BX_new_next_arg (char *str, char **new_ptr)
{
	char	*ptr,
		*start;

	if (!str || !*str)
		return NULL;

	ptr = str;
	while (*ptr && risspace(*ptr))
		ptr++;

	if (*ptr == '"')
	{
		start = ++ptr;
		for (str = start; *str; str++)
		{
			if (*str == '\\' && str[1])
				str++;
			else if (*str == '"')
			{
				*(str++) = 0;
				if (risspace(*str))
					str++;
				break;
			}
		}
	}
	else if (*ptr)
	{
		str = ptr;
		while (*str && !risspace(*str))
			str++;
		if (*str)
			*str++ = 0;
	}

	if (!*str || !*ptr)
		str = empty_string;

	if (new_ptr)
		*new_ptr = str;

	return ptr;
}

/*
 * This function is "safe" because it doesn't ever return NULL.
 * XXXX - this is an ugly kludge that needs to go away
 */
char	*safe_new_next_arg (char *str, char **new_ptr)
{
	char	*ptr,
		*start;

	if (!str || !*str)
		return empty_string;

	if ((ptr = inv_strpbrk(str, " \t")) != NULL)
	{
		if (*ptr == '"')
		{
			start = ++ptr;
			while ((str = strpbrk(ptr, "\"\\")) != NULL)
			{
				switch (*str)
				{
					case '"':
						*str++ = '\0';
						if (*str == ' ')
							str++;
						if (new_ptr)
							*new_ptr = str;
						return (start);
					case '\\':
						if (*(str + 1) == '"')
							ov_strcpy(str, str + 1);
						ptr = str + 1;
				}
			}
			str = empty_string;
		}
		else
		{
			if ((str = strpbrk(ptr, " \t")) != NULL)
				*str++ = '\0';
			else
				str = empty_string;
		}
	}
	else
		str = empty_string;

	if (new_ptr)
		*new_ptr = str;

	if (!ptr)
		return empty_string;

	return ptr;
}

char	*BX_new_new_next_arg (char *str, char **new_ptr, char *type)
{
	char	*ptr,
		*start;

	if (!str || !*str)
		return NULL;

	if ((ptr = inv_strpbrk(str, " \t")) != NULL)
	{
		if ((*ptr == '"') || (*ptr == '\''))
		{
			char accept[] = { *ptr, '\\', 0 };

			*type = *ptr;
			start = ++ptr;
			while ((str = strpbrk(ptr, accept)) != NULL)
			{
				switch (*str)
				{
				case '\'':
				case '"':
					*str++ = '\0';
					if (*str == ' ')
						str++;
					if (new_ptr)
						*new_ptr = str;
					return (start);
				case '\\':
					if (str[1] == *type)
						ov_strcpy(str, str + 1);
					ptr = str + 1;
				}
			}
			str = empty_string;
		}
		else
		{
			*type = '\"';
			if ((str = strpbrk(ptr, " \t")) != NULL)
				*str++ = 0;
			else
				str = empty_string;
		}
	}
	else
		str = empty_string;
	if (new_ptr)
		*new_ptr = str;
	return ptr;
}

unsigned char stricmp_table [] = 
{
	0,	1,	2,	3,	4,	5,	6,	7,
	8,	9,	10,	11,	12,	13,	14,	15,
	16,	17,	18,	19,	20,	21,	22,	23,
	24,	25,	26,	27,	28,	29,	30,	31,
	32,	33,	34,	35,	36,	37,	38,	39,
	40,	41,	42,	43,	44,	45,	46,	47,
	48,	49,	50,	51,	52,	53,	54,	55,
	56,	57,	58,	59,	60,	61,	62,	63,
	64,	65,	66,	67,	68,	69,	70,	71,
	72,	73,	74,	75,	76,	77,	78,	79,
	80,	81,	82,	83,	84,	85,	86,	87,
	88,	89,	90,	91,	92,	93,	94,	95,
	96,	65,	66,	67,	68,	69,	70,	71,
	72,	73,	74,	75,	76,	77,	78,	79,
	80,	81,	82,	83,	84,	85,	86,	87,
	88,	89,	90,	91,	92,	93,	126,	127,

	128,	129,	130,	131,	132,	133,	134,	135,
	136,	137,	138,	139,	140,	141,	142,	143,
	144,	145,	146,	147,	148,	149,	150,	151,
	152,	153,	154,	155,	156,	157,	158,	159,
	160,	161,	162,	163,	164,	165,	166,	167,
	168,	169,	170,	171,	172,	173,	174,	175,
	176,	177,	178,	179,	180,	181,	182,	183,
	184,	185,	186,	187,	188,	189,	190,	191,
	192,	193,	194,	195,	196,	197,	198,	199,
	200,	201,	202,	203,	204,	205,	206,	207,
	208,	209,	210,	211,	212,	213,	214,	215,
	216,	217,	218,	219,	220,	221,	222,	223,
	224,	225,	226,	227,	228,	229,	230,	231,
	232,	233,	234,	235,	236,	237,	238,	239,
	240,	241,	242,	243,	244,	245,	246,	247,
	248,	249,	250,	251,	252,	253,	254,	255
};

/* my_stricmp: case insensitive version of strcmp */
int	BX_my_stricmp (const char *str1, const char *str2)
{
	while (*str1 && *str2 && (stricmp_table[(unsigned char)*str1] == stricmp_table[(unsigned char)*str2]))
		str1++, str2++;
	return (stricmp_table[(unsigned char)*str1] -
		stricmp_table[(unsigned char)*str2]);

}

/* my_strnicmp: case insensitive version of strncmp */
int	BX_my_strnicmp (const char *str1, const char *str2, size_t n)
{
	while (n && *str1 && *str2 && (stricmp_table[(unsigned char)*str1] == stricmp_table[(unsigned char)*str2]))
		str1++, str2++, n--;
	return (n ?
		(stricmp_table[(unsigned char)*str1] -
		stricmp_table[(unsigned char)*str2]) : 0);
}

/* my_strnstr: case insensitive version of strstr */
int	BX_my_strnstr (const char *str1, const char *str2, size_t n)
{
	const char *p = str1;
	if (!p) return 0;
	for (; *p; p++)
		if (!strncasecmp(p, str2, strlen(str2)))
			return 1;
	return 0;
}

/* chop -- chops off the last character. capiche? */
char *BX_chop (char *stuff, int nchar)
{
	size_t sl = strlen(stuff);
	if (nchar > 0 && sl > 0 && nchar <= sl)
		stuff[sl - nchar] = 0;
	else if (nchar > sl)
		stuff[0] = 0;
	return stuff;
}

/*
 * strext: Makes a copy of the string delmited by two char pointers and
 * returns it in malloced memory.  Useful when you don't want to munge up
 * the original string with a null.  End must be one place beyond where
 * you want to copy, ie, it's the first character you don't want to copy.
 */
char *strext(char *start, char *end)
{
	char *ptr, *retval;

	ptr = retval = (char *)new_malloc(end-start+1);
	while (start < end)
		*ptr++ = *start++;
	*ptr = 0;
	return retval;
}


/*
 * strmcpy: Well, it's like this, strncpy doesn't append a trailing null if
 * strlen(str) == maxlen.  strmcpy always makes sure there is a trailing null 
 */
char *	BX_strmcpy (char *dest, const char *src, int maxlen)
{
	strlcpy(dest, src, maxlen + 1);
	return dest;
}

/*
 * strmcat: like strcat, but truncs the dest string to maxlen (thus the dest
 * should be able to handle maxlen+1 (for the null)) 
 */
char *	BX_strmcat(char *dest, const char *src, int maxlen)
{
	strlcat(dest, src, maxlen + 1);
	return dest;
}

#ifdef INCLUDE_DEADCODE
/*
 * strmcat_ue: like strcat, but truncs the dest string to maxlen (thus the dest
 * should be able to handle maxlen + 1 (for the null)). Also unescapes
 * backslashes.
 */
char *	strmcat_ue(char *dest, const char *src, int maxlen)
{
	int	dstlen;

	dstlen = strlen(dest);
	dest += dstlen;
	maxlen -= dstlen;
	while (*src && maxlen > 0)
	{
		if (*src == '\\')
		{
			if (strchr("npr0", src[1]))
				*dest++ = '\020';
			else if (*(src + 1))
				*dest++ = *++src;
			else
				*dest++ = '\\';
		}
		else
			*dest++ = *src;
		src++;
	}
	*dest = '\0';
	return dest;
}
#endif

/*
 * m_strcat_ues: Given two strings, concatenate the 2nd string to
 * the end of the first one, but if the "unescape" argument is 1, do
 * unescaping (like in strmcat_ue).
 * (Malloc_STRCAT_UnEscape Special, in case you were wondering. ;-))
 *
 * This uses a cheating, "not-as-efficient-as-possible" algorithm,
 * but it works with negligible cpu lossage.
 */
char *	n_m_strcat_ues(char **dest, char *src, int unescape, const char *module, const char *file, const int line)
{
	int total_length;
	char *ptr, *ptr2;
	int z;

	if (!unescape)
	{
		n_malloc_strcat(dest, src, module, file, line);
		return *dest;
	}

	z = total_length = (*dest) ? strlen(*dest) : 0;
	total_length += strlen(src);

/*	RESIZE(*dest, char, total_length + 2);*/
	*dest = n_realloc(*dest, sizeof(char) * (total_length + 2), module, file,  line);
	if (z == 0)
		**dest = 0;

	ptr2 = *dest + z;
	for (ptr = src; *ptr; ptr++)
	{
		if (*ptr == '\\')
		{
			switch (*++ptr)
			{
				case 'n': case 'p': case 'r': case '0':
					*ptr2++ = '\020';
					break;
				case (char) 0:
					*ptr2++ = '\\';
					goto end_strcat_ues;
					break;
				default:
					*ptr2++ = *ptr;
			}
		}
		else
			*ptr2++ = *ptr;
	}
end_strcat_ues:
	*ptr2 = '\0';

	return *dest;
}

/*
 * scanstr: looks for an occurrence of str in source.  If not found, returns
 * 0.  If it is found, returns the position in source (1 being the first
 * position).  Not the best way to handle this, but what the hell 
 */
extern	int	BX_scanstr (char *str, char *source)
{
	int	i,
		max,
		len;

	len = strlen(str);
	max = strlen(source) - len;
	for (i = 0; i <= max; i++, source++)
	{
		if (!my_strnicmp(source, str, len))
			return (i + 1);
	}
	return (0);
}

#if defined(WINNT) || defined(__EMX__)
char *convert_dos(char *str)
{
register char *p;
	for (p = str; *p; p++)
		if (*p == '/')
			*p = '\\';
	return str;
}

char *convert_unix(char *arg)
{
register char *x = arg;
	while (*x)
	{
		if (*x == '\\')
			*x = '/';
		x++;
	}
	return arg;
}

int is_dos(char *filename)
{
	if (strlen(filename) > 3 && ( (*(filename+1) == ':') && (*(filename+2) == '/' || *(filename+2) == '\\')) )
		return 1;
	else
		return 0;
}

#endif


/* expand_twiddle: expands ~ in pathnames. */
char	*BX_expand_twiddle (char *str)
{
	char	buffer[BIG_BUFFER_SIZE/4 + 1];
	char *str2;

#ifdef WINNT
	convert_unix(str);
#endif
	if (*str == '~')
	{
		str++;

#if defined(WINNT) || defined(__EMX__)
		if (*str == '\\' || *str == '/')
#else
		if (*str == '/')
#endif
		{
			strlcpy(buffer, my_path, sizeof buffer);
			strlcat(buffer, str, sizeof buffer);
		}
		else
		{
			char	*rest;
			struct	passwd *entry;
			char	*p = NULL;

			if ((rest = strchr(str, '/')) != NULL)
				*rest++ = '\0';
#if defined(WINNT) || defined(__EMX__)
			if (((entry = getpwnam(str)) != NULL) || (p = getenv("HOME")))
			{
				if (p)
					strlcpy(buffer, p, sizeof buffer);
				else
					strlcpy(buffer, entry->pw_dir, sizeof buffer);
#else
			if ((entry = getpwnam(str)) != NULL || (p = getenv("HOME")))
			{
				if (p)
					strlcpy(buffer, p, sizeof buffer);
				else
					strlcpy(buffer, entry->pw_dir, sizeof buffer);
#endif
				if (rest)
				{
					strlcat(buffer, "/", sizeof buffer);
					strlcat(buffer, rest, sizeof buffer);
				}
			}
			else
				return (char *) NULL;
		}
	}
	else
		strlcpy(buffer, str, sizeof buffer);

	/* This isn't legal! */
	str2 = NULL;
	malloc_strcpy(&str2, buffer);
#ifdef __EMX__
	convert_unix(str2);
#endif
	return str2;
}

/* islegal: true if c is a legal nickname char anywhere but first char */
#define islegal(c) ((((c) >= 'A') && ((c) <= '}')) || \
		    (((c) >= '0') && ((c) <= '9')) || \
		     ((c) == '-') || ((c) == '_'))

/*
 * check_nickname: checks is a nickname is legal.  If the first character is
 * bad new, null is returned.  If the first character is bad, the string is
 * truncd down to only legal characters and returned 
 *
 * rewritten, with help from do_nick_name() from the server code (2.8.5),
 * phone, april 1993.
 */
char	*BX_check_nickname (char *nick)
{
	char	*s;
	int	len = 0;
	if (!nick || *nick == '-' || isdigit((unsigned char)*nick) ||
	    !islegal(*nick))
		return NULL;

	for (s = nick; *s && (s - nick) < NICKNAME_LEN ; s++, len++)
		if (!islegal(*s) || my_isspace(*s))
			break;
	*s = '\0';
	
	return *nick ? nick : NULL;
}

/* inv_strcpbrk()
 *
 * Returns a pointer to the first character in a string which is NOT found
 * in 'reject'.
 *
 * This is the inverse of the standard function strpbrk().
 */
char *inv_strpbrk(const char *s, const char *reject)
{
	s += strspn(s, reject);

	if (!*s)
		return NULL;

	return (char *)s;
}

/* sindex()
 *
 * Returns a pointer to the first matching character in a string, or NULL if
 * there are no matching characters.
 *
 * A matching character is any character in 'group', unless the first
 * character of 'group' is a ^, in which case a matching character is any
 * character NOT in 'group'.
 */
char *BX_sindex(const char *string, const char *group)
{
	if (!string || !group)
		return NULL;

	if (*group == '^')
		return inv_strpbrk(string, group + 1);
	else
		return strpbrk(string, group);
}

/* rsindex()
 *
 * Returns a pointer to the last matching character that appears in a string BEFORE the
 * initial search point (to search the entire string, set the initial search point to
 * the terminating NUL). Returns NULL if no matching characters are found.
 *
 * The string to be searched starts at 'start' and the initial search point is 'ptr'.
 *
 * A matching character is any character in 'group', unless the first character of 'group'
 * is a ^, in which case a matching character is any character NOT in 'group'.
 */
char *BX_rsindex(const char *ptr, const char *start, const char *group)
{
	if (ptr && start && group && start <= ptr)
	{
		int invert = 0;

		if (*group == '^')
		{
			group++;
			invert = 1;
		}

		while (ptr > start)
		{
			ptr--;
			if (invert == !strchr(group, *ptr))
				return (char *)ptr;
		}
	}
	return NULL;
}

/* is_number: returns true if the given string is a number, false otherwise */
int	BX_is_number (const char *str)
{
	if (!str || !*str)
		return 0;
	while (*str == ' ')
		str++;
	if (*str == '-' || *str == '+')
		str++;
	if (*str)
	{
		for (; *str; str++)
		{
			if (!isdigit((unsigned char)(*str)))
				return (0);
		}
		return 1;
	}
	else
		return 0;
}

/* rfgets: exactly like fgets, cept it works backwards through a file!  */
char	*BX_rfgets (char *buffer, int size, FILE *file)
{
	char	*ptr;
	off_t	pos;

	if (fseek(file, -2L, SEEK_CUR))
		return NULL;
	do
	{
		switch (fgetc(file))
		{
		case EOF:
			return NULL;
		case '\n':
			pos = ftell(file);
			ptr = fgets(buffer, size, file);
			fseek(file, pos, 0);
			return ptr;
		}
	}
	while (fseek(file, -2L, SEEK_CUR) == 0);
	rewind(file);
	pos = 0L;
	ptr = fgets(buffer, size, file);
	fseek(file, pos, 0);
	return ptr;
}

/*
 * path_search: given a file called name, this will search each element of
 * the given path to locate the file.  If found in an element of path, the
 * full path name of the file is returned in a static string.  If not, null
 * is returned.  Path is a colon separated list of directories 
 */
char	*BX_path_search (char *name, char *path)
{
	char *free_path, *ptr;
	static char buffer[BIG_BUFFER_SIZE/2];

	/* A "relative" path is valid if the file exists */
	/* A "relative" path is searched in the path if the
	   filename doesn't really exist from where we are */
	if (strchr(name, '/'))
		if (!access(name, F_OK))
			return name;

	/* an absolute path is always checked, never searched */
#if defined(WINNT) || defined(__EMX__)
	if (name[0] == '/' || name[0] == '\\')
#else
	if (name[0] == '/')
#endif
		return (access(name, F_OK) ? NULL : name);
	
	if (!path)
		return NULL;

	/* This is cheating. >;-) */
	free_path = LOCAL_COPY(path);
	path = free_path;

#ifdef __EMX__
	convert_unix(path);
#endif
	while (path)
	{
#if defined(WINNT) || defined(__EMX__)
		if (((ptr = strchr(path, ';')) != NULL) || ((ptr = strchr(path, ':')) != NULL))
#else
		if ((ptr = strchr(path, ':')) != NULL)
#endif
			*ptr++ = '\0';
		*buffer = 0;
		if (path[0] == '~')
		{
			strlcat(buffer, my_path, sizeof buffer);
			path++;
		}
		strlcat(buffer, path, sizeof buffer);
		strlcat(buffer, "/", sizeof buffer);
		strlcat(buffer, name, sizeof buffer);

		if (access(buffer, F_OK) == 0)
			break;
		path = ptr;
	}

	return (path != NULL) ? buffer : NULL;
}

/*
 * double_quote: Given a str of text, this will quote any character in the
 * set stuff with the QUOTE_CHAR. It returns a malloced quoted, null
 * terminated string 
 */
char	*BX_double_quote (const char *str, const char *stuff, char *buffer)
{
	register char	c;
	register int	pos;

	*buffer = 0;
	if (!stuff)
		return buffer;
	for (pos = 0; (c = *str); str++)
	{
		if (strchr(stuff, c))
		{
			if (c == '$')
				buffer[pos++] = '$';
			else
				buffer[pos++] = '\\';
		}
		buffer[pos++] = c;
	}
	buffer[pos] = '\0';
	return buffer;
}

void	BX_ircpanic (char *format, ...)
{
	char buffer[3 * BIG_BUFFER_SIZE + 1];
	extern char cx_function[];
	static int recursion = 0;
	if (recursion || x_debug & DEBUG_CRASH)
		abort();

	recursion++;	
	if (format)
	{
		va_list arglist;
		va_start(arglist, format);
		vsnprintf(buffer, BIG_BUFFER_SIZE, format, arglist);
		va_end(arglist);
	}

	yell("An unrecoverable logic error has occurred.");
	yell("Please email " BUG_EMAIL " and include the following message:");

	yell("Panic: [%s:%s %s]", irc_version, buffer, cx_function);
	dump_call_stack();
	irc_exit(1, "BitchX panic... Could it possibly be a bug?  Nahhhh...", NULL);
}

/* Not very complicated, but very handy function to have */
int BX_end_strcmp (const char *one, const char *two, int bytes)
{
	if (bytes < strlen(one))
		return (strcmp(one + strlen (one) - (size_t) bytes, two));
	else
		return -1;
}

/* beep_em: Not hard to figure this one out */
void BX_beep_em (int beeps)
{
	int i;

	for (i = 0; i < beeps; i++)
		term_beep();
}

FILE *open_compression (char *executable, char *filename, int hook)
{
	FILE *file_pointer = NULL;
	int pipes[2];

	
	pipes[0] = -1;
	pipes[1] = -1;

	if (pipe (pipes) == -1)
	{
		if (hook)
			yell("Cannot start decompression: %s\n", strerror(errno));
		if (pipes[0] != -1)
		{
			close (pipes[0]);
			close (pipes[1]);
		}
		return NULL;
	}

	switch (fork ())
	{
		case -1:
		{
			if (hook)
				yell("Cannot start decompression: %s\n", strerror(errno));
			return NULL;
		}
		case 0:
		{
			int i;
#if !defined(WINNT) && !defined(__EMX__)
			setsid();
#endif			
			setgid(getgid());
			setuid(getuid());
			dup2 (pipes[1], 1);
			close (pipes[0]);
			for (i = 2; i < 256; i++)
				close(i);
#ifdef ZARGS
			execl (executable, executable, "-c", ZARGS, filename, NULL);
#else
			execl (executable, executable, "-c", filename, NULL);
#endif
			_exit (0);
		}
		default :
		{
			close (pipes[1]);
			if ((file_pointer = fdopen(pipes[0], "r")) == NULL)
			{
				if (hook)
					yell("Cannot start decompression: %s\n", strerror(errno));
				return NULL;
			}
#if 0
			setlinebuf(file_pointer);
			setvbuf(file_pointer, NULL, _IONBF, 0);
#endif
			break;
		}
	}
	return file_pointer;
}

/* Front end to fopen() that will open ANY file, compressed or not, and
 * is relatively smart about looking for the possibilities, and even
 * searches a path for you! ;-)
 */
FILE *BX_uzfopen (char **filename, char *path, int hook)
{
	static int	setup				= 0;
	int 		ok_to_decompress 		= 0;
	char *		filename_path;
	char 		*filename_trying;
	char		*filename_blah;
	static char 	*path_to_gunzip = NULL;
	static char	*path_to_uncompress = NULL;
	static char	*path_to_bunzip2 = NULL;
	FILE *		doh = NULL;

	filename_trying = alloca(MAXPATHLEN+1);
	
	if (!setup)
	{
		char *gzip = path_search("gunzip", getenv("PATH"));
		char *compress = NULL;
		char *bzip = NULL;
		if (!gzip)
			gzip = empty_string;
		path_to_gunzip = m_strdup(gzip);

		if (!(compress = path_search("uncompress", getenv("PATH"))))
			compress = empty_string;
		path_to_uncompress = m_strdup(compress);

		if (!(bzip = path_search("bunzip2", getenv("PATH"))))
			bzip = empty_string;
		path_to_bunzip2 = m_strdup(bzip);
		setup = 1;
	}

	/* It is allowed to pass to this function either a true filename
	   with the compression extension, or to pass it the base name of
	   the filename, and this will look to see if there is a compressed
	   file that matches the base name */

	/* Start with what we were given as an initial guess */
	if (**filename == '~')
	{
		filename_blah = expand_twiddle(*filename);
		strlcpy(filename_trying, filename_blah, MAXPATHLEN);
		new_free(&filename_blah);		
	}
	else
		strlcpy(filename_trying, *filename, MAXPATHLEN);

	/* Look to see if the passed filename is a full compressed filename */
	if ((! end_strcmp (filename_trying, ".gz", 3)) ||
	    (! end_strcmp (filename_trying, ".z", 2))) 
	{
		if (path_to_gunzip)
		{	
			ok_to_decompress = 1;
			filename_path = path_search (filename_trying, path);
		}
		else
		{
			if (hook)
				yell("Cannot open file %s because gunzip was not found", filename_trying);
			new_free(filename);
			return NULL;
		}
	}
	else if (! end_strcmp (filename_trying, ".Z", 2))
	{
		if (path_to_gunzip || path_to_uncompress)
		{
			ok_to_decompress = 1;
			filename_path = path_search (filename_trying, path);
		}
		else
		{
			if (hook)
				yell("Cannot open file %s because uncompress was not found", filename_trying);
			new_free(filename);
			return NULL;
		}
	}
	else if (!end_strcmp(filename_trying, ".bz2", 4))
	{
		if (*path_to_bunzip2)
		{
			ok_to_decompress = 3;
			filename_path = path_search(filename_trying, path);
		}
		else
		{
			if (hook)
				yell("Cannot open file %s because bunzip2 was not found", filename_trying);
			new_free(filename);
			return NULL;
		}
	}
	/* Right now it doesn't look like the file is a full compressed fn */
	else
	{
		struct stat file_info;

		/* Trivially, see if the file we were passed exists */
		filename_path = path_search (filename_trying, path);

		/* Nope. it doesn't exist. */
		if (!filename_path)
		{
			/* Is there a "filename.gz"? */
			strlcpy (filename_trying, *filename, MAXPATHLEN);
			strlcat (filename_trying, ".gz", MAXPATHLEN);
			filename_path = path_search (filename_trying, path);

			/* Nope. no "filename.gz" */
			if (!filename_path)
			{
				/* Is there a "filename.Z"? */
				strlcpy (filename_trying, *filename, MAXPATHLEN);
				strlcat (filename_trying, ".Z", MAXPATHLEN);
				filename_path = path_search (filename_trying, path);
				
				/* Nope. no "filename.Z" */
				if (!filename_path)
				{
					/* Is there a "filename.z"? */
					strlcpy (filename_trying, *filename, MAXPATHLEN);
					strlcat (filename_trying, ".z", MAXPATHLEN);
					filename_path = path_search (filename_trying, path);

					if (!filename_path)
					{
						strlcpy(filename_trying, *filename, MAXPATHLEN);
						strlcat(filename_trying, ".bz2", MAXPATHLEN);
						filename_path = path_search(filename_trying, path);
						if (!filename_path)
						{
							if (hook)
								yell("File not found: %s", *filename);
							new_free(filename);
							return NULL;
						}
						else
							/* found a bz2 */
							ok_to_decompress = 3;
					}
					/* Yep. there's a "filename.z" */
					else
						ok_to_decompress = 2;
				}
				/* Yep. there's a "filename.Z" */
				else
					ok_to_decompress = 1;
			}
			/* Yep. There's a "filename.gz" */
			else
				ok_to_decompress = 2;
		}
		/* Imagine that! the file exists as-is (no decompression) */
		else
			ok_to_decompress = 0;

		stat (filename_path, &file_info);
		if (file_info.st_mode & S_IFDIR)
		{
			if (hook)
				yell("%s is a directory", filename_trying);
			new_free(filename);
			return NULL;
		}
#ifndef WINNT
		if (file_info.st_mode & 0111)
		{
			char *p;
			if ((p = strrchr(filename_path, '.')))
			{
				p++;
				if (!strcmp(p, "so"))
				{
					malloc_strcpy(filename, filename_path);
					return NULL;
				}
			}
			if (hook)
				yell("Cannot open %s -- executable file", filename_trying);
			new_free(filename);
			return NULL;
		}
#endif
	}

	malloc_strcpy (filename, filename_path);

	/* at this point, we should have a filename in the variable
	   filename_trying, and it should exist.  If ok_to_decompress
	   is one, then we can gunzip the file if guzip is available,
	   else we uncompress the file */
	if (ok_to_decompress)
	{
		if (ok_to_decompress <= 2 && *path_to_gunzip)
			return open_compression (path_to_gunzip, filename_path, hook);
		else if ((ok_to_decompress == 1) && *path_to_uncompress)
			return open_compression (path_to_uncompress, filename_path, hook);
		else if ((ok_to_decompress == 3) && *path_to_bunzip2)
			return open_compression(path_to_bunzip2, filename_path, hook);
			
		if (hook)
			yell("Cannot open compressed file %s because no uncompressor was found", filename_trying);
		new_free(filename);
		return NULL;
	}

	/* Its not a compressed file... Try to open it regular-like. */
	if ((doh = fopen(filename_path, "r")) != NULL)
		return doh;

	/* nope.. we just can't seem to open this file... */
	if (hook)
		yell("Cannot open file %s: %s", filename_path, strerror(errno));
	new_free(filename);
	return NULL;
}


#ifdef INCLUDE_DEADCODE
/* some more string manips by hop (june, 1995) */
extern int fw_strcmp(comp_len_func *compar, char *one, char *two)
{
	int len = 0;
	char *pos = one;

	while (!my_isspace(*pos))
		pos++, len++;

	return compar(one, two, len);
}


/* 
 * Compares the last word in 'one' to the string 'two'.  You must provide
 * the compar function.  my_stricmp is a good default.
 */
extern int lw_strcmp(comp_func *compar, char *one, char *two)
{
	char *pos = one + strlen(one) - 1;

	if (pos > one)			/* can't do pos[-1] if pos == one */
		while (!my_isspace(pos[-1]) && (pos > one))
			pos--;
	else
		pos = one;

	if (compar)
		return compar(pos, two);
	else
		return my_stricmp(pos, two);
}

/* 
 * you give it a filename, some flags, and a position, and it gives you an
 * fd with the file pointed at the 'position'th byte.
 */
extern int opento(char *filename, int flags, off_t position)
{
	int file;

	file = open(filename, flags, 777);
	lseek(file, position, SEEK_SET);
	return file;
}
#endif

/* swift and easy -- returns the size of the file */
off_t file_size (char *filename)
{
	struct stat statbuf;

	if (!stat(filename, &statbuf))
		return (off_t)(statbuf.st_size);
	else
		return -1;
}

/* Gets the time in second/usecond if you can,  second/0 if you can't. */
struct timeval BX_get_time(struct timeval *timer)
{
	static struct timeval timer2;
#ifdef HAVE_GETTIMEOFDAY
	if (timer)
	{
		gettimeofday(timer, NULL);
		return *timer;
	}
	gettimeofday(&timer2, NULL);
	return timer2;
#else
	time_t time2 = time(NULL);

	if (timer)
	{
		timer->tv_sec = time2;
		timer->tv_usec = 0;
		return *timer;
	}
	timer2.tv_sec = time2;
	timer2.tv_usec = 0;
	return timer2;
#endif
}

/* 
 * calculates the time elapsed between 'one' and 'two' where they were
 * gotten probably with a call to get_time.  'one' should be the older
 * timer and 'two' should be the most recent timer.
 */
double BX_time_diff (struct timeval one, struct timeval two)
{
	struct timeval td;

	td.tv_sec = two.tv_sec - one.tv_sec;
	td.tv_usec = two.tv_usec - one.tv_usec;

	return (double)td.tv_sec + ((double)td.tv_usec / 1000000.0);
}

/* Calculate time elapsed since a time in the past. */
double time_since(const struct timeval *tv_from)
{
    struct timeval tv_now;

	get_time(&tv_now);
	return BX_time_diff(*tv_from, tv_now);
}

/* Calculate time from now until a time in the future. */
double time_until(const struct timeval *tv_to)
{
	struct timeval tv_now;

	get_time(&tv_now);
	return BX_time_diff(tv_now, *tv_to);
}

/* Compare two timevals - returns 1, 0 or -1 if a is >, = or < b */
int time_cmp(const struct timeval *a, const struct timeval *b)
{
	if (a->tv_sec == b->tv_sec)
		return (a->tv_usec > b->tv_usec) - (a->tv_usec < b->tv_usec);
	else
		return (a->tv_sec > b->tv_sec) - (a->tv_sec < b->tv_sec);
}

/* Add an offset, in seconds, to a timeval */
struct timeval *time_offset(struct timeval *tv, double offset)
{
	time_t seconds = offset;		/* Discards fractional part */

	offset -= seconds;
	if (offset < 0.0)
	{
		/* This ensures we never make tv_usec go negative, since it might be unsigned. */
		offset += 1.0;
		seconds -= 1;
	}
	tv->tv_usec += offset * 1000000.0;
	seconds += tv->tv_usec / 1000000;
	tv->tv_usec %= 1000000;
	tv->tv_sec += seconds;

	return tv;
}

int BX_time_to_next_minute (void)
{
	time_t now = time(NULL);
	static int which = 0;
	
	if (which == 1)
		return 60 - now % 60;
	else
	{	
		struct tm *now_tm = gmtime(&now);

		if (!which)
		{
			if (now_tm->tm_sec == now % 60)
				which = 1;
			else
				which = 2;
		}
		return 60-now_tm->tm_sec;
	}
}

char *BX_plural (int number)
{
	return (number != 1) ? "s" : empty_string;
}

char *BX_my_ctime (time_t when)
{
	return chop(ctime(&when), 1);
}

char *BX_my_ltoa (long foo)
{
	static char buffer[BIG_BUFFER_SIZE/8+1];
	char *pos = buffer + BIG_BUFFER_SIZE/8-1;
	unsigned long my_absv;
	int negative;

	my_absv = (foo < 0) ? (unsigned long)-foo : (unsigned long)foo;
	negative = (foo < 0) ? 1 : 0;

	buffer[BIG_BUFFER_SIZE/8] = 0;
	for (; my_absv > 9; my_absv /= 10)
		*pos-- = (my_absv % 10) + '0';
	*pos = (my_absv) + '0';

	if (negative)
		*--pos = '-';

	return pos;
}

/*
 * Formats "src" into "dest" using the given length.  If "length" is
 * negative, then the string is right-justified.  If "length" is
 * zero, nothing happens.  Sure, i cheat, but its cheaper then doing
 * two sprintf's.
 */
char *BX_strformat (char *dest, const char *src, int length, char padc)
{
	char *ptr1 = dest, 
	     *ptr2 = (char *)src;
	int tmplen = length;
	int abslen;
		
	abslen = (length >= 0 ? length : -length);
	if (!padc)
		padc = ' ';

	/* Cheat by spacing out 'dest' */
	for (tmplen = abslen - 1; tmplen >= 0; tmplen--)
		dest[tmplen] = padc;
	dest[abslen] = 0;

	/* Then cheat further by deciding where the string should go. */
	if (length > 0)		/* left justified */
	{
		while ((length-- > 0) && *ptr2)
			*ptr1++ = *ptr2++;
	}
	else if (length < 0)	/* right justified */
	{
		length = -length;
		ptr1 = dest;
		ptr2 = (char *)src;
		if (strlen(src) < length)
			ptr1 += length - strlen(src);
		while ((length-- > 0) && *ptr2)
			*ptr1++ = *ptr2++;
	}
	return dest;
}


/* MatchingBracket returns the next unescaped bracket of the given type */
char	*BX_MatchingBracket(register char *string, register char left, register char right)
{
	int	bracket_count = 1;

	if (left == '(')
	{
		for (; *string; string++)
		{
			switch (*string)
			{
				case '(':
					bracket_count++;
					break;
				case ')':
					bracket_count--;
					if (bracket_count == 0)
						return string;
					break;
				case '\\':
					if (string[1])
						string++;
					break;
			}
		}
	}
	else if (left == '[')
	{
		for (; *string; string++)
		{
			switch (*string)
	    		{
				case '[':
					bracket_count++;
					break;
				case ']':
					bracket_count--;
					if (bracket_count == 0)
						return string;
					break;
				case '\\':
					if (string[1])
						string++;
					break;
			}
		}
	}
	else		/* Fallback for everyone else */
	{
		while (*string && bracket_count)
		{
			if (*string == '\\' && string[1])
				string++;
			else if (*string == left)
				bracket_count++;
			else if (*string == right)
			{
				if (--bracket_count == 0)
					return string;
			}
			string++;
		}
	}

	return NULL;
}

/*
 * parse_number: returns the next number found in a string and moves the
 * string pointer beyond that point	in the string.  Here's some examples: 
 *
 * "123harhar"  returns 123 and str as "harhar" 
 *
 * while: 
 *
 * "hoohar"     returns -1  and str as "hoohar" 
 */
extern	int	BX_parse_number(char **str)
{
	long ret;
	char *ptr = *str;	/* sigh */

	ret = strtol(ptr, str, 10);
	if (*str == ptr)
		ret = -1;

	return (int)ret;
}

#if 0
extern char *chop_word(char *str)
{
	char *end = str + strlen(str) - 1;

	while (my_isspace(*end) && (end > str))
		end--;
	while (!my_isspace(*end) && (end > str))
		end--;

	if (end >= str)
		*end = 0;

	return str;
}
#endif

extern int BX_splitw (char *str, char ***to)
{
	int numwords = word_count(str);
	int counter;
	if (numwords)
	{
		*to = (char **)new_malloc(sizeof(char *) * numwords);
		for (counter = 0; counter < numwords; counter++)
			(*to)[counter] = new_next_arg(str, &str);
	}
	else
		*to = NULL;
	return numwords;
}

char * BX_unsplitw (char ***container, int howmany)
{
	char *retval = NULL;
	char **str = *container;

	while (howmany)
	{
		m_s3cat(&retval, space, *str);
		str++, howmany--;
	}

	new_free((char **)container);
	return retval;
}

char *BX_m_2dup (const char *str1, const char *str2)
{
	size_t msize = strlen(str1) + strlen(str2) + 1;
	return strcat(strcpy((char *)new_malloc(msize), str1), str2);
}

char *BX_m_e3cat (char **one, const char *yes1, const char *yes2)
{
	if (*one && **one)
		return m_3cat(one, yes1, yes2);
	else
		*one = m_2dup(yes1, yes2);
	return *one;
}


double strtod();
extern int BX_check_val (char *sub)
{
	long sval;
	char *endptr;

	if (!sub || !*sub)
		return 0;

#ifdef __EMX__
	if(strlen(sub) > 4 && strnicmp(sub, "infin", 5) == 0)
		return 0;
#endif

	/* get the numeric value (if any). */
	sval = strtod(sub, &endptr);

	/* Its OK if:
	 *  1) the f-val is not zero.
	 *  2) the first illegal character was not a null.
	 *  3) there were no valid f-chars.
	 */
	if (sval || *endptr || (sub == endptr))
		return 1;

	return 0;
}

char *BX_on_off(int var)
{
	if (var)
		return ("On");
	return ("Off");
}


/*
 * Appends 'num' copies of 'app' to the end of 'str'.
 */
extern char *BX_strextend(char *str, char app, int num)
{
	char *ptr = str + strlen(str);

	for (;num;num--)
		*ptr++ = app;

	*ptr = (char) 0;
	return str;
}

const char *BX_strfill(char c, int num)
{
static char buffer[BIG_BUFFER_SIZE/4+1];
int i = 0;
	if (num > BIG_BUFFER_SIZE/4)
		num = BIG_BUFFER_SIZE/4;
	for (i = 0; i < num; i++)
		buffer[i] = c;
	buffer[num] = 0;
	return buffer;
}

#ifdef INCLUDE_DEADCODE
/*
 * Appends the given character to the string
 */
char *strmccat(char *str, char c, int howmany)
{
	int x = strlen(str);

	if (x < howmany)
		str[x] = c;
	str[x+1] = 0;

	return str;
}

/*
 * Pull a substring out of a larger string
 * If the ending delimiter doesn't occur, then we don't pass
 * anything (by definition).  This is because we don't want
 * to introduce a back door into CTCP handlers.
 */
extern char *BX_pullstr (char *source_string, char *dest_string)
{
	char delim = *source_string;
	char *end;

	end = strchr(source_string + 1, delim);

	/* If there is no closing delim, then we punt. */
	if (!end)
		return NULL;

	*end = 0;
	end++;

	strcpy(dest_string, source_string + 1);
	strcpy(source_string, end);
	return dest_string;
}
#endif

extern int BX_empty (const char *str)
{
	while (str && *str && *str == ' ')
		str++;

	if (str && *str)
		return 0;

	return 1;
}

/* makes foo[one][two] look like tmp.one.two -- got it? */
char *BX_remove_brackets (const char *name, const char *args, int *arg_flag)
{
	char *ptr, *right, *result1, *rptr, *retval = NULL;

	/* XXXX - ugh. */
	rptr = m_strdup(name);

	while ((ptr = strchr(rptr, '[')))
	{
		*ptr++ = 0;
		right = ptr;
		if ((ptr = MatchingBracket(right, '[', ']')))
			*ptr++ = 0;

		if (args)
			result1 = expand_alias(right, args, arg_flag, NULL);
		else
			result1 = right;

		retval = m_3dup(rptr, ".", result1);
		if (ptr)
			malloc_strcat(&retval, ptr);

		if (args)
			new_free(&result1);
		if (rptr)
			new_free(&rptr);
		rptr = retval;
	}
	return upper(rptr);
}

long BX_my_atol (const char *str)
{
	if (str)
		return (long) strtol(str, NULL, 0);
	else
		return 0L;
}

#if 0
u_long hashpjw (char *text, u_long prime)
{
	char *p;
	u_long h = 0, g;

	for (p = text; *p; p++)
	{
		h <<= 4;
		h += *p;
		if ((g = h & 0xf0000000))
		{
			h ^= (g >> 24);
			h ^= g;
		}
	}
	return h % prime;
}
#endif

char *BX_m_dupchar(int i)
{
	char c = (char) i;	/* blah */
	char *ret = (char *)new_malloc(2);

	ret[0] = c;
	ret[1] = 0;
	return ret;
}

#ifdef INCLUDE_DEADCODE
/*
 * This checks to see if ``root'' is a proper subname for ``var''.
 */
int is_root (char *root, char *var, int descend)
{
	int rootl, varl;

	/* ``root'' must end in a dot */
	rootl = strlen(root);
	if (root[rootl] != '.')
		return 0;

	/* ``root'' must be shorter than ``var'' */
	varl = strlen(var);
	if (varl <= rootl)
		return 0;

	/* ``var'' must contain ``root'' as a leading subset */
	if (my_strnicmp(root, var, rootl))
		return 0;

	/* 
	 * ``var'' must not contain any additional dots
	 * if we are checking for the current level only
	 */
	if (!descend && strchr(var + rootl, '.'))
		return 0;

	/* Looks like its ok */
	return 1;
}
#endif

/* Returns the number of characters they are equal at. */
size_t BX_streq (const char *one, const char *two)
{
	size_t cnt = 0;

	while (*one && *two && (*one == *two))
		cnt++, one++, two++;

	return cnt;
}

/* Returns the number of characters they are equal at. */
size_t BX_strieq (const char *one, const char *two)
{
	size_t cnt = 0;

	while (*one && *two && (toupper(*one) == toupper(*two)))
		cnt++, one++, two++;

	return cnt;
}

char *n_m_strndup (const char *str, size_t len, const char *module, const char *file, const int line)
{
	char *retval = (char *)n_malloc(len + 1, module, file, line);
	return strmcpy(retval, (char *)str, len);
}

#if 0
char *remove_nl (char *str)
{
	char *ptr;

	if ((ptr = strrchr(str, '\n')))
		*ptr = 0;

	return str;
}

char *spanstr (const char *str, const char *tar)
{
	int cnt = 1;
	const char *p;

	for ( ; *str; str++, cnt++)
	{
		for (p = tar; *p; p++)
		{
			if (*p == *str)
				return (char *)p;
		}
	}

	return 0;
}

char *s_next_arg (char **from)
{
	char *next = strchr(*from, ' ');
	char *keep = *from;
	*from = next;
	return keep;
}
#endif

char *BX_strmopencat (char *dest, int maxlen, ...)
{
	va_list args;
	int size;
	char *this_arg = NULL;
	int this_len;

	size = strlen(dest);
	va_start(args, maxlen);

	while (size < maxlen)
	{
		if (!(this_arg = va_arg(args, char *)))
			break;

		if (size + ((this_len = strlen(this_arg))) > maxlen)
			strncat(dest, this_arg, maxlen - size);
		else
			strcat(dest, this_arg);

		size += this_len;
	}

	va_end(args);
	return dest;
}

/*
 * A strcpy that is guaranteed to be safe for overlaps.
 */
char *BX_ov_strcpy (char *one, const char *two)
{
	return memmove(one, two, strlen(two) + 1);
}

char *BX_next_in_comma_list (char *str, char **after)
{
	*after = str;

	while (*after && **after && **after != ',')
		(*after)++;

	if (*after && **after == ',')
	{
		**after = 0;
		(*after)++;
	}

	return str;
}

#ifdef INCLUDE_DEADCODE
/* 
 * Checks if ansi string only sets colors/attributes
 * ^[[m won't work anymore !!!!
 */
void FixColorAnsi(unsigned char *str)
{
#if !defined(WINNT) && !defined(__EMX__)
    register unsigned char *tmpstr;
    register unsigned char *tmpstr1=NULL;
    int  what=0;
    int  numbers=0;
    int	 val = 0;

	tmpstr=str;
	while (*tmpstr) 
	{
		if ((*tmpstr>='0' && *tmpstr<='9')) 
		{
			numbers = 1;
			val = val * 10 + (*tmpstr - '0');
		}
		else if (*tmpstr==';')
			numbers = 1, val = 0;
		else if (!(*tmpstr=='m' || *tmpstr=='C')) 
			numbers = val = 0;
		if (*tmpstr==0x1B) 
		{
			if (what && tmpstr1) 
				*tmpstr1+=64;
			what=1;
			tmpstr1=tmpstr;
		}
		else if (*tmpstr==0x18 || *tmpstr==0x0E) 
			*tmpstr+=64;
		if (what && numbers && (val > 130))
		{
			what = numbers = val = 0;
			*tmpstr1+=64;
		}
		if (what && *tmpstr=='m') 
		{
			if (!numbers || (val == 12))
			{
				*tmpstr1+=64;
				tmpstr1=tmpstr;
			}
			what=0;
			numbers = val = 0;
		}
		else if (what && *tmpstr=='C') 
		{
			if (!numbers)
			{
				*tmpstr1+=64;
				tmpstr1=tmpstr;
			}
			what=0;
			numbers = val = 0;
		}
		else if (what && *tmpstr=='J')
		{
			val = numbers = 0;
			*tmpstr1 +=64;
			tmpstr1=tmpstr;
			what = 0;
		}
		else if (what && *tmpstr=='(') 
			what=2;
		else if (what == 2 && (*tmpstr == '0'))
			*tmpstr1 += 64;
		else if (what == 2 && (*tmpstr=='U' || *tmpstr=='B'))
			what=0;
		tmpstr++;
	}
	if (what && tmpstr1 && *tmpstr1) 
		*tmpstr1+=64;
#endif
}

#endif
/* Dest should be big enough to hold "src" */
void	BX_strip_control (const char *src, char *dest)
{
	for (; *src; src++)
	{
		if (isgraph((unsigned char)*src) || isspace((unsigned char)*src))
			*dest++ = *src;
	}

	*dest++ = 0;
}

/*
 * figure_out_address
 */
int	BX_figure_out_address (char *nuh, char **nick, char **user, char **host, char **domain, int *ip)
{
	static char 	*mystuff = NULL;
	char 	*firstback, *secondback, *thirdback, *fourthback;
	char 	*bang, *at, *myhost = star, *endstring;
	int	number;

	/* Don't bother with channels, they're OK. */
	if (*nuh == '#' || *nuh == '&')
		return -1;

	malloc_strcpy(&mystuff, nuh);

	*nick = *user = *host = *domain = star;
	*ip = 0;

	bang = strchr(mystuff, '!');
	at = strchr(mystuff, '@');

	if (!bang && !at)
	{
		if (strchr(mystuff, '.') || strchr(mystuff, ':'))
			myhost = mystuff;
		else
		{
			*nick = mystuff;
			return 0;
		}
	}

	if (bang == mystuff)
		*user = bang + 1;
	else if (bang)
	{
		*nick = mystuff;
		*bang = 0;
		*user = bang + 1;
	}
	else
		*user = mystuff;
		
	if (at)
	{
		if (*user == star)
			*user = mystuff;
		*at = 0;
		myhost = at + 1;
	}

	/*
	 * At this point, 'myhost' points what what we think the hostname
	 * is.  We chop it up into discrete parts and see what we end up with.
	 */
	endstring = myhost + strlen(myhost);
	firstback = strnrchr(myhost, '.', 1);
	secondback = strnrchr(myhost, '.', 2);
	thirdback = strnrchr(myhost, '.', 3);
	fourthback = strnrchr(myhost, '.', 4);

	/* Track foo@bar or some such thing. */
	if (!firstback)
	{
		*host = myhost;
		return 0;
	}

	/*
	 * IP address (A.B.C.D)
	 */
	if (my_atol(firstback + 1))
	{
		*ip = 1;
		*domain = myhost;

		number = my_atol(myhost);
		if (number < 128)
			*host = thirdback;
		else if (number < 192)
			*host = secondback;
		else
			*host = firstback;

		**host = 0;
		(*host)++;
	}
	/*
	 *	(*).(*.???) 
	 *			Handles *.com, *.net, *.edu, etc
	 */
	else if (secondback && (endstring - firstback == 4))
	{
		*host = myhost;
		*domain = secondback;
		**domain = 0;
		(*domain)++;
	}
	/*
	 *	(*).(*.k12.??.us)
	 *			Handles host.school.k12.state.us
	 */
	else if (fourthback && 
			(firstback - secondback == 3) &&
			strbegins(thirdback, ".k12.") &&
			strbegins(firstback, ".us"))
	{
		*host = myhost;
		*domain = fourthback;
		**domain = 0;
		(*domain)++;
	}
	/*
	 *	()(*.k12.??.us)
	 *			Handles school.k12.state.us
	 */
	else if (thirdback && !fourthback && 
			(firstback - secondback == 3) &&
			strbegins(thirdback, ".k12.") &&
			strbegins(firstback, ".us"))
	{
		*host = empty_string;
		*domain = myhost;
	}
	/*
	 *	(*).(*.???.??)
	 *			Handles host.domain.com.au
	 */
	else if (thirdback && 
			(endstring - firstback == 3) &&
			(firstback - secondback == 4))
	{
		*host = myhost;
		*domain = thirdback;
		**domain = 0;
		(*domain)++;
	}
	/*
	 *	()(*.???.??)
	 *			Handles domain.com.au
	 */
	else if (secondback && !thirdback && 
			(endstring - firstback == 3) &&
		 	(firstback - secondback == 4))
	{
		*host = empty_string;
		*domain = myhost;
	}
	/*
	 *	(*).(*.??.??)
	 *			Handles host.domain.co.uk
	 */
	else if (thirdback && 
			(endstring - firstback == 3) &&
			(firstback - secondback == 3))
	{
		*host = myhost;
		*domain = thirdback;
		**domain = 0;
		(*domain)++;
	}
	/*
	 *	()(*.??.??)
	 *			Handles domain.co.uk
	 */
	else if (secondback && !thirdback &&
			(endstring - firstback == 3) &&
			(firstback - secondback == 3))
	{
		*host = empty_string;
		*domain = myhost;
	}
	/*
	 *	(*).(*.??)
	 *			Handles domain.de
	 */
	else if (secondback && (endstring - firstback == 3))
	{
		*host = myhost;
		*domain = secondback;
		**domain = 0;
		(*domain)++;
	}
	/*
	 *	Everything else...
	 */
	else
	{
		*host = empty_string;
		*domain = myhost;
	}

	return 0;
}

#ifdef INCLUDE_DEADCODE
int count_char (const unsigned char *src, const unsigned char look)
{
	const unsigned char *t;
	int	cnt = 0;

	while ((t = strchr(src, look)))
		cnt++, src = t + 1;

	return cnt;
}
#endif

char 	*BX_strnrchr(char *start, char which, int howmany)
{
	char *ends = start + strlen(start);

	while (ends > start && howmany)
	{
		if (*--ends == which)
			howmany--;
	}
	if (ends == start)
		return NULL;
	else
		return ends;
}

/*
 * This replaces some number of numbers (1 or more) with a single asterisk.
 * We know that the final strcpy() is safe, since we never make a string that
 * is longer than the source string, always less than or equal in size.
 */
void	BX_mask_digits (char **hostname)
{
	char	*src_ptr;
	char 	*retval, *retval_ptr;

	retval = retval_ptr = alloca(strlen(*hostname) + 1);
	src_ptr = *hostname;

	while (*src_ptr)
	{
		if (isdigit((unsigned char)*src_ptr))
		{
			while (*src_ptr && isdigit((unsigned char)*src_ptr))
				src_ptr++;

			*retval_ptr++ = '*';
		}
		else
			*retval_ptr++ = *src_ptr++;
	}

	*retval_ptr = 0;
	strcpy(*hostname, retval);
	return;
}

/*
 * Its like strcspn, except the seconD arg is NOT a string.
 */
size_t 	BX_ccspan (const char *string, int s)
{
	size_t count = 0;
	char c = (char) s;

	while (string && *string && *string != c)
		string++, count++;

	return count;
}

#ifdef INCLUDE_DEADCODE
int 	last_char (const char *string)
{
	while (string && string[0] && string[1])
		string++;

	return (int)*string;
}
#endif

int	BX_charcount (const char *string, char what)
{
	int x = 0;
	const char *place = string;

	while (*place)
		if (*place++ == what)
			x++;

	return x;
}

char *	encode(const char *str, int len)
{
	char *retval;
	char *ptr;

	if (len == -1)
		len = strlen(str);

	ptr = retval = new_malloc(len * 2 + 1);

	while (len)
	{
		*ptr++ = (*str >> 4) + 0x41;
		*ptr++ = (*str & 0x0f) + 0x41;
		str++;
		len--;
 	}
	*ptr = 0;
	return retval;
}

char *	decode(const char *str)
{
	char *retval;
	char *ptr;
	int len = strlen(str);

	ptr = retval = new_malloc(len / 2 + 1);
	while (len >= 2)
	{
		*ptr++ = ((str[0] - 0x41) << 4) | (str[1] - 0x41);
		str += 2;
		len -= 2;
	}
	*ptr = 0;
	return retval;
}

char *	chomp (char *s)
{
	char *e = s + strlen(s);

	if (e == s)
		return s;

	while (*--e == '\n')
	{
		*e = 0;
		if (e == s)
			break;
	}

	return s;
}

char	*BX_strpcat (char *source, const char *format, ...)
{
	va_list args;
	char	buffer[BIG_BUFFER_SIZE + 1];

	va_start(args, format);
	vsnprintf(buffer, BIG_BUFFER_SIZE, format, args);
	va_end(args);

	strcat(source, buffer);
	return source;
}

char *	BX_strmpcat (char *source, size_t siz, const char *format, ...)
{
	va_list args;
	char	buffer[BIG_BUFFER_SIZE + 1];

	va_start(args, format);
	vsnprintf(buffer, BIG_BUFFER_SIZE, format, args);
	va_end(args);

	strmcat(source, buffer, siz);
	return source;
}



char *BX_strcpy_nocolorcodes(char *dest, const char *source)
{
	char *save = dest;

	do
	{
		while (*source == COLOR_CHAR)
			source = skip_ctl_c_seq(source, NULL, NULL, 0);
		*dest++ = *source;
	}
	while (*source++);

	return save;
}

char *crypt();

char *BX_cryptit(const char *string) 
{
static char saltChars[] = 
	"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ./0123456789";
char *cpass = (char *)string;
char salt[3];
	salt[0] = saltChars[random_number(0) % (sizeof(saltChars) - 1)];
	salt[1] = saltChars[random_number(0) % (sizeof(saltChars) - 1)];
	salt[2] = 0;
#if !defined(WINNT)
	cpass = crypt(string, salt);
#endif
	return cpass;
}

int checkpass(const char *password, const char *old)
{
	char seed[3], *p;
	seed[0] = old[0];
	seed[1] = old[1];
	seed[2] = 0;
#if !defined(WINNT)
	p = crypt(password, seed);
#else
	p = password;
#endif
	return strcmp(p, old);
}


char *BX_stripdev(char *ttynam)
{
	if (ttynam == NULL)
		return NULL;
#ifdef SVR4
  /* unixware has /dev/pts012 as synonym for /dev/pts/12 */
	if (strbegins(ttynam, "/dev/pts") && ttynam[8] >= '0' && ttynam[8] <= '9')
	{
		static char b[13];
		sprintf(b, "pts/%d", atoi(ttynam + 8));
		return b;
	}
#endif /* SVR4 */
	if (strbegins(ttynam, "/dev/"))
		return ttynam + 5;
	return ttynam;
}

const char *init_socketpath(void)
{
	static char socket_path[BIG_BUFFER_SIZE];

	snprintf(socket_path, sizeof socket_path, "%s/.BitchX/screens", my_path);

	if (mkdir(socket_path, 0700) != -1)
		(void)chown(socket_path, getuid(), getgid());

	return socket_path;
}

/*
 * This mangles up 'incoming' corresponding to the current values of
 * /set mangle_inbound or /set mangle_outbound.  
 * 'incoming' needs to be at _least_ thrice as big as necessary 
 * (ie, sizeof(incoming) >= strlen(incoming) * 3 + 1)
 */
size_t	BX_mangle_line	(char *incoming, int how, size_t how_much)
{
	int	stuff;
	char	*buffer;
	int	i;
	char	*s;

	stuff = how;
	buffer = alloca(how_much + 1);	/* Absurdly large */

#if notyet
	if (stuff & STRIP_CTCP2)
	{
		char *output;

		output = strip_ctcp2(incoming);
		strlcpy(incoming, output, how_much);
		new_free(&output);
	}
	else if (stuff & MANGLE_INBOUND_CTCP2)
	{
		char *output;

		output = ctcp2_to_ircII(incoming);
		strlcpy(incoming, output, how_much);
		new_free(&output);
	}
	else if (stuff & MANGLE_OUTBOUND_CTCP2)
	{
		char *output;

		output = ircII_to_ctcp2(incoming);
		strlcpy(incoming, output, how_much);
		new_free(&output);
	}
#endif

	if (stuff & MANGLE_ESCAPES)
	{
		for (i = 0; incoming[i]; i++)
		{
			if (incoming[i] == '\x1b')
				incoming[i] = '\x5b';
		}
	}

	if (stuff & MANGLE_ANSI_CODES)
	{
		/* strip_ansi can expand up to three times */
		char *output;

		strip_ansi_never_xlate = 1;	/* XXXXX */
		output = strip_ansi(incoming);
		strip_ansi_never_xlate = 0;	/* XXXXX */
		if (strlcpy(incoming, output, how_much) > how_much)
			say("Mangle_line truncating results (%d > %d) - "
				"please email " BUG_EMAIL,
				strlen(output), how_much);
		new_free(&output);
	}

	/*
	 * Now we mangle the individual codes
	 */
	for (i = 0, s = incoming; *s; s++)
	{
		switch (*s)
		{
			case COLOR_CHAR:	/* color codes */
			{
				int lhs = 0, 
					rhs = 0;
				char *end;

				end = skip_ctl_c_seq(s, &lhs, &rhs, 0);
				if (!(stuff & STRIP_COLOR))
				{
					while (s < end)
						buffer[i++] = *s++;
				}
				s = end - 1;
				break;
			}
			case REV_TOG:		/* Reverse */
			{
				if (!(stuff & STRIP_REVERSE))
					buffer[i++] = REV_TOG;
				break;
			}
			case UND_TOG:		/* Underline */
			{
				if (!(stuff & STRIP_UNDERLINE))
					buffer[i++] = UND_TOG;
				break;
			}
			case BOLD_TOG:		/* Bold */
			{
				if (!(stuff & STRIP_BOLD))
					buffer[i++] = BOLD_TOG;
				break;
			}
			case BLINK_TOG: 	/* Flashing */
			{
				if (!(stuff & STRIP_BLINK))
					buffer[i++] = BLINK_TOG;
				break;
			}
			case ROM_CHAR:		/* Special rom-chars */
			{
				if (!(stuff & STRIP_ROM_CHAR))
					buffer[i++] = ROM_CHAR;
				break;
			}
			case ND_SPACE:		/* Nondestructive spaces */
			{
				if (!(stuff & STRIP_ND_SPACE))
					buffer[i++] = ND_SPACE;
				break;
			}
			case ALT_TOG:		/* Alternate character set */
			{
				if (!(stuff & STRIP_ALT_CHAR))
					buffer[i++] = ALT_TOG;
				break;
			}
			case ALL_OFF:		/* ALL OFF attribute */
			{
				if (!(stuff & STRIP_ALL_OFF))
					buffer[i++] = ALL_OFF;
				break;
			}
			default:
				buffer[i++] = *s;
		}
	}

	buffer[i] = 0;
	return strlcpy(incoming, buffer, how_much);
}

void strip_chars(char *buffer, char *strip, char replace)
{
char *p;
	if (!buffer || !*buffer || !strip || !*strip || !replace)
		return;
	while (*strip)
	{
		while ((p = strchr(buffer, *strip)))
			*p = replace;
		strip++;
	}
}

char *longcomma(long val)
{
char buffer[40];
static char buff[40];
char *s = buff;
int i = 0, j = 0, len;
	sprintf(buffer, "%ld", val);
	len = strlen(buffer);
	for (i = len % 3; i > 0; i--)
		*s++ = buffer[j++];	 
	if (len > 3 && len % 3)
		*s++ = ',';
	len -= (len % 3);	
	while (len --)
	{
		*s++ = buffer[j++];
		if (!(len % 3) && len)
			*s++ = ',';
	}
	*s = 0;
	return buff;
}

char *ulongcomma(unsigned long val)
{
char buffer[40];
static char buff[40];
char *s = buff;
int i = 0, j = 0, len;
	sprintf(buffer, "%lu", val);
	len = strlen(buffer);
	for (i = len % 3; i > 0; i--)
		*s++ = buffer[j++];	 
	if (len > 3 && len % 3)
		*s++ = ',';
	len -= (len % 3);	
	while (len --)
	{
		*s++ = buffer[j++];
		if (!(len % 3) && len)
			*s++ = ',';
	}
	*s = 0;
	return buff;
}

/* XXXX this doesn't belong here. I'm not sure where it goes, though. */
char *	get_userhost (void)
{
	strlcpy(userhost, username, sizeof userhost);
	strlcat(userhost, "@", sizeof userhost);
	strlcat(userhost, hostname, sizeof userhost);
	return userhost;
}



/* RANDOM NUMBERS */
/*
 * Random number generator #1 -- pseudo-random sequence
 * If you do not have /dev/random and do not want to use gettimeofday(), then
 * you can use the pseudo-random number generator.  Its performance varies
 * from weak to moderate.  It is a predictable mathematical sequence that
 * varies depending on the seed, and it provides very little repetition,
 * but with 4 or 5 samples, it should be trivial for an outside person to
 * find the next numbers in your sequence.
 *
 * If 'l' is not zero, then it is considered a "seed" value.  You want 
 * to call it once to set the seed.  Subsequent calls should use 'l' 
 * as 0, and it will return a value.
 */
unsigned long randm(unsigned long l)
{
	/* patch from Sarayan to make $rand() better */
	static const long RAND_A = 16807L;
	static const long RAND_M = 2147483647L;
	static const long RAND_Q = 127773L;
	static const int RAND_R = 2836L;
	static unsigned long z = 0;

	if (z == 0)
	{
		struct timeval tv;

		get_time(&tv);
		z = tv.tv_usec << 12;
		z ^= (unsigned long)tv.tv_sec;
		z ^= (unsigned long)getuid() << 16;
		z ^= (unsigned long)getpid();
	}

	if (l == 0)
	{
		long t = RAND_A * (z % RAND_Q) - RAND_R * (z / RAND_Q);
		if (t > 0)
			z = t;
		else
			z = t + RAND_M;
		return (z >> 8) | ((z & 255) << 23);
	}
	else
	{
		z = l;
		return 0;
	}
}

/*
 * Random number generator #2 -- gettimeofday().
 * If you have gettimeofday(), then we could use it.  Its performance varies
 * from weak to moderate.  At best, it is a source of modest entropy, with 
 * distinct linear qualities. At worst, it is a linear sequence.  If you do
 * not have gettimeofday(), then it uses randm() instead.
 */
static unsigned long randt_2 (void)
{
	struct timeval 	tp1;
	get_time(&tp1);
	return (unsigned long) tp1.tv_usec;
}

unsigned long randt(unsigned long l)
{
#ifdef HAVE_GETTIMEOFDAY
	unsigned long t1, t2, t;

	if (l != 0)
		return 0;

	t1 = randt_2();
	t2 = randt_2();
	t = (t1 & 65535) * 65536 + (t2 & 65535);
	return t;
#else
	return randm(0);
#endif
}


/*
 * Random number generator #3 -- /dev/urandom.
 * If you have the /dev/urandom device, then we will use it.  Its performance
 * varies from moderate to very strong.  At best, it is a source of pretty
 * substantial unpredictable numbers.  At worst, it is mathematical pseudo-
 * random sequence (which randm() is).
 */
unsigned long randd(unsigned long l)
{
	unsigned long	value;
static	int		random_fd = -1;

	if (l != 0)
		return 0;	/* No seeding appropriate */

	if (random_fd == -2)
		return randm(0);

	else if (random_fd == -1)
	{
		if ((random_fd = open("/dev/urandom", O_RDONLY)) == -1)
		{
			random_fd = -2;
			return randm(0);	/* Fall back to randm */
		}
	}

	read(random_fd, (void *)&value, sizeof(value));
	return value;
}

unsigned long BX_random_number(unsigned long l)
{
	/* Always use the strongest random source for internal client use. */
	return randd(l);
}

/*
 * Copyright (c) 1995-2001 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * This is licensed under the 3-clause BSD license, which is found above.
 */
/*
 * Return a malloced, base64 string representation of the first 'size' bytes
 * starting at 'data'.
 */
char *base64_encode(const void *data, size_t size)
{
	static const char base64_chars[] = 
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char *s, *p;
	size_t i;
	unsigned c;
	const unsigned char * const q = data;

	p = s = new_malloc((size + 2) / 3 * 4 + 1);

	for (i = 0; i < size;) {
		c = q[i++];
		c *= 256;
		if (i < size)
			c += q[i];
		i++;
		c *= 256;
		if (i < size)
			c += q[i];
		i++;
		p[0] = base64_chars[(c & 0x00fc0000) >> 18];
		p[1] = base64_chars[(c & 0x0003f000) >> 12];
		p[2] = base64_chars[(c & 0x00000fc0) >> 6];
		p[3] = base64_chars[(c & 0x0000003f) >> 0];
		if (i > size)
			p[3] = '=';
		if (i > size + 1)
			p[2] = '=';
		p += 4;
	}
	*p = 0;

	return s;
}
