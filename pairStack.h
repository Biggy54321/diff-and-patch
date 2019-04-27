//pair stack using linked structures
typedef struct  pair{
	int i;
	int j;
} pair;

typedef struct pairNode {
	pair m;
	struct pairNode *next;
} pairNode;

typedef pairNode *pairStack;

void sinit(pairStack *);
void push(pairStack *, pair);
pair pop(pairStack *);
int isempty(pairStack *);
