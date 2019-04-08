#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include<pthread.h>

// -------------------- Global Varaibles --------------------
  int winProb;
  int maximumThiefInterval;
  int maximumHunterInterval;
  typedef  struct timeval previousThiefArrivalTime;
  typedef  struct timeval previousHunterArrivalTime;
  static int seed = 1;

// semaphores purpose:
// visitorCount:  total count of visitors; used to wake up Smaug
// thiefCount:    total count of enchanted Thiefs, used since Smaugs prefers Thiefs over Hunters
// hunterCount:   total count of enchanted Hunters, used since value of 0 for visitorCount will still wake up Smaug
// thiefPlay:     total number of Thiefs that are waiting to play with Smaug, used to command Thief to play
// hunterFight:   total number of Hunters that are waiting to fight with Smaug, used to command Hunter to fight
// smaugSleep:    semaphore controlling whether or not Smaug can go back to sleep (after playing/fighting)
// smaugExist:    semaphore that allows Smaug process inform other processes about its termination
sem_t *visitorCount;  //incremented by Thief/Bounty Hunter, decremented by Smaug
sem_t *thiefCount;    // incremented/decremented only by Thief Process
sem_t *hunterCount;   // incremented/decremented only by Bunty Hunter Process
sem_t *thiefPlay;     // decremented(locked) by Thief Process, incremented by Smaug 
sem_t *hunterFight;   // decremented(locked) by Hunter Process, incremented by Smaug 
sem_t *smaugSleep;    // decremented(locked) by Smaug, incremented by Thief/Hunter
sem_t *smaugExist;    // incremented and decremented by Smaug

// semaphore names to created Named Semaphores
const char *visitorCountName = "/visitorCount";
const char *thiefCountName = "/thiefCount";
const char *hunterCountName = "/hunterCount";
const char *thiefPlayName = "/thiefPlay";
const char *hunterFightName = "/hunterFight";
const char *smaugSleepName = "/smaugSleep";
const char *smaugExistName = "/smaugExist";

// -------------------- Functions --------------------
void try_wait(sem_t *sem){
    if (sem_wait(sem) < 0){
        printf("failed: cannot wait semaphore, terminating program\n");
        fflush(stdout);
        exit(0);
    }
}

void try_signal(sem_t *sem){
    if (sem_post(sem) < 0){
        printf("failed: semaphore cannot signal, terminating program\n");
        fflush(stdout);
        exit(0);
    }
}

// returns false if smaug wins
bool PlayFight()
{
    if(winProb == 100){return true;}
    else if (winProb == 0){return false;}
    return rand()%(100 - 0 + 1) < winProb;
}

// Function simulating Smaug's behaviour
int smaugRoutine()
{
    int smaugJewels = 30;
    int defeatedThiefs = 0;
    int defeatedHunters = 0;
    int enchantedThiefsCount;
    int enchantedHuntersCount;
    int enchantedVisitorsCount;

    while (smaugJewels < 80 && smaugJewels > 0 && defeatedThiefs < 3 && defeatedHunters < 4)
    {
      // Smaug is sleeping after playing/fighting
      printf("Smaug goes to sleep... \n");
      fflush(stdout);
      try_wait(visitorCount);
      printf("Smaug wakes up... \n");
      fflush(stdout);
      printf("Smaug takes a deep breath... \n");
      fflush(stdout);
      
      sem_getvalue(thiefCount, &enchantedThiefsCount);
      printf("Smaug smells %i Thiefs...\n", enchantedThiefsCount);
      fflush(stdout);

      sem_getvalue(hunterCount, &enchantedHuntersCount);
      printf("Smaug smells %i Bounty Hunters...\n", enchantedHuntersCount);
      fflush(stdout);
      
      // If Thief is present, play with a Thief
      if (enchantedThiefsCount > 0)
      {
          printf("Smaug plays with a Thief... \n");
          fflush(stdout);
          try_signal(thiefPlay);
          try_wait(smaugSleep);
          if (PlayFight())
          {
            smaugJewels -= 8;
            printf("Smaug has lost the game against a Thief...\n");
            fflush(stdout);
          }
          else
          {
            smaugJewels += 20;
            defeatedThiefs += 1;
            printf("Smaug has won the game against a Thief...\n");
            fflush(stdout);
            printf("Smaug has defated %i Thiefs...\n", defeatedThiefs);
            fflush(stdout);	
          }
          printf("Smaug's jewels count: %i...\n", smaugJewels);
      }     
      
      // Otherwise, fight with a Bounty Hunter
      // NOTE: we still must check for # of enchanted hunters: semaphore of value 0 allows successfull lock with sem_wait
      else if (enchantedHuntersCount)
      {
          printf("Smaug fights with a Bounty Hunter... \n");
          fflush(stdout);
          try_signal(hunterFight);
          try_wait(smaugSleep);
          if (PlayFight()) 
          {
            smaugJewels -= 10;
            printf("Smaug has lost the fight against a Bounty Hunter...\n");
            fflush(stdout);
          }
          else
          {
            smaugJewels += 5;
            defeatedHunters += 1;
            printf("Smaug has won the fight against Bounty Hunter...\n");
            fflush(stdout);
            printf("Smaug has defated %i Bounty Hunters...\n", defeatedHunters);
            fflush(stdout);	
          }
          printf("Smaug's jewels count: %i...\n", smaugJewels);
          fflush(stdout);
      }
  }
  
  // Report teason for Smaug temination
  if (smaugJewels >= 80) {printf("Smaug has 80 smaugJewels now! \n");}
  else if (smaugJewels <= 0) {printf("Smaug has lost his treasure! \n");}
  else if (defeatedThiefs >=3 ) {printf("Smaug has defeated 3 Thiefs! \n");}
  else {printf("Smaug has defeated 4 Bounty Hunters! \n");}
  fflush(stdout);
  printf("*************************************************\n************* SMAUG TERMINATES HERE *************\n*************************************************\n");
  fflush(stdout);

  try_wait(smaugExist); //Message dependant processes that Smaug is about to be terminated

  // Notice and message every enchanted Thief that Smaug is about to be terminated ("Remove enchantment")
  sem_getvalue(thiefCount, &enchantedThiefsCount);
  printf("%i Thiefs remain in the valley. LEAVE NOW!...\n", enchantedThiefsCount);
  fflush(stdout);
  for (int i=0; i<enchantedThiefsCount; i++)
  {
    try_signal(thiefPlay);
    try_signal(visitorCount);
  }

  // Notice and message every enchanted Hunter that Smaug is about to be terminated ("Remove enchantment")
  sem_getvalue(hunterCount, &enchantedHuntersCount);
  printf("%i Bounty Hunters remain in the valley. LEAVE NOW!...\n", enchantedHuntersCount);
  fflush(stdout);
  for (int i=0; i<enchantedHuntersCount; i++)
  {
    try_signal(hunterFight);
    try_signal(visitorCount);
  }

  exit(0);
}

// Function simulating Thief's behaviour
int thiefRoutine()
{
  int smaugAlive; 
  printf("Thief %i enters the valley...\n",getpid());
  fflush(stdout);
  try_signal(thiefCount);
  try_signal(visitorCount);	
  sem_getvalue(smaugExist, &smaugAlive);
  if (!smaugAlive)
  {
    // Executed when Thief is generated after Smaug terminates
    printf("Smaug is not here anymore: Thief %i leaves the valley...\n", getpid());
    fflush(stdout);
    exit(0);
  }
  printf("Thief %i becomes enchanted...\n",getpid());
  fflush(stdout);
  try_wait(thiefPlay);
	try_signal(smaugSleep);
  try_wait(thiefCount);
	printf("Thief %i leaves the valley...\n", getpid());
  fflush(stdout);
	exit(0);
}

//Function simulating Hunter's behaviour
int hunterRoutine()
{
  int smaugAlive;
  printf("Hunter %i enters the valley...\n", getpid());
  fflush(stdout);
  try_signal(hunterCount);
  try_signal(visitorCount);
  sem_getvalue(smaugExist, &smaugAlive);
  if (!smaugAlive)
  {
    // Executed when Hunter is generated after Smaug terminates
    printf("Smaug is not here anymore: Hunter %i leaves the valley...\n", getpid());
    fflush(stdout);
    exit(0);
  }
  printf("Hunter %i becomes enchanted...\n", getpid());
  fflush(stdout);
	try_wait(hunterFight);
	try_signal(smaugSleep);
  try_wait(hunterCount);
  printf("Bounty Hunter %i leaves the valley...\n", getpid());
  fflush(stdout);
	exit(0);
}

// Function generating a Thief
int generateThief()
{
  int thiefInterval;
  int exit_status;
  int smaugAlive = 1;

  while(smaugAlive) 
  {
      waitpid(-1, &exit_status, WNOHANG); // -1 means wait for any, WNOHANG ensures non-blocking
      thiefInterval = 0 + rand()%(maximumThiefInterval - 0 + 1);
      usleep(thiefInterval);
      pid_t thiefId = fork();
      if(thiefId<0)
      {
          perror("Failed to generate thief. Fork return value <= 0. \n");
          exit(EXIT_FAILURE);
      }
      else if(thiefId==0){
          thiefRoutine();
          continue;
      }
      else 
      {
        // check if Smaug process still exists
        sem_getvalue(smaugExist, &smaugAlive);
      }
  }
  return 0;
}

// Function generating a Hunter
int generateHunter()
{
  int hunterInterval;
  int exit_status;
  int smaugAlive = 1;

  while(smaugAlive) 
  { 
      waitpid(-1, &exit_status, WNOHANG); // -1 means wait for any, WNOHANG ensures non-blocking
      hunterInterval = 0 + rand()%(maximumHunterInterval - 0 + 1);
      usleep(hunterInterval);
      pid_t hunterId = fork();
      if(hunterId<0)
      {
          perror("Failed to generate hunter. Fork return value <= 0. \n");
          exit(EXIT_FAILURE);
      }
      else if(hunterId==0)
      {
          hunterRoutine();
          continue;
      }
      else 
      {
        // check if Smaug process still exists
        sem_getvalue(smaugExist, &smaugAlive);
      }  
  }
  return 0;
}

// -------------------- Main --------------------
int main()
{
  srand(seed);
  // Request user to enter winProb, maximumThiefInterval, maximumHunterInterval
  printf("Enter winProb (0-100): ");
  scanf("%d", &winProb);
  printf("\nEnter maximumThiefInterval in miliseconds: ");
  scanf("%d", &maximumThiefInterval);
  maximumThiefInterval = 1000*maximumThiefInterval;
  printf("\nEnter maximumHunterInterval in miliseconds: ");
  scanf("%d", &maximumHunterInterval);
  maximumHunterInterval = 1000*maximumHunterInterval;
  printf("\n");
  
  // initialize semaphroes
  visitorCount = sem_open(visitorCountName, O_CREAT, 0600, 0);
  thiefCount = sem_open(thiefCountName, O_CREAT, 0600, 0);
  hunterCount = sem_open(hunterCountName, O_CREAT, 0600, 0);
  thiefPlay = sem_open(thiefPlayName, O_CREAT, 0600, 0);
  hunterFight = sem_open(hunterFightName, O_CREAT, 0600, 0);
  smaugSleep = sem_open(smaugSleepName, O_CREAT, 0600, 0);
  smaugExist = sem_open(smaugExistName, O_CREAT, 0600, 0);

  if (visitorCount == SEM_FAILED || thiefCount == SEM_FAILED || hunterCount == SEM_FAILED || thiefPlay == SEM_FAILED || hunterFight == SEM_FAILED || smaugSleep == SEM_FAILED || smaugExist == SEM_FAILED )
  {
      perror("\n Failed to open semphores, terminating program\n");
      exit(0);
  }
  
  // Initializing Smaug
  pid_t smaugId = fork();
  if (smaugId < 0)
  {
      perror("Failed to initialize Smaug. Fork return value <= 0. \n");
      exit(EXIT_FAILURE);
  }  
  else if (smaugId == 0)
  {
      // *** Executed only by Smaug process
      try_signal(smaugExist);
      smaugRoutine();
      // smaug will terminate itself inside smaugRoutine 
  }

  // Initialize generateHunter
  pid_t generateHuntersId = fork();
  if(generateHuntersId<0){
      perror("Failed to initialize generateHunters. Fork return value <= 0. \n");
      exit(EXIT_FAILURE);
  }
  else if(generateHuntersId==0){
      // *** Executed only by generateHunter process ***
      generateHunter(); 
  }
  
  // Initialize generateThief
  pid_t generateThiefsId = fork();
  if(generateThiefsId<0){
      perror("Failed to initialize generateThiefs. Fork return value <= 0. \n");
      exit(EXIT_FAILURE);
  }
  else if(generateThiefsId==0){
      // *** Executed only by generateThief process ***
      generateThief(); 
  }

  // wait until children processes have terminated before moving on in the code
  int exit_status;
  pid_t wpid;
  while ((wpid = waitpid(-1, &exit_status, WNOHANG)) >= 0){}; 

  // destroy semaphroes
  sem_unlink (visitorCountName);
  sem_unlink (thiefCountName);
  sem_unlink (hunterCountName);
  sem_unlink (thiefPlayName);
  sem_unlink (hunterFightName);
  sem_unlink (smaugSleepName);
  sem_unlink (smaugExistName);
  
  // terminate main
  exit(0);
}