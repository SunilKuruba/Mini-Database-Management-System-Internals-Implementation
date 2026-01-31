#include "dberror.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Provide both symbol names to be compatible across assignments */
char *_RC_message = NULL;
char *RC_message = NULL;

/* print a message to standard out describing the error */
void 
printError (RC error)
{
	/* prefer _RC_message if present (some code uses that symbol) */
	if (_RC_message != NULL)
		printf("EC (%i), \"%s\"\n", error, _RC_message);
	else if (RC_message != NULL)
		printf("EC (%i), \"%s\"\n", error, RC_message);
	else
		printf("EC (%i)\n", error);
}

char *
errorMessage (RC error)
{
	char *message;

	if (_RC_message != NULL)
	{
		message = (char *) malloc(strlen(_RC_message) + 30);
		sprintf(message, "EC (%i), \"%s\"\n", error, _RC_message);
	}
	else if (RC_message != NULL)
	{
		message = (char *) malloc(strlen(RC_message) + 30);
		sprintf(message, "EC (%i), \"%s\"\n", error, RC_message);
	}
	else
	{
		message = (char *) malloc(30);
		sprintf(message, "EC (%i)\n", error);
	}

	return message;
}
