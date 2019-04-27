#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/times.h>
#include <string.h>
#include <errno.h>
#include "fileList.h"

#define PASS 1
#define FAIL 0
#define UP 100
#define DOWN 200

int RFlag = 0;

/*For the structure hunk -
  start and end are the actual positions of the hunk in the original file
  lines is the number of lines in the hunk
  buffer stores the hunk lines*/
typedef struct hunk {
	int start, end, lines, hunkNum;
	fileList buffer;
} hunk;

//The function sets the start and end values according to the operation which can be either a, d or c
void readOperation(char *line, char *operation, hunk *hunk1, hunk *hunk2) {
	char *ptr, num[1024];
	int i;
	//reading the line numbers of the first file
	i = 0;
	ptr = line;
	//reading the starting line position corresponding to the first file
	while(*ptr >= '0' && *ptr <= '9') {
		num[i] = *ptr;
		ptr++;
		i++;
	}
	num[i] = '\0';
	hunk1->start = atoi(num);
	//reading the ending line position corresponding to the first file
	if(*ptr != ',') {
		if(hunk1->start != 0)
			hunk1->end = hunk1->start;
		else
			hunk1->end = -1;
		*operation = *ptr;
	}
	else {
		i = 0;
		ptr++;
		while(*ptr >= '0' && *ptr <= '9') {
			num[i] = *ptr;
	 		ptr++;
			i++;
		}
		num[i] = '\0';
		hunk1->end = atoi(num);
		*operation = *ptr;
	}
	//reading the starting line position corresponding to the second file
	i = 0;
	ptr++;
	while(*ptr >= '0' && *ptr <= '9') {
		num[i] = *ptr;
		ptr++;
		i++;
	}
	num[i] = '\0';
	hunk2->start = atoi(num);
	//reading the ending line position corresponding to the second file
	if(*ptr != ',') {
		if(hunk2->start != 0)
			hunk2->end = hunk2->start;
		else
			hunk2->end = -1;
	}
	else {
		i = 0;
		ptr++;
		while(*ptr >= '0' && *ptr <= '9') {
			num[i] = *ptr;
	 		ptr++;
			i++;
		}
		num[i] = '\0';
		hunk2->end = atoi(num);
	}
	hunk1->lines = hunk1->end - hunk1->start + 1;
	hunk2->lines = hunk2->end - hunk2->start + 1;
}

/*The function below reads the line corresponding to the line numbers of the hunk in the actual file and then
  intializes the hunk's start and end with the required values*/
void readHunkHeader(FILE *fpPatch, hunk *hunkVar) {
	char *str, *ptr, num1[1024], num2[1024];
	int i;
	str = (char *)malloc(sizeof(char) * 1024);
	fgets(str, 1024, fpPatch);
	//checking if the line read has the correct format
	if(strstr(str, "*** ") && strstr(str, "--- ")) {
		fprintf(stderr, "patch: **** Only garbage was found in patch\n");
		exit(1);
	}
	ptr = str + 4;
	i = 0;
	while(*ptr != ',' && *ptr != ' ') {
		num1[i] = *ptr;
		ptr++;
		i++;
	}
	num1[i] = '\0';
	hunkVar->start = atoi(num1);
	i = 0;
	if(*ptr != ' ') {
		ptr++;
		while(*ptr != ' ') {
			num2[i] = *ptr;
			ptr++;
			i++;
		}
		num2[i] = '\0';
		hunkVar->end = atoi(num2);
	}
	else {
		if(hunkVar->start != 0)
			hunkVar->end = hunkVar->start;
		else
			hunkVar->end = -1;
	}
	hunkVar->lines = hunkVar->end - hunkVar->start + 1;
	free(str);
}

/*The function finds the right position of the hunk in the file to be patched to produce an offset in any
  It is used for finding the position of the hunk, when the patch file has diff in default format*/
int findOffset(fileList fileBuffer, hunk *searchHunk, int currentline) {
	int hunkNotFound = 0, offset, i, j, start, end, upStart, upEnd, downStart, downEnd, roof = 1, floor = 1, direction = UP, fileLen = flength(&fileBuffer);
	/*roof = 1 -> we have not yet reached the upperlimit of the search which is the currentline number of the 
	  and roof = 0 -> we have reached the upperlimit of the search and cannot go beyond.
	  floor = 1 -> we have not yet reached the lowerlimit of the search which is the end of the readFile and 
	  floor = 0 -> we have reached the lowerlimit of the search and cannot go beyond*/
	char *line1, *line2;
	start = searchHunk->start;
	end = searchHunk->end;
	upStart = start - 1;//used when we search upwards
	upEnd = end - 1;//used when we search upwards
	downStart = start + 1;//used when we search downwards
	downEnd = end + 1;//used when we search downwards
	while(roof || floor) {
		hunkNotFound = 0;
		i = start - 1;
		j = end;
		while(i < j) {
			line1 = fgetline(&fileBuffer, i);
			if(line1 == NULL) {//i.e. if the readFile does not have a line at position 'i'
				hunkNotFound = 1;
				break;
			}
			line2 = fgetline(&(searchHunk->buffer), i - start + 1);
			line2 = line2 + 2;
			if(strcmp(line1, line2)) {//i.e. if the lines do not match
				hunkNotFound = 1;
				break;
			}
			else
				i++;
		}
		if(hunkNotFound) {//hunk not found
				label: if(roof && direction == UP) {
					start = upStart--;
					end = upEnd--;
					//flip the direction only if floor is not reached
					if(floor)
						direction = DOWN;
					//if start is valid then continue else dont continue
					if(start >= currentline)
						continue;
					else
						roof = 0;
				}
				if(floor && direction == DOWN) {
					start = downStart++;
					end = downEnd++;
					//if roof not reached then only flip
					if(roof)
						direction = UP;
					//if end is valid then only continue the iteration
					if(end < fileLen + 1)
						continue;
					else
						floor = 0;
				}
				/*if control comes to the following point it means that we have reached the 
				  floor, but if we have reached the floor then roof may or may not be still
				  reached so check*/
				if(roof)
					goto label;
		}
		else {//hunk found
			offset = start - searchHunk->start;
			if(offset)
				printf("Hunk #%d succeeded at %d (offset %d line).\n", searchHunk->hunkNum, start, offset);
			//updating the start and end
			searchHunk->start = start;
			searchHunk->end =  end;
			return PASS;
		}
	}
	//if control reaches this point it implies that the hunk is not found
	printf("Hunk #%d FAILED at %d\n", searchHunk->hunkNum, searchHunk->start);
	return FAIL;
}

/*The function below finds the position of the searchHunk in the fileBuffer and updates its position to right values
  if any offset or fuzz is found, it also updates the line number of the writeHunk and the initializes the shift
  variable to 0, 1, 2 in case of fuzz*/
int findOffsetAndFuzz(fileList fileBuffer, hunk *searchHunk, hunk *writeHunk, int currentline, int *shift) {
	int fuzz = 0, offset, i, j, k, start, end, upStart, upEnd, downStart, downEnd, hunkNotFound, roof = 1, floor = 1, direction = UP, fileLen = flength(&fileBuffer);
	//roof = 1 (when start not reached currentline line)
	//roof = 0 (when start has reached currentline line)
	//floor = 1(when end has not reached length of the fileBuffer)
	//floor = 0(when end has reached length of the fileBuffer)
	char *line1, *line2;
	start = searchHunk->start;
	end = searchHunk->end;
	upStart = start - 1;
	upEnd =  end - 1;
	downStart = start + 1;
	downEnd = end + 1;
	while(roof || floor) {//iterate while the roof and floor both have not reached
		hunkNotFound = 0;
		*shift = 0;
		i = start - 1;//start - 1 because line numbers in arrays start from zero
		j = end;
		//adjust start and end with respect to fuzz
		/*in case of fuzz getting the line from the searchHunk is enough but we get the line from the fileBuffer
		  too as, if we ignore a line from top or bottom from the hunk then we must also ignore a line from the
		  fileBuffer, so if we have got a line from the searchHunk but we have not got a line from the fileBuffer
		  then we skip the current iteration as the fuzz is not valid*/
		if(fuzz == 1) {
			line1 = fgetline(&fileBuffer, i);//getting from the fileBuffer to check
			if(line1 == NULL) {
				hunkNotFound = 1;
				goto skip;
			}
			line1 = fgetline(&(searchHunk->buffer), 0);//getting from the searchHunk
			if(line1[0] == ' ') {//increment only is the line belongs to the LCS
				i++;
				(*shift)++;
			}				
			line1 = fgetline(&fileBuffer, end - 1);
			if(line1 == NULL) {
				hunkNotFound = 1;
				goto skip;
			}
			line1 = fgetline(&(searchHunk->buffer), flength(&(searchHunk->buffer)) - 1);
			if(line1[0] == ' ')//decrement only if the line belongs to the LCS
				j--;
		}
		if(fuzz == 2) {
			for(k = 0; k < 2; k++) {
				line1 = fgetline(&fileBuffer, i);
				if(line1 == NULL) {
					hunkNotFound = 1;
					goto skip;
				}
				line1 = fgetline(&(searchHunk->buffer), k);
				if(line1[0] == ' ') {
					i++;
					(*shift)++;
				}
				else
					break;
			}
			for(k = 0; k < 2; k++) {
				line1 = fgetline(&fileBuffer, j - 1);
				if(line1 == NULL) {
					hunkNotFound = 1;
					goto skip;
				}
				line1 = fgetline(&(searchHunk->buffer), flength(&(searchHunk->buffer)) - k - 1);
				if(line1[0] == ' ')
					j--;
				else
					break;
			}
		}
		//search the hunk in the original file i.e. in the fileBuffer
		while(i < j) {
			line1 = fgetline(&fileBuffer, i);
			if(line1 == NULL)  {
				hunkNotFound = 1;
				break;
			}
			line2 = fgetline(&(searchHunk->buffer), i - start + 1);
			line2 = line2 + 2;
			if(strcmp(line1, line2)) {
				hunkNotFound = 1;
				break;
			}
			else
				i++;
		}
		skip: if(hunkNotFound) {//not found
			if(fuzz < 2)
				fuzz++;
			else {
				fuzz = 0;
				//go in the below if only when roof is not reached and direction is UP
			   	label: if(roof && direction == UP) {
					start = upStart--;
					end = upEnd--;
					//flip the direction only if floor is not reached
					if(floor)
						direction = DOWN;
					//if start is valid then continue else dont continue
					if(start >= currentline)
						continue;
					else
						roof = 0;
				}
				if(floor && direction == DOWN) {
					start = downStart++;
					end = downEnd++;
					//if roof not reached then only flip
					if(roof)
						direction = UP;
					//if end is valid then only continue the iteration
					if(end < fileLen + 1)
						continue;
					else
						floor = 0;
				}
				/*if control comes to the following point it means that we have reached the 
				  floor, but if we have reached the floor then roof may or may not be still
				  reached so check*/
				if(roof)
					goto label;
			}
		}
		else {//found
			offset = start - searchHunk->start;
			if(offset || fuzz) {
				printf("Hunk #%d succeeded at %d", searchHunk->hunkNum, start);
				if(fuzz)
					printf(" with fuzz %d", fuzz);
				if(offset)
					printf(" (offset %d line)", offset);
				printf(".\n");
			}
			//update the hunk start and end of the search hunk and the lines of the write hunk;
			searchHunk->start = start + *shift;
			searchHunk->end = j;
			searchHunk->lines = searchHunk->end - searchHunk->start + 1;
			writeHunk->lines = writeHunk->lines - (end - j);//reduced number of lines in case of fuzz
			return PASS;
		}
	}
	//if control reaches this point then the hunk is not found
	printf("Hunk #%d FAILED at %d\n", searchHunk->hunkNum, searchHunk->start);
	return FAIL;
}


/*The function below patches the file using the patch file having diff in default format*/
void patch(FILE *fpPatch, FILE *fpRead, FILE *fpWrite, char *readFile) {
	int currentline = 1, failCount = 0, result, fileLen, rejectHeader = 0, lines, i, hunkNum = 1, patchCount = 0;
	//patchCount gives the currentline number in the patchFile
	//failCount give number of failed hunks
	//hunkNum gives the current hunk number
	//currentline gives the line number in the file to be patched
	FILE *fpReject;
	char *line, *linePtr, operation;
	hunk hunk1, hunk2;
	fileList fileBuffer;
	finit(&fileBuffer);
	line = (char *)malloc(sizeof(char) * 2048);//max line size is 2048 chars
	//read the readFile completely
	while(fgets(line, 2048, fpRead))
		fappend(&fileBuffer, line);
	fileLen = flength(&fileBuffer);
	while(fgets(line, 2048, fpPatch)) {//loop continues till end of the patchFile
		finit(&hunk1.buffer);
		finit(&hunk2.buffer);
		//the following function will set the int starts and ends to respective values.
		readOperation(line, &operation, &hunk1, &hunk2);
		patchCount++;
		//reading the first part of the hunk only in case of 'delete' or 'change' i.e. lines with '<' at start
		if(operation == 'd' || operation == 'c') {
			i = 0;
			while(i < (hunk1.lines + 1) && fgets(line, 2048, fpPatch)) {
				if(line[0] != '<') {
					//checking for error
					if(i != hunk1.lines) {
						fprintf(stderr, "patch: **** '<' expected at line %d of patch\n", patchCount + 1);
						exit(1);
					}
					if(line[0] == '\\') {//it the line is no newline condition
						linePtr = fpop(&hunk1.buffer);
						linePtr[strlen(linePtr) - 1] = '\0';
						fappend(&hunk1.buffer, linePtr);
						patchCount++;
					}
					else if(line[0] != '-') {//if the line is the operation for the next hunk
						//if the operation is 'change' but we have not yet passed through '---'
						if(operation == 'c') {
							fprintf(stderr, "patch: **** expected '---' at line %d of patch\n", patchCount + 1);
							exit(1);
						}
						fseek(fpPatch, -strlen(line), SEEK_CUR);
						break;
					}
					else {//if the line is '---'
						patchCount++;
						break;
					}
				}
				else {
					fappend(&hunk1.buffer, line);
					i++;
					patchCount++;
				}
			}
			//end of file abruptly
			if(i != hunk1.lines) {
				fprintf(stderr, "patch: **** unexpected end of file in patch at line %d\n", patchCount);
				exit(1);
			}
		}
		//reading the second part of the hunk only in case of 'add' or 'change' i.e. lines with '>' at start
		if(operation == 'a' || operation == 'c') {
			i = 0;
			while(i < (hunk2.lines + 1) && fgets(line, 2048, fpPatch)) {
				if(line[0] != '>') {
					//checking for error
					if(i != hunk2.lines) {
						fprintf(stderr, "patch: **** '>' expected at line %d of patch\n", patchCount + 1);
						exit(1);
					}
					if(line[0] == '\\') {//the line is no newline condition
						linePtr = fpop(&hunk2.buffer);
						linePtr[strlen(linePtr) - 1] = '\0';
						fappend(&hunk2.buffer, linePtr);
						patchCount++;
					}
					else {//the line is the operation for the next hunk
						fseek(fpPatch, -strlen(line), SEEK_CUR);
						break;
					}
				}
				else {
					fappend(&hunk2.buffer, line);
					i++;
					patchCount++;
				}
			}
			//patch file abruptly ended
			if(i != hunk2.lines) {
				fprintf(stderr, "patch: **** unexpected end of file in patch at line %d\n", patchCount);
				exit(1);
			}
		}
		hunk1.hunkNum = hunk2.hunkNum = hunkNum;
		//in normal patch search for offset only if the operation is 'delete' or 'change' i.e. first hunk
		if(!RFlag) {
			if(operation == 'd' || operation == 'c')
				result = findOffset(fileBuffer, &hunk1, currentline);
			else
				result = PASS;
		}
		//in reverse patch search for offset only if the operation is 'add' or 'change' i.e. second hunk
		else {
			if(operation == 'a' || operation == 'c')
				result = findOffset(fileBuffer, &hunk2, currentline);
			else
				result = PASS;
		}
		if(result == PASS) {
			if(!RFlag) {
				/*No need to check if the line we get from the fileBuffer is NULL, because we are getting the
				  line before the fileLen of the fileBuffer as well it is checked in the offset function*/
				while(currentline < hunk1.start && currentline <= fileLen) {
					linePtr = fgetline(&fileBuffer, currentline - 1);
					fprintf(fpWrite, "%s", linePtr);
					currentline++;
				}
				/*In case of 'add' in normal diff the line number of the first hunk is the line number
				  after which we have to add a line so print that extra line in case of 'add' only
				  also the third comparison is done to prevent wrong patch in case of addition from
				  0th line*/
				if(operation == 'a' && currentline <= fileLen && hunk1.start > 0) {
					linePtr = fgetline(&fileBuffer, currentline - 1);				
					fprintf(fpWrite, "%s", linePtr);
				}
				currentline = currentline + hunk1.lines;
				//write the second part of the hunk only if the operation is either 'add' or 'change'
				if(operation == 'a' || operation == 'c') {
					for(i = 0; i < hunk2.lines; i++) {
						linePtr = fgetline(&hunk2.buffer, i);
						fprintf(fpWrite, "%s", linePtr + 2);
					}
				}
			}
			else {
				while(currentline < hunk2.start && currentline <= fileLen) {
					linePtr = fgetline(&fileBuffer, currentline - 1);
					fprintf(fpWrite, "%s", linePtr);
					currentline++;
				}
				//in reverse patch delete corresponds to add hence print that extra line
				if(operation == 'd' && currentline <= fileLen && hunk2.start > 0) {
					
					linePtr = fgetline(&fileBuffer, currentline - 1);				
					fprintf(fpWrite, "%s", linePtr);
				}
				currentline = currentline + hunk2.lines;
				//write the first part of the hunk only if the operation is either 'delete' or 'change'
				if(operation == 'd' || operation == 'c') {
					for(i = 0; i < hunk1.lines; i++) {
						linePtr = fgetline(&hunk1.buffer, i);
						fprintf(fpWrite, "%s", linePtr + 2);
					}
				}
			}
		}
		else {
			failCount++;
			//printing the timestamps only once
			if(!rejectHeader) {
				strcpy(line, readFile);
				strcat(line, ".rej");
				fpReject = fopen(line, "w+");
				if(fpReject == NULL) {
					fprintf(stderr, "patch: **** Can't create reject file\n");
					exit(errno);
				}
				fprintf(fpReject, "*** /dev/null\n--- /dev/null\n");
				rejectHeader = 1;
			}
			fprintf(fpReject, "***************\n");
			//writing the first hunk
			if(hunk1.start == 0 && hunk1.end == -1)
				fprintf(fpReject, "*** 0 ****\n");
			else if(hunk1.start == hunk1.end)
				fprintf(fpReject, "*** %d ****\n", hunk1.start);
			else
				fprintf(fpReject, "*** %d,%d ****\n", hunk1.start, hunk1.end);
			if(!RFlag)
				lines = hunk1.lines;
			else
				lines = hunk2.lines;
			for(i = 0; i < lines; i++) {
				if(!RFlag)
					linePtr = fgetline(&hunk1.buffer, i);
				else
					linePtr = fgetline(&hunk2.buffer, i);
				fprintf(fpReject, "%c%s", '-', linePtr + 1);
				if(i == lines - 1) {
					if(linePtr[strlen(linePtr) - 1] != '\n')
						fprintf(fpReject, "\n\\ No newline at end of file\n");						
				}
			}
			//writing the second hunk
			if(hunk2.start == 0 && hunk2.end == -1)
				fprintf(fpReject, "--- 0 ----\n");
			else if(hunk2.start == hunk2.end)
				fprintf(fpReject, "--- %d ----\n", hunk2.start);
			else
				fprintf(fpReject, "--- %d,%d ----\n", hunk2.start, hunk2.end);
			if(!RFlag)
				lines = hunk2.lines;
			else
				lines = hunk1.lines;
			for(i = 0; i < lines; i++) {
				if(!RFlag)
					linePtr = fgetline(&hunk2.buffer, i);
				else
					linePtr = fgetline(&hunk1.buffer, i);
				fprintf(fpReject, "%c%s", '+', linePtr + 1);
				if(i == lines - 1) {
					if(linePtr[strlen(linePtr) - 1] != '\n')
						fprintf(fpReject, "\n\\ No newline at end of file\n");
				}
			}
		}
		hunkNum++;
		fdestroy(&hunk1.buffer);
		fdestroy(&hunk2.buffer);
	}
	//printing the lines from the readFile which are not mentioned in the patchFile if any
	while(currentline <= fileLen) {
		linePtr = fgetline(&fileBuffer, currentline - 1);
		fprintf(fpWrite, "%s", linePtr);
		currentline++;
	}
	if(failCount) {
		printf("%d out of %d hunk FAILED -- saving rejects to file %s.rej\n", failCount, hunkNum - 1, readFile);
		fclose(fpReject);
	}
	free(line);
	fdestroy(&fileBuffer);
}

/*The function below patches the file using the patch file having diff in context format*/
void cPatch(FILE *fpPatch, FILE *fpRead, FILE *fpWrite, char *readFile, char *timeStamp1, char *timeStamp2) {
	int i, hunkNum = 1, currentline = 1, fileLen, result, failCount = 0, rejectHeader = 0, patchCount = 0, shift;
	//currentline = actual line number being handled, in the readFile
	//patchCount = line number being handled, in patchFile
	//shift = number of lines to skip from the hunk(only from hunk's start) used for writing, in case of fuzz
	FILE *fpReject;//file pointer to reject file in any
	char *line, *linePtr;
	hunk hunk1, hunk2;
	fileList fileBuffer;
	finit(&fileBuffer);
	line = (char *)malloc(sizeof(char) * 2048);//max line size is 2048 chars
	if(timeStamp1)
		patchCount++;
	if(timeStamp2)
		patchCount++;
	//read the readFile completely
	while(fgets(line, 2048, fpRead))
		fappend(&fileBuffer, line);
	fileLen = flength(&fileBuffer);
	while(fgets(line, 2048, fpPatch)) {
		//checking if the first line read is 15 stars
		if(strcmp(line, "***************\n")) {
			fprintf(stderr, "patch: **** Only garbage was found in patch\n");
			exit(1);
		}
		finit(&(hunk1.buffer));
		finit(&(hunk2.buffer));
		//read the start and end of the first hunk
		readHunkHeader(fpPatch, &hunk1);
		patchCount = patchCount + 2;
	     	i = 0;
	     	//storing the hunk1 lines in the first hunk buff
		while(i < (hunk1.lines + 1) && fgets(line, 2048, fpPatch)) {
			if(line[1] == '-') {
				if(i != 0 && i != hunk1.lines) {
					fprintf(stderr, "patch: **** Premature '---' at line %d; check line numbers at line %d\n", patchCount + 1, patchCount - i);
					exit(1);
				}
				fseek(fpPatch, -strlen(line), SEEK_CUR);
				break;
			}
			else if(line[0] == '\\') {
				if(i != 0 && i != hunk1.lines) {
					fprintf(stderr, "patch: **** malformed patch at line %d: %s\n", patchCount + 1, line);
					exit(1);
				}
				linePtr = fpop(&(hunk1.buffer));
				linePtr[strlen(linePtr) - 1] = '\0';
				fappend(&(hunk1.buffer), linePtr);
				patchCount++;
				break;
			}
			else {
				fappend(&(hunk1.buffer), line);
				i++;
				patchCount++;
			}
		}
		if(i != 0 && i != hunk1.lines) {
			fprintf(stderr, "patch: **** unexpected end of file in patch\n");
			exit(1);
		}
		//read the start and the end of the second hunk
		readHunkHeader(fpPatch, &hunk2);
		patchCount++;
		i = 0;
		//storing the hunk2 lines in the second hunk buff
		while(i < (hunk2.lines + 1) && fgets(line, 2048, fpPatch)) {
			if(line[0] == '*') {
				if(i != 0 && i!= hunk2.lines) {
					fprintf(stderr, "patch: **** unexpected end of hunk at line %d\n", patchCount + 1);
					exit(1);
				}
				fseek(fpPatch, -strlen(line), SEEK_CUR);
				break;
			}
			else if(line[0] == '\\') {
				if(i != 0 && i != hunk2.lines) {
					fprintf(stderr, "patch: **** malformed patch at line %d: %s\n", patchCount + 1, line);
					exit(1);
				}
				linePtr = fpop(&(hunk2.buffer));
				linePtr[strlen(linePtr) - 1] = '\0';
				fappend(&(hunk2.buffer), linePtr);
				patchCount++;
				break;
			}
			else {
				fappend(&(hunk2.buffer), line);
				i++;
				patchCount++;
			}
		}
		if(i != 0 && i != hunk2.lines) {
			fprintf(stderr, "patch: **** unexpected end of file in patch\n");
			exit(1);
		}
		//if hunk2 is empty then enter the lines from hunk1 to hunk2 which are common
		if(flength(&(hunk2.buffer)) == 0) {
			for(i = 0; i < hunk1.lines; i++) {
				linePtr = fgetline(&(hunk1.buffer), i);
				if(linePtr[0] == ' ')
					fappend(&(hunk2.buffer), linePtr);
			}
		}
		//if hunk1 is empty then enter the lines from hunk2 to hunk1 which are common
		if(flength(&(hunk1.buffer)) == 0) {
			for(i = 0; i < hunk2.lines; i++) {
				linePtr = fgetline(&(hunk2.buffer), i);
				if(linePtr[0] == ' ')
					fappend(&(hunk1.buffer), linePtr);
			}
		}
		hunk1.hunkNum = hunk2.hunkNum = hunkNum;
		if(!RFlag)//in normal patch the hunk1 is searched in the original file
			result = findOffsetAndFuzz(fileBuffer, &hunk1, &hunk2, currentline, &shift);
		else//in reverse patch the hunk2 is searched in the original file
			result = findOffsetAndFuzz(fileBuffer, &hunk2, &hunk1, currentline, &shift);
		if(result == PASS) {
			if(!RFlag) {
				/*printing the lines from the file which are not printed in diff
				  here the main comparison is the currentline < start, the second
				  comparison is made to ensure that we get a line which is surely
				  in the fileBuffer*/
				while(currentline < hunk1.start && currentline <= fileLen) {
					linePtr = fgetline(&fileBuffer, currentline - 1);
					fprintf(fpWrite, "%s", linePtr);
					currentline++;
				}
				//printing the lines from the write hunk, here hunk2
				for(i = shift; i < hunk2.lines; i++) {
					linePtr = fgetline(&(hunk2.buffer), i);
					fprintf(fpWrite, "%s", linePtr + 2);
				}
				currentline = currentline + hunk1.lines;
			}
			else {
				//printing the lines from the file which belong to the LCS
				while(currentline < hunk2.start && currentline <= fileLen) {
					linePtr = fgetline(&fileBuffer, currentline - 1);
					fprintf(fpWrite, "%s", linePtr);
					currentline++;
				}
				//printing the lines from the write hunk, here hunk1
				for(i = shift; i < hunk1.lines; i++) {
					linePtr = fgetline(&(hunk1.buffer), i);
					fprintf(fpWrite, "%s", linePtr + 2);
				}
				currentline = currentline + hunk2.lines;
			}
		}
		else {
			failCount++;
			if(!rejectHeader) {//printing the timestamp(s) only once and then setting rejectHeader to 1
				strcpy(line, readFile);
				strcat(line, ".rej");
				fpReject = fopen(line, "w+");
				if(fpReject == NULL) {
					fprintf(stderr, "patch: **** Can't create reject file\n");
					exit(errno);
				}
				if(timeStamp1)
					fprintf(fpReject, "%s", timeStamp1);
				if(timeStamp2)
					fprintf(fpReject, "%s", timeStamp2);
				rejectHeader = 1;
			}
			fprintf(fpReject, "***************\n");
			//writing the first hunk
			if(hunk1.start == 0 && hunk1.end == -1)
				fprintf(fpReject, "*** 0 ****\n");
			else if(hunk1.start == hunk1.end)
				fprintf(fpReject, "*** %d ****\n", hunk1.start);
			else
				fprintf(fpReject, "*** %d,%d ****\n", hunk1.start, hunk1.end);
			for(i = 0; i < hunk1.lines; i++) {
				linePtr = fgetline(&hunk1.buffer, i);
				fprintf(fpReject, "%s", linePtr);
				if(i == hunk1.lines - 1) {
					if(linePtr[strlen(linePtr) - 1] != '\n')
						fprintf(fpReject, "\n\\ No newline at end of file\n");						
				}
			}
			//writing the second hunk
			if(hunk2.start == 0 && hunk2.end == -1)
				fprintf(fpReject, "--- 0 ----\n");
			else if(hunk2.start == hunk2.end)
				fprintf(fpReject, "--- %d ----\n", hunk2.start);
			else
				fprintf(fpReject, "--- %d,%d ----\n", hunk2.start, hunk2.end);
			for(i = 0; i < hunk2.lines; i++) {
				linePtr = fgetline(&hunk2.buffer, i);
				fprintf(fpReject, "%s", linePtr);
				if(i == hunk2.lines - 1) {
					if(linePtr[strlen(linePtr) - 1] != '\n')
						fprintf(fpReject, "\n\\ No newline at end of file\n");
				}
			}
		}
		hunkNum++;
		fdestroy(&(hunk1.buffer));
		fdestroy(&(hunk2.buffer));
	}
	//printing the lines from currentline to the end of the file if any
	while(currentline <= fileLen) {
		linePtr = fgetline(&fileBuffer, currentline - 1);
		fprintf(fpWrite, "%s", linePtr);
		currentline++;
	}
	if(failCount) {
		printf("%d out of %d hunk FAILED -- saving rejects to file %s.rej\n", failCount, hunkNum - 1, readFile);
		fclose(fpReject);
	}
	free(line);
}

int main(int argc, char *argv[]) {
	FILE *fpPatch, *fpRead, *fpWrite;
	int argumentCount = 0, option, cPatchFlag = 1;
	extern int optind;
	char *patchFile, *readFile, *tempFile, *timeStamp1, *timeStamp2;
	patchFile = (char *)malloc(sizeof(char) * 1024);//To store the patch file name
	readFile = (char *)malloc(sizeof(char) * 1024);//To store the name of the first file
	tempFile = (char *)malloc(sizeof(char) * 1024);//To store the name of the tmp file
	timeStamp1 = (char *)malloc(sizeof(char) * 1024);//To store the timestamp of first file
	timeStamp2 = (char *)malloc(sizeof(char) * 1024);//To store the timestamp of second file
	//check for the reverse patch option from the command line
	while((option = getopt(argc, argv, "R")) != -1) {
		switch(option) {
			case 'R':
			  	RFlag = 1;
				break;
		 	default:
			  	break;
		}
	}
	//store the names of the file to be patched and the patch file in the respective strings
	while(argv[optind]) {
		if(argumentCount == 0)
			strcpy(readFile, argv[optind]);
		else if(argumentCount == 1)
			strcpy(patchFile, argv[optind]);
		optind++;
		argumentCount++;
		if(argumentCount > 2)
			break;
	}
	if(argumentCount > 2) {
		fprintf(stderr, "patch: **** Extra operand %s\n", argv[optind - 1]);
		return 0;
	}
	if(argumentCount < 2) {
		fprintf(stderr, "patch: **** Missing operand after %s\n", argv[argc - 1]);
		return 0;
	}
	//opening the patch file
	fpPatch = fopen(patchFile, "r");
	if(fpPatch == NULL) {
		fprintf(stderr, "patch: **** Can't open patch file %s : ", patchFile);
		perror("");
		exit(errno);
	}
	//read the first two lines from the patch file and check if there are no lines if yes then quit
	if(!fgets(timeStamp1, 1024, fpPatch) || !fgets(timeStamp2, 1024, fpPatch))
		return 0;
	/*The patch file may have normal diff or context diff format. To check it has normal diff we have to check
	  if it has either a, d, c in the first line as well as no *** and --- in the line*/
	if((strstr(timeStamp1, "a") || strstr(timeStamp1, "d") || strstr(timeStamp1, "c")) && !strstr(timeStamp1, "***") && !strstr(timeStamp1, "---")) {
		cPatchFlag = 0;
		fseek(fpPatch, -(strlen(timeStamp1) + strlen(timeStamp2)), SEEK_CUR);
		if(strstr(timeStamp1, "<") || strstr(timeStamp1, ">")) {//if the first line is not the line numbers
			fprintf(stderr, "patch: **** Only garbage was found in patch\n");
			return 0;
		}
	}
	//checking if the timestamps are valid in case if they are not mentioned in the patch
	if(cPatchFlag) {
		if(!strcmp(timeStamp1, "***************\n")) {
			fseek(fpPatch, -(strlen(timeStamp1) + strlen(timeStamp2)), SEEK_CUR);
			timeStamp1 = timeStamp2 = NULL;
		}
		else if(!strcmp(timeStamp2, "***************\n")) {
			fseek(fpPatch, -(strlen(timeStamp2)), SEEK_CUR);
			if(timeStamp1[0] == '-') {
				strcpy(timeStamp2, timeStamp1);
				timeStamp1 = NULL;
			}
			else if(timeStamp1[0] == '*')
				timeStamp2 = NULL;
		}
	}
	//opening the file to be patched
	fpRead = fopen(readFile, "r");
	if(fpRead == NULL) {
		fprintf(stderr, "patch: **** Can't open file %s : ", readFile);
		perror("");
		exit(errno);
	}
	strcpy(tempFile, readFile);
	strcat(tempFile, ".tmp");
	//opening the temp file to write the changes i.e. the second file
	fpWrite = fopen(tempFile, "w+");
	if(fpWrite == NULL ) {
		fprintf(stderr, "patch: **** Can't create temporary file\n");
		exit(errno);
	}
	printf("patching file %s\n", readFile);
	//calling the patch function depeding on the type of diff in the patch file
	if(cPatchFlag)
		cPatch(fpPatch, fpRead, fpWrite, readFile, timeStamp1, timeStamp2);
	else
		patch(fpPatch, fpRead, fpWrite, readFile);
	//closing the files and renaming the tempFile to readFile
	fclose(fpPatch);
	fclose(fpRead);
	fclose(fpWrite);
	remove(readFile);
      rename(tempFile, readFile);
	free(patchFile);
	free(readFile);
	free(tempFile);
	free(timeStamp1);
	free(timeStamp2);
      return 0;
}
