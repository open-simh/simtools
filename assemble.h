#ifndef ASSEMBLE__H
#define ASSEMBLE__H


#include "object.h"
#include "stream2.h"


#define DOT (current_pc->value)        /* Handy reference to the current location */


int             assemble_stack(
    STACK *stack,
    TEXT_RLD *tr);

int             get_next_lsb(
    void);


#endif /* ASSEMBLE__H */
