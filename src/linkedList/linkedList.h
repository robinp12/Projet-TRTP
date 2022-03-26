#ifndef __LINKEDLIST_H_
#define __LINKEDLIST_H_

#include "../packet_implem.h"

typedef struct node {
    pkt_t* pkt;
    struct node* next;
} node_t;

typedef struct linked_list {
    node_t* head;
    node_t* tail;
    uint8_t size;
} linkedList_t;

/*
* Create a new linked list and allocate memory
*/
linkedList_t* linkedList_create(void);

/*
* Delete linked list and free its nodes
*/
int linkedList_del(linkedList_t* linkedList);

/*
* Add a node at the end of the linked list
*/
int linkedList_add(linkedList_t* linkedList, node_t* n);


/*
* Remove the node at the beginning
*/
int linkedList_remove(linkedList_t* linkedList);


#endif