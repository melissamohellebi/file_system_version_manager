#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main () {
	char *p = "toto";
	char request[] = "test 2";
	char *file_name = strtok(request, " ");
	char * request_version = strtok(NULL, request);
	
	printf("%s, %s, %s \n ", file_name, request_version, p);
}
