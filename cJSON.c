/*
  Copyright (c) 2009 Dave Gamble

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

/* cJSON */
/* JSON parser in C. */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include "cJSON.h"

#ifdef _WIN32
	#include <tchar.h>
	#define strlen _tcslen
	#define strcpy _tcscpy 
	#define strncmp _tcsncmp
	#define sprintf _stprintf
	#define strchr _tcschr
#else
	#define _T
#endif 

static const PFCHAR *ep;

const PFCHAR *cJSON_GetErrorPtr(void) {return ep;}

static int cJSON_strcasecmp(const PFCHAR *s1,const PFCHAR *s2)
{
	if (!s1) return (s1==s2)?0:1;if (!s2) return 1;
	for(; tolower(*s1) == tolower(*s2); ++s1, ++s2)	if(*s1 == 0)	return 0;
	return tolower(*(const UPFCHAR *)s1) - tolower(*(const UPFCHAR *)s2);
}

static void *(*cJSON_malloc)(size_t sz) = malloc;
static void (*cJSON_free)(void *ptr) = free;

static PFCHAR* cJSON_strdup(const PFCHAR* str)
{
      size_t len;
      PFCHAR* copy;

      len = strlen(str) + 1;
      if (!(copy = (PFCHAR*)cJSON_malloc(len*sizeof(PFCHAR)))) return 0;
	  memset(copy,0,len*sizeof(PFCHAR));
      memcpy(copy,str,len*sizeof(PFCHAR));
      return copy;
}

void cJSON_InitHooks(cJSON_Hooks* hooks)
{
    if (!hooks) { /* Reset hooks */
        cJSON_malloc = malloc;
        cJSON_free = free;
        return;
    }

	cJSON_malloc = (hooks->malloc_fn)?hooks->malloc_fn:malloc;
	cJSON_free	 = (hooks->free_fn)?hooks->free_fn:free;
}

/* Internal constructor. */
static cJSON *cJSON_New_Item(void)
{
	cJSON* node = (cJSON*)cJSON_malloc(sizeof(cJSON));
	if (node) memset(node,0,sizeof(cJSON));
	return node;
}

/* Delete a cJSON structure. */
void cJSON_Delete(cJSON *c)
{
	cJSON *next;
	while (c)
	{
		next=c->next;
		if (!(c->type&cJSON_IsReference) && c->child) cJSON_Delete(c->child);
		if (!(c->type&cJSON_IsReference) && c->valuestring) cJSON_free(c->valuestring);
		if (c->string) cJSON_free(c->string);
		cJSON_free(c);
		c=next;
	}
}

/* Parse the input text to generate a number, and populate the result into item. */
static const PFCHAR *parse_number(cJSON *item,const PFCHAR *num)
{
	double n=0,sign=1,scale=0;int subscale=0,signsubscale=1;

	if (*num==_T('-')) sign=-1,num++;	/* Has sign? */
	if (*num==_T('0')) num++;			/* is zero */
	if (*num>=_T('1') && *num<=_T('9'))	do	n=(n*10.0)+(*num++ -_T('0'));	while (*num>=_T('0') && *num<=_T('9'));	/* Number? */
	if (*num==_T('.') && num[1]>=_T('0') && num[1]<=_T('9')) {num++;		do	n=(n*10.0)+(*num++ -_T('0')),scale--; while (*num>=_T('0') && *num<=_T('9'));}	/* Fractional part? */
	if (*num==_T('e') || *num==_T('E'))		/* Exponent? */
	{	num++;if (*num==_T('+')) num++;	else if (*num==_T('-')) signsubscale=-1,num++;		/* With sign? */
		while (*num>=_T('0') && *num<=_T('9')) subscale=(subscale*10)+(*num++ - _T('0'));	/* Number? */
	}

	n=sign*n*pow(10.0,(scale+subscale*signsubscale));	/* number = +/- number.fraction * 10^+/- exponent */
	
	item->valuedouble=n;
	item->valueint=(int)n;
	item->type=cJSON_Number;
	return num;
}

/* Render the number nicely from the given item into a string. */
static PFCHAR *print_number(cJSON *item)
{
	PFCHAR *str;
	double d=item->valuedouble;
	if (fabs(((double)item->valueint)-d)<=DBL_EPSILON && d<=INT_MAX && d>=INT_MIN)
	{
		str=(PFCHAR*)cJSON_malloc(21*sizeof(PFCHAR));	/* 2^64+1 can be represented in 21 PFCHARs. */
		if (str) sprintf(str,_T("%d"),item->valueint);
	}
	else
	{
		str=(PFCHAR*)cJSON_malloc(64*sizeof(PFCHAR));	/* This is a nice tradeoff. */
		if (str)
		{
			if (fabs(floor(d)-d)<=DBL_EPSILON && fabs(d)<1.0e60)sprintf(str,_T("%.0f"),d);
			else if (fabs(d)<1.0e-6 || fabs(d)>1.0e9)			sprintf(str,_T("%e"),d);
			else												sprintf(str,_T("%f"),d);
		}
	}
	return str;
}

static unsigned parse_hex4(const PFCHAR *str)
{
	unsigned h=0;
	if (*str>=_T('0') && *str<=_T('9')) h+=(*str)-_T('0'); else if (*str>=_T('A') && *str<=_T('F')) h+=10+(*str)-_T('A'); else if (*str>=_T('a') && *str<=_T('f')) h+=10+(*str)-_T('a'); else return 0;
	h=h<<4;str++;
	if (*str>=_T('0') && *str<=_T('9')) h+=(*str)-_T('0'); else if (*str>=_T('A') && *str<=_T('F')) h+=10+(*str)-_T('A'); else if (*str>=_T('a') && *str<=_T('f')) h+=10+(*str)-_T('a'); else return 0;
	h=h<<4;str++;
	if (*str>=_T('0') && *str<=_T('9')) h+=(*str)-_T('0'); else if (*str>=_T('A') && *str<=_T('F')) h+=10+(*str)-_T('A'); else if (*str>=_T('a') && *str<=_T('f')) h+=10+(*str)-_T('a'); else return 0;
	h=h<<4;str++;
	if (*str>=_T('0') && *str<=_T('9')) h+=(*str)-_T('0'); else if (*str>=_T('A') && *str<=_T('F')) h+=10+(*str)-_T('A'); else if (*str>=_T('a') && *str<=_T('f')) h+=10+(*str)-_T('a'); else return 0;
	return h;
}

/* Parse the input text into an unescaped cstring, and populate item. */
static const UPFCHAR firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static const PFCHAR *parse_string(cJSON *item,const PFCHAR *str)
{
	const PFCHAR *ptr=str+1;PFCHAR *ptr2;PFCHAR *out;int len=0;
#ifdef USE_UNICODE
	PFCHAR uc,uc2;
#else
	unsigned uc,uc2;
#endif
	if (*str!=_T('\"')) {ep=str;return 0;}	/* not a string! */
	
	while (*ptr!=_T('\"') && *ptr && ++len) if (*ptr++ == _T('\\')) ptr++;	/* Skip escaped quotes. */
	
	out=(PFCHAR*)cJSON_malloc((len+1)*sizeof(PFCHAR));	/* This is how long we need for the string, roughly. */
	if (!out) return 0;
	
	ptr=str+1;ptr2=out;
	while (*ptr!=_T('\"') && *ptr)
	{
		if (*ptr!=_T('\\')) *ptr2++=*ptr++;
		else
		{
			ptr++;
			switch (*ptr)
			{
				case _T('b'): *ptr2++=_T('\b');	break;
				case _T('f'): *ptr2++=_T('\f');	break;
				case _T('n'): *ptr2++=_T('\n');	break;
				case _T('r'): *ptr2++=_T('\r');	break;
				case _T('t'): *ptr2++=_T('\t');	break;
				case _T('u'):	 /* transcode utf16 to utf8. */
#ifdef USE_UNICODE
					uc=parse_hex4(ptr+1);ptr+=4;
					*ptr2++ = uc;
#else
					uc=parse_hex4(ptr+1);ptr+=4;	/* get the unicode char. */

					if ((uc>=0xDC00 && uc<=0xDFFF) || uc==0)	break;	/* check for invalid.	*/

					if (uc>=0xD800 && uc<=0xDBFF)	/* UTF16 surrogate pairs.	*/
					{
						if (ptr[1]!=_T('\\') || ptr[2]!=_T('u'))	break;	/* missing second-half of surrogate.	*/
						uc2=parse_hex4(ptr+3);ptr+=6;
						if (uc2<0xDC00 || uc2>0xDFFF)		break;	/* invalid second-half of surrogate.	*/
						uc=0x10000 + (((uc&0x3FF)<<10) | (uc2&0x3FF));
					}

					len=4;
					if (uc<0x80) 
						len=1;
					else if (uc<0x800) 
						len=2;
					else if (uc<0x10000) 
						len=3; 

					ptr2+=len;

					switch (len) {
					case 4: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
					case 3: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
					case 2: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
					case 1: *--ptr2 =(uc | firstByteMark[len]);
					}
					ptr2+=len;
					
#endif
					break;
				default:  *ptr2++=*ptr; break;
			}
			ptr++;
		}
	}
	*ptr2=0;
	if (*ptr==_T('\"')) ptr++;
	item->valuestring=out;
	item->type=cJSON_String;
	return ptr;
}

/* Render the cstring provided to an escaped version that can be printed. */
static PFCHAR *print_string_ptr(const PFCHAR *str)
{
	const PFCHAR *ptr;PFCHAR *ptr2,*out;int len=0;UPFCHAR token;
	
	if (!str) return cJSON_strdup(_T(""));
	ptr=str;while ((token=*ptr) && ++len) {if (strchr(_T("\"\\\b\f\n\r\t"),token)) len++; else if (token<32) len+=5;ptr++;}
	
	out=(PFCHAR*)cJSON_malloc((len+3)*sizeof(PFCHAR));
	if (!out) return 0;

	ptr2=out;ptr=str;
	*ptr2++=_T('\"');
	while (*ptr)
	{
		if ((UPFCHAR)*ptr>31 && *ptr!=_T('\"') && *ptr!=_T('\\')) *ptr2++=*ptr++;
		else
		{
			*ptr2++=_T('\\');
			switch (token=*ptr++)
			{
				case _T('\\'):	*ptr2++=_T('\\');	break;
				case _T('\"'):	*ptr2++=_T('\"');	break;
				case _T('\b'):	*ptr2++=_T('b');	break;
				case _T('\f'):	*ptr2++=_T('f');	break;
				case _T('\n'):	*ptr2++=_T('n');	break;
				case _T('\r'):	*ptr2++=_T('r');	break;
				case _T('\t'):	*ptr2++=_T('t');	break;
				default: sprintf(ptr2,_T("u%04x"),token);ptr2+=5;	break;	/* escape and print */
			}
		}
	}
	*ptr2++=_T('\"');*ptr2++=0;
	return out;
}
/* Invote print_string_ptr (which is useful) on an item. */
static PFCHAR *print_string(cJSON *item)	{return print_string_ptr(item->valuestring);}

/* Predeclare these prototypes. */
static const PFCHAR *parse_value(cJSON *item,const PFCHAR *value);
static PFCHAR *print_value(cJSON *item,int depth,int fmt);
static const PFCHAR *parse_array(cJSON *item,const PFCHAR *value);
static PFCHAR *print_array(cJSON *item,int depth,int fmt);
static const PFCHAR *parse_object(cJSON *item,const PFCHAR *value);
static PFCHAR *print_object(cJSON *item,int depth,int fmt);

/* Utility to jump whitespace and cr/lf */
static const PFCHAR *skip(const PFCHAR *in) {while (in && *in && (UPFCHAR)*in<=32) in++; return in;}

/* Parse an object - create a new root, and populate. */
cJSON *cJSON_ParseWithOpts(const PFCHAR *value,const PFCHAR **return_parse_end,int require_null_terminated)
{
	const PFCHAR *end=0;
	cJSON *c=cJSON_New_Item();
	ep=0;
	if (!c) return 0;       /* memory fail */

	end=parse_value(c,skip(value));
	if (!end)	{cJSON_Delete(c);return 0;}	/* parse failure. ep is set. */

	/* if we require null-terminated JSON without appended garbage, skip and then check for a null terminator */
	if (require_null_terminated) {end=skip(end);if (*end) {cJSON_Delete(c);ep=end;return 0;}}
	if (return_parse_end) *return_parse_end=end;
	return c;
}
/* Default options for cJSON_Parse */
cJSON *cJSON_Parse(const PFCHAR *value) {return cJSON_ParseWithOpts(value,0,0);}

/* Render a cJSON item/entity/structure to text. */
PFCHAR *cJSON_Print(cJSON *item)				{return print_value(item,0,1);}
PFCHAR *cJSON_PrintUnformatted(cJSON *item)	{return print_value(item,0,0);}

/* Parser core - when encountering text, process appropriately. */
static const PFCHAR *parse_value(cJSON *item,const PFCHAR *value)
{
	if (!value)						return 0;	/* Fail on null. */
	if (!strncmp(value,_T("null"),4))	{ item->type=cJSON_NULL;  return value+4; }
	if (!strncmp(value,_T("false"),5))	{ item->type=cJSON_False; return value+5; }
	if (!strncmp(value,_T("true"),4))	{ item->type=cJSON_True; item->valueint=1;	return value+4; }
	if (*value==_T('\"'))				{ return parse_string(item,value); }
	if (*value==_T('-') || (*value>=_T('0') && *value<=_T('9')))	{ return parse_number(item,value); }
	if (*value==_T('['))				{ return parse_array(item,value); }
	if (*value==_T('{'))				{ return parse_object(item,value); }

	ep=value;return 0;	/* failure. */
}

/* Render a value to text. */
static PFCHAR *print_value(cJSON *item,int depth,int fmt)
{
	PFCHAR *out=0;
	if (!item) return 0;
	switch ((item->type)&255)
	{
		case cJSON_NULL:	out=cJSON_strdup(_T("null"));	break;
		case cJSON_False:	out=cJSON_strdup(_T("false"));break;
		case cJSON_True:	out=cJSON_strdup(_T("true")); break;
		case cJSON_Number:	out=print_number(item);break;
		case cJSON_String:	out=print_string(item);break;
		case cJSON_Array:	out=print_array(item,depth,fmt);break;
		case cJSON_Object:	out=print_object(item,depth,fmt);break;
	}
	return out;
}

/* Build an array from input text. */
static const PFCHAR *parse_array(cJSON *item,const PFCHAR *value)
{
	cJSON *child;
	if (*value!=_T('['))	{ep=value;return 0;}	/* not an array! */

	item->type=cJSON_Array;
	value=skip(value+1);
	if (*value==_T(']')) return value+1;	/* empty array. */

	item->child=child=cJSON_New_Item();
	if (!item->child) return 0;		 /* memory fail */
	value=skip(parse_value(child,skip(value)));	/* skip any spacing, get the value. */
	if (!value) return 0;

	while (*value==_T(','))
	{
		cJSON *new_item;
		if (!(new_item=cJSON_New_Item())) return 0; 	/* memory fail */
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_value(child,skip(value+1)));
		if (!value) return 0;	/* memory fail */
	}

	if (*value==_T(']')) return value+1;	/* end of array */
	ep=value;return 0;	/* malformed. */
}

/* Render an array to text */
static PFCHAR *print_array(cJSON *item,int depth,int fmt)
{
	PFCHAR **entries;
	PFCHAR *out=0,*ptr,*ret;int len=5;
	cJSON *child=item->child;
	int numentries=0,i=0,fail=0;
	
	/* How many entries in the array? */
	while (child) numentries++,child=child->next;
	/* Explicitly handle numentries==0 */
	if (!numentries)
	{
		out=(PFCHAR*)cJSON_malloc(3*sizeof(PFCHAR));
		if (out) strcpy(out,_T("[]"));
		return out;
	}
	/* Allocate an array to hold the values for each */
	entries=(PFCHAR**)cJSON_malloc(numentries*sizeof(PFCHAR*));
	if (!entries) return 0;
	memset(entries,0,numentries*sizeof(PFCHAR*));
	/* Retrieve all the results: */
	child=item->child;
	while (child && !fail)
	{
		ret=print_value(child,depth+1,fmt);
		entries[i++]=ret;
		if (ret) len+=strlen(ret)+2+(fmt?1:0); else fail=1;
		child=child->next;
	}
	
	/* If we didn't fail, try to malloc the output string */
	if (!fail) out=(PFCHAR*)cJSON_malloc(len*sizeof(PFCHAR));
	/* If that fails, we fail. */
	if (!out) fail=1;

	/* Handle failure. */
	if (fail)
	{
		for (i=0;i<numentries;i++) if (entries[i]) cJSON_free(entries[i]);
		cJSON_free(entries);
		return 0;
	}
	
	/* Compose the output array. */
	*out=_T('[');
	ptr=out+1;*ptr=0;
	for (i=0;i<numentries;i++)
	{
		strcpy(ptr,entries[i]);ptr+=strlen(entries[i]);
		if (i!=numentries-1) {*ptr++=_T(',');if(fmt)*ptr++=_T(' ');*ptr=0;}
		cJSON_free(entries[i]);
	}
	cJSON_free(entries);
	*ptr++=_T(']');*ptr++=0;
	return out;	
}

/* Build an object from the text. */
static const PFCHAR *parse_object(cJSON *item,const PFCHAR *value)
{
	cJSON *child;
	if (*value!=_T('{'))	{ep=value;return 0;}	/* not an object! */
	
	item->type=cJSON_Object;
	value=skip(value+1);
	if (*value==_T('}')) return value+1;	/* empty array. */
	
	item->child=child=cJSON_New_Item();
	if (!item->child) return 0;
	value=skip(parse_string(child,skip(value)));
	if (!value) return 0;
	child->string=child->valuestring;child->valuestring=0;
	if (*value!=_T(':')) {ep=value;return 0;}	/* fail! */
	value=skip(parse_value(child,skip(value+1)));	/* skip any spacing, get the value. */
	if (!value) return 0;
	
	while (*value==_T(','))
	{
		cJSON *new_item;
		if (!(new_item=cJSON_New_Item()))	return 0; /* memory fail */
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_string(child,skip(value+1)));
		if (!value) return 0;
		child->string=child->valuestring;child->valuestring=0;
		if (*value!=_T(':')) {ep=value;return 0;}	/* fail! */
		value=skip(parse_value(child,skip(value+1)));	/* skip any spacing, get the value. */
		if (!value) return 0;
	}
	
	if (*value==_T('}')) return value+1;	/* end of array */
	ep=value;return 0;	/* malformed. */
}

/* Render an object to text. */
static PFCHAR *print_object(cJSON *item,int depth,int fmt)
{
	PFCHAR **entries=0,**names=0;
	PFCHAR *out=0,*ptr,*ret,*str;int len=7,i=0,j;
	cJSON *child=item->child;
	int numentries=0,fail=0;
	/* Count the number of entries. */
	while (child) numentries++,child=child->next;
	/* Explicitly handle empty object case */
	if (!numentries)
	{
		out=(PFCHAR*)cJSON_malloc((fmt?depth+4:3)*sizeof(PFCHAR));
		if (!out)	return 0;
		ptr=out;*ptr++=_T('{');
		if (fmt) {*ptr++=_T('\n');for (i=0;i<depth-1;i++) *ptr++=_T('\t');}
		*ptr++=_T('}');*ptr++=0;
		return out;
	}
	/* Allocate space for the names and the objects */
	entries=(PFCHAR**)cJSON_malloc(numentries*sizeof(PFCHAR*));
	if (!entries) return 0;
	names=(PFCHAR**)cJSON_malloc(numentries*sizeof(PFCHAR*));
	if (!names) {cJSON_free(entries);return 0;}
	memset(entries,0,sizeof(PFCHAR*)*numentries);
	memset(names,0,sizeof(PFCHAR*)*numentries);

	/* Collect all the results into our arrays: */
	child=item->child;depth++;if (fmt) len+=depth;
	while (child&&!fail)
	{
		names[i]=str=print_string_ptr(child->string);
		entries[i++]=ret=print_value(child,depth,fmt);
		if (str && ret) len+=strlen(ret)+strlen(str)+2+(fmt?2+depth:0); else fail=1;
		child=child->next;
	}
	
	/* Try to allocate the output string */
	if (!fail) out=(PFCHAR*)cJSON_malloc(len*sizeof(PFCHAR));
	if (!out) fail=1;

	/* Handle failure */
	if (fail)
	{
		for (i=0;i<numentries;i++) {if (names[i]) cJSON_free(names[i]);if (entries[i]) cJSON_free(entries[i]);}
		cJSON_free(names);cJSON_free(entries);
		return 0;
	}
	
	/* Compose the output: */
	*out=_T('{');ptr=out+1;if (fmt)*ptr++=_T('\n');*ptr=0;
	for (i=0;i<numentries;i++)
	{
		if (fmt) for (j=0;j<depth;j++) *ptr++=_T('\t');
		strcpy(ptr,names[i]);ptr+=strlen(names[i]);
		*ptr++=_T(':');if (fmt) *ptr++=_T('\t');
		strcpy(ptr,entries[i]);ptr+=strlen(entries[i]);
		if (i!=numentries-1) *ptr++=_T(',');
		if (fmt) *ptr++=_T('\n');*ptr=0;
		cJSON_free(names[i]);cJSON_free(entries[i]);
	}
	
	cJSON_free(names);cJSON_free(entries);
	if (fmt) for (i=0;i<depth-1;i++) *ptr++=_T('\t');
	*ptr++=_T('}');*ptr++=0;
	return out;	
}

/* Get Array size/item / object item. */
int    cJSON_GetArraySize(cJSON *array)							{cJSON *c=array->child;int i=0;while(c)i++,c=c->next;return i;}
cJSON *cJSON_GetArrayItem(cJSON *array,int item)				{cJSON *c=array->child;  while (c && item>0) item--,c=c->next; return c;}
cJSON *cJSON_GetObjectItem(cJSON *object,const PFCHAR *string)	{cJSON *c=object->child; while (c && cJSON_strcasecmp(c->string,string)) c=c->next; return c;}

/* Utility for array list handling. */
static void suffix_object(cJSON *prev,cJSON *item) {prev->next=item;item->prev=prev;}
/* Utility for handling references. */
static cJSON *create_reference(cJSON *item) {cJSON *ref=cJSON_New_Item();if (!ref) return 0;memcpy(ref,item,sizeof(cJSON));ref->string=0;ref->type|=cJSON_IsReference;ref->next=ref->prev=0;return ref;}

/* Add item to array/object. */
void   cJSON_AddItemToArray(cJSON *array, cJSON *item)						{cJSON *c=array->child;if (!item) return; if (!c) {array->child=item;} else {while (c && c->next) c=c->next; suffix_object(c,item);}}
void   cJSON_AddItemToObject(cJSON *object,const PFCHAR *string,cJSON *item)	{if (!item) return; if (item->string) cJSON_free(item->string);item->string=cJSON_strdup(string);cJSON_AddItemToArray(object,item);}
void	cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item)						{cJSON_AddItemToArray(array,create_reference(item));}
void	cJSON_AddItemReferenceToObject(cJSON *object,const PFCHAR *string,cJSON *item)	{cJSON_AddItemToObject(object,string,create_reference(item));}

cJSON *cJSON_DetachItemFromArray(cJSON *array,int which)			{cJSON *c=array->child;while (c && which>0) c=c->next,which--;if (!c) return 0;
	if (c->prev) c->prev->next=c->next;if (c->next) c->next->prev=c->prev;if (c==array->child) array->child=c->next;c->prev=c->next=0;return c;}
void   cJSON_DeleteItemFromArray(cJSON *array,int which)			{cJSON_Delete(cJSON_DetachItemFromArray(array,which));}
cJSON *cJSON_DetachItemFromObject(cJSON *object,const PFCHAR *string) {int i=0;cJSON *c=object->child;while (c && cJSON_strcasecmp(c->string,string)) i++,c=c->next;if (c) return cJSON_DetachItemFromArray(object,i);return 0;}
void   cJSON_DeleteItemFromObject(cJSON *object,const PFCHAR *string) {cJSON_Delete(cJSON_DetachItemFromObject(object,string));}

/* Replace array/object items with new ones. */
void   cJSON_ReplaceItemInArray(cJSON *array,int which,cJSON *newitem)		{cJSON *c=array->child;while (c && which>0) c=c->next,which--;if (!c) return;
	newitem->next=c->next;newitem->prev=c->prev;if (newitem->next) newitem->next->prev=newitem;
	if (c==array->child) array->child=newitem; else newitem->prev->next=newitem;c->next=c->prev=0;cJSON_Delete(c);}
void   cJSON_ReplaceItemInObject(cJSON *object,const PFCHAR *string,cJSON *newitem){int i=0;cJSON *c=object->child;while(c && cJSON_strcasecmp(c->string,string))i++,c=c->next;if(c){newitem->string=cJSON_strdup(string);cJSON_ReplaceItemInArray(object,i,newitem);}}

/* Create basic types: */
cJSON *cJSON_CreateNull(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_NULL;return item;}
cJSON *cJSON_CreateTrue(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_True;return item;}
cJSON *cJSON_CreateFalse(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_False;return item;}
cJSON *cJSON_CreateBool(int b)					{cJSON *item=cJSON_New_Item();if(item)item->type=b?cJSON_True:cJSON_False;return item;}
cJSON *cJSON_CreateNumber(double num)			{cJSON *item=cJSON_New_Item();if(item){item->type=cJSON_Number;item->valuedouble=num;item->valueint=(int)num;}return item;}
cJSON *cJSON_CreateString(const PFCHAR *string)	{cJSON *item=cJSON_New_Item();if(item){item->type=cJSON_String;item->valuestring=cJSON_strdup(string);}return item;}
cJSON *cJSON_CreateArray(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_Array;return item;}
cJSON *cJSON_CreateObject(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_Object;return item;}

/* Create Arrays: */
cJSON *cJSON_CreateIntArray(const int *numbers,int count)		{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateFloatArray(const float *numbers,int count)	{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateDoubleArray(const double *numbers,int count)	{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateStringArray(const PFCHAR **strings,int count)	{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateString(strings[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}

/* Duplication */
cJSON *cJSON_Duplicate(cJSON *item,int recurse)
{
	cJSON *newitem,*cptr,*nptr=0,*newchild;
	/* Bail on bad ptr */
	if (!item) return 0;
	/* Create new item */
	newitem=cJSON_New_Item();
	if (!newitem) return 0;
	/* Copy over all vars */
	newitem->type=item->type&(~cJSON_IsReference),newitem->valueint=item->valueint,newitem->valuedouble=item->valuedouble;
	if (item->valuestring)	{newitem->valuestring=cJSON_strdup(item->valuestring);	if (!newitem->valuestring)	{cJSON_Delete(newitem);return 0;}}
	if (item->string)		{newitem->string=cJSON_strdup(item->string);			if (!newitem->string)		{cJSON_Delete(newitem);return 0;}}
	/* If non-recursive, then we're done! */
	if (!recurse) return newitem;
	/* Walk the ->next chain for the child. */
	cptr=item->child;
	while (cptr)
	{
		newchild=cJSON_Duplicate(cptr,1);		/* Duplicate (with recurse) each item in the ->next chain */
		if (!newchild) {cJSON_Delete(newitem);return 0;}
		if (nptr)	{nptr->next=newchild,newchild->prev=nptr;nptr=newchild;}	/* If newitem->child already set, then crosswire ->prev and ->next and move on */
		else		{newitem->child=newchild;nptr=newchild;}					/* Set newitem->child and move to it */
		cptr=cptr->next;
	}
	return newitem;
}

void cJSON_Minify(PFCHAR *json)
{
	PFCHAR *into=json;
	while (*json)
	{
		if (*json==_T(' ')) json++;
		else if (*json==_T('\t')) json++;	/* Whitespace characters. */
		else if (*json==_T('\r')) json++;
		else if (*json==_T('\n')) json++;
		else if (*json==_T('/') && json[1]==_T('/'))  while (*json && *json!=_T('\n')) json++;	/* double-slash comments, to end of line. */
		else if (*json==_T('/') && json[1]==_T('*')) {while (*json && !(*json==_T('*') && json[1]==_T('/'))) json++;json+=2;}	/* multiline comments. */
		else if (*json==_T('\"')){*into++=*json++;while (*json && *json!=_T('\"')){if (*json==_T('\\')) *into++=*json++;*into++=*json++;}*into++=*json++;} /* string literals, which are \" sensitive. */
		else *into++=*json++;			/* All other characters. */
	}
	*into=0;	/* and null-terminate. */
}
