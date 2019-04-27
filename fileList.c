#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "fileList.h"
//A chunk stores 128 lines max
//There is an array of char double pointer in which each element represents a chunk
//Each double pointer points to array of 128 char pointer which in turn point to the string
void finit(fileList *l) {
	l->chunkIndex = l->lineIndex = 0;
	l->lineMax = 128;//i.e. max 128 lines in one chunk
	l->chunkMax = 64;//i.e. max 64 chunks in the array primarily
	l->arr = (char ***)malloc(sizeof(char **) * l->chunkMax);//chunk array (can have any number of chunks)
	l->arr[0] = (char **)malloc(sizeof(char *) * l->lineMax);//line array (max 128 lines)
}

void fappend(fileList *l, char *str) {
	(l->arr[l->chunkIndex])[l->lineIndex] = (char *)malloc(sizeof(char) * 2048);//malloc the string
	strcpy((l->arr[l->chunkIndex])[l->lineIndex], str);
	l->lineIndex++;
	if(l->lineIndex == l->lineMax) {//when the current chunk becomes full
		l->lineIndex = 0;
		l->chunkIndex++;
		if(l->chunkIndex == l->chunkMax) {//when all the chunks become full
			l->chunkMax = l->chunkMax * 2;
			l->arr = (char ***)realloc(l->arr, sizeof(char **) * l->chunkMax);
		}
		l->arr[l->chunkIndex] = (char **)malloc(sizeof(char *) * l->lineMax);
	}
}

char *fgetline(fileList *l, int index) {
	if(index < 0 || index >= (l->chunkIndex * l->lineMax + l->lineIndex))
		return NULL;
	//for chunk number divide it by the max number of lines in a chunk
	//for index number take modulo of the index with the number of lines inf a chunk
	return (l->arr[index / l->lineMax])[index % l->lineMax];
}

char *fpop(fileList *l) {//returns the last line in the list
	char *str;
	if(l->lineIndex == 0 && l->chunkIndex == 0) {
		fprintf(stderr, "fileList: Popping from empty list\n");
		exit(errno);
	}
	if(l->lineIndex == 0) {
		l->lineIndex = l->lineMax - 1;
		free(l->arr[l->chunkIndex]);
		l->chunkIndex--;
		str = l->arr[l->chunkIndex][l->lineIndex];
	}
	else {
		str = l->arr[l->chunkIndex][l->lineIndex - 1];
		l->lineIndex--;
	}
	return str;
}

int flength(fileList *l) {//returns the number of lines present in the list
	return (l->chunkIndex * l->lineMax + l->lineIndex);
}

void fdestroy(fileList *l) {//frees the allocated memory
	int i, size;
	size = l->chunkIndex * l->lineMax + l->lineIndex;//total number of lines
	for(i = 0; i < size; i++)
		free(l->arr[i / l->lineMax][i % l->lineMax]);
	for(i = 0; i <= l->chunkIndex; i++)
		free(l->arr[i]);
	free(l->arr);
}
