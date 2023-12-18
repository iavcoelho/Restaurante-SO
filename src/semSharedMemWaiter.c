/**
 *  \file semSharedWaiter.c (implementation file)
 *
 *  \brief Problem name: Restaurant
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the waiter:
 *     \li waitForClientOrChef
 *     \li informChef
 *     \li takeFoodToTable
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

/** \brief waiter waits for next request */
static request waitForClientOrChef();

/** \brief waiter takes food order to chef */
static void informChef(int group);

/** \brief waiter takes food to table */
static void takeFoodToTable(int group);

/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the
 * problem: the waiter.
 */
int main(int argc, char *argv[]) {
  int key;    /*access key to shared memory and semaphore set */
  char *tinp; /* numerical parameters test flag */

  /* validation of command line parameters */
  if (argc != 4) {
    freopen("error_WT", "a", stderr);
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

  /* simulation of the life cycle of the waiter */
  int nReq = 0;
  request req;
  while (nReq < sh->fSt.nGroups * 2) {
    req = waitForClientOrChef();
    switch (req.reqType) {
    case FOODREQ:
      informChef(req.reqGroup);
      break;
    case FOODREADY:
      takeFoodToTable(req.reqGroup);
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
 *  \brief waiter waits for next request
 *
 *  Waiter updates state and waits for request from group or from chef, then
 * reads request. The waiter should signal that new requests are possible. The
 * internal state should be saved.
 *
 *  \return request submitted by group or chef
 */
static request waitForClientOrChef() {
  request req;
  if (semDown(semgid, sh->mutex) == -1) { /* enter critical region */
    perror("error on the up operation for semaphore access (WT)");
    exit(EXIT_FAILURE);
  }

  // First we need to update the state of the waiter to be available for
  // requests
  sh->fSt.st.waiterStat = WAIT_FOR_REQUEST;
  saveState(nFic, &sh->fSt);

  if (semUp(semgid, sh->mutex) == -1) { /* exit critical region */
    perror("error on the down operation for semaphore access (WT)");
    exit(EXIT_FAILURE);
  }

  // After doing this, we have to wait for someone to send us a request;
  if (semDown(semgid, sh->waiterRequest) == -1) {
    perror("error on the up operation for semaphore access (WT)");
    exit(EXIT_FAILURE);
  }

  // If we get a request, we need to enter the critical region again,
  // so we can get the data needed to process said request;

  if (semDown(semgid, sh->mutex) == -1) { /* enter critical region */
    perror("error on the up operation for semaphore access (WT)");
    exit(EXIT_FAILURE);
  }

  // We need to get the data from the request given to the waiter,
  // so we can then process it.
  req = sh->fSt.waiterRequest;

  if (semUp(semgid, sh->mutex) == -1) { /* exit critical region */
    perror("error on the down operation for semaphore access (WT)");
    exit(EXIT_FAILURE);
  }

  // After all this, we need to signal that the waiter is now able to process
  // the request, since he now has the data

  if (semUp(semgid, sh->waiterRequestPossible) == -1) {
    perror("error on the down operation for semaphore access (WT)");
    exit(EXIT_FAILURE);
  }

  return req;
}

/**
 *  \brief waiter takes food order to chef
 *
 *  Waiter updates state and then takes food request to chef.
 *  Waiter should inform group that request is received.
 *  Waiter should wait for chef receiving request.
 *  The internal state should be saved.
 *
 */
static void informChef(int group_id) {
  int table_id;
  if (semDown(semgid, sh->mutex) == -1) { /* enter critical region */
    perror("error on the up operation for semaphore access (WT)");
    exit(EXIT_FAILURE);
  }

  // If we are giving a request to the chef, then we need to update our state
  sh->fSt.st.waiterStat = INFORM_CHEF;
  saveState(nFic, &sh->fSt);
  // Then we need to setup all the flags and request for the chef
  sh->fSt.foodOrder = 1;
  sh->fSt.foodGroup = group_id;
  // We also need to know the table that did the request
  table_id = sh->fSt.assignedTable[group_id];

  if (semUp(semgid, sh->mutex) == -1) /* exit critical region */
  {
    perror("error on the down operation for semaphore access (WT)");
    exit(EXIT_FAILURE);
  }
  // We then signal the group that their request has been received

  if (semUp(semgid, sh->requestReceived[table_id]) == -1) {
    perror("error on the down operation for semaphore access (WT)");
    exit(EXIT_FAILURE);
  }

  if (semUp(semgid, sh->waitOrder) == -1) {
    perror("error on the down operation for semaphore access (WT)");
    exit(EXIT_FAILURE);
  }

  // We also have to wait for the order to be received

  if (semDown(semgid, sh->orderReceived) == -1) {
    perror("error on the up operation for semaphore access (WT)");
    exit(EXIT_FAILURE);
  }
}

/**
 *  \brief waiter takes food to table
 *
 *  Waiter updates its state and takes food to table, allowing the meal to
 * start. Group must be informed that food is available. The internal state
 * should be saved.
 *
 */

static void takeFoodToTable(int group_id) {
  if (semDown(semgid, sh->mutex) == -1) { /* enter critical region */
    perror("error on the up operation for semaphore access (WT)");
    exit(EXIT_FAILURE);
  }

  // If the waiter is taking the food to the table, we once again
  // have to update his state
  sh->fSt.st.waiterStat = TAKE_TO_TABLE;
  saveState(nFic, &sh->fSt);

  // Then we signaled the group that their food has arrived and that
  // they can start eating.

  int table_id = sh->fSt.assignedTable[group_id];
  if (semUp(semgid, sh->foodArrived[table_id]) == -1) {
    perror("error on the down operation for semaphore access (WT)");
    exit(EXIT_FAILURE);
  }

  if (semUp(semgid, sh->mutex) == -1) { /* exit critical region */
    perror("error on the down operation for semaphore access (WT)");
    exit(EXIT_FAILURE);
  }
}
