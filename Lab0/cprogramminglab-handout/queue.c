/*
 * Code for basic C skills diagnostic.
 * Developed for courses 15-213/18-213/15-513 by R. E. Bryant, 2017
 * Modified to store strings, 2018
 */

 /*
  * This program implements a queue supporting both FIFO and LIFO
  * operations.
  *
  * It uses a singly-linked list to represent the set of queue elements
  */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "queue.h"

  /*
	Create empty queue.
	Return NULL if could not allocate space.
  */
queue_t* q_new()
{
	queue_t* q = malloc(sizeof(queue_t));
	if (q == NULL) {
		return NULL;
	}
	q->head = NULL;
	q->tail = NULL;
	q->size = 0;
	return q;
}

/* Free all storage used by queue */
void q_free(queue_t* q)
{
	if (!q) {
		return;
	}
	/* How about freeing the list elements and the strings? */
	list_ele_t* cur = q->head;
	list_ele_t* post = q->head;
	while (cur != NULL) {
		post = cur->next;
		free(cur->value);
		free(cur);
		cur = post;
	}
	/* Free queue structure */
	free(q);
}

/*
  Attempt to insert element at head of queue.
  Return true if successful.
  Return false if q is NULL or could not allocate space.
  Argument s points to the string to be stored.
  The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_head(queue_t* q, char* s)
{
	if (!q) {
		return false;
	}
	list_ele_t* newh;
	newh = malloc(sizeof(list_ele_t));
	if (!newh) {
		return false;
	}
	/* Don't forget to allocate space for the string and copy it */
	/* What if either call to malloc returns NULL? */
	int len = 0;
	char* sCopy = s;
	while (*sCopy) {
		++len;
		++sCopy;
	}
	newh->value = malloc((len + 1) * sizeof(char));
	if (newh->value == NULL) {
		free(newh);  // be careful of memory leak
		return false;
	}
	for (int i = 0; i <= len; ++i) {
		newh->value[i] = s[i];
	}
	newh->next = q->head;
	q->head = newh;
	++q->size;
	if (q->size == 1) {
		q->tail = newh;
	}
	return true;
}


/*
  Attempt to insert element at tail of queue.
  Return true if successful.
  Return false if q is NULL or could not allocate space.
  Argument s points to the string to be stored.
  The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_tail(queue_t* q, char* s)
{
	/* You need to write the complete code for this function */
	/* Remember: It should operate in O(1) time */
	if (!q) {
		return false;
	}
	list_ele_t* newh = malloc(sizeof(list_ele_t));
	if (!newh) {
		return false;
	}

	int len = 0;
	char* sCopy = s;
	while (*sCopy) {
		++len;
		++sCopy;
	}
	newh->value = malloc((len + 1) * sizeof(char));
	if (newh->value == NULL) {
		free(newh);
		return false;
	}
	for (int i = 0; i < len; ++i) {
		newh->value[i] = s[i];
	}
	newh->value[len] = '\0';

	q->tail->next = newh;
	newh->next = NULL;
	q->tail = newh;
	++q->size;
	return true;
}

/*
  Attempt to remove element from head of queue.
  Return true if successful.
  Return false if queue is NULL or empty.
  If sp is non-NULL and an element is removed, copy the removed string to *sp
  (up to a maximum of bufsize-1 characters, plus a null terminator.)
  The space used by the list element and the string should be freed.
*/
bool q_remove_head(queue_t* q, char* sp, size_t bufsize)
{
	/* You need to fix up this code. */
	if (!q || !q->head || !sp) {
		return false;
	}

	char* tmp = q->head->value;
	int len = 0;
	while (*tmp) {
		++len;
		++tmp;
	}

	tmp = q->head->value;
	if (len >= bufsize) {
		for (int i = 0; i < bufsize - 1; ++i) {
			sp[i] = tmp[i];
		}
		sp[bufsize - 1] = '\0';
	}
	else {
		for (int i = 0; i < len; ++i) {
			sp[i] = tmp[i];
		}
		sp[len] = '\0';
		--q->size;
	}

	list_ele_t* rm = q->head;
	q->head = q->head->next;
	free(tmp);
	free(rm);
	return true;
}

/*
  Return number of elements in queue.
  Return 0 if q is NULL or empty
 */
int q_size(queue_t* q)
{
	/* You need to write the code for this function */
	/* Remember: It should operate in O(1) time */
	if (!q || !q->head) {
		return 0;
	}
	return q->size;
}

/*
  Reverse elements in queue
  No effect if q is NULL or empty
  This function should not allocate or free any list elements
  (e.g., by calling q_insert_head, q_insert_tail, or q_remove_head).
  It should rearrange the existing ones.
 */
void q_reverse(queue_t* q)
{
	if (!q || !q->head) {
		return;
	}
	/* You need to write the code for this function */
	list_ele_t* cur = q->head;
	while (cur->next) {
		list_ele_t* post = cur->next;
		cur->next = post->next;
		post->next = q->head;
		q->head = post;
	}
	q->tail = cur;
}
