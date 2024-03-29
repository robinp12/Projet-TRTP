#ifndef __LINKEDLIST_H_
#define __LINKEDLIST_H_

#include "../packet_implem.h"

typedef struct node {
    pkt_t* pkt;
    struct node* next;
} node_t;

typedef struct linkedList {
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
* Add a packet at the end of the linked lsit
*/
int linkedList_add_pkt(linkedList_t* linkedList, pkt_t* pkt);


/*
* Remove the node at the beginning
*/
int linkedList_remove(linkedList_t* linkedList);

/*
* Remove the node at the end
*/
int linkedList_remove_end(linkedList_t* linkedList);

/*
* Remove the given node
* @args node : a node in the linked list, who is not head or tail
*/
void linkedList_remove_middle(linkedList_t* linkedList, node_t* previous, node_t* current);


#endif