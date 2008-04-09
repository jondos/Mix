/*
Copyright (c) The JAP-Team, JonDos GmbH

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation and/or
       other materials provided with the distribution.
    * Neither the name of the University of Technology Dresden, Germany, nor the name of
       the JonDos GmbH, nor the names of their contributors may be used to endorse or
       promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define PAYMENT

#include <expat.h>
#include <stdio.h>
#include <string.h>
#include "../monitoringDefs.h"
#include "check_mix.h"

/* @todo: this should become the nagios_plugin for server monitoring,
 * still a lot of work todo : ) 
 * */
static void XMLCALL startElement(void *userData, const char *name, const char **atts)
{
	status_type_t *currStatusTypePtr = (status_type_t *) userData;
	if(setCurrentStatusType(currStatusTypePtr, name)==NO_STATUS_TYPE_FOUND)
	{
		puts("No status type");
	}
}

static int setCurrentStatusType(status_type_t *currStatusTypePtr, const char *name)
{
	int elemNameLength = strlen(name);
	if(strncmp(name, "NetworkingStatus", elemNameLength) == CMP_EQUAL)
	{
		*currStatusTypePtr = stat_networking;
		puts("NetworkingStatus found");
		return STATUS_TYPE_FOUND;
	}
	else if(strncmp(name, "PaymentStatus", elemNameLength) == CMP_EQUAL)
	{
		*currStatusTypePtr = stat_payment;
		puts("PaymentStatus found");
		return STATUS_TYPE_FOUND;
	}
	else if(strncmp(name, "SystemStatus", elemNameLength) == CMP_EQUAL)
	{
		*currStatusTypePtr = stat_system;
		puts("SystemStatus found");
		return STATUS_TYPE_FOUND;
	}
	return NO_STATUS_TYPE_FOUND;
}

static void XMLCALL endElement(void *userData, const char *name)
{
  /* int *depthPtr = (int *)userData; 
  depthPtr -= 1; */
}

int main(int argc, char *argv[])
{
  char buf[BUFSIZ];
  XML_Parser parser = XML_ParserCreate("UTF-8");
  int done;
  status_type_t curr_status = stat_undef;
  XML_SetUserData(parser, &curr_status);
  XML_SetElementHandler(parser, startElement, endElement);
  do {
    int len = (int)fread(buf, 1, sizeof(buf), stdin);
    done = len < sizeof(buf);
    if (XML_Parse(parser, buf, len, done) == XML_STATUS_ERROR) {
      /*fprintf(stderr,
              "%s at line %u\n",
              XML_ErrorString(XML_GetErrorCode(parser)),
              XML_GetCurrentLineNumber(parser));*/
      return 1;
    }
  } while (!done);
  XML_ParserFree(parser);
  return 0;
}
