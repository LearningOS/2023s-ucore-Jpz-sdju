#include "queue.h"
#include "defs.h"

void init_queue(struct queue *q)
{
	q->front = q->tail = 0;
	q->empty = 1;
}

void push_queue(struct queue *q, int value)
{
	if (!q->empty && q->front == q->tail) {
		panic("queue shouldn't be overflow");
	}
	q->empty = 0;
	q->data[q->tail] = value;
	q->tail = (q->tail + 1) % NPROC;
}

int pop_queue(struct queue *q)
{
	if (q->empty)
		return -1;
	int value = q->data[q->front];
	q->front = (q->front + 1) % NPROC;
	if (q->front == q->tail)
		q->empty = 1;
	return value;
}
extern struct  proc pool[NPROC];

int find_smallest_stride()
{

	int min = 0;
	for (int i = 0; i < NPROC; i++)
	{
		if(pool[i].state == RUNNABLE &&pool[i].current_stride <= pool[min].current_stride){
			min = i;
		}
	}
	pool[min].current_stride += pool[min].pass;
	// if(min == 0){
	// 	return -1;
	// }
	return min;
};
