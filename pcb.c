#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include "pcb.h"
#include "list.h"

List *list_ready_high = NULL;
List *list_ready_norm = NULL;
List *list_ready_low = NULL;

List *list_waiting_send = NULL;
List *list_waiting_receive = NULL;

PCB* pcb_init = NULL;
PCB* pcb_curr = NULL;

int totalPID = 0;
Semaphore* semaphore[5];



List* priorityList(int priority){
    List* returnVal = NULL;
    switch (priority){
        case 0:
            return list_ready_high;
        case 1:
            return list_ready_norm;
        case 2:
            return list_ready_low;
        default:
            printf("Failed to get list. (List* priorityList) \n");
            exit(FAILURE);
    }
}

char* priorityChar(int priority){
    switch (priority){
        case 0:
            return "High";
        case 1:
            return "Normal";
        case 2:
            return "Low";
        default:
            return "Un-initialized";
    }
}

bool list_comparator(void* pcb, void* receiver){
    return(((PCB*)pcb)->pid == *((int*)receiver));
}
PCB* pcb_search(int pid){
    COMPARATOR_FN pComparatorFn = &list_comparator;
    PCB* returnPCB = NULL;
    //if List_search fails, the pointer would point NULL
    List_first(priorityList(0));
    List_first(priorityList(1));
    List_first(priorityList(2));
    List_first(list_waiting_send);
    List_first(list_waiting_receive);
    returnPCB = List_search(priorityList(0), pComparatorFn, &pid);
    if(returnPCB != NULL) return returnPCB;
    returnPCB = List_search(priorityList(1), pComparatorFn, &pid);
    if(returnPCB != NULL) return returnPCB;
    returnPCB = List_search(priorityList(2), pComparatorFn, &pid);
    if(returnPCB != NULL) return returnPCB;
    returnPCB = List_search(list_waiting_receive, pComparatorFn, &pid);
    if(returnPCB != NULL) return returnPCB;
    returnPCB = List_search(list_waiting_send, pComparatorFn, &pid);
    if(returnPCB != NULL) return returnPCB;
    //if search cannot find List then NULL will be returned
    //TODO Aerin check to see if this change interferes with send/receive
    return returnPCB;

}

// wake up the next blocked process, according to its priority, and set it as the curr_pcb
void pcb_next() {

    // highest priority first
    if (List_count(list_ready_high) != 0) {
        List_first(list_ready_high);
        pcb_curr = List_trim(list_ready_high);
    } else if (List_count(list_ready_norm) != 0) {
        List_first(list_ready_norm);
        pcb_curr = List_trim(list_ready_norm);
    } else if (List_count(list_ready_low) != 0) {
        List_first(list_ready_low);
        pcb_curr = List_trim(list_ready_low);
    } else {
        pcb_curr = pcb_init;
    }
    pcb_curr->state = RUNNING;
}




int pcb_initialize(){
    //initialize list


    list_ready_high = List_create();
    list_ready_norm = List_create();
    list_ready_low = List_create();
    list_waiting_send = List_create();
    list_waiting_receive = List_create();

    if( list_ready_high == NULL ||
        list_ready_norm == NULL ||
        list_ready_low == NULL ||
        list_waiting_send == NULL ||
        list_waiting_receive == NULL){
            printf("Initializing list failed!\n");
            return(FAILURE);
        }
    for(int i = 0; i < SEM_MAX; i++){
        semaphore[i] = NULL;
    }

    //initialize init PCB
    pcb_init = malloc(sizeof(PCB));
    pcb_init->state = RUNNING;
    pcb_init->pid = 0;
    pcb_init->priority = 0;
    pcb_init->msg = NULL;
    totalPID++;
    pcb_curr = pcb_init;

    return SUCCESS;

}

int pcb_create(int priority){
    //if totalPID is 0, initialize
    if(totalPID == 0){
        printf("init has not run yet! Cannot create new process");
        return FAILURE;
    }

    //initialize new PCB
    PCB* newPCB = malloc(sizeof(PCB));
    newPCB->priority = priority;
    newPCB->pid = totalPID;
    newPCB->state = READY;
    newPCB->waitReceiveState = NONE;

    totalPID++;

    //check priority number
    bool appendSuccess = false;
    switch (priority) {
        case 0: //high priority
            appendSuccess = (List_prepend(list_ready_high, (void*)newPCB) == 0);
            break;
        case 1: //normal priority
            appendSuccess = (List_prepend(list_ready_norm, (void*)newPCB) == 0);
            break;
        case 2: //low priority
            appendSuccess = (List_prepend(list_ready_low, (void*)newPCB) == 0);
            break;
    }
    if(!appendSuccess){
        printf("Append new process to list failed!\n");
        return(FAILURE);
    }
    printf("Process created successfully. Its PID is: %d with %s\n", newPCB->pid, priorityChar(newPCB->priority));
    return SUCCESS;
}

int pcb_fork(){
    if(pcb_curr == pcb_init){
        printf("Current process is init! Fork() cannot be executed,\n");
        return FAILURE;
    }
    if(pcb_curr == NULL){
        printf("Current PCB is null. Cannot fork.\n");
        return(FAILURE);
    }
    PCB *newProcess = malloc(sizeof(PCB));
    memcpy(newProcess, pcb_curr, sizeof(PCB));
    newProcess->pid = totalPID;
    totalPID++;
    List* list = priorityList(pcb_curr->priority);
    List_prepend(list, newProcess);
    //TODO: I'm not sure if we can call pcb_next(), it's not in the specifications
    //    pcb_next();
    printf("Process forked successfully. The new PID is: %d with priority: %s (%d)\n", newProcess->pid, priorityChar(newProcess->priority), newProcess->priority);
    return SUCCESS;
}

int pcb_kill(int pid){
    //search for the PCB corresponding to the pid, remove it from the list and free that pcb's memory
    COMPARATOR_FN pComparatorFn = &list_comparator;
    PCB* PCB_kill = pcb_search(pid);
    if(PCB_kill == NULL){
        printf("Process with PID #%d cannot be found.\n" ,pid);
        return FAILURE;
    }
    List* listItemRemove = priorityList(PCB_kill->priority);
    List_search(listItemRemove, pComparatorFn, &pid);
    List_remove(listItemRemove);
    free(PCB_kill);
    //TODO is this necessary (going to next PCB)
    pcb_next();
    int pid_next = pcb_curr->pid;
    printf("PCB PID# %d removed. Current PCB is PID# %d", pid, pid_next);
    return SUCCESS;
}

int pcb_exit(){

}

void list_pcb_free(void* pcb){
    free((PCB*)pcb);
}
//TODO is this exit()?
void pcb_terminate(){
    FREE_FN pFreeFn = &list_pcb_free;
    List_free(list_ready_high, pFreeFn);
    List_free(list_ready_norm, pFreeFn);
    List_free(list_ready_low, pFreeFn);
    List_free(list_waiting_send, pFreeFn);
    List_free(list_waiting_receive, pFreeFn);
}

int pcb_quantum(){

    int currPID = pcb_curr->pid;
    int currPriority = pcb_curr->priority;
    PCB* preQuantumPCB = pcb_curr;
    pcb_next();
    int nextPID = pcb_curr->pid;
    if(currPID == 0){
        printf("Exiting from initial process. The current active process is PCB #%d", nextPID);

    }
    else{
        List_prepend(priorityList(currPriority), preQuantumPCB);
        printf("PCB #%d is put back into %s. The current active process is PCB #%d", currPID, priorityChar(currPriority), nextPID);

    }
    return SUCCESS;


}

/**
 * @brief
 *
 * @param pid (pid of process to send msg to)
 * @param msg (null-terminated msg string, 40 char max)
 * @return int
 */
int pcb_send(int pid, char* msg){
    COMPARATOR_FN pComparatorFn = &list_comparator;
    PCB* receiver;
    
    // check if there's any process waiting for msg sent to it
    if(List_count(list_waiting_receive) != 0){
        if(List_search(list_waiting_receive, pComparatorFn, &pid) != NULL){
            receiver = List_trim(list_waiting_receive);
            receiver->msg = msg;
            receiver->state = READY;
            List_prepend(priorityList(receiver->priority), receiver);

            printf("The message has been successfully sent.\n");
            printf("Sender pid:\t%d with %s priority.\n", pcb_curr->pid, priorityChar(pcb_curr->priority));
            printf("Receiver pid:\t%d with %s priority.\n", pid, priorityChar(receiver->priority));
            printf("Message sent:\t%s", msg);

            if(pcb_curr->pid != 0){
                pcb_curr->state = BLOCKED;
                List_prepend(list_waiting_send, pcb_curr);
                pcb_next();
            }
            
            return SUCCESS;
        }
    }

    // still need to be fixed  ************
    if(pid == pcb_curr->pid){   // if curr process sends msg to itself
        pcb_curr->msg = msg;
        pcb_curr->state = BLOCKED;
        List_prepend(list_waiting_send, pcb_curr);

        printf("The message has been successfully sent.\n");
        printf("Sender pid:\t%d with %s priority.\n", pcb_curr->pid, priorityChar(pcb_curr->priority));
        printf("Receiver pid:\t%d with %s priority.\n", pid, priorityChar(receiver->priority));
        printf("Message sent:\t%s", msg);

        pcb_next();
        return SUCCESS;
    }

    // search all three ready queues
    // 1. high priority
    if(List_count(list_ready_high) != 0){
        if(List_search(list_ready_high, pComparatorFn, &pid) != NULL){
            receiver = List_trim(list_ready_high);
            receiver->msg = msg;
            receiver->state = READY;
            List_prepend(priorityList(receiver->priority), receiver);

            printf("The message has been successfully sent.\n");
            printf("Sender pid:\t%d with %s priority.\n", pcb_curr->pid, priorityChar(pcb_curr->priority));
            printf("Receiver pid:\t%d with %s priority.\n", pid, priorityChar(receiver->priority));
            printf("Message sent:\t%s", msg);


            if(pcb_curr->pid == 0){
                pcb_curr->state = BLOCKED;
                List_prepend(list_waiting_send, pcb_curr);
                pcb_next();
            }

            return SUCCESS;
        }
    }

    // 2. normal priority
    if(List_count(list_ready_norm) != 0){
        if(List_search(list_ready_norm, pComparatorFn, &pid) != NULL){
            receiver = List_trim(list_ready_norm);
            receiver->msg = msg;
            receiver->state = READY;
            List_prepend(priorityList(receiver->priority), receiver);

            printf("The message has been successfully sent.\n");
            printf("Sender pid:\t%d with %s priority.\n", pcb_curr->pid, priorityChar(pcb_curr->priority));
            printf("Receiver pid:\t%d with %s priority.\n", pid, priorityChar(receiver->priority));
            printf("Message sent:\t%s", msg);

            if(pcb_curr->pid == 0){
                pcb_curr->state = BLOCKED;
                List_prepend(list_waiting_send, pcb_curr);
                pcb_next();
            }
            return SUCCESS;
        }
    }

    // 3. low priority
    if(List_count(list_ready_low) != 0){
        if(List_search(list_ready_low, pComparatorFn, &pid) != NULL){
            receiver = List_trim(list_ready_low);
            receiver->msg = msg;
            receiver->state = READY;
            List_prepend(priorityList(receiver->priority), receiver);
            
            printf("The message has been successfully sent.\n");
            printf("Sender pid:\t%d with %s priority.\n", pcb_curr->pid, priorityChar(pcb_curr->priority));
            printf("Receiver pid:\t%d with %s priority.\n", pid, priorityChar(receiver->priority));
            printf("Message sent:\t%s", msg);
           
            if(pcb_curr->pid == 0){
                pcb_curr->state = BLOCKED;
                List_prepend(list_waiting_send, pcb_curr);
                pcb_next();
            }
            return SUCCESS;
        }
    }
    
    // no match:
    printf("Failed to send the message:\t'%s'.\n", msg);
    printf("No receiver (pid = %d)\n", pid);
    printf("Try again with a different pid.\n");
    return FAILURE;
}

// Check if the currently executing process has received any messages
int pcb_receive(){
    if(pcb_curr->msg == NULL){
        printf("Current pcb (pid = %d, priority = %s) hasn't received any messages.\n", pcb_curr->pid, priorityChar(pcb_curr->priority));
        
        if(pcb_curr->pid != 0){
            pcb_curr->state = BLOCKED;
            List_prepend(list_waiting_receive, pcb_curr);
            
            pcb_next();
        }

        printf("New pcb:\tpid = %d, priority = %s\n", pcb_curr->pid, priorityChar(pcb_curr->priority));
        return FAILURE;
    }else{
        printf("Current pcb (pid = %d, priority = %s) has successfully received the message.\n", pcb_curr->pid, priorityChar(pcb_curr->priority));
        printf("Message received:\t'%s'\n", pcb_curr->msg);

        // reset the msg and partner_pid
        pcb_curr->msg = NULL;

        return SUCCESS;
    }
}

int pcb_reply(int pid, char* msg){
    COMPARATOR_FN pComparatorFn = &list_comparator;
    PCB* receiver;
    
    if(pid == 0){
        printf("The reply message has been successfully sent.\n");
        printf("Replier pid:\t%d with %s priority.\n", pcb_curr->pid, priorityChar(pcb_curr->priority));
        printf("Receiver pid:\t%d with %s priority.\n", pid, priorityChar(receiver->priority));
        printf("Reply sent:\t%s", msg); 
    }

    // check if there's any process waiting for msg sent to it
    if(List_count(list_waiting_send) != 0){
        
        if(List_search(list_waiting_send, pComparatorFn, &pid) != NULL){
            receiver = List_trim(list_waiting_send);
            receiver->msg = msg;
            receiver->state = READY;
            List_prepend(priorityList(receiver->priority), receiver);

            printf("The reply message has been successfully sent.\n");
            printf("Replier pid:\t%d with %s priority.\n", pcb_curr->pid, priorityChar(pcb_curr->priority));
            printf("Receiver pid:\t%d with %s priority.\n", pid, priorityChar(receiver->priority));
            printf("Reply sent:\t%s", msg);

            return SUCCESS;
        }
    }else{
        printf("There's no pcb waiting for replies.\n");
        return FAILURE;
    }

    printf("Failed to find a send_blocked pcb with pid = %d.\n", pid);
    printf("Try another pid.\n");

    return FAILURE;
}

int pcb_create_semaphore(int sid, int init){
    if(sid < 0 || sid >= SEM_MAX){
        printf("Invalid sid. The value sid will be from 0 to 4.\n");
        return FAILURE;
    }

    if(semaphore[sid] == NULL){
        semaphore[sid] = malloc(sizeof(Semaphore));
        if(semaphore[sid] == NULL){
            printf("Failed to allocate memory for semaphore of the given sid: %d\n", sid);
            return FAILURE;
        }
        semaphore[sid]->val = init;
        semaphore[sid]->plist = List_create();
        if(semaphore[sid]->plist == NULL){
            printf("Failed to create a list for processes waiting on semaphore (sid = %d)\n", sid);
            return FAILURE;
        }
    }else{
        printf("Semaphore with the given sid (%d) already exists. Try again.\n", sid);
        return FAILURE;
    }
    return SUCCESS;
}

int pcb_P(int sid){
    // Check for the validity of sid
    if(sid < 0 || sid >= SEM_MAX){
        printf("Invalid sid. The value sid will be from 0 to 4.\n");
        return FAILURE;
    }

    // Implementation based on the lecture note
    semaphore[sid]->val--;
    if(semaphore[sid]->val < 0){
        List_prepend(semaphore[sid]->plist, pcb_curr);
        pcb_next(); // switch the currently running process to the next process available
    }

}

int pcb_V(int sid){
    // Check for the validity of sid
    if(sid < 0 || sid >= SEM_MAX){
        printf("Invalid sid. The value sid will be from 0 to 4.\n");
        return FAILURE;
    }

    PCB* pcb_wake;

    semaphore[sid]->val++;
    if(semaphore[sid]->val <= 0){
        pcb_wake = List_trim(semaphore[sid]->plist);
        if(pcb_wake == NULL){
            printf("There is no process to unblock for the semaphore of sid: %d\n", sid);
            return FAILURE;
        }
        List_prepend(priorityList(pcb_wake->priority), pcb_wake);   // wake up a process in pList and add it to the correct ready list
    }
}

char* stateChar(enum pcb_states states){
    switch(states){
        case RUNNING:
            return "Running";
        case BLOCKED:
            return "Blocked";
        case READY:
            return "Ready";
        default:
            perror("Error in parsing state in character form!");
            exit(FAILURE);
    }
}

void pcb_procinfo(int pid){
    PCB* temp = pcb_search(pid);
    printf("\nProcess PID#%d is in state: '%s' and is in queue %s(%d).",
           temp->pid, stateChar(temp->state), priorityChar(temp->priority), temp->priority);
    if(temp->waitReceiveState == NONE){
        printf("It is not in a wait/receive state.\n");
    }
    if(temp->waitReceiveState == SEND){
        printf("It is in SEND state.\n");
    }
    if(temp->waitReceiveState == RECEIVE){
        printf("It is in RECEIVE state.\n");
    }


}

totalinfo_helper(List* list, int priority){
    char* priorityCharOutput = priorityChar(priority);
    PCB* pcb_temp = List_first(list);
    printf("Processes with priority %s: ", priorityCharOutput);
    for(int i = 0; pcb_temp != NULL; i++){
    printf("%d ", pcb_temp->pid);
    pcb_temp = List_next(list);
}
    printf(".\n");
}

void pcb_totalinfo(){
    printf("\nTotal information about the system:\n");
    printf("The current active process: PID# %d, priority: %s(%d).\n",
           pcb_curr->pid, priorityChar(pcb_curr->pid), pcb_curr->priority);
    totalinfo_helper(list_ready_high, 0);
    totalinfo_helper(list_ready_norm, 1);
    totalinfo_helper(list_ready_low, 2);


    PCB* pcb_waitingSend = List_first(list_waiting_send);
    printf("Processes in waiting send list: ");
    for(int i = 0; pcb_waitingSend != NULL; i++){
        pcb_waitingSend = List_next(list_waiting_send);
        printf("%d ", pcb_waitingSend->pid);
    }
    printf(".\n");

    PCB* pcb_waitingReceive = List_first(list_waiting_receive);
    printf("Processes in waiting send list: ");
    for(int i = 0; pcb_waitingReceive != NULL; i++){
        pcb_waitingReceive = List_next(list_waiting_receive);
        printf("%d ", pcb_waitingReceive->pid);
    }
    printf(".\n");

    //need to add semaphore
}
