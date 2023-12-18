/**
 *  \file semSharedReceptionist.c (implementation file)
 *
 *  \brief Problem name: Restaurant
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the receptionist:
 *     \li waitForGroup
 *     \li provideTableOrWaitingRoom
 *     \li receivePayment
 *
 *  \author Nuno Lau - December 2023
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "logging.h"
#include "probConst.h"
#include "probDataStruct.h"
#include "semaphore.h"
#include "sharedDataSync.h"
#include "sharedMemory.h"

/** \brief logging file name */
static char nFic[51];

/** \brief shared memory block access identifier */
static int shmid;

/** \brief semaphore set access identifier */
static int semgid;

/** \brief pointer to shared memory region */
static SHARED_DATA *sh;

/* constants for groupRecord */
#define TOARRIVE 0
#define WAIT 1
#define ATTABLE 2
#define DONE 3

/** \brief receptioninst view on each group evolution (useful to decide table
 * binding) */
static int groupRecord[MAXGROUPS];

/** \brief receptionist waits for next request */
static request waitForGroup();

/** \brief receptionist waits for next request */
static void provideTableOrWaitingRoom(int n);

/** \brief receptionist receives payment */
static void receivePayment(int n);

/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the
 * problem: the receptionist.
 */
int main(int argc, char *argv[]) {
  int key;    /*access key to shared memory and semaphore set */
  char *tinp; /* numerical parameters test flag */

  /* validation of command line parameters */
  if (argc != 4) {
    freopen("error_RT", "a", stderr);
    fprintf(stderr, "Number of parameters is incorrect!\n");
    return EXIT_FAILURE;
  } else {
    freopen(argv[3], "w", stderr);
    setbuf(stderr, NULL);
  }

  strcpy(nFic, argv[1]);
  key = (unsigned int)strtol(argv[2], &tinp, 0);
  if (*tinp != '\0') {
    fprintf(stderr, "Error on the access key communication!\n");
    return EXIT_FAILURE;
  }

  /* connection to the semaphore set and the shared memory region and mapping
     the shared region onto the process address space */
  if ((semgid = semConnect(key)) == -1) {
    perror("error on connecting to the semaphore set");
    return EXIT_FAILURE;
  }
  if ((shmid = shmemConnect(key)) == -1) {
    perror("error on connecting to the shared memory region");
    return EXIT_FAILURE;
  }
  if (shmemAttach(shmid, (void **)&sh) == -1) {
    perror("error on mapping the shared region on the process address space");
    return EXIT_FAILURE;
  }

  /* initialize random generator */
  srandom((unsigned int)getpid());

  /* initialize internal receptionist memory */
  int g;
  for (g = 0; g < sh->fSt.nGroups; g++) {
    groupRecord[g] = TOARRIVE;
  }

  /* simulation of the life cycle of the receptionist */
  int nReq = 0;
  request req;
  while (nReq < sh->fSt.nGroups * 2) {
    req = waitForGroup();
    switch (req.reqType) {
    case TABLEREQ:
      provideTableOrWaitingRoom(req.reqGroup);
      break;
    case BILLREQ:
      receivePayment(req.reqGroup);
      break;
    }
    nReq++;
  }

  /* unmapping the shared region off the process address space */
  if (shmemDettach(sh) == -1) {
    perror(
        "error on unmapping the shared region off the process address space");
    return EXIT_FAILURE;
    ;
  }

  return EXIT_SUCCESS;
}

/**
 *  \brief decides table to occupy for group n or if it must wait.
 *
 *  Checks current state of tables and groups in order to decide table or wait.
 *
 *  \return table id or -1 (in case of wait decision)
 */
static int decideTableOrWait(int group_id) {
  assert(groupRecord[group_id] <
         2); // We need to check if the group hasnt already been seated, which
             // means if they are arriving or waiting;
  int group;
  for (int table = 0; table < NUMTABLES; table++) {
    for (group = 0; group < sh->fSt.nGroups; group++) {
      if (sh->fSt.assignedTable[group] == table) {
        break;
      }
    }
    if (group != sh->fSt.nGroups)
      continue;
    return table;
  }
  return -1;
}

/**
 *  \brief called when a table gets vacant and there are waiting groups
 *         to decide which group (if any) should occupy it.
 *
 *  Checks current state of tables and groups in order to decide group.
 *
 *  \return group id or -1 (in case of wait decision)
 */

static int decideNextGroup() {
  if (sh->fSt.groupsWaiting <= 0)
    return -1;

  // If there are groups waiting, we need to select one to have the table;
  for (int group_id = 0; group_id < sh->fSt.nGroups; group_id++) {
    if (groupRecord[group_id] == WAIT) {
      return group_id;
    }
  }
  return -1;
}

/**
 *  \brief receptionist waits for next request
 *
 *  Receptionist updates state and waits for request from group, then reads
 * request, and signals availability for new request. The internal state should
 * be saved.
 *
 *  \return request submitted by group
 */
static request waitForGroup() {
  request ret;

  fprintf(stderr, "Entered critical region at waitForGroup(1)\n");
  if (semDown(semgid, sh->mutex) == -1) { /* enter critical region */
    perror("error on the up operation for semaphore access (RT)");
    exit(EXIT_FAILURE);
  }

  // Update state for receptionist, saying he is avaliable to receive a request
  // (up semaphore); then leave the critical region;
  sh->fSt.st.receptionistStat = WAIT_FOR_REQUEST;
  saveState(nFic, &sh->fSt);

  if (semUp(semgid, sh->mutex) == -1) { /* exit critical region */
    perror("error on the down operation for semaphore access (RT)");
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "Exited critical region at waitForGroup(1)\n");

  // Wait for any requests to the waiter
  if (semDown(semgid, sh->receptionistReq) == -1) {
    perror("error on the up operation for semaphore access (RT)");
    exit(EXIT_FAILURE);
  }

  fprintf(stderr, "Entered critical region at waitForGroup(2)\n");
  if (semDown(semgid, sh->mutex) == -1) { /* enter
                                             critical
                                             region */
    perror("error on the up operation for semaphore access (RT)");
    exit(EXIT_FAILURE);
  }

  // Formulate the request;
  ret = sh->fSt.receptionistRequest;

  if (semUp(semgid, sh->mutex) == -1) { /* exit critical region */
    perror("error on the down operation for semaphore access (RT)");
    exit(EXIT_FAILURE);
  }

  fprintf(stderr, "Exited critical region at waitForGroup(2)\n");
  if (semUp(semgid, sh->receptionistRequestPossible) == -1) {
    perror("error on the down operation for semaphore access (RT)");
    exit(EXIT_FAILURE);
  }

  return ret;
}

/**
 *  \brief receptionist decides if group should occupy table or wait
 *
 *  Receptionist updates state and then decides if group occupies table
 *  or waits. Shared (and internal) memory may need to be updated.
 *  If group occupies table, it must be informed that it may proceed.
 *  The internal state should be saved.
 *
 */
static void provideTableOrWaitingRoom(int group_id) {

  if (semDown(semgid, sh->mutex) == -1) { /* enter critical region */
    perror("error on the up operation for semaphore access (RT)");
    exit(EXIT_FAILURE);
  }

  // TODO insert your code here
  sh->fSt.st.receptionistStat = ASSIGNTABLE;
  saveState(nFic, &sh->fSt);
  // See if a table is available for this group;
  int table_id = decideTableOrWait(group_id);

  // If no table is available, set the group to waiting;
  if (table_id < 0) {
    sh->fSt.groupsWaiting++;
    groupRecord[group_id] = WAIT;
  }
  // Else, sit the group;
  else {
    groupRecord[group_id] = ATTABLE;
    sh->fSt.assignedTable[group_id] = table_id;
    if (semUp(semgid, sh->waitForTable[group_id]) == -1) {
      perror("error on the down operation for semaphore access (RT)");
      exit(EXIT_FAILURE);
    }
  }

  if (semUp(semgid, sh->mutex) == -1) { /* exit critical region */
    perror("error on the down operation for semaphore access (RT)");
    exit(EXIT_FAILURE);
  }
}

/**
 *  \brief receptionist receives payment
 *
 *  Receptionist updates its state and receives payment.
 *  If there are waiting groups, receptionist should check if table that just
 * became vacant should be occupied. Shared (and internal) memory should be
 * updated. The internal state should be saved.
 *
 */

static void receivePayment(int group_id) {
  fprintf(stderr, "Entered critical region at receivePayment\n");
  if (semDown(semgid, sh->mutex) == -1) { /* enter critical region */
    perror("error on the up operation for semaphore access (RT)");
    exit(EXIT_FAILURE);
  }

  sh->fSt.st.receptionistStat = RECVPAY;
  saveState(nFic, &sh->fSt);
  groupRecord[group_id] = DONE;
  // If the group is paying, then the table is now vacant!
  int table_id = sh->fSt.assignedTable[group_id];
  sh->fSt.assignedTable[group_id] =
      -1; // Which means we need to define the table as empty!
  // If there are groups waiting, then we can sit them at that table!
  if (sh->fSt.groupsWaiting > 0) {
    int new_group_id = decideNextGroup();
    if (new_group_id > -1) {
      groupRecord[new_group_id] = ATTABLE;
      if (semUp(semgid, sh->waitForTable[new_group_id]) == -1) {
        perror("error on the down operation for semaphore access (RT)");
        exit(EXIT_FAILURE);
      }
      sh->fSt.groupsWaiting--;
      sh->fSt.assignedTable[new_group_id] = table_id;
    }
  }

  if (semUp(semgid, sh->mutex) == -1) { /* exit critical region */
    perror("error on the down operation for semaphore access (RT)");
    exit(EXIT_FAILURE);
  }

  if (semUp(semgid, sh->tableDone[table_id]) == -1) {
    perror("error on the down operation for semaphore access (RT)");
    exit(EXIT_FAILURE);
  }
}
