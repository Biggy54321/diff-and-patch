typedef struct fileList {
	char ***arr;
	int chunkIndex, lineIndex, chunkMax, lineMax;
	//chunkIndex points to the current chunk being handled
	//lineIndex points to the current line index being handled
	//chunkMax indicates the max number of chunks the structure can handle
	//lineMax indicates the max number of lines in one chunk
} fileList;

void finit(fileList *);
void fappend(fileList *, char *);
char *fgetline(fileList *, int);
char *fpop(fileList *);
int flength(fileList *);
void fdestroy(fileList *);
