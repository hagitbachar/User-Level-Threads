# User-Level-Threads
implementation of user-level threads library, implementation of the Round-Robin scheduling algorithm to manage the threads in the library.

Initially, a program is comprised of the default main thread, whose ID is 0. All other threads will be explicitly
created. Each existing thread has a unique thread ID, which is a non-negative integer. The ID given to a
new thread must be the smallest non-negative integer not already taken by an existing thread.
At any given time during the running of the user's program, each of the threads in the program is in one of
the states: RUNNING, READY, BLOCKED,  Transitions from state to state occur as a result of calling
one of the library functions, or from elapsing of time.
