#include <sys/types.h>

enum nodeColor { RED, BLACK };
typedef struct {
    double p_runtime; 
    __pid_t pid; 
    int nice;
} task_struct;

typedef struct rbNode{
    int color;
    task_struct task;
    struct rbNode *link[2];
} rbNode;
rbNode* rbt_create_node(double p_runtime, __pid_t pid, int nice);
void rbt_insert(double p_runtime, __pid_t pid, int nice);
int rbt_pop_first(task_struct *task);
rbNode* root = NULL;

__pid_t fork_and_exec(char ch);