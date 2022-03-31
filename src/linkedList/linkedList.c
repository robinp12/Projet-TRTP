#include <stdlib.h>


#include "linkedList.h"
#include "../log.h"
#include "../packet_implem.h"

/*
* Create a new linked list and allocate memory
*/
linkedList_t* linkedList_create(void)
{
    linkedList_t* linkedList = malloc(sizeof(linkedList_t));
    linkedList->size = 0;
    return linkedList;
}

/*
* Delete linked list and free its nodes
*/
int linkedList_del(linkedList_t* linkedList)
{
    if (linkedList->size == 0){
        free(linkedList);
        return 0;
    }
    if (linkedList->size == 1){
        pkt_del(linkedList->head->pkt);
        free(linkedList->head);
        free(linkedList);
        return 0;
    }
    node_t* current = linkedList->head;
    while (current->next != NULL)
    {
        pkt_del(current->pkt);
        node_t* odlNode = current;
        current = current->next;
        free(odlNode);
    }
    free(linkedList);
    return 0;
}

/*
* Add a node at the end of the linked list
*/
int linkedList_add(linkedList_t* linkedList, node_t* n)
{
    if (linkedList->size == 0){
        linkedList->head = n;
        linkedList->tail = n;
        linkedList->size = 1;
        return 0;
    }
    linkedList->tail->next = n;
    linkedList->tail = n;
    linkedList->size++;
    return 0;
}


/*
* Add a packet at the end of the linked lsit
*/
int linkedList_add_pkt(linkedList_t* linkedList, pkt_t* pkt)
{
    node_t* node = malloc(sizeof(node_t));
    node->pkt = pkt;
    return linkedList_add(linkedList, node);
}


/*
* Remove the node at the beginning
*/
int linkedList_remove(linkedList_t* linkedList)
{
    if (linkedList->size == 0){
        return -1;
    }
    if (linkedList->size == 1){
        pkt_del(linkedList->head->pkt);
        free(linkedList->head);
        linkedList->size = 0;
        return 0;
    }
    pkt_del(linkedList->head->pkt);
    node_t* old = linkedList->head;
    linkedList->head = linkedList->head->next;
    linkedList->size--;
    free(old);
    return 0;
}
