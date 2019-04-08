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
bool simulationDone = false;
typedef  struct timeval previousThiefArrivalTime;
typedef  struct timeval previousHunterArrivalTime;
static int seed = 1;


// mutexes

//semaphores
// sem1: thiefPlaying waited on by thiefs, signaled by Smaug
// sem2: hunterFighting waited on by hunter, signaled by Smaug
// sem3: smaugSleep waited on by Smaug and signaled by theif/hunter after they are done playing/fighting and about to be terminated
// sem4: smaugHasVisitors
// mutex4: smaugJewel ???
const char *visitorCountName = "/visitorCount";
const char *thiefCountName = "/thiefCount";
const char *hunterCountName = "/hunterCount";
const char *thiefPlayName = "/thiefPlay";
const char *hunterFightName = "/hunterFight";
const char *smaugSleepName = "/smaugSleep";
const char *smaugExistName = "/smaugExist";

sem_t *visitorCount;  //Incremented by Thief/Bounty Hunter, decremented by Smaug
sem_t *thiefCount;    // incremented/decremented only by Thief Process
sem_t *hunterCount;   // incremented/decremented only by Bunty Hunter Process
sem_t *thiefPlay;    // decremented(locked) by Thief Process, incremented by Smaug 
sem_t *hunterFight;  // decremented(locked) by Hunter Process, incremented by Smaug 
sem_t *smaugSleep;
sem_t *smaugExist;      


// -------------------- Functions --------------------

int NumberOfVisitors(sem_t * sem, int * value)
{
  sem_getvalue(sem, value);
  return *value; 
}

void try_wait(sem_t *sem){
    if (sem_wait(sem) < 0){
        printf("failed: cannot wait semaphore, terminating program\n");
        exit(0);
    }
}

void try_signal(sem_t *sem){
    if (sem_post(sem) < 0){
        printf("failed: semaphore cannot signal, terminating program\n");
        exit(0);
    }
}

bool PlayFight(){// returns false if smaug wins
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
    int *enchantedThiefsCount;
    int *enchantedHuntersCount;
    int *enchantedVisitorsCount;

    while (smaugJewels < 80 && smaugJewels > 0 && defeatedThiefs < 3 && defeatedHunters < 4)
    {
        // Smaug is sleeping after playing/fighting
        printf("Smaug goes to sleep... \n");
        try_wait(visitorCount);
        printf("Smaug wakes up... \n");
        printf("Smaug takes a deep breath... \n");
        
        sem_getvalue(visitorCount, enchantedVisitorsCount);
        printf("Smaug smells %i Visitors...\n", *enchantedVisitorsCount);
        
        sem_getvalue(thiefCount, enchantedThiefsCount);
        printf("Smaug smells %i Thiefs...\n", *enchantedThiefsCount);

        sem_getvalue(hunterCount, enchantedHuntersCount);
        printf("Smaug smells %i Bounty Hunters...", *enchantedHuntersCount);
      
        
        // If Thief is present, play with a Thief
        if (*enchantedThiefsCount > 0)
        {
            printf("Smaug plays with a Thief... \n");
            try_signal(thiefPlay);
            try_wait(smaugSleep);
            if (PlayFight())
            {
              smaugJewels -= 8;
              printf("Smaug has lost the game against a Thief...\n");
            }
            else
            {
              smaugJewels += 20;
              defeatedThiefs += 1;
              printf("Smaug has won the game against a Thief...\n");
              printf("Smaug has defated %i Thiefs...\n", defeatedThiefs);	
            }
            printf("Smaug's jewels count: %i...\n", smaugJewels);
        }     
        // Otherwise, fight with a Bounty Hunter
        else
        {
            printf("Smaug fights with a Bounty Hunter... \n");
            try_signal(hunterFight);
            try_wait(smaugSleep);
            if (PlayFight()) 
            {
              smaugJewels -= 10;
              printf("Smaug has lost the fight against a Bounty Hunter...\n");
            }
            else
            {
              smaugJewels += 5;
              defeatedHunters += 1;
              printf("Smaug has won the fight against Bounty Hunter...\n");
              printf("Smaug has defated %i Bounty Hunters...\n", defeatedHunters);	
            }
            printf("Smaug's jewels count: %i...\n", smaugJewels);
        }
    }
    
    // Smaug is being terminated
    if (smaugJewels >= 80) {printf("Smaug has 80 smaugJewels now! \n");}
    else if (smaugJewels <= 0) {printf("Smaug has lost his treasure! \n");}
    else if (defeatedThiefs >=3 ) {printf("Smaug has defeated 3 Thiefs! \n");}
    else {printf("Smaug has defeated 4 Bounty Hunters! \n");}
    try_wait(smaugExist);
    exit(0);
}

// Function simulating Thief's behaviour
int thiefRoutine()
{
  printf("Thief %i enters the valley...\n",getpid());
  try_signal(thiefCount);
	printf("Thief %i becomes enchanted...\n",getpid());
  try_signal(visitorCount);	
	try_wait(thiefPlay);
	try_signal(smaugSleep);
  try_wait(thiefCount);
	printf("Thief %i leaves the valley...\n", getpid());
	exit(0);
}

//Function simulating Hunter's behaviour
int hunterRoutine()
{
  printf("Hunter %i enters the valley...\n", getpid());
  try_signal(hunterCount);
  printf("Hunter %i becomes enchanted...\n", getpid());
  try_signal(visitorCount);
	try_wait(hunterFight);
	try_signal(smaugSleep);
  try_wait(hunterCount);
  printf("Bounty Hunter %i leaves the valley...\n", getpid());
	exit(0);
}

// Function generating a Thief
int generateThief()
{
    int thiefInterval;
    int exit_status;
    int *smaugAlive;  

    while(!simulationDone) 
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
    }
    return 0;
}

// Function generating a Hunter
int generateHunter()
{
    int hunterInterval;
    int exit_status;
    int *smaugAlive;

    while(!simulationDone) 
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
        else if(hunterId==0){
            hunterRoutine();
            continue;
        }  
    }
    return 0;
}

// -------------------- Main --------------------
int main()
{
    srand(seed);
    // Request user to enter winProb, maximumThiefInterval, maximumHunterInterval,

    /*
    printf("Enter winProb (0-100): ");
    scanf("%d", &winProb);
    printf("\nEnter maximumThiefInterval in msec: ");
    scanf("%d", &maximumThiefInterval);
    printf("\nEnter maximumHunterInterval in msec: ");
    scanf("%d", &maximumHunterInterval);
    printf("\n");
    */

    winProb = 75;
    maximumThiefInterval = 1000000;
    maximumHunterInterval = 1000000;

    
    // initialize mutexes
    // TO DO
    
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
    
    // Smaug
    else if (smaugId == 0)
    {
        try_signal(smaugExist);
        smaugRoutine(); // smaug will terminate itself in smaugRoutine
    }
	
    // Generate Thiefs and Hunters
    pid_t generateHuntersId = fork();
    if(generateHuntersId<0){
        perror("Failed to initialize generateHunters. Fork return value <= 0. \n");
        exit(EXIT_FAILURE);
    }
    else if(generateHuntersId==0){
        generateHunter();
    }
    
	pid_t generateThiefsId = fork();
    if(generateThiefsId<0){
        perror("Failed to initialize generateThiefs. Fork return value <= 0. \n");
        exit(EXIT_FAILURE);
    }
    else if(generateThiefsId==0){
        generateThief();
    }

    // wait until childrem processes have terminated before moving on in the code
    int exit_status;
    pid_t wpid;
    while ((wpid = wait(&exit_status)) > 0){};
    

    // destroy mutexes
    // TO DO
    // destroy semaphroes
    sem_destroy (visitorCount);
    sem_destroy (thiefCount);
    sem_destroy (hunterCount);
    sem_destroy (thiefPlay);
    sem_destroy (hunterFight);
    sem_destroy (smaugSleep);
    
    // terminate main
    exit(0);
}

