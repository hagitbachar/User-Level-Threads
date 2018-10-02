//
// Created by yael.chaimov on 5/1/18.
//

#include "Thread.h"

typedef unsigned long address_t;



/**
 * constructor
 * @param id - the id of the new thread
 */
Thread::Thread(int id, void (*f)(void))
{
    sp = (address_t)(this->getStack()) + STACK_SIZE - sizeof(address_t);
    pc = (address_t)f;
    this->id = id;
    this->is_block_by_sync = false;
    this->state = Ready;
    quantums = 0;
    is_block = false;
}