
#include "test_linkedList.h"
#include "linkedList.h"
#include "../packet_implem.h"

pkt_t* dummy_pkt(const char* msg)
{
    pkt_t* pkt = pkt_new();
    pkt_set_type(pkt, PTYPE_DATA);
    pkt_set_payload(pkt, msg, strlen(msg));
    return pkt;
}

void test_one_node(void)
{
    linkedList_t* list = linkedList_create();
    CU_ASSERT_EQUAL(list->size, 0);
    node_t* node = malloc(sizeof(node_t));

    char* msg = "First packet";
    node->pkt = dummy_pkt(msg);

    CU_ASSERT_EQUAL(linkedList_add(list, node), 0);
    CU_ASSERT_EQUAL(list->size, 1);
    const char* list_msg = pkt_get_payload(list->head->pkt);
    
    CU_ASSERT_EQUAL(strcmp(msg, list_msg), 0);

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

    if (CU_add_test(suite_basic, "Test one node", test_one_node) == NULL){
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_basic_run_tests();
    CU_basic_show_failures(CU_get_failure_list());

    CU_cleanup_registry();

    return 0;
}
