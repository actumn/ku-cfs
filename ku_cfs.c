#include <stdio.h> 
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include "ku_cfs.h"

double weight[] = {
    (1 / 1.25) * (1 / 1.25), 
    1 / 1.25, 
    1,
    1.25,
    1.25 * 1.25
};

__pid_t fork_and_exec(const char ch) {
    __pid_t pid = fork();
    if (pid == 0) {
        // child
        char arg1[2] = "\0";
        arg1[0] = ch;
        execl("ku_app", "ku_app", arg1, (char*)NULL);

        // cannot be reached here.
        exit(0);
    }

    return pid;
}


int timer_count = 0;
task_struct current_task;

void timer_handler(int signal) {
    // printf("SIGARM received timeslice: %d\n", timer_count);
    timer_count += 1;

    // STOP current task
    kill(current_task.pid, SIGSTOP);
    // insert to process queue
    rbt_insert(
        current_task.p_runtime + weight[current_task.nice], 
        current_task.pid, 
        current_task.nice);

    // pop from process queue
    rbt_pop_first(&current_task);
    // CONTINUE current task
    kill(current_task.pid, SIGCONT);
}

int main(int argc, char* argv[]) {
	if (argc != 7) {
		// fprintf(stderr, "usage: %s <n1 <n2> <n3> <n4> <n5> <ts>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
    int n1 = atoi(argv[1]);
    int n2 = atoi(argv[2]);
    int n3 = atoi(argv[3]);
    int n4 = atoi(argv[4]);
    int n5 = atoi(argv[5]);
    int ts = atoi(argv[6]);

    /* Prepare process using fork and exec */
    char ch = 'A';
    for (int i = 0; i < n1; i++) {
        __pid_t pid = fork_and_exec(ch++);
        rbt_insert(0.0, pid, 0);
    }
    for (int i = 0; i < n2; i++) {
        __pid_t pid = fork_and_exec(ch++);
        rbt_insert(0.0, pid, 1);
    }
    for (int i = 0; i < n3; i++) {
        __pid_t pid = fork_and_exec(ch++);
        rbt_insert(0.0, pid, 2);
    }
    for (int i = 0; i < n4; i++) {
        __pid_t pid = fork_and_exec(ch++);
        rbt_insert(0.0, pid, 3);
    }
    for (int i = 0; i < n5; i++) {
        __pid_t pid = fork_and_exec(ch++);
        rbt_insert(0.0, pid, 4);
    }
    // Make sure that all applications are successfully initalized before scheduling.
    // A simple and dumb solution. Just for the assignment.
    sleep(5);

    /* Configuring timer for scheduling. */
    struct itimerval timer;
    signal(SIGALRM, timer_handler);
    timer.it_value.tv_sec = 1;  // Configure the timer to expire after 1 sec
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 1; // and every 1 sec after that. 
    timer.it_interval.tv_usec = 0;


    /* Start scheduling */
    if (rbt_pop_first(&current_task)) {
        // No process error.
        return 1;
    }
    kill(current_task.pid, SIGCONT);
    setitimer(ITIMER_REAL, &timer, NULL); // start scheduling timer
    while(ts > timer_count) {
        pause(); // wait for timer signals
    }
    kill(current_task.pid, SIGSTOP);

    /* Scheduling finished */
    while(!rbt_pop_first(&current_task)) {
        kill(current_task.pid, SIGKILL);
    }

    return 0;
}



rbNode* rbt_create_node(const double p_runtime, const __pid_t pid, const int nice) {
    rbNode* newNode;

    newNode = (rbNode*) malloc(sizeof(rbNode));
    newNode->task.p_runtime = p_runtime;
    newNode->task.pid = pid;
    newNode->task.nice = nice;
    newNode->color = RED;
    newNode->link[0] = newNode->link[1] = NULL;

    return newNode;
}

void rbt_insert(const double p_runtime, const __pid_t pid, const int nice) {
    rbNode *stack[98], *ptr, *newNode, *xPtr, *yPtr;
    int direction[98], ht, index;
    if (!root) {
        root = rbt_create_node(p_runtime, pid, nice);
        return;
    }
    
    ptr = root;
    stack[0] = root;
    direction[0] = 0;
    ht = 1;

    while (ptr != NULL) {
        stack[ht] = ptr;
        
        index = ptr->task.p_runtime > p_runtime ? 0 : 1;
        // printf("ptr->p_runtime: %.2f, p_runtime: %.2f, index: %d\n", ptr->p_runtime, p_runtime, index);
        ptr = ptr->link[index];
        direction[ht] = index;
        ht += 1;
    }

    stack[ht - 1]->link[index] = newNode = rbt_create_node(p_runtime, pid, nice);
    while((ht >= 3) && (stack[ht - 1]->color == RED)) {
        if (direction[ht - 2] == 0) {
            // Get uncle node.
            yPtr = stack[ht - 2]->link[1]; 
            
            // If uncle node is red, recoloring.
            if (yPtr != NULL && yPtr->color == RED) {  
                stack[ht - 2]->color = RED;
                stack[ht - 1]->color = yPtr->color = BLACK;
                ht = ht - 2;
            }
            // If uncle node is black, restructuring.
            else {
                if (direction[ht - 1] == 0) {
                    yPtr = stack[ht - 1];
                } else {
                    xPtr = stack[ht - 1];
                    yPtr = xPtr->link[1];
                    xPtr->link[1] = yPtr->link[0];
                    yPtr->link[0] = xPtr;
                    stack[ht - 2]->link[0] = yPtr;
                }
                xPtr = stack[ht - 2];
                xPtr->color = RED;
                yPtr->color = BLACK;
                xPtr->link[0] = yPtr -> link[1];
                yPtr->link[1] = xPtr;

                if (xPtr == root) {
                    root = yPtr;
                } else {
                    stack[ht - 3]->link[direction[ht - 3]] = yPtr;
                }
                break;
            }
        }
        else {
            yPtr = stack[ht - 2]->link[0]; // get uncle node.
            if ((yPtr != NULL) && (yPtr->color == RED)) {
                stack[ht - 2]->color = RED;
                stack[ht - 1]->color = yPtr->color = BLACK;
                ht = ht - 2;
            } 
            else {
                if (direction[ht - 1] == 1) {
                    yPtr = stack[ht - 1];
                } else {
                    xPtr = stack[ht - 1];
                    yPtr = xPtr->link[0];
                    xPtr->link[0] = yPtr->link[1];
                    yPtr->link[1] = xPtr;
                    stack[ht - 2]->link[1] = yPtr;
                }
                xPtr = stack[ht - 2];
                yPtr->color = BLACK;
                xPtr->color = RED;
                xPtr->link[1] = yPtr->link[0];
                yPtr->link[0] = xPtr;

                if (xPtr == root) {
                    root = yPtr;
                } else {
                    stack[ht - 3]->link[direction[ht - 3]] = yPtr;
                }
                break;
            }
        }
    }
    root->color = BLACK;
}

int rbt_pop_first(task_struct *task) {
    rbNode *stack[98], *ptr, *xPtr, *yPtr;
    rbNode *pPtr, *qPtr, *rPtr;
    int direction[98], ht;
    enum nodeColor color;

    if (!root) {
        // printf("Tree not available\n");
        task->p_runtime = 0;
        task->pid = 0;
        task->nice = 0;
        return -1;
    }

    ptr = root;
    ht = 0;
    while (ptr->link[0] != NULL) {
        stack[ht] = ptr;

        // Find the left most node
        direction[ht] = 0;
        ptr = ptr->link[0];
        ht += 1;
    }
    task->p_runtime = ptr->task.p_runtime;
    task->pid = ptr->task.pid;
    task->nice = ptr->task.nice;


    // If right child empty.
    if (ptr->link[1] == NULL) {
        // Only root is left.
        if ((ptr == root) && (ptr->link[0] == NULL)) {
            free(ptr);
            root = NULL;
        }
        // Remove root.
        else if (ptr == root) {
            root = ptr->link[0];
            free(ptr);
        } else {
            stack[ht - 1]->link[direction[ht - 1]] = ptr->link[0];
        }
    }
    // The node has right child.
    // Restructuring based on the right child.
    else {
        xPtr = ptr->link[1];
        // Right child has no left child.
        // Inherit left child, color
        if (xPtr->link[0] == NULL) {
            xPtr->link[0] = ptr->link[0];
            color = xPtr->color;
            xPtr->color = ptr->color;
            ptr->color = color;

            if (ptr == root) {
                root = xPtr;
            } else {
                stack[ht - 1]->link[direction[ht - 1]] = xPtr;
            }

            direction[ht] = 1;
            stack[ht] = xPtr;
            ht += 1;
        }
        // The node has two children. 
        else {
            int i = ht;
            ht += 1;
            while (1) {
                direction[ht] = 0;
                stack[ht] = xPtr;
                ht += 1;
                yPtr = xPtr->link[0];
                if (!yPtr->link[0])
                    break;
                xPtr = yPtr;
            }

            direction[i] = 1;
            stack[i] = yPtr;
            if (i > 0)
                stack[i - 1]->link[direction[i - 1]] = yPtr;
            
            yPtr->link[0] = ptr->link[0];

            xPtr->link[0] = yPtr->link[1];
            yPtr->link[1] = ptr->link[1];

            if (ptr == root) {
                root = yPtr;
            }

            color = yPtr->color;
            yPtr->color = ptr->color;
            ptr->color = color;
        }
    }

    if (ht < 1)
        return 0;
    if (ptr->color == BLACK) {
        while (1) {
            pPtr = stack[ht - 1]->link[direction[ht - 1]];
            if (pPtr && pPtr->color == RED) {
                pPtr->color = BLACK;
                break;
            }

            if (ht < 2)
                break;
            
            if (direction[ht - 2] == 0) {
                rPtr = stack[ht - 1]->link[1];

                if (!rPtr)
                    break;

                if (rPtr->color == RED) {
                    stack[ht - 1]->color = RED;
                    rPtr->color =BLACK;
                    stack[ht - 1]->link[1] = rPtr->link[0];
                    rPtr->link[0] = stack[ht - 1];

                    if (stack[ht - 1] == root) {
                        root = rPtr;
                    } else {
                        stack[ht - 2]->link[direction[ht - 2]] = rPtr;
                    }
                    direction[ht] = 0;
                    stack[ht] = stack[ht - 1];
                    stack[ht - 1] = rPtr;
                    ht++;

                    rPtr = stack[ht - 1]->link[1];
                }

                if ((!rPtr->link[0] || rPtr->link[0]->color == BLACK) &&
                    (!rPtr->link[1] || rPtr->link[1]->color == BLACK)) {
                    rPtr->color = RED;
                } else {
                    if (!rPtr->link[1] || rPtr->link[1]->color == BLACK) {
                        qPtr = rPtr->link[0];
                        rPtr->color = RED;
                        qPtr->color = BLACK;
                        rPtr->link[0] = qPtr->link[1];
                        qPtr->link[1] = rPtr;
                        rPtr = stack[ht - 1]->link[1] = qPtr;
                    }

                    rPtr->color = stack[ht - 1]->color;
                    stack[ht - 1]->color = BLACK;
                    rPtr->link[1]->color = BLACK;
                    stack[ht - 1]->link[1] = rPtr->link[0];
                    rPtr->link[0] = stack[ht - 1];
                    if (stack[ht - 1] == root) {
                        root = rPtr;
                    } else {
                        stack[ht - 2]->link[direction[ht - 2]] = rPtr;
                    }
                    break;
                }
            } else {
                rPtr = stack[ht - 1]->link[0];
                if (!rPtr)
                    break;
                
                if (rPtr->color == RED) {
                    stack[ht - 1]->color == RED;
                    rPtr->color = BLACK;
                    stack[ht - 1]->link[0] = rPtr->link[1];
                    rPtr->link[1] = stack[ht - 1];

                    if (stack[ht - 1] == root) {
                        root = rPtr;
                    } else {
                        stack[ht - 2]->link[direction[ht - 2]] = rPtr;
                    }
                    direction[ht] = 1;
                    stack[ht] = stack[ht - 1];
                    stack[ht - 1] = rPtr;
                    ht++;

                    rPtr = stack[ht - 1]->link[0];
                }

                if ((!rPtr->link[0] || rPtr->link[0]->color == BLACK) &&
                    (!rPtr->link[1] || rPtr->link[1]->color == BLACK)) {
                    rPtr->color = RED;
                } else {
                    if (!rPtr->link[0] || rPtr->link[0]->color == BLACK) {
                        qPtr = rPtr->link[1];
                        rPtr->color = RED;
                        qPtr->color = BLACK;
                        rPtr->link[1] = qPtr->link[0];
                        qPtr->link[0] = rPtr;
                        rPtr = stack[ht - 1]->link[0] = qPtr;
                    }

                    rPtr->color = stack[ht - 1]->color;
                    stack[ht - 1]->color = BLACK;
                    rPtr->link[0]->color = BLACK;
                    stack[ht - 1]->link[0] = rPtr->link[1];
                    rPtr->link[1] = stack[ht - 1];

                    if (stack[ht - 1] == root) {
                        root = rPtr;
                    } else {
                        stack[ht - 2]->link[direction[ht - 2]] = rPtr;
                    }
                    break;
                }
            }
            ht -= 1;
        }
    }

    return 0;
}


