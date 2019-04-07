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
sem_t *thiefPlaying;
sem_t *hunterFighting;
sem_t *smaugHasVisitors;
sem_t *smaugSleep;
pthread_mutex_t thiefsCount;
pthread_mutex_t huntersCount;
int enchantedThiefs = 0;
int enchantedHunters = 0;

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
    int numberOfVisitors;

    while (smaugJewels < 80 && smaugJewels > 0 && defeatedThiefs < 3 && defeatedHunters < 4)
    {
        // Smaug is sleeping after playing/fighting
        printf("Smaug goes to sleep... \n");
        try_wait(smaugHasVisitors);
        printf("Smaug wakes up... \n");
        printf("Smaug takes a deep breath... \n");
        printf("Smaug smells: %i Thiefs \n", NumberOfVisitors(thiefPlaying,&numberOfVisitors));
         
        // If Thief is present, play with a Thief
        if (NumberOfVisitors(thiefPlaying,&numberOfVisitors) > 0)
        {
            printf("Smaug smells a Thief... \n");
            try_signal(thiefPlaying);
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
            try_wait(smaugSleep);
        }     
        // Otherwise, fight with a Bounty Hunter
        else
        {
            printf("Smaug smells a Bounty Hunter... \n");
            try_signal(hunterFighting);
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
            try_wait(smaugSleep);
        }
    }
    
    // Smaug is being terminated
    if (smaugJewels >= 80) {printf("Smaug has 80 smaugJewels now! \n");}
    else if (smaugJewels <= 0) {printf("Smaug has lost his treasure! \n");}
    else if (defeatedThiefs >3 ) {printf("Smaug has defeated 3 Thiefs! \n");}
    else {printf("Smaug has defeated 4 Bounty Hunters! \n");}
    simulationDone = true;
    exit(0);
}

// Function simulating Thief's behaviour
int thiefRoutine()
{
  printf("Thief %i enters the valley...\n",getpid());
	try_signal(smaugHasVisitors);
	printf("Thief %i becomes enchanted...\n",getpid());	
	try_wait(thiefPlaying);
	try_signal(smaugSleep);
	printf("Thief %i leaves the valley...\n", getpid());
	exit(0);
    return 0;
}

//Function simulating Hunter's behaviour
int hunterRoutine()
{
  printf("Hunter %i enters the valley...\n", getpid());
  try_signal(smaugHasVisitors);
  printf("Hunter %i becomes enchanted...\n", getpid());
	try_wait(hunterFighting);
  printf("Bounty Hunter %i leaves the valley...\n", getpid());
	try_signal(smaugSleep);
	exit(0);
  return 0;
}

// Function generating a Thief
int generateThief()
{
    int thiefInterval;
    int exit_status;
    while(!simulationDone) {
        pid_t thiefId = fork();
        if(thiefId<0){
            perror("Failed to generate thief. Fork return value <= 0. \n");
            exit(EXIT_FAILURE);
        }
        else if(thiefId==0){
            thiefRoutine();
            continue;
        }
        else
        {
          waitpid(-1, &exit_status, WNOHANG); // -1 means wait for any, WNOHANG ensures non-blocking
          thiefInterval = 0 + rand()%(maximumThiefInterval - 0 + 1);
          usleep(thiefInterval);
        }
    }
    return 0;
}

// Function generating a Hunter
int generateHunter()
{
    int hunterInterval;
    int exit_status;
    while(!simulationDone) {
        pid_t hunterId = fork();
        if(hunterId<0){
            perror("Failed to generate hunter. Fork return value <= 0. \n");
            exit(EXIT_FAILURE);
        }
        else if(hunterId==0){
            hunterRoutine();
            continue;
        }
        else
        {
          waitpid(-1, &exit_status, WNOHANG); // -1 means wait for any, WNOHANG ensures non-blocking
          hunterInterval = 0 + rand()%(maximumHunterInterval - 0 + 1);
          usleep(hunterInterval);
        }
    }
    return 0;
}

// -------------------- Main --------------------
int main()
{
    srand(seed);
    // Request user to enter winProb, maximumThiefInterval, maximumHunterInterval,
    printf("Enter winProb (0-100): ");
    scanf("%d", &winProb);
    printf("\nEnter maximumThiefInterval in msec: ");
    scanf("%d", &maximumThiefInterval);
    printf("\nEnter maximumHunterInterval in msec: ");
    scanf("%d", &maximumHunterInterval);
    printf("\n");
    
    // initialize mutexes
    // TO DO
    
    // initialize semaphroes
    smaugHasVisitors = sem_open("smaugHasVisitors", O_CREAT, 0600, 0);
    thiefPlaying = sem_open("thiefPlaying", O_CREAT, 0600, 0);
    hunterFighting = sem_open("hunterFighting", O_CREAT, 0600, 0);
    smaugSleep = sem_open("smaugSleep", O_CREAT, 0600, 0);
    
    if (smaugHasVisitors == SEM_FAILED || thiefPlaying == SEM_FAILED || hunterFighting == SEM_FAILED || smaugSleep == SEM_FAILED){
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
    sem_destroy(smaugHasVisitors);
    sem_destroy(thiefPlaying);
    sem_destroy(hunterFighting);
    sem_destroy(smaugSleep);
    
    // terminate main
    exit(0);
}

