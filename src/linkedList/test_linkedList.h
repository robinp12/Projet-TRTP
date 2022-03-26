#ifndef _TEST_LINKED_LIST_H__
#define _TEST_LINKED_LIST_H__

#include "linkedList.h"

#include <stdio.h>
#include <stdlib.h>
#include <CUnit/CUnit.h>
#include <CUnit/Automated.h>
#include <CUnit/Basic.h>
#include <CUnit/Console.h>
#include <CUnit/CUError.h>
#include <CUnit/CUnit_intl.h>
#include <CUnit/MyMem.h>
#include <CUnit/TestDB.h>
#include <CUnit/TestRun.h>
#include <CUnit/Util.h>
#include <string.h>

/*
* Create a dummy data pkt to fill in the linked list (and check for memory leaks)
* with a msg payload
*/
pkt_t* dummy_pkt(const char* msg);

void test_one_node(void);

int setup(void);

int teardown(void);

int main(void);

#endif