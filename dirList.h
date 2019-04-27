typedef struct dirUnit {
	char name[1024];
	char path[1024];
	int type;
} dirUnit;

typedef struct dirList {
	dirUnit *arr;
	int i, max;
} dirList;

void dinit(dirList *);
void dappend(dirList *, dirUnit);
dirUnit dgetdir(dirList *, int);
void dsort(dirList *);
int dlength(dirList *);
void ddestroy(dirList *);
