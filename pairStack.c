#include <stdlib.h>
#include "pairStack.h"
void sinit(pairStack *s) {//initializing the head pointer
	*s = NULL;
}

void push(pairStack *s, pair m) {//appending the new structure at the end of the list
	pairNode *temp = (pairNode *)malloc(sizeof(pairNode));
	temp->m = m;
	temp->next = *s;
	*s = temp;
}

pair pop(pairStack *s) {//returning the structure at the end of the list and removing it from the list
	pairNode *temp;
	temp = *s;
	pair m = (*s)->m;
	*s = (*s)->next;
	free(temp);
	return m;
}

int isempty(pairStack *s) {//checking if the stack is empty or not
	return (*s == NULL);
}
