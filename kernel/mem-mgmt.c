#include <mem-mgmt.h>
#include <memory.h>
#include <mpx/vm.h>
#include <sys_req.h>
#include <stdlib.h>
#include <string.h>

void* heapPtr = NULL;
mem_list* free_list;
mem_list* alloc_list;


void initialize_heap(size_t size) {
    heapPtr = kmalloc(size, 0, NULL);

    mcb* new_mcb = (mcb*)sys_alloc_mem(sizeof(mcb));

    new_mcb->Start_addr = (int)heapPtr + sizeof(mcb);
    new_mcb->Size = size - sizeof(mcb);
    new_mcb->NextPtr = NULL;
    new_mcb->PrevPtr = NULL;

    free_list = (mem_list*)sys_alloc_mem(sizeof(mem_list));

    alloc_list = (mem_list*)sys_alloc_mem(sizeof(mem_list));

    free_list->headPtr = new_mcb;
    free_list->tailPtr = new_mcb;  // Set tail pointer
    free_list->tailPtr->NextPtr = free_list->headPtr;  // circular list(to allow compaction from tail to head)
    free_list->count++;

    // Print head pointer address
    int heapPtr_int = (int)heapPtr;
    char* heapPtr_str = " ";
    itoa((int)heapPtr_int, heapPtr_str);
    sys_req(WRITE, COM1, heapPtr_str, 10);
}

void* allocate_memory(size_t size) {
    mcb* temp_mcb = free_list->headPtr;
    if (temp_mcb == NULL) {
        return NULL;
    }
    while (temp_mcb != NULL) {
        if (temp_mcb->Size >= size) {
            size_t remaining_size = temp_mcb->Size - size;
            mcb* allocated_mcb = (mcb*)(temp_mcb->Start_addr);
            allocated_mcb->Size = size + sizeof(mcb);
            allocated_mcb->Start_addr = temp_mcb->Start_addr;
            allocated_mcb->PrevPtr = NULL;
            allocated_mcb->NextPtr = NULL;

            temp_mcb->Size = remaining_size;
            temp_mcb->Start_addr += (size + sizeof(mcb));

            if (alloc_list != NULL) {
                allocated_mcb->NextPtr = alloc_list->headPtr;
                if (alloc_list->headPtr != NULL) {
                    alloc_list->headPtr->PrevPtr = allocated_mcb;
                } else {
                    alloc_list->tailPtr = allocated_mcb;
                }
                alloc_list->headPtr = allocated_mcb;
                alloc_list->count++;
            }
            if (allocated_mcb->NextPtr != NULL) {
                allocated_mcb->NextPtr->PrevPtr = allocated_mcb;
            }
            return (void*)allocated_mcb->Start_addr;
        }
        temp_mcb = temp_mcb->NextPtr;
    }
    sys_req(WRITE, COM1, "No free block big enough for the allocation request\n", 53);
    return NULL;
}

int free_memory(void* memptr) {
    mcb* a_temp_mcb = alloc_list->headPtr;
    mcb* prev_a_temp_mcb = NULL;
    mcb* t_temp_mcb = (mcb*)sys_alloc_mem(sizeof(mcb));
    t_temp_mcb->NextPtr = NULL;
    t_temp_mcb->PrevPtr = NULL;
    t_temp_mcb->Start_addr = 0;
    t_temp_mcb->Size = 0;

    while (a_temp_mcb != NULL) {
        if (a_temp_mcb->Start_addr == (int)memptr) {
            sys_req(WRITE,COM1,"[free memory]Found the block to free\n",38);
            t_temp_mcb->Start_addr = a_temp_mcb->Start_addr;
            t_temp_mcb->Size = a_temp_mcb->Size;

            // Remove from allocated list
            if (prev_a_temp_mcb == NULL) {  // First element
                alloc_list->headPtr = a_temp_mcb->NextPtr;
            } else {
                prev_a_temp_mcb->NextPtr = a_temp_mcb->NextPtr;
            }
            if (a_temp_mcb->NextPtr == NULL) {  // Last element
                alloc_list->tailPtr = prev_a_temp_mcb;
            }

            alloc_list->count--;
            break;
        }
        prev_a_temp_mcb = a_temp_mcb;
        a_temp_mcb = a_temp_mcb->NextPtr;
    }

    if (t_temp_mcb->Start_addr == 0 && t_temp_mcb->Size == 0) {  // Block not found in allocated list
        //sys_free_mem(t_temp_mcb);  // Free the temporary block
        return 1;
    }

    // Add to free list
    t_temp_mcb->NextPtr = free_list->headPtr;
    if (free_list->headPtr != NULL) {
        free_list->headPtr->PrevPtr = t_temp_mcb;
    }
    free_list->headPtr = t_temp_mcb;
    if (free_list->tailPtr == NULL || t_temp_mcb->NextPtr == NULL) {
        free_list->tailPtr = t_temp_mcb;  // Update tail pointer
    }
    free_list->count++;

    // Combine adjacent free blocks
    combine_free_blocks();

    return 0;
}

int check_valid(int memptr) {
    mcb* a_temp_mcb = alloc_list->headPtr;

    while (a_temp_mcb != NULL) {
        if (memptr == a_temp_mcb->Start_addr) {
            return 1;
        }
        a_temp_mcb = a_temp_mcb->NextPtr;
    }
    return 0;
}

void combine_free_blocks(void) {
    if (free_list == NULL || free_list->headPtr == NULL) {
        return;
    }

    
    int nodesChecked = 0;
    // Iterates from head to tail to compact the free list
    mcb* current = free_list->headPtr;
    //current != NULL && current->NextPtr != NULL && 
    int iterate = 0;
    while (nodesChecked < (int)free_list->count + 1) {
        iterate = 0;
        mcb* next = current->NextPtr;
        mcb* back = next->NextPtr;

        for(int i = 0; i < free_list->count; i++){
        if ((int)current->Start_addr + (int)current->Size == (int)next->Start_addr) {

            current->Size += next->Size - sizeof(mcb);
            if(next != next->NextPtr)
            {
            current->NextPtr = next->NextPtr;
            next->NextPtr->PrevPtr = current;
            }
            else{
                while(back->PrevPtr != current)
                {
                    back = back->PrevPtr;
                }
                current->NextPtr = back;
            }
            if(next == free_list->headPtr)
            {
                free_list->headPtr = current;
            }
            // Checks nextPtr has a node
            if (next->NextPtr != current && next->NextPtr != NULL) {
                next->NextPtr->PrevPtr = current;   // Set the 'next next' node prevPtr to current
            } else {
                free_list->tailPtr = current;  // Update tail pointer if next was the last element
            }

            //sys_free_mem(next);
            free_list->count--;
            nodesChecked = 0;
            char* nodesChecked_str = " ";
            itoa(nodesChecked, nodesChecked_str);
            sys_req(WRITE, COM1, "[1st if testing in combine_free_blocks()] checking node #", 51);
            sys_req(WRITE, COM1, nodesChecked_str, strlen(nodesChecked_str));
            sys_req(WRITE, COM1, "\n", 2);
            current = free_list->headPtr;
            iterate = 1;
            i--;
            next = next->PrevPtr;
            if(free_list->count == 2)
            {
                while(current->PrevPtr != NULL)
                {
                    current = current->PrevPtr;
                }
            }
        }
        if((int)next->Start_addr + (int)next->Size == (int)current->Start_addr)
        {
            next->Size += current->Size;
            current->Start_addr = next->Start_addr;
            current->Size = next->Size - sizeof(mcb);
            current->NextPtr = next->NextPtr;
             // Checks nextPtr has a node
            if (next->NextPtr != current) {
                next->NextPtr->PrevPtr = current;   // Set the 'next next' node prevPtr to current
            } else {
                free_list->tailPtr = current;  // Update tail pointer if next was the last element
            }

            //sys_free_mem(next);
            free_list->count--;
            nodesChecked = 0;
            char* nodesChecked_str = " ";
            itoa(nodesChecked, nodesChecked_str);
            sys_req(WRITE, COM1, "[2nd if testing in combine_free_blocks()] checking node #", 51);
            sys_req(WRITE, COM1, nodesChecked_str, strlen(nodesChecked_str));
            sys_req(WRITE, COM1, "\n", 2); 
            current = free_list->headPtr;
            i--;
            next = next->PrevPtr;
            iterate = 1;
        }
        
        if(next != next->NextPtr){
        next = next->NextPtr;
        }
        else{
            while(next->PrevPtr != NULL)
            {
                next = next->PrevPtr;
            }
        }
        }
        if(iterate == 0){
            current = next;
            nodesChecked += 1; 
        }
        
    }

//shouldn't be necessary if circular list is implemented correctly
    // // Iterates from tail to head to compact the free list
    // current = free_list->tailPtr;
    // while (current != NULL && current->PrevPtr != NULL) {
    //     mcb* prev = current->PrevPtr;
    //     if ((int)prev->Start_addr + (int)prev->Size + (int)sizeof(mcb) == (int)current->Start_addr) {
    //         prev->Size += current->Size + sizeof(mcb);
    //         prev->NextPtr = current->NextPtr;
    //         if (prev->PrevPtr != NULL) {    // Checks 'prev prev' ptr has a node
    //             prev->PrevPtr->NextPtr = prev;   // Set the 'next next' node prevPtr to current
    //         } else {
    //             free_list->tailPtr = prev;  // Update tail pointer if current was the last element
    //         }
    //         sys_free_mem(current);
    //     }
    //     else{
    //         current = prev;
    //     }
    // }
}
