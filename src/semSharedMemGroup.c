/**
 *  \file semSharedMemGroup.c (implementation file)
 *
 *  \brief Problem name: Restaurant
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the groups:
 *     \li goToRestaurant
 *     \li checkInAtReception
 *     \li orderFood
 *     \li waitFood
 *     \li eat
 *     \li checkOutAtReception
 *
 *  \author Nuno Lau - December 2023
 */

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

static void goToRestaurant(int id);
static void checkInAtReception(int id);
static void orderFood(int id);
static void waitFood(int id);
static void eat(int id);
static void checkOutAtReception(int id);

/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the
 * problem: the group.
 */
int main(int argc, char *argv[]) {
  int key;    /*access key to shared memory and semaphore set */
  char *tinp; /* numerical parameters test flag */
  int n;

  /* validation of command line parameters */
  if (argc != 5) {
    freopen("error_GR", "a", stderr);
    fprintf(stderr, "Number of parameters is incorrect!\n");
    return EXIT_FAILURE;
  } else {
    //  freopen (argv[4], "w", stderr);
    setbuf(stderr, NULL);
  }

  n = (unsigned int)strtol(argv[1], &tinp, 0);
  if ((*tinp != '\0') || (n >= MAXGROUPS)) {
    fprintf(stderr, "Group process identification is wrong!\n");
    return EXIT_FAILURE;
  }
  strcpy(nFic, argv[2]);
  key = (unsigned int)strtol(argv[3], &tinp, 0);
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

  /* simulation of the life cycle of the group */
  goToRestaurant(n);
  checkInAtReception(n);
  orderFood(n);
  waitFood(n);
  eat(n);
  checkOutAtReception(n);

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
 *  \brief normal distribution generator with zero mean and stddev deviation.
 *
 *  Generates random number according to normal distribution.
 *
 *  \param stddev controls standard deviation of distribution
 */
static double normalRand(double stddev) {
  int i;

  double r = 0.0;
  for (i = 0; i < 12; i++) {
    r += random() / (RAND_MAX + 1.0);
  }
  r -= 6.0;

  return r * stddev;
}

/**
 *  \brief group goes to restaurant
 *
 *  The group takes its time to get to restaurant.
 *
 *  \param id group id
 */
static void goToRestaurant(int id) {
  double startTime = sh->fSt.startTime[id] + normalRand(STARTDEV);

  if (startTime > 0.0) {
    usleep((unsigned int)startTime);
  }
}

/**
 *  \brief group eats
 *
 *  The group takes his time to eat a pleasant dinner.
 *
 *  \param id group id
 */
static void eat(int id) {
  double eatTime = sh->fSt.eatTime[id] + normalRand(EATDEV);

  if (eatTime > 0.0) {
    usleep((unsigned int)eatTime);
  }
}

/**
 *  \brief group checks in at reception
 *
 *  Group should, as soon as receptionist is available, ask for a table,
 *  signaling receptionist of the request.
 *  Group may have to wait for a table in this method.
 *  The internal state should be saved.
 *
 *  \param id group id
 *
 *  \return true if first group, false otherwise
 */
static void checkInAtReception(int group_id) {
  // Before this group can do anything, we need to check if he can
  // make a request to the receptionist

  if (semDown(semgid, sh->receptionistRequestPossible) == -1) {
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }
  // Only after we know the receptionist is available can we start to
  // formulate the request
  request req;

  if (semDown(semgid, sh->mutex) == -1) { /* enter critical region */
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  // If he can make a request to the receptionist, we need to update its state
  sh->fSt.st.groupStat[group_id] = ATRECEPTION;
  saveState(nFic, &sh->fSt);

  // Now we have to formulate the request to the receptionist
  req.reqType = TABLEREQ;
  req.reqGroup = group_id;

  // And we give the request to the receptionist
  sh->fSt.receptionistRequest = req;

  // We also have to signal him that the request data is now available

  if (semUp(semgid, sh->receptionistReq) == -1) {
    perror("error on the up operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  if (semUp(semgid, sh->mutex) == -1) { /* exit critical region */
    perror("error on the up operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  // Now we have to wait for a table to be assigned to the group
  if (semDown(semgid, sh->waitForTable[group_id]) == -1) {
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }
}

/**
 *  \brief group orders food.
 *
 *  The group should update its state, request food to the waiter and
 *  wait for the waiter to receive the request.
 *
 *  The internal state should be saved.
 *
 *  \param id group id
 */
static void orderFood(int group_id) {
  // Before we can do anything, we need to check whether or not the waiter is
  // available to take a request

  if (semDown(semgid, sh->waiterRequestPossible) == -1) {
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  // If the waiter is available, we can formulate a request
  request req;

  if (semDown(semgid, sh->mutex) == -1) { /* enter critical region */
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  // After entering the critical region, we can get the required data
  // to formulate the request
  req.reqGroup = group_id;
  req.reqType = FOODREQ;

  // After that, we can signal to the waiter that he has a request
  sh->fSt.waiterRequest = req;
  if (semUp(semgid, sh->waiterRequest) == -1) { /* exit critical region */
    perror("error on the up operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }
  // Only when the waiter is available to make a request can we change
  // our state
  sh->fSt.st.groupStat[group_id] = FOOD_REQUEST;
  saveState(nFic, &sh->fSt);

  // We also need the table id
  int table_id = sh->fSt.assignedTable[group_id];

  if (semUp(semgid, sh->mutex) == -1) { /* exit critical region */
    perror("error on the up operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  // Now we wait for the waiter to get the request
  if (semDown(semgid, sh->requestReceived[table_id]) == -1) {
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }
}

/**
 *  \brief group waits for food.
 *
 *  The group updates its state, and waits until food arrives.
 *  It should also update state after food arrives.
 *  The internal state should be saved twice.
 *
 *  \param id group id
 */
static void waitFood(int group_id) {
  if (semDown(semgid, sh->mutex) == -1) { /* enter critical region */
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  // The first thing we need to do is update the state of the group
  sh->fSt.st.groupStat[group_id] = WAIT_FOR_FOOD;
  saveState(nFic, &sh->fSt);

  // Now we need to get the id of the table that was assigned to us
  int table_id = sh->fSt.assignedTable[group_id];

  if (semUp(semgid, sh->mutex) == -1) { /* exit critical region */
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  // First we have to check if the food has arrived

  if (semDown(semgid, sh->foodArrived[table_id]) == -1) {
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  if (semDown(semgid, sh->mutex) == -1) { /* enter critical region */
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  // If we have already received the food, we can eat
  // So we can update our state

  sh->fSt.st.groupStat[group_id] = EAT;
  saveState(nFic, &sh->fSt);

  if (semUp(semgid, sh->mutex) == -1) { /* enter critical region */
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }
}

/**
 *  \brief group check out at reception.
 *
 *  The group, as soon as receptionist is available, updates its state and
 *  sends a payment request to the receptionist.
 *  Group waits for receptionist to acknowledge payment.
 *  Group should update its state to LEAVING, after acknowledge.
 *  The internal state should be saved twice.
 *
 *  \param id group id
 */
static void checkOutAtReception(int group_id) {

  // To checkout, we need to check whether or not the receptionist is available
  // to receive a request

  if (semDown(semgid, sh->receptionistRequestPossible) == -1) {
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  // If the receptionist is available, we can formulate the request
  request req;

  if (semDown(semgid, sh->mutex) == -1) { /* enter critical region */
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  // Since the receptionist is available, we can update the state of the group
  sh->fSt.st.groupStat[group_id] = CHECKOUT;
  saveState(nFic, &sh->fSt);

  // Once we can get all the data, we can fill the request and then send it to
  // the receptionist

  int table_id = sh->fSt.assignedTable[group_id];
  req.reqGroup = group_id;
  req.reqType = BILLREQ;

  // Then we give the request to the receptionist, signaling him
  sh->fSt.receptionistRequest = req;
  if (semUp(semgid, sh->receptionistReq) == -1) {
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  if (semUp(semgid, sh->mutex) == -1) { /* exit critical region */
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  // Now we wait for the receptionist to process the payment
  if (semDown(semgid, sh->tableDone[table_id]) == -1) {
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  if (semDown(semgid, sh->mutex) == -1) { /* enter critical region */
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }

  sh->fSt.st.groupStat[group_id] = LEAVING;
  saveState(nFic, &sh->fSt);

  if (semUp(semgid, sh->mutex) == -1) { /* exit critical region */
    perror("error on the down operation for semaphore access (CT)");
    exit(EXIT_FAILURE);
  }
}
