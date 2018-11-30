/*
 *
 */

#ifndef DLL_H_
#define DLL_H_

#include <strings.h>
//http://c.learncodethehardway.org/book/ex32.html
struct ListNode;

typedef struct ListNode {
	struct ListNode *next;
	struct ListNode *prev;
	void *value;
} ListNode;

typedef struct List {
	int count;
	ListNode *first;
	ListNode *last;
} List;

List *List_create();
void List_destroy(List *list);
void List_clear(List *list);
void List_clear_destroy(List *list);

#define List_count(A) ((A)->count)
#define List_first(A) ((A)->first != NULL ? (A)->first->value : NULL)
#define List_last(A) ((A)->last != NULL ? (A)->last->value : NULL)

ListNode *List_push(List *list, void *value);
void *List_pop(List *list);
void List_unshift(List *list, void *value);
void *List_shift(List *list);
void *List_remove(List *list, ListNode *node);
ListNode *List_remove_(List *list, ListNode *node);

#define LIST_FOREACH_LINKEDLIST(L, S, M, V) \
    ListNode *_node = NULL;\
    ListNode *V = NULL;\
    for(V = (_node = L->S); _node != NULL; V = (_node = _node->M))

#define LIST_FOREACH_(L, S, M, V) \
    for(V = L->S; V != NULL; V = V->M)

#define LIST_FOREACH_DOUBLYLINKEDLIST(LIST, FIRST, NEXT, NODE_CUR, _node_next) \
    ListNode *_node_next = NULL;\
    ListNode *NODE_CUR = NULL;\
    for(NODE_CUR = (_node_next = LIST->FIRST); _node_next != NULL; NODE_CUR = _node_next){\
      _node_next = _node_next->NEXT;

#endif /* DLL_H_ */
