
#include "linkedList.h"
#include "test_linkedList.h"
#include "../packet_implem.h"

pkt_t* dummy_pkt(const char* msg)
{
    pkt_t* pkt = pkt_new();
    pkt_set_type(pkt, PTYPE_DATA);
    pkt_set_payload(pkt, msg, strlen(msg));
    pkt_set_tr(pkt, 0);
    pkt_set_crc1(pkt, 1);
    pkt_set_crc2(pkt, 2);
    pkt_set_seqnum(pkt, 1);
    pkt_set_timestamp(pkt, 1);
    pkt_set_window(pkt, 1);
    
    return pkt;
}

void test_one_node(void)
{
    linkedList_t* list = linkedList_create();
    CU_ASSERT_EQUAL(list->size, 0);
    node_t* node = malloc(sizeof(node_t));

    const char msg[] = "First packet";
    node->pkt = dummy_pkt(msg);

    CU_ASSERT_EQUAL(linkedList_add(list, node), 0);
    CU_ASSERT_EQUAL(list->size, 1);
    const char* list_msg = pkt_get_payload(list->head->pkt);
    
    CU_ASSERT_EQUAL(strcmp(msg, list_msg), 0);

    CU_ASSERT_EQUAL(linkedList_del(list), 0);

}

void test_multiple_node(void)
{
    linkedList_t* list = linkedList_create();

    const char* msgs[5] = {"Packet 1\0", "Packet 2\0", "Packet 3\0", "Packet 4\0", "Packet 5\0"};

    for (int i = 0; i < 5; i++)
    {
        pkt_t* pkt = dummy_pkt(msgs[i]);
        CU_ASSERT_EQUAL(linkedList_add_pkt(list, pkt), 0);
        CU_ASSERT_EQUAL(list->size, i+1);

    }

    node_t* node = list->head;
    int i = 0;
    while (node != NULL){
        CU_ASSERT_EQUAL(strcmp(pkt_get_payload(node->pkt), msgs[i]), 0);
        node = node->next;
        i++;
    }

    i = 1;
    while (list->size > 0){
        CU_ASSERT_EQUAL(linkedList_remove(list), 0);
        if (list->size > 0){
            CU_ASSERT_EQUAL(strcmp(pkt_get_payload(list->head->pkt), msgs[i]), 0);
        }

        CU_ASSERT_EQUAL(list->size, 5-i);
        i++;
    }

    CU_ASSERT_EQUAL(linkedList_del(list), 0);
    
}

void test_remove_end(void)
{
    linkedList_t* list = linkedList_create();

    const char* msgs[5] = {"Packet 1\0", "Packet 2\0", "Packet 3\0", "Packet 4\0", "Packet 5\0"};

    for (int i = 0; i < 5; i++)
    {
        pkt_t* pkt = dummy_pkt(msgs[i]);
        linkedList_add_pkt(list, pkt);
    }


    int i = 3;
    while (list->size > 0){
        CU_ASSERT_EQUAL(linkedList_remove_end(list), 0);
        if (list->size > 0){
            CU_ASSERT_EQUAL(strcmp(pkt_get_payload(list->tail->pkt), msgs[i]), 0);
        }
        i--;
    }

    CU_ASSERT_EQUAL(linkedList_del(list), 0);
}


int setup(void) {
    return 0;
}

int teardown(void) {
    return 0;
}

int main()
{
    if (CUE_SUCCESS != CU_initialize_registry()){
        return CU_get_error();
    }

    CU_pSuite suite_basic = NULL;
    suite_basic = CU_add_suite("Linked list basic suit", setup, teardown);
    if (suite_basic == NULL){
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (CU_add_test(suite_basic, "One node", test_one_node) == NULL ||
        CU_add_test(suite_basic, "Multiples nodes", test_multiple_node) == NULL ||
        CU_add_test(suite_basic, "Remove end", test_remove_end) == NULL){
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_basic_run_tests();
    CU_basic_show_failures(CU_get_failure_list());

    CU_cleanup_registry();


    return 0;
}
