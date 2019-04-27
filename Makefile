#diff
all: project1 project2
project1: diff.o fileList.o pairStack.o dirList.o
	cc -Wall diff.o fileList.o pairStack.o dirList.o -o project1
diff.o: diff.c fileList.h pairStack.h dirList.h
	cc -c -Wall diff.c
fileList.o: fileList.c fileList.h
	cc -c -Wall fileList.c
pairStack.o: pairStack.c pairStack.h
	cc -c -Wall pairStack.c
dirList.o: dirList.c dirList.h
	cc -c -Wall dirList.c
#patch
project2: patch.o fileList.o
	cc -Wall patch.o fileList.o -o project2
patch.o: patch.c fileList.h
	cc -c -Wall patch.c
