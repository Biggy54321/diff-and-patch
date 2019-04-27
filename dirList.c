#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dirList.h"
#include <errno.h>

//quicksort function - used by the dsort function
int partition(dirList *dir, int lo, int hi) {
	dirUnit temp, pivot;
	pivot = dir->arr[lo];
	int i = lo - 1;
	int j = hi + 1;
	while(1) {
		do {
			i = i + 1;
		} while(strcmp(dir->arr[i].name, pivot.name) < 0);
		do {
			j = j - 1;
		} while(strcmp(dir->arr[j].name, pivot.name) > 0);
		if(i >= j)
			return j;
		temp = dir->arr[i];
		dir->arr[i] = dir->arr[j];
		dir->arr[j] = temp;
	}
}

void quicksort(dirList *dir, int lo, int hi) {
	int p;
	if(lo < hi) {
        	p = partition(dir, lo, hi);
        	quicksort(dir, lo, p);
        	quicksort(dir, p + 1, hi);
	}
}

void dinit(dirList *dir) {//initializing the dirList by mallocing some memory
	dir->i = 0;
	dir->max = 1024;
	dir->arr = (dirUnit *)malloc(sizeof(dirUnit) * dir->max);
	if(dir->arr == NULL) {
		perror("dirList");
		exit(errno);
	}
}

void dappend(dirList *dir, dirUnit element) {//adding new dirUnit at the end of the dirList
	dir->arr[dir->i++] = element;
	if(dir->i == dir->max) {
		dir->max = dir->max * 2;
		dir->arr = (dirUnit *)realloc(dir->arr, sizeof(dirUnit) * dir->max);
		if(dir->arr == NULL) {
			perror("dirList");
			exit(errno);
		}
	}
}

dirUnit dgetdir(dirList *dir, int index) {//returns the dirUnit specified at the location index
	if(index < 0 || index >= dir->i) {
		printf("dirList: Invalid index in dgetdir\n");
		exit(1);
	}
	return dir->arr[index];
}

void dsort(dirList *dir) {//this function sorts the dirUnits in the dirList with their 'name' as the key
	quicksort(dir, 0, (dir->i - 1));
}

int dlength(dirList *dir) {//return the number of dirUnits present in the dirList
	return dir->i;
}

void ddestroy(dirList *dir) {//frees the allocated memory
	free(dir->arr);
	dir->i = 0;
}
