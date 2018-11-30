/*
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "dll.h"

static void check (uint32_t i, char *msg){
  if (i == 0){
    printf("%s\n", msg);
    abort();
  }
  return;
}

static void check_mem (void *ptr){
  if (ptr == NULL){
    printf("pointer is NULL\n");
    abort();
  }
  return;
}


List *List_create()
{
  List *ret = calloc(1, sizeof(List));
  bzero(ret, sizeof(List));
  return ret;
}

void List_destroy(List *list)
{
  LIST_FOREACH_LINKEDLIST(list, first, next, cur) {
    if(cur->prev) {
      free(cur->prev);
    }
  }

  free(list->last);
  free(list);
}


void List_clear(List *list)
{
  LIST_FOREACH_LINKEDLIST(list, first, next, cur) {
    free(cur->value);
  }
}


void List_clear_destroy(List *list)
{
  List_clear(list);
  List_destroy(list);
}


ListNode *List_push(List *list, void *value)
{
  ListNode *node = calloc(1, sizeof(ListNode));
  check_mem(node);

  node->value = value;

  if(list->last == NULL) {
    list->first = node;
    list->last = node;
  } else {
    list->last->next = node;
    node->prev = list->last;
    list->last = node;
  }

  list->count++;
  return node;
}

void *List_pop(List *list)
{
    ListNode *node = list->first;
    return (node != NULL) ? List_remove(list, node) : NULL;
}

void List_unshift(List *list, void *value)
{
    ListNode *node = calloc(1, sizeof(ListNode));
    check_mem(node);

    node->value = value;

    if(list->first == NULL) {
        list->first = node;
        list->last = node;
    } else {
        node->next = list->first;
        list->first->prev = node;
        list->first = node;
    }

    list->count++;

// error:
    return;
}

void *List_shift(List *list)
{
    ListNode *node = list->first;
    return node != NULL ? List_remove(list, node) : NULL;
}


// remove ListNode node from List list, free the ListNode, return the value in it.
void *List_remove(List *list, ListNode *node)
{
  void *result = NULL;
  check(list->first && list->last, "List is empty.");
//  check((uint32_t)node, "node can't be NULL");

  if(node == list->first && node == list->last) {
    list->first = NULL;
    list->last = NULL;
  } else if(node == list->first) {
    list->first = node->next;
    check(list->first != NULL, "Invalid list, somehow got a first that is NULL.");
    list->first->prev = NULL;
  } else if (node == list->last) {
    list->last = node->prev;
    check(list->last != NULL, "Invalid list, somehow got a next that is NULL.");
    list->last->next = NULL;
  } else {
    ListNode *after = node->next;
    ListNode *before = node->prev;
    after->prev = before;
    before->next = after;
  }

  list->count--;
  result = node->value;
  free(node);

  return result;
}


// remove ListNode node from List list, free the ListNode, return the value in it.
ListNode *List_remove_(List *list, ListNode *node){
  ListNode *result = NULL;

  //  check((list->first) && (list->last), "List is empty.");
  //  check(node, "node can't be NULL");

  if(node == list->first && node == list->last) {
    list->first = NULL;
    list->last = NULL;
  } else if(node == list->first) {
    list->first = node->next;
    check(list->first != NULL, "Invalid list, somehow got a first that is NULL.");
    list->first->prev = NULL;
    result = list->first;
  } else if (node == list->last) {
    list->last = node->prev;
    check(list->last != NULL, "Invalid list, somehow got a next that is NULL.");
    list->last->next = NULL;
    result = list->last;
  } else {
    ListNode *after = node->next;
    ListNode *before = node->prev;
    after->prev = before;
    before->next = after;
    result = before;
  }

  list->count--;
  //    result = node->value;
  free(node);
  return result;
}
