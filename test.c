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

#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#ifdef USE_UNICODE
#include <tchar.h>
#endif

/* Parse text to JSON, then render back to text, and print! */
void doit(PFCHAR *text)
{
	PFCHAR *out;cJSON *json;
	
	json=cJSON_Parse(text);
	if (!json) {wprintf(_T("Error before: [%s]\n"),cJSON_GetErrorPtr());}
	else
	{
		out=cJSON_Print(json);
		cJSON_Delete(json);
		wprintf(_T("%s\n"),out);
		free(out);
	}
}

/* Read a file, parse, render back, etc. */
void dofile(char *filename)
{
	char *data=0;

	long len=0;
	FILE *f=fopen(filename,"rb");
	fseek(f,0,SEEK_END);
	len=ftell(f);
	fseek(f,0,SEEK_SET);
	data=(char*)malloc(len+1);
	fread(data,sizeof(char),len,f);
	data[len]='\0';
	fclose(f);

	doit((data));
	free(data);
}

/* Used by some code below as an example datatype. */
struct record {const PFCHAR *precision;double lat,lon;const PFCHAR *address,*city,*state,*zip,*country; };

/* Create a bunch of objects as demonstration. */
void create_objects()
{
	cJSON *root,*fmt,*img,*thm,*fld;
	PFCHAR *out;
	int i;	/* declare a few. */
	/* Our "days of the week" array: */
	const PFCHAR *strings[7]={_T("Sunday"),_T("Monday"),_T("Tuesday"),_T("Wednesday"),_T("Thursday"),_T("Friday"),_T("Saturday")};
	/* Our matrix: */
	int numbers[3][3]={{0,-1,0},{1,0,0},{0,0,1}};
	/* Our "gallery" item: */
	int ids[4]={116,943,234,38793};
	/* Our array of "records": */
	struct record fields[2]={
		{_T("zip"),37.7668,-1.223959e+2,_T(""),_T("SAN FRANCISCO"),_T("CA"),_T("94107"),_T("US")},
		{_T("zip"),37.371991,-1.22026e+2,_T(""),_T("SUNNYVALE"),_T("CA"),_T("94085"),_T("US")}};


	/* Here we construct some JSON standards, from the JSON site. */
	
	/* Our "Video" datatype: */
	root=cJSON_CreateObject();	
	cJSON_AddItemToObject(root, _T("name"), cJSON_CreateString(_T("Jack (\"Bee\") Nimble")));
	cJSON_AddItemToObject(root, _T("format"), fmt=cJSON_CreateObject());
	cJSON_AddStringToObject(fmt,_T("type"),		_T("rect"));
	cJSON_AddNumberToObject(fmt,_T("width"),		1920);
	cJSON_AddNumberToObject(fmt,_T("height"),		1080);
	cJSON_AddFalseToObject (fmt,_T("interlace"));
	cJSON_AddNumberToObject(fmt,_T("frame rate"),	24);
	
	out=cJSON_Print(root);	
	cJSON_Delete(root);	
	wprintf(_T("%s\n"),out);	
	free(out);	/* Print to text, Delete the cJSON, print it, release the string. */

	root=cJSON_CreateStringArray(strings,7);

	out=cJSON_Print(root);	cJSON_Delete(root);	wprintf(_T("%s\n"),out);	free(out);

	root=cJSON_CreateArray();
	for (i=0;i<3;i++) cJSON_AddItemToArray(root,cJSON_CreateIntArray(numbers[i],3));

/*	cJSON_ReplaceItemInArray(root,1,cJSON_CreateString("Replacement")); */
	
	out=cJSON_Print(root);	cJSON_Delete(root);	wprintf(_T("%s\n"),out);	free(out);


	root=cJSON_CreateObject();
	cJSON_AddItemToObject(root, _T("Image"), img=cJSON_CreateObject());
	cJSON_AddNumberToObject(img,_T("Width"),800);
	cJSON_AddNumberToObject(img,_T("Height"),600);
	cJSON_AddStringToObject(img,_T("Title"),_T("View from 15th Floor"));
	cJSON_AddItemToObject(img, _T("Thumbnail"), thm=cJSON_CreateObject());
	cJSON_AddStringToObject(thm, _T("Url"), _T("http:/*www.example.com/image/481989943"));
	cJSON_AddNumberToObject(thm,_T("Height"),125);
	cJSON_AddStringToObject(thm,_T("Width"),_T("100"));
	cJSON_AddItemToObject(img,_T("IDs"), cJSON_CreateIntArray(ids,4));

	out=cJSON_Print(root);	cJSON_Delete(root);	wprintf(_T("%s\n"),out);	free(out);

	root=cJSON_CreateArray();
	for (i=0;i<2;i++)
	{
		cJSON_AddItemToArray(root,fld=cJSON_CreateObject());
		cJSON_AddStringToObject(fld, _T("precision"), fields[i].precision);
		cJSON_AddNumberToObject(fld, _T("Latitude"), fields[i].lat);
		cJSON_AddNumberToObject(fld, _T("Longitude"), fields[i].lon);
		cJSON_AddStringToObject(fld, _T("Address"), fields[i].address);
		cJSON_AddStringToObject(fld, _T("City"), fields[i].city);
		cJSON_AddStringToObject(fld, _T("State"), fields[i].state);
		cJSON_AddStringToObject(fld, _T("Zip"), fields[i].zip);
		cJSON_AddStringToObject(fld, _T("Country"), fields[i].country);
	}
	
/*	cJSON_ReplaceItemInObject(cJSON_GetArrayItem(root,1),"City",cJSON_CreateIntArray(ids,4)); */
	
	out=cJSON_Print(root);	cJSON_Delete(root);	wprintf(_T("%s\n"),out);	free(out);

}

int main (int argc, const char * argv[]) {
	/* a bunch of json: */
	PFCHAR text1[]=_T("{\n\"name\": \"Jack (\\\"Bee\\\") Nimble\", \n\"format\": {\"type\":       \"rect\", \n\"width\":      1920, \n\"height\":     1080, \n\"interlace\":  false,\"frame rate\": 24\n}\n}");
	PFCHAR text2[]=_T("[\"Sunday\", \"Monday\", \"Tuesday\", \"Wednesday\", \"Thursday\", \"Friday\", \"Saturday\"]");
	PFCHAR text3[]=_T("[\n    [0, -1, 0],\n    [1, 0, 0],\n    [0, 0, 1]\n	]\n");
	PFCHAR text4[]=_T("{\n		\"Image\": {\n			\"Width\":  800,\n			\"Height\": 600,\n			\"Title\":  \"View from 15th Floor\",\n			\"Thumbnail\": {\n				\"Url\":    \"http:/*www.example.com/image/481989943\",\n				\"Height\": 125,\n				\"Width\":  \"100\"\n			},\n			\"IDs\": [116, 943, 234, 38793]\n		}\n	}");
	PFCHAR text5[]=_T("[\n	 {\n	 \"precision\": \"zip\",\n	 \"Latitude\":  37.7668,\n	 \"Longitude\": -122.3959,\n	 \"Address\":   \"\",\n	 \"City\":      \"SAN FRANCISCO\",\n	 \"State\":     \"CA\",\n	 \"Zip\":       \"94107\",\n	 \"Country\":   \"US\"\n	 },\n	 {\n	 \"precision\": \"zip\",\n	 \"Latitude\":  37.371991,\n	 \"Longitude\": -122.026020,\n	 \"Address\":   \"\",\n	 \"City\":      \"SUNNYVALE\",\n	 \"State\":     \"CA\",\n	 \"Zip\":       \"94085\",\n	 \"Country\":   \"US\"\n	 }\n	 ]");
	PFCHAR text6[]=_T("{\"id\":\"11821\",\"title\":\"\\uacf5\\ud56d \\uac00\\ub294 \\uae38\",\"artist\":\"My Aunt Mary\",\"album\":\"Just Pop\",\"mp3\":\"http://emo.luoo.net/low/luoo/radio601/01.mp3\",\"poster\":\"http://img3.luoo.net/pics/albums/6514/cover.jpg_580x580.jpg\",\"poster_small\":\"http://img3.luoo.net/pics/albums/6514/cover.jpg_60x60.jpg\",\"is_fav\":0}");
	/* Process each json textblock by parsing, then rebuilding: */
	doit(text6);
	doit(text1);
	doit(text2);	
	doit(text3);
	doit(text4);
	doit(text5);

	/* Parse standard testfiles: */
/*	dofile("../../tests/test1"); */
/*	dofile("../../tests/test2"); */
/*	dofile("../../tests/test3"); */
/*	dofile("../../tests/test4"); */
/*	dofile("../../tests/test5"); */

	/* Now some samplecode for building objects concisely: */
	create_objects();
	
	return 0;
}
