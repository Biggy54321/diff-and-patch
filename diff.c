#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "dirList.h"
#include "fileList.h"
#include "pairStack.h"

#define FILES 8			//used to check if the type of the dirUnit being operated on is a file
#define DIRECTORY 4		//used to check if the type of the dirUnit being operated on is a directory

typedef struct lcs {//used for storing the indices of the lines from the files which belong to the common subsequence
	int *ptr;
	int count;
} lcs;

typedef struct hunk {//used in -c option to mark the beginning of the hunk, whether to print it or not and the operation
	int start;
	int printStatus;
	char *operation;
} hunk;

/*below flags represent the status of the options specified in the command line, if mention in comman line then they
   are reset*/
int bFlag = 0;
int iFlag = 0;
int wFlag = 0;
int yFlag = 0;
int cFlag = 0;
int tFlag = 0;
int rFlag = 0;

enum state {//used to determine the current state of operation in printing the diff of two files
	START, ADD, DELETE, CHANGE
};

int iStrcmp(char *str1, char *str2) {//used if iFlag is 1(ignores uppercase and lowercase)
	int i = 0, s1, s2;
	while(str1[i] != '\0' && str2[i] != '\0') {
		s1 = str1[i];
		s2 = str2[i];
		if((s1 == s2) || (s1 == (s2 - 32)) || (s1 == (s2 + 32)))
			i++;
		else
			return s1 - s2;
	}
	if(str1[i] == '\0' && str2[i] == '\0')
		return 0;
	else
		return str1[i] - str2[i];
}

int bStrcmp(char *str1, char *str2) {//used if bFlag is 1(ignores change in amount of existing white spaces)
	int i = 0, j = 0;
	while(str1[i] != '\0' && str2[j] != '\0') {
		if((str1[i] == str2[j]) || (str1[i] == '\t' && str2[j] == ' ') || (str1[i] == ' ' && str2[j] == '\t')) {
			if(str1[i] == ' ' || str1[i] == '\t') {
				while(str1[i] == ' ' || str1[i] == '\t')
					i++;
				while(str2[j] == ' ' || str2[j] == '\t')
					j++;
			}
			else {
				i++;
				j++;
			}
		}
		else
			return str1[i] - str2[j];
	}
	if(str1[i] == '\0' && str2[j] == '\0')
		return 0;
	else
		return str1[i] - str2[j];
}

int wStrcmp(char *str1, char *str2) {//used if wFlag is 1(ignores all the white spaces)
	int i = 0, j = 0;
	while(str1[i] != '\0' && str2[j] != '\0') {
		while(str1[i] == ' ' || str1[i] == '\t')
			i++;
		while(str2[j] == ' ' || str2[j] == '\t')
			j++;
		if(str1[i] == str2[j]) {
			i++;
			j++;
		}
		else
			return str1[i] - str2[j];
	}
	if(str1[i] == '\0' && str2[j] == '\0')
		return 0;
	else
		return str1[i] - str2[j];
}

int iwStrcmp(char *str1, char *str2) {//used if iFlag and wFlag are both 1(ignores case and all white spaces)
	int i = 0, j = 0, s1, s2;
	while(str1[i] != '\0' && str2[j] != '\0') {
		while(str1[i] == ' ' || str1[i] == '\t')
			i++;
		while(str2[j] == ' ' || str2[j] == '\t')
			j++;
		s1 = str1[i];
		s2 = str2[j];
		if((s1 == s2) || (s1 == (s2 - 32)) || (s1 == (s2 + 32))) {
			i++;
			j++;
		}
		else
			return s1 - s2;
	}
	if(str1[i] == '\0' && str2[j] == '\0')
		return 0;
	else
		return str1[i] - str2[j];
}

int ibStrcmp(char *str1, char *str2) {//used if iFlag and bFlag are both 1(ignores case and change in the white spaces)
	int i = 0, j = 0, s1, s2;
	while(str1[i] != '\0' && str2[j] != '\0') {
		s1 = str1[i];
		s2 = str2[j];
		if((s1 == s2) || (s1 == (s2 - 32)) || (s1 == (s2 + 32)) || (str1[i] == ' ' && str2[j] == '\t') || (str1[i] == '\t' && str2[j] == ' ')) {
			if(str1[i] == ' ' || str1[i] == '\t') {
				while(str1[i] == ' ' || str1[i] == '\t')
					i++;
				while(str2[j] == ' ' || str2[j] == '\t')
					j++;
			}
			else {
				i++;
				j++;
			}
		}
		else
			return s1 - s2;
	}
	if(str1[i] == '\0' && str2[j] == '\0')
		return 0;
	else
		return str1[i] - str2[j];
}

int getLine(int rfd, char *str) {//reads the line from the specified file char by char and returns the number of char read and returns INT_MAX in case of binary character
	int i = 0;
	while(read(rfd, &str[i], 1)) {
		if(str[i] == '\n') {
			str[i + 1] = '\0';
			i = i + 1;
			break;
		}
		else {
			if(str[i] == '\032' || str[i] == '\033')//check for binary content
				return INT_MAX;
			i++;
		}
	}
	str[i] = '\0';
	return i;
}

int getDirectory(char *path, dirList *dir) {//function return 1 for successful read else returns 0 if dir not found
	dirUnit temp;
	struct dirent *de;//directory entry structure
	DIR *dp = opendir(path);//directory pointer
	if(dp == NULL)
		return 0;
	if(path[0] == '/' && path[1] == '/')//in case of root folder there is no need of '/' again
		path++;
	while((de = readdir(dp)) != NULL) {
		if(strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {
			strcpy(temp.name, de->d_name);
			strcpy(temp.path, path);
			temp.type = de->d_type;
			dappend(dir, temp);
		}
	}
	closedir(dp);
	return 1;
}

int search(lcs *lcs, int num) {//used to find the whether the current index is in the lcs array if yes return 1 else 0
	//uses binary search algorithm
	/*Here linear search algorithm would do better, as the lines are accessed in a sequence. However it is found
	  to be expensive if the line that is being found is not in the lcs array*/
	int mid, lo = 0, hi = lcs->count - 1, flag = 0;
	while((lo != hi + 1) && (lo - 1 != hi)) {
		mid = (lo + hi) / 2;
		if(lcs->ptr[mid] == num) {
			flag = 1;
			break;
		}
		else if(num < lcs->ptr[mid])
			hi = mid - 1;
		else if(num > lcs->ptr[mid])
			lo = mid + 1;
	}
	return flag;
}

void expandTabs(char *str) {//converts the tabs in the string to spaces till the next tabstop
	int i = 0, j, numOfTabs, length = strlen(str);
	while(i < length) {
		if(str[i] == '\t') {
			numOfTabs = 8 - (i % 8);//finding the number of spaces to next tabstop
			j = length - 1;
			while(j > i) {
				str[j + numOfTabs - 1] = str[j];
				j--;
			}
			j = 0;
			while(j < numOfTabs) {
				str[i] = ' ';
				i++;
				j++;
			}
			length = length + numOfTabs - 1;
		}
		else
			i++;
	}
	str[i] = '\0';
}

int tabStrlen(char *str) {//returns the string length considering the tabs in the string are expanded
	int count = 0, tabCount, shift = 0, i = 0;
	while(str[i] != '\0') {
		if(str[i] == '\t') {
			tabCount = 8 - ((i + shift) % 8);
			tabCount--;
			count = count + tabCount;
			shift = shift + tabCount;
		}
		i++;
		count++;
	}
	return count;
}

void myTruncate(char *str) {//truncates the string so as to fit in 61 char space for -y option
	int i = 0, count = 0, shift = 0, tabCount = 0;
	while(count <= 61 && str[i] != '\0') {
		if(str[i] == '\t') {
			tabCount = 8 - ((i + shift) % 8);
			tabCount--;
			count = count + tabCount;
			shift = shift + tabCount;
		}
		count++;
		i++;
	}
	str[i - 1] = '\0';
}

void conflictError() {//used when both -y and -c options are passed by the user
	printf("diff: conflicting output style options\n");
}

//the following function uses the myer's diff algorithm to find the shortest edit script for two files
void findCommonSubsequence(fileList file1, fileList file2, lcs *lcs1, lcs *lcs2) {
	pair p;
	pairStack s;
	sinit(&s);
	char *tempStr1, *tempStr2;
	int n = flength(&file1), m = flength(&file2), max = n + m, x, xCurr, xPrev, y, yCurr, yPrev, d, k, lcsCount = 0;
	//d represents the number of moves completed and k represents the diagonal lines on the edit graph (k = x - y)
	int **editGraph, *kMaxReal, *kMax, endFlag = 0;
	//editGraph is the 2d array (d v/s k) storing the max value of (x) on each k line for corresponding d
	//kMaxReal is the array which stores the maxmimum value of x on each k line, indices represent the k lines
	//k lines start from -d to +d so we have a pointer at the middle of the kMaxReal array which is kMax
	kMaxReal = (int *)malloc(sizeof(int) * (2 * max + 1));
	if(kMaxReal == NULL) {
		perror("diff");
		exit(errno);
	}
	editGraph = (int **)malloc(sizeof(int *) * (max + 1));
	if(editGraph == NULL) {
		perror("diff");
		exit(errno);
	}
	for(d = 0; d <= max; d++) {
		editGraph[d] = (int *)malloc(sizeof(int) * (2 * d + 1));
		if(editGraph[d] == NULL) {
			perror("diff");
			exit(errno);
		}
	}
	kMax = kMaxReal + m + n;
	kMax[1] = 0;
	//Shortest edit script length will be stored in 'd' and the value of 'k' will be set accordingly to n - m
	/*The algorithm finds the maximum value of (x) on each k line, it starts from the (0, 0) position and 
	  traverses the editGraph till it reaches the opposite point i.e. (n, m)*/
	for(d = 0; d <= max; d++) {
		for(k = -d; k <= d; k = k + 2) {
			if(k == -d || (k != d && kMax[k - 1] < kMax[k + 1]))//move downward hence x remains the same
				//move downward decreases the value of the k line by 1 hence take value of x from (k + 1)
				x = kMax[k + 1];
			else//move right hence increase the value of k by 1
				//move in right increases the value of the k line by 1 hence take value of x from (k - 1)
				x = kMax[k - 1] + 1;
			y = x - k;
			while(x < n && y < m){
				tempStr1 = fgetline(&file1, x);
				tempStr2 = fgetline(&file2, y);
				if(!strcmp(tempStr1, tempStr2) ||
				(!iStrcmp(tempStr1, tempStr2) && iFlag) ||
				(!wStrcmp(tempStr1, tempStr2) && wFlag) ||
				(!bStrcmp(tempStr1, tempStr2) && bFlag) ||
				(!iwStrcmp(tempStr1, tempStr2) && iFlag && wFlag) ||
				(!ibStrcmp(tempStr1, tempStr2) && iFlag && bFlag)) {
					x++;
					y++;
				}
				else
					break;
			}
			kMax[k] = x;
			editGraph[d][k + d] = kMax[k];
			if(x >= n && y >= m) {
				endFlag = 1;
				break;
			}
		}
		if(endFlag)
			break;
	}
	//Tracing the shortest path for the common lines from (n, m) to (0, 0)
	while(k != 0 || d != 0) {
		if(k == -d || (k != d && editGraph[d - 1][k + d] > editGraph[d - 1][k + d - 2])) {
			xCurr = editGraph[d][k + d];
			yCurr = xCurr - k;
			xPrev = editGraph[d - 1][k + d];
			yPrev = xPrev - (k + 1);
			while(xCurr != xPrev || yCurr != yPrev + 1) {
				p.i = xCurr - 1;
				p.j = yCurr - 1;
				push(&s, p);
				xCurr--;
				yCurr--;
				lcsCount++;
			}
			d--;
			k++;
		}
		else {
			xCurr = editGraph[d][k + d];
			yCurr = xCurr - k;
			xPrev = editGraph[d - 1][k + d - 2];
			yPrev = xPrev - (k - 1);
			while(xCurr != xPrev + 1 || yCurr != yPrev) {
				p.i = xCurr - 1;
				p.j = yCurr - 1;
				push(&s, p);
				xCurr--;
				yCurr--;
				lcsCount++;
			}
			d--;
			k--;
		}
	}
	//Finding the SNAKE from (0, 0) if any
	xCurr = editGraph[d][k + d];
	yCurr = xCurr - k;
	while(xCurr != 0 || yCurr != 0) {
		p.i = xCurr - 1;
		p.j = yCurr - 1;
		push(&s, p);
		xCurr--;
		yCurr--;
		lcsCount++;
	}
	if(lcsCount != 0) {
		lcs1->ptr = (int*)malloc(sizeof(int) * lcsCount);
		if(lcs1->ptr == NULL) {
			perror("diff");
			exit(errno);
		}
		lcs2->ptr = (int*)malloc(sizeof(int) * lcsCount);
		if(lcs2->ptr == NULL) {
			perror("diff");
			exit(errno);
		}
	}
	else
		lcs1->ptr = lcs2->ptr = NULL;//if no common subsequence
	while(!isempty(&s)) {//popping from the stack and storing in the lcs structure
		p = pop(&s);
		lcs1->ptr[lcs1->count++] = p.i;//common lines for file1
		lcs2->ptr[lcs2->count++] = p.j;//common lines for file2
	}
	free(kMaxReal);
	for(d = 0; d <= max; d++)
		free(editGraph[d]);
	free(editGraph);
}

//set of functions for files' diff
//PrintDiff prints the diff in the normal manner
void defaultPrintDiff(fileList file1, fileList file2, lcs *lcs1, lcs *lcs2) {
	enum state STATE = START;
	pair m;//used to store the pair of integers which denote the start of the hunk in the two files
	int i = 0, j = 0, k, isLineOneInLcs = 0, isLineTwoInLcs = 0, file1Len = flength(&file1), file2Len = flength(&file2);
	char *temp;
	while(i < file1Len || j < file2Len || STATE != START) {
		//setting first flag
		if(i != file1Len)
			isLineOneInLcs = search(lcs1, i);
		else
			isLineOneInLcs = 1;
		//setting second flag
		if(j != file2Len)
			isLineTwoInLcs = search(lcs2, j);
		else
			isLineTwoInLcs = 1;
		//the program can be in 4 possible states - ADD, DELETE, CHANGE, or SAME lines here given by START
		switch(STATE) {
			case START:
				if(isLineOneInLcs && !isLineTwoInLcs) {//add
					m.i = i;
					m.j = j;
					if(j < file2Len)
						j++;
					STATE = ADD;
				}
				else if(!isLineOneInLcs && isLineTwoInLcs) {//delete
					m.i = i;
					m.j = j;
					if(i < file1Len)
						i++;
					STATE = DELETE;
				}
				else if(!isLineOneInLcs && !isLineTwoInLcs) {//change
					m.i = i;
					m.j = j;
					if(i < file1Len)
						i++;
					if(j < file2Len)
						j++;
					STATE = CHANGE;
				}
				else if(isLineOneInLcs && isLineTwoInLcs) {//same
					if(i < file1Len)
						i++;
					if(j < file2Len)
						j++;
					STATE = START;
				}
				break;
			case ADD:
				if(i == file1Len && j == file2Len) {//end of file
					if(j == m.j + 1)
						printf("%da%d\n", m.i, j);//single line added
					else
						printf("%da%d,%d\n", m.i, (m.j + 1), j);//multiple lines added
					for(k = m.j; k < j; k++) {
						temp = fgetline(&file2, k);
						if(tFlag)//checking for -t option
							expandTabs(temp);
						printf("> %s", temp);
						if(k == file2Len - 1) {//incomplete lines
							if(temp[strlen(temp) - 1] != '\n')
								printf("\n\\ No newline at end of file\n");
						}
					}
					STATE = START;
				}
				else if(isLineOneInLcs && !isLineTwoInLcs) {//add
					if(j < file2Len)
						j++;
					STATE = ADD;
				}
				else if(isLineOneInLcs && isLineTwoInLcs) {//same
					if(j == m.j + 1)
						printf("%da%d\n", m.i, j);//single addition
					else
						printf("%da%d,%d\n", m.i, (m.j + 1), j);//multiple lines added
					for(k = m.j; k < j; k++) {
						temp = fgetline(&file2, k);
						if(tFlag)//checking for t option
							expandTabs(temp);
						printf("> %s", temp);
						if(k == file2Len - 1) {
							if(temp[strlen(temp) - 1] != '\n')
								printf("\n\\ No newline at end of file\n");
						}
					}
					STATE = START;
				}
				break;
			case DELETE:
				if(i == file1Len && j == file2Len) {//end of file
					if(i == m.i + 1)
						printf("%dd%d\n", i, m.j);//single line delete
					else
					 	printf("%d,%dd%d\n", m.i + 1, i, m. j);//multiple lines deleted
					for(k = m.i; k < i; k++) {
						temp = fgetline(&file1, k);
						if(tFlag)//checking for t option
							expandTabs(temp);
					 	printf("< %s", temp);
					 	if(k == file1Len - 1) {//incomplete line check
							if(temp[strlen(temp) - 1] != '\n')
								printf("\n\\ No newline at end of file\n");
						}
					 }
					STATE = START;
				}
				else if(!isLineOneInLcs && isLineTwoInLcs) {//delete
					if(i < file1Len)
						i++;
					STATE = DELETE;
				}
				else if(isLineOneInLcs && isLineTwoInLcs) {//same
					if(i == m.i + 1)
						printf("%dd%d\n", i, m.j);//single line delete
					else
					 	printf("%d,%dd%d\n", m.i + 1, i, m. j);//multiple lines delete
					for(k = m.i; k < i; k++) {
						temp = fgetline(&file1, k);
						if(tFlag)//checking for t option
							expandTabs(temp);
					 	printf("< %s", temp);
					 	if(k == file1Len - 1) {//incomplete line check
							if(temp[strlen(temp) - 1] != '\n')
								printf("\n\\ No newline at end of file\n");
						}
					 }
					 STATE = START;
				}
				break;
			case CHANGE:
				if(i == file1Len && j == file2Len) {//end of file
					if(i == m.i + 1 && j == m.j + 1)
						printf("%dc%d\n", i, j);//one to one change
					else if(i == m.i + 1 && j != m.j + 1)
						printf("%dc%d,%d\n", i, m.j + 1, j);//one to many change
					else if(i != m.i + 1 && j == m.j + 1)
						printf("%d,%dc%d\n",m.i + 1, i, j);//many to one change
					else
						printf("%d,%dc%d,%d\n", m.i + 1, i, m.j + 1, j);//many to many change
					for(k = m.i; k < i; k++) {
						temp = fgetline(&file1, k);
						if(tFlag)//checking for t option
							expandTabs(temp);
					 	printf("< %s", temp);
					 	if(k == file1Len - 1) {//incomplete lines check
							if(temp[strlen(temp) - 1] != '\n')
								printf("\n\\ No newline at end of file\n");
						}
					 }
					printf("---\n");
					for(k = m.j; k < j; k++) {
						temp = fgetline(&file2, k);
						if(tFlag)//checking for t option
							expandTabs(temp);
						printf("> %s", temp);
						if(k == file2Len - 1) {//incomplete lines check
							if(temp[strlen(temp) - 1] != '\n')
								printf("\n\\ No newline at end of file\n");
						}
					}
					STATE = START;
				}
				else if(!isLineOneInLcs && !isLineTwoInLcs) {//change
					if(i < file1Len)
						i++;
					if(j < file2Len)
						j++;
					STATE = CHANGE;
				}
				else if(isLineOneInLcs && !isLineTwoInLcs) {//add in change from the second file
					if(j < file2Len)
						j++;
					STATE = CHANGE;
				}
				else if(!isLineOneInLcs && isLineTwoInLcs) {//delete in change from the first file
					if(i < file1Len)
						i++;
					STATE = CHANGE;

				}
				else if(isLineOneInLcs && isLineTwoInLcs) {//same
					if(i == m.i + 1 && j == m.j + 1)
						printf("%dc%d\n", i, j);//one to one change
					else if(i == m.i + 1 && j != m.j + 1)
						printf("%dc%d,%d\n", i, m.j + 1, j);//one to many change
					else if(i != m.i + 1 && j == m.j + 1)
						printf("%d,%dc%d\n",m.i + 1, i, j);//many to one change
					else
						printf("%d,%dc%d,%d\n", m.i + 1, i, m.j + 1, j);//many to many change
					for(k = m.i; k < i; k++) {
						temp = fgetline(&file1, k);
						if(tFlag)//checking for t option
							expandTabs(temp);
					 	printf("< %s", temp);
					 	if(k == file1Len - 1) {//incomplete lines check
							if(temp[strlen(temp) - 1] != '\n')
								printf("\n\\ No newline at end of file\n");
						}
					 }
					printf("---\n");
					for(k = m.j; k < j; k++) {
						temp = fgetline(&file2, k);
						if(tFlag)//checking for t option
							expandTabs(temp);
						printf("> %s", fgetline(&file2, k));
						if(k == file2Len - 1) {//incomplete lines check
							if(temp[strlen(temp) - 1] != '\n')
								printf("\n\\ No newline at end of file\n");
						}
					}
					STATE = START;
				}
				break;
		}
	}
}

//yPrintDiff prints the diff in side by side format
void yPrintDiff(fileList file1, fileList file2, lcs *lcs1, lcs *lcs2) {
	int i = 0, j = 0, isLineOneInLcs = 0, isLineTwoInLcs = 0, wordMax, newline1, newline2, file1Len = flength(&file1), file2Len = flength(&file2), str1Len, str2Len, tab1Len, tab2Len;
	char *tempStr1, *tempStr2;
	if(tFlag)
		wordMax = 64;//of which 63 are printed
	else
		wordMax = 62;//of which 61 are printed
	while(i < file1Len || j < file2Len) {
		//setting first flag
		if(i != file1Len)
			isLineOneInLcs = search(lcs1, i);
		else
			isLineOneInLcs = 1;
		//setting second flag
		if(j != file2Len)
			isLineTwoInLcs = search(lcs2, j);
		else 
			isLineTwoInLcs = 1;
		//finding the operation
		if(isLineOneInLcs && !isLineTwoInLcs) {//add
			tempStr2 = fgetline(&file2, j);
			str2Len = strlen(tempStr2);
			//check newline for the line
			if(tempStr2[str2Len - 1] == '\n') {
				newline2 = 1;
				tempStr2[str2Len - 1] = '\0';
				str2Len--;
			}
			else
				newline2 = 0;
			if(tFlag) {
				expandTabs(tempStr2);
				tab2Len = tabStrlen(tempStr2);
				str2Len = tab2Len;
				if(tab2Len >= wordMax)//if chars greater than 63
					tempStr2[wordMax - 1] = '\0';
			}
			else {
				tab2Len = tabStrlen(tempStr2);
				if(tab2Len >= wordMax)//if chars greater than 61
					myTruncate(tempStr2);
			}
			//print the operation
			printf("%*s> %c", -wordMax, "\0", (tFlag)? ' ': '\0');
			//print the second line only
			printf("%s", tempStr2);
			if(newline2)
				printf("\n");
			if(j < file2Len)
				j++;
		}
		else if(!isLineOneInLcs && isLineTwoInLcs) {//delete
			tempStr1 = fgetline(&file1, i);
			str1Len = strlen(tempStr1);
			//check newline for the line
			if(tempStr1[str1Len - 1] == '\n') {
				newline1 = 1;
				tempStr1[str1Len - 1] = '\0';
				str1Len--;
			}
			else
				newline1 = 0;
			if(tFlag) {
				expandTabs(tempStr1);
				tab1Len = tabStrlen(tempStr1);
				str1Len = tab1Len;
				if(tab1Len >= wordMax)//if chars greater than 63
					tempStr1[wordMax - 1] = '\0';
			}
			else {
				tab1Len = tabStrlen(tempStr1);
				if(tab1Len >= wordMax) {//if chars greater than 61
					myTruncate(tempStr1);
					tab1Len = tabStrlen(tempStr1);//recalculate the expanded length which will be <= 61
					str1Len = strlen(tempStr1);
				}
			}
			//print the first line only with operation
			printf("%*s<", -(wordMax - tab1Len + str1Len), tempStr1);
			if(newline1)
				printf("\n");
			if(i < file1Len)
				i++;
		}
		else if(!isLineOneInLcs && !isLineTwoInLcs) {//change
			tempStr1 = fgetline(&file1, i);
			tempStr2 = fgetline(&file2, j);
			str1Len = strlen(tempStr1);
			str2Len = strlen(tempStr2);
			//check newline for first line
			if(tempStr1[str1Len - 1] == '\n') {
				newline1 = 1;
				tempStr1[str1Len - 1] = '\0';
				str1Len--;
			}
			else
				newline1 = 0;
			//check newline for second line
			if(tempStr2[str2Len - 1] == '\n') {
				newline2 = 1;
				tempStr2[str2Len - 1] = '\0';
				str2Len--;
			}
			else
				newline2 = 0;
			if(tFlag) {
				expandTabs(tempStr1);
				expandTabs(tempStr2);
				tab1Len = tabStrlen(tempStr1);
				str1Len = tab1Len;
				tab2Len = tabStrlen(tempStr2);
				str2Len = tab2Len;
				if(tab1Len >= wordMax)//if chars greater than 63
					tempStr1[wordMax - 1] = '\0';
				if(tab2Len >= wordMax)//if chars greater than 63
					tempStr2[wordMax - 1] = '\0';
			}
			else {
				tab1Len = tabStrlen(tempStr1);
				tab2Len = tabStrlen(tempStr2);
				if(tab1Len >= wordMax) {//if chars greater than 61
					myTruncate(tempStr1);
					tab1Len = tabStrlen(tempStr1);
					str1Len = strlen(tempStr1);					
				}
				if(tab2Len >= wordMax)//if chars greater than 61
					myTruncate(tempStr2);
			}
			//print the first line
			printf("%*s", -(wordMax - tab1Len + str1Len), tempStr1);
			//print the operation
			if(newline1 && !newline2)
				printf("/ %c", (tFlag)? ' ': '\0');
			else if(!newline1 && newline2)
				printf("\\ %c", (tFlag)? ' ': '\0');
			else
				printf("| %c", (tFlag)? ' ': '\0');
			//print the second line
			printf("%s", tempStr2);
			if(newline1 || newline2)
				printf("\n");
			if(i < file1Len)
				i++;
			if(j < file2Len)
				j++;
		}
		else if(isLineOneInLcs && isLineTwoInLcs) {
			tempStr1 = fgetline(&file1, i);
			tempStr2 = fgetline(&file2, j);
			str1Len = strlen(tempStr1);
			str2Len = strlen(tempStr2);
			//check newline for first line
			if(tempStr1[str1Len - 1] == '\n') {
				newline1 = 1;
				tempStr1[str1Len - 1] = '\0';
				str1Len--;
			}
			else
				newline1 = 0;
			//check newline for second line
			if(tempStr2[str2Len - 1] == '\n') {
				newline2 = 1;
				tempStr2[str2Len - 1] = '\0';
				str2Len--;
			}
			else
				newline2 = 0;
			if(tFlag) {
				expandTabs(tempStr1);
				expandTabs(tempStr2);
				tab1Len = tabStrlen(tempStr1);
				str1Len = tab1Len;
				tab2Len = tabStrlen(tempStr2);
				str2Len = tab2Len;
				if(tab1Len >= wordMax)//if chars greater than 63
					tempStr1[wordMax - 1] = '\0';
				if(tab2Len >= wordMax)//if chars greater than 63
					tempStr2[wordMax - 1] = '\0';
			}
			else {
				tab1Len = tabStrlen(tempStr1);
				tab2Len = tabStrlen(tempStr2);
				if(tab1Len >= wordMax) {//if chars greater than 61
					myTruncate(tempStr1);
					tab1Len = tabStrlen(tempStr1);
					str1Len = strlen(tempStr1);                                                    
				}
				if(tab2Len >= wordMax)//if chars greater than 61
					myTruncate(tempStr2);
			}
			//print the first line
			printf("%*s  %c", -(wordMax - tab1Len + str1Len), tempStr1, (tFlag)? ' ': '\0');
			//print the second line
			printf("%s", tempStr2);
			if(newline1 || newline2)
				printf("\n");
			if(i < file1Len)
				i++;
			if(j < file2Len)
				j++;
		}
	}
}

//cPrintDiff prints the diff in context format
void cPrintDiff(fileList file1, fileList file2, lcs *lcs1, lcs *lcs2, char *filename1, char *filename2) {
	enum state STATE = START;
	int file1Len = flength(&file1), file2Len = flength(&file2), headerFlag = 0, i = 0, j = 0, k, isLineOneInLcs = 0, isLineTwoInLcs = 0, equalCount = 0;
	char *temp;
	hunk h1, h2;
	//initialization of the two hunks
	h1.start = h2.start = h1.printStatus = h2.printStatus = 0;
	if(file1Len != 0) {//malloc only a file having non-zero number of lines
		h1.operation = (char *)malloc(sizeof(char) * file1Len);
		if(h1.operation == NULL) {
			perror("diff");
			exit(errno);
		}
	}
	if(file2Len != 0) {//malloc only a file having non-zero number of lines
		h2.operation = (char *)malloc(sizeof(char) * file2Len);
		if(h2.operation == NULL) {
			perror("diff");
			exit(errno);
		}
	}
	//to print the timestamp at the beginning of the diff
	struct stat filestat1, filestat2;
	stat(filename1, &filestat1);
	stat(filename2, &filestat2);
	while(i < file1Len || j < file2Len || STATE != START) {
		//setting first flag
		if(i != file1Len)
			isLineOneInLcs = search(lcs1, i);
		else
			isLineOneInLcs = 1;
		//setting second flag
		if(j != file2Len)
			isLineTwoInLcs = search(lcs2, j);
		else
			isLineTwoInLcs = 1;
		switch(STATE) {
			case START:
				if(isLineOneInLcs && !isLineTwoInLcs) {//add
					h2.printStatus = 1;
					h2.operation[j] = '+';
					equalCount = 0;
					if(j < file2Len)
						j++;
					STATE = ADD;
				}
				else if(!isLineOneInLcs && isLineTwoInLcs) {//delete
					h1.printStatus = 1;
					h1.operation[i] = '-';
					equalCount = 0;
					if(i < file1Len)
						i++;
					STATE = DELETE;
				}
				else if(!isLineOneInLcs && !isLineTwoInLcs) {//change
					h1.printStatus = 1;
					h2.printStatus = 1;
					h1.operation[i] = '!';
					h2.operation[j] = '!';
					equalCount = 0;
					if(i < file1Len)
						i++;
					if(j < file2Len)
						j++;
					STATE = CHANGE;
				}
				else if(isLineOneInLcs && isLineTwoInLcs) {//same
					h1.operation[i] = ' ';
					h2.operation[j] = ' ';
					equalCount++;
					if(i < file1Len)
						i++;
					if(j < file2Len)
						j++;
					if(i == file1Len && j == file2Len) {//if we reach the end of file then print the hunk
						if(!headerFlag) {
							printf("*** %s\t%s", filename1, ctime(&filestat1.st_mtime));
							printf("--- %s\t%s", filename2, ctime(&filestat2.st_mtime));
							headerFlag = 1;
						}
						if(h1.printStatus || h2.printStatus) {//print the hunk only if they have differences
							if(equalCount > 3) {//dont print the lines below 3 equal lines so change the values of 'i' & 'j'
								i = i - equalCount + 3;
								j = j - equalCount + 3;
							}
							printf("***************\n");
							if(h1.start + 1 == i || i == 0)
								printf("*** %d ****\n", i);
							else
								printf("*** %d,%d ****\n", h1.start + 1, i);
							if(h1.printStatus) {
								for(k = h1.start; k < i; k++) {
									temp = fgetline(&file1, k);
									//print the operation
									printf("%c ", h1.operation[k]);
									if(tFlag)//checking for t option
										expandTabs(temp);
									//print the line
									printf("%s", temp);
									if(k == file1Len - 1) {//incomplete lines
										if(temp[strlen(temp) - 1] != '\n')
										printf("\n\\ No newline at end of file\n");
									}
								}
							}
							if(h2.start + 1 == j || j == 0)
								printf("--- %d ----\n", j);
							else
								printf("--- %d,%d ----\n", h2.start + 1, j);
							if(h2.printStatus) {
								for(k = h2.start; k < j; k++) {
									temp = fgetline(&file2, k);
									//print the operation
									printf("%c ", h2.operation[k]);
									if(tFlag)//checking for t option
										expandTabs(temp);
									printf("%s", temp);
									if(k == file2Len - 1) {//incomplete lines
										if(temp[strlen(temp) - 1] != '\n')
										printf("\n\\ No newline at end of file\n");
									}
								}
							}
							//reset the values of 'i' and 'j' which were changed above
							if(equalCount > 3) {
								i = i + equalCount - 3;
								j = j + equalCount - 3;
							}
						}
					}
					/*hunk has 3 common lines at top so if we get more common lines than 3 we increment
					  the start of the hunk*/
					else if((equalCount > 3) && (!h1.printStatus && !h2.printStatus)) {
						h1.start++;
						h2.start++;
						equalCount--;
					}
					/*The hunks are divided only if we get more than 6 common lines between the
					  two differences, and the program divides the hunk as soon as it gets 7 common
					  lines*/
					else if(equalCount > 6 && (h1.printStatus || h2.printStatus)) {
						if(!headerFlag) {
							printf("*** %s\t%s", filename1, ctime(&filestat1.st_mtime));
							printf("--- %s\t%s", filename2, ctime(&filestat2.st_mtime));
							headerFlag = 1;
						}
						if(h1.printStatus || h2.printStatus) {
							printf("***************\n");
							printf("*** %d,%d ****\n", h1.start + 1, i - 4);
							if(h1.printStatus) {
								for(k = h1.start; k < i - 4; k++) {
									temp = fgetline(&file1, k);
									//print the operation
									printf("%c ", h1.operation[k]);
									if(tFlag)//checking for t option
										expandTabs(temp);
									printf("%s", temp);
									if(k == file1Len - 1) {//incomplete lines
										if(temp[strlen(temp) - 1] != '\n')
										printf("\n\\ No newline at end of file\n");
									}
								}
								h1.printStatus = 0;//reset condition for the hunk
							}
							printf("--- %d,%d ----\n", h2.start + 1, j - 4);
							if(h2.printStatus) {
								for(k = h2.start; k < j - 4; k++) {
									temp = fgetline(&file2, k);
									//print the operation
									printf("%c ", h2.operation[k]);
									if(tFlag)//checking for t option
										expandTabs(temp);
									printf("%s", temp);
									if(k == file2Len - 1) {//incomplete lines
										temp = fgetline(&file2, k);
										if(temp[strlen(temp) - 1] != '\n')
										printf("\n\\ No newline at end of file\n");
									}
								}
								h2.printStatus = 0;//reset condition for the hunk
							}
							h1.start = i - 3;//as next hunk will have 3 common lines at start
							h2.start = j - 3;
						}
					}
					STATE = START;
				}
				break;
			case ADD:
				if(i == file1Len && j == file2Len) {//end of file
					if(!headerFlag) {
						printf("*** %s\t%s", filename1, ctime(&filestat1.st_mtime));
						printf("--- %s\t%s", filename2, ctime(&filestat2.st_mtime));
						headerFlag = 1;
					}
					if(h1.printStatus || h2.printStatus) {
						printf("***************\n");
						if(h1.start + 1 == i || i == 0)
							printf("*** %d ****\n", i);
						else
							printf("*** %d,%d ****\n", h1.start + 1, i);
						if(h1.printStatus) {
							for(k = h1.start; k < i; k++) {
								temp = fgetline(&file1, k);
								//print the operation
								printf("%c ", h1.operation[k]);
								if(tFlag)//checking for t option
									expandTabs(temp);
								printf("%s", temp);
								if(k == file1Len - 1) {//incomplete lines
									if(temp[strlen(temp) - 1] != '\n')
									printf("\n\\ No newline at end of file\n");
								}
							}
						}
						if(h2.start + 1 == j || j == 0)
							printf("--- %d ----\n", j);
						else
							printf("--- %d,%d ----\n", h2.start + 1, j);
						if(h2.printStatus) {
							for(k = h2.start; k < j; k++) {
								temp = fgetline(&file2, k);
								//print the operation
								printf("%c ", h2.operation[k]);
								if(tFlag)//checking for t option
									expandTabs(temp);
								printf("%s", temp);
								if(k == file2Len - 1) {//incomplete lines
									if(temp[strlen(temp) - 1] != '\n')
									printf("\n\\ No newline at end of file\n");
								}
							}
						}
					}
					STATE = START;
				}
				else if(isLineOneInLcs && !isLineTwoInLcs) {//add
					h2.operation[j] = '+';
					if(j < file2Len)
						j++;
					STATE = ADD;
				}
				else if(isLineOneInLcs && isLineTwoInLcs)//same
					STATE = START;
				break;
			case DELETE:
				if(i == file1Len && j == file2Len) {//end of file
					if(!headerFlag) {
						printf("*** %s\t%s", filename1, ctime(&filestat1.st_mtime));
						printf("--- %s\t%s", filename2, ctime(&filestat2.st_mtime));
						headerFlag = 1;
					}
					if(h1.printStatus || h2.printStatus) {
						printf("***************\n");
						if(h1.start + 1 == i || i == 0)
							printf("*** %d ****\n", i);
						else
							printf("*** %d,%d ****\n", h1.start + 1, i);
						if(h1.printStatus) {
							for(k = h1.start; k < i; k++) {
								temp = fgetline(&file1, k);
								//print the operation
								printf("%c ", h1.operation[k]);
								if(tFlag)//checking for t option
									expandTabs(temp);
								printf("%s", temp);
								if(k == file1Len - 1) {//incomplete lines
									if(temp[strlen(temp) - 1] != '\n')
									printf("\n\\ No newline at end of file\n");
								}
							}
						}
						if(h2.start + 1 == j || j == 0)
							printf("--- %d ----\n", j);
						else
							printf("--- %d,%d ----\n", h2.start + 1, j);
						if(h2.printStatus) {
							for(k = h2.start; k < j; k++) {
								temp = fgetline(&file2, k);
								//print the operation
								printf("%c ", h2.operation[k]);
								if(tFlag)//checking for t option
									expandTabs(temp);
								printf("%s", temp);
								if(k == file2Len - 1) {//incomplete lines
									if(temp[strlen(temp) - 1] != '\n')
									printf("\n\\ No newline at end of file\n");
								}
							}
						}
					}
					STATE = START;
				}
				else if(!isLineOneInLcs && isLineTwoInLcs) {//delete
					h1.operation[i] = '-';
					if(i < file1Len)
						i++;
					STATE = DELETE;
				}
				else if(isLineOneInLcs && isLineTwoInLcs)//same
					 STATE = START;
				break;
			case CHANGE:
				if(i == file1Len && j == file2Len) {//end of file
					if(!headerFlag) {
						printf("*** %s\t%s", filename1, ctime(&filestat1.st_mtime));
						printf("--- %s\t%s", filename2, ctime(&filestat2.st_mtime));
						headerFlag = 1;
					}
					if(h1.printStatus || h2.printStatus) {
						printf("***************\n");
						if(h1.start + 1 == i || i == 0)
							printf("*** %d ****\n", i);
						else
							printf("*** %d,%d ****\n", h1.start + 1, i);
						if(h1.printStatus) {
							for(k = h1.start; k < i; k++) {
								temp = fgetline(&file1, k);
								//print the operation
								printf("%c ", h1.operation[k]);
								if(tFlag)//checking for t option
									expandTabs(temp);
								printf("%s", temp);
								if(k == file1Len - 1) {//incomplete lines
									if(temp[strlen(temp) - 1] != '\n')
									printf("\n\\ No newline at end of file\n");
								}
							}
						}
						if(h2.start + 1 == j || j == 0)
							printf("--- %d ----\n", j);
						else
							printf("--- %d,%d ----\n", h2.start + 1, j);
						if(h2.printStatus) {
							for(k = h2.start; k < j; k++) {
								temp = fgetline(&file2, k);
								//print the operation
								printf("%c ", h2.operation[k]);
								if(tFlag)//checking for t option
									expandTabs(temp);
								printf("%s", temp);
								if(k == file2Len - 1) {//incomplete lines
									if(temp[strlen(temp) - 1] != '\n')
									printf("\n\\ No newline at end of file\n");
								}
							}
						}
					}
					STATE = START;
				}
				else if(!isLineOneInLcs && !isLineTwoInLcs) {//change
					h1.operation[i] = '!';
					h2.operation[j] = '!';
					if(i < file1Len)
						i++;
					if(j < file2Len)
						j++;
					STATE = CHANGE;
				}
				else if(isLineOneInLcs && !isLineTwoInLcs) {//add in change from the second file
					h2.operation[j] = '!';
					if(j < file2Len)
						j++;
					STATE = CHANGE;
				}
				else if(!isLineOneInLcs && isLineTwoInLcs) {//remove in change from the first file
					h1.operation[i] = '!';
					if(i < file1Len)
						i++;
					STATE = CHANGE;

				}
				else if(isLineOneInLcs && isLineTwoInLcs)//same
					STATE = START;
				break;
		}
	}
	if(file1Len != 0)
		free(h1.operation);
	if(file2Len != 0)
		free(h2.operation);
}

void diffForFiles(char *filename1, char *filename2, int TYPE) {
	int  fp1, fp2, error = 0, file1Len, file2Len, binaryCheck;
	char *str = (char *)malloc(sizeof(char) * 2048);//max line size is 2048 chars
	fileList file1, file2;
	finit(&file1);
	finit(&file2);
	lcs lcs1, lcs2;
	lcs1.count = lcs2.count = 0;
	if(opendir(filename1)) {//filename1 is a directory
		//if filename1 is a directory then filename2 must be a file
		strcat(filename1, "/");
		strcat(filename1, filename2);
		//we now open the file filename1/filename2
	}
	fp1 = open(filename1, O_RDONLY);
	if(fp1 == -1) {
		fprintf(stderr, "diff: %s: ", filename1);
		perror("");
		error = 1;
	}
	if(opendir(filename2)) {//filename2 is a directory
		//if filename2 is directory then filename1 must be a file
		strcat(filename2, "/");
		strcat(filename2, filename1);
		//we now open the file filename2/filename1
	}
	fp2 = open(filename2, O_RDONLY);
	if(fp2 == -1) {
		fprintf(stderr, "diff: %s: ", filename2);
		perror("");
		error = 1;
	}
	if(error)
		return;
	//reading first file
	while((binaryCheck = getLine(fp1, str))) {
		if(binaryCheck == INT_MAX) {//check if it has binary content
			fprintf(stderr, "Binary files %s and %s differ\n", filename1, filename2);
			exit(1);
		}
		fappend(&file1, str);
	}
	//reading second file
	while((binaryCheck = getLine(fp2, str))) {
		if(binaryCheck == INT_MAX) {//check if it has binary content
			fprintf(stderr, "Binary files %s and %s differ\n", filename1, filename2);
			exit(1);
		}
		fappend(&file2, str);
	}
	file1Len = flength(&file1);
	file2Len = flength(&file2);
	if(file1Len != 0 && file2Len != 0)//dont find LCS if any one of the file is null
		findCommonSubsequence(file1, file2, &lcs1, &lcs2);//this function sets the LCS type objects with indices of the common lines
	if(lcs1.count != file1Len || lcs2.count != file2Len) {//dont print the diff if the files are same
		if(TYPE == DIRECTORY)
			printf("diff %s %s\n", filename1, filename2);
		if(yFlag)
			yPrintDiff(file1, file2, &lcs1, &lcs2);
		else if(cFlag)
			cPrintDiff(file1, file2, &lcs1, &lcs2, filename1, filename2);
		else
			defaultPrintDiff(file1, file2, &lcs1, &lcs2);
	}
	//freeing the allocated memory
	close(fp1);
	close(fp2);
	fdestroy(&file1);
	fdestroy(&file2);
	if(file1Len != 0 && file2Len != 0 && lcs1.ptr != NULL && lcs2.ptr != NULL) {
		free(lcs1.ptr);
		free(lcs2.ptr);
	}
	free(str);
}

//set of functions for directories diff
void rPrintDiff(dirList dir1, dirList dir2) {
	int i, j, comparison, read1, read2, dir1Len = dlength(&dir1), dir2Len = dlength(&dir2);
	dirList temp_dir1, temp_dir2;//used in case of recursion
	dirUnit unit1, unit2;
	char *dir1Name, *dir2Name;
	dir1Name = (char *)malloc(sizeof(char) * 1024);//name of subdir1 to be given to rdiff in case of recursion
	dir2Name = (char *)malloc(sizeof(char) * 1024);//name of subdir2 to be given to rdiff in case of recursion
	//sort the two lists
	dsort(&dir1);
	dsort(&dir2);
	i = j = 0;
	while(i < dir1Len || j < dir2Len) {
		if(i < dir1Len && j < dir2Len) {//if i and j within the limits then only do the comparison
			unit1 = dgetdir(&dir1, i);
			unit2 = dgetdir(&dir2, j);
			comparison = strcmp(unit1.name, unit2.name);
		}
		else {// if i or j are out of bounds of the dirUnit length then make the following changes
			if(i == dir1Len) {
				unit2 = dgetdir(&dir2, j);
				comparison = 1;// greater than zero to print dir2
			}
			else if(j == dir2Len) {
				unit1 = dgetdir(&dir1, i);	
				comparison = -1;// less than zero to print dir1
			}
		}
		if(comparison < 0) {//print the file1 changes
			printf("Only in %s: %s\n", unit1.path, unit1.name);
			if(i < dir1Len)
				i++;
		}
		else if(comparison > 0) {//print the file2 changes
			printf("Only in %s: %s\n", unit2.path, unit2.name);
			if(j < dir2Len)
				j++;
		}
		else {//if the names of the two files of subdirectory is same
			strcpy(dir1Name, unit1.path);
			strcat(dir1Name, "/");
			strcat(dir1Name, unit1.name);
			strcpy(dir2Name, unit2.path);
			strcat(dir2Name, "/");
			strcat(dir2Name, unit2.name);
			if(unit1.type == FILES)
				diffForFiles(dir1Name, dir2Name, DIRECTORY);
			else {//type == DIRECTORY
				if(rFlag) {//if rFlag is set then call recursively or just print the common directories
					dinit(&temp_dir1);
					dinit(&temp_dir2);
					if((read1 = getDirectory(dir1Name, &temp_dir1)) && (read2 = getDirectory(dir2Name, &temp_dir2)))
						rPrintDiff(temp_dir1, temp_dir2);
					else {//if read fails
						if(!read1) {
							fprintf(stderr, "diff: %s: ", dir1Name);
							perror("");
						}
						if(!read2) {
							fprintf(stderr, "diff: %s: ", dir2Name);
							perror("");
						}
					}
					ddestroy(&temp_dir1);
					ddestroy(&temp_dir2);
						
				}
				else
					printf("Common subdirectories: %s/%s and %s/%s\n", unit1.path, unit1.name, unit2.path, unit2.name);
			}
			if(i < dir1Len)
				i++;
			if(j < dir2Len)
				j++;	
		}
	}
}

void diffForDirectories(char *dirname1, char *dirname2) {
	dirList dir1, dir2;
	dinit(&dir1);
	dinit(&dir2);
	int dir1Len, dir2Len;
	dir1Len = dir2Len = 0;
	//read the directories in the directory structure
	if(!getDirectory(dirname1, &dir1) || !getDirectory(dirname2, &dir2))
		return;
	dir1Len = dlength(&dir1);
	dir2Len = dlength(&dir2);
	//call the directory diff
	if(dir1Len != 0 || dir2Len != 0)
		rPrintDiff(dir1, dir2);
	//freeing the allocated memory
	ddestroy(&dir1);
	ddestroy(&dir2);
	exit(1);
}

int main(int argc, char *argv[]) {
	int option, fileCount = 0;
	extern int optind;
	char *argument1, *argument2;
	argument1 = (char *)malloc(sizeof(char) * 1024);//name of the first argument
	argument2 = (char *)malloc(sizeof(char) * 1024);//name of the second argument
	//calling getopt() to set the global flags for each option
	while((option = getopt(argc, argv, "ycrtibw")) != -1 ) {
		switch(option) {
			case 'y':
				if(!cFlag)
					yFlag = 1;
				else {
					conflictError();
					return 0;
				}
				break;
			case 'c':
				if(!yFlag)
					cFlag = 1;
				else {
					conflictError();
					return 0;
				}
				break;
			case 'r':
				rFlag = 1;
				break;
			case 't':
				tFlag = 1;
				break;
			case 'i':
				iFlag = 1;
				break;
			case 'b':
				bFlag = 1;
				break;
			case 'w':
				wFlag = 1;
				break;
			default:
				return 0;
				break;
			}
	}
	//getting the file/dir name from the command line
	while(argv[optind] != NULL) {
		if(fileCount == 0)
			strcpy(argument1, argv[optind]);
		else
			strcpy(argument2, argv[optind]);
		optind++;
		if(fileCount == 2) {
			fileCount++;
			break;
		}
		fileCount++;
	}
	//missing operand error
	if(fileCount < 2) {
		printf("diff: missing operand after '%s'\n", argv[optind - 1]);
		return 0;
	}
	//extra operand error
	if(fileCount > 2) {
		printf("diff: extra operand '%s'\n", argv[optind - 1]);
		return 0;
	}
	if(!strcmp(argument1, argument2))//if file names are same
		return 0;
	/*if the control passes over the above steps implies that we have exactly two files and no conflicting, missing or extra operand errors*/
	//primarily call the diff for directories
	diffForDirectories(argument1, argument2);
	//calling the function for showing diff of files as the opendir() has failed for the passed arguments
	diffForFiles(argument1, argument2, FILES);
	return 0;
}
