//
// Created by yael.chaimov on 5/1/18.
//

#ifndef OS2_THREADS_H
#define OS2_THREADS_H
#define STACK_SIZE 4096 /* stack size per thread (in bytes) */

#include <vector>
#include <setjmp.h>
#include <esd.h>

/**
 * the thread class
 */
class Thread {

private:
    int id, quantums;
    bool is_block_by_sync;
    char stack[STACK_SIZE];
    bool is_block;
    typedef unsigned long address_t;
    address_t sp, pc;


public:

    enum State {Running, Block, Ready};
    State state;
    std::vector<int> dependent;
    sigjmp_buf env;
    /**
     * default constructor
     */
    Thread();
    /**
     * constructor
     * @param id - the id of the new thread
     * @param f - the thread function
     */
    Thread(int id, void (*f)(void));
    /**
     * get id of the thread
     * @return id
     */
    int getId(){ return id; }
    /**
     * get the pointer to the stack of the thread
     * @return pointer to the stack
     */
    char* getStack(){ return stack;}

    /**
     * get the state of the thread
     * @return state
     */
    State getState(){ return state; }
    /**
     * set the state of the thread
     * @param newState - the new state
     */
    void setState(State newState){this->state = newState;}

    /**
     * add the thread quantum by one
     */
    void addQuantums(){quantums++;}
    /**
     * get quantum of the thread
     * @return quantum
     */
    int getQuantum(){ return quantums;}

    /**
     * get get_is_block_by_sync of the thread
     * @return true iff the the thread is block by the sync function
     */
    bool get_is_block_by_sync(){ return  is_block_by_sync;}
    /**
     * block the thread by sync
     */
    void block_by_sync(){this->is_block_by_sync = true;}
    /**
     * block the thread by sync
     */
    void un_block_by_sync(){this->is_block_by_sync = false;}

    /**
     * get get_is_block of the thread
     * @return true iff the the thread is block
     */
    bool get_is_block(){ return  is_block;}
    /**
     * block the thread
     */
    void block(){this->is_block = true;}
    /**
     * un block the thread
     */
    void un_block(){this->is_block = false;}

    /**
     * add a thread to the dependency list
     * @param dependId - the thread id to add
     */
    void add_dependency(int dependId){dependent.push_back(dependId);}
    /**
     * get sp
     * @return the sp
     */
    address_t get_sp(){ return this->sp;}
    /**
     * get pc
     * @return the pc
     */
    address_t get_pc(){ return this->pc;}


};

#endif //OS2_THREADS_H
