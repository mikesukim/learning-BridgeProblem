#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TRUE  1
#define FALSE 0
#define MAXIMUM_SIZE 100

//enum
typedef enum {car,truck} VehicleType;
typedef enum {north,south} VehicleDirection;
typedef enum {northBZone,southBZone,bridgeZone} Zone; //zone == parkinglot
typedef enum {arrived,crossing,exiting} vehicleStatus; 

//struct
typedef struct VehicleInfo{
    int id;
    VehicleType type;
    VehicleDirection direction;
    int groupNum;
} VehicleInfo;

typedef struct SouthBoundZone{
    VehicleInfo vehicleList[MAXIMUM_SIZE];
    int carCount;    
    int truckCount;
    int vehicleCount;
} SouthBoundZone;

typedef struct NorthBoundZone{
    VehicleInfo vehicleList[MAXIMUM_SIZE];
    int carCount;
    int truckCount;
    int vehicleCount;
} NorthBoundZone;

typedef struct Bridge{
    VehicleInfo vehicleList[MAXIMUM_SIZE]; 
    int carCount;
    int truckCount;
    int vehicleCount;
    int isFull;
    VehicleDirection direction;
    VehicleType type;
} Bridge;


//mutex&cond   
pthread_mutex_t GroupNumLock;          
pthread_cond_t GroupNumCond;  

pthread_mutex_t NorthBoundLock; 
pthread_cond_t NorthBoundCond;         
pthread_mutex_t SouthBoundLock;          
pthread_cond_t SouthBoundCond;
pthread_mutex_t BridgeLock; 
pthread_cond_t BridgeNorthCarCond;
pthread_cond_t BridgeNorthTruckCond;
pthread_cond_t BridgeSouthCarCond;
pthread_cond_t BridgeSouthTruckCond;

pthread_mutex_t PrintLock;
pthread_cond_t PrintCond;

pthread_mutex_t DirectionLock;
pthread_cond_t DirectionCond;


//gloabal variables
SouthBoundZone southBoundZone;
NorthBoundZone northBoundZone;
Bridge bridge;

    //used for waiting all other vehicles to come in group
int currentGroupIndex = 0;  
int currentVehicleCount= 0;
int vehicleCountInGroup = 0;
int startedVehicleCount = 0;

//methods
int getScheduleNumFromInput(int argc, char *argv[]);
void launchSchedule(int scheduleNum);
void initiateVehicles(double carProb, double truckProb, int totalVehiclesCount, int startingVehiclesCount, int vehicleGroupCount, int delays[]);
void Arrive(VehicleInfo *info);
void Cross(VehicleInfo *info);
void Leave(VehicleInfo *info);
void addVehicleAtZone(VehicleInfo *info , int forBridge);
void subtractVehicleAtZone(VehicleInfo *info , int forBridge);
void printOutAllZoneStatus();
void printOutZoneStatus(Zone zone);
void printOutVehicleStatus(vehicleStatus status, VehicleInfo *info);

//// Thread Methods
void *Vehicle_Routine(void *vehicleInfo){
    
    VehicleInfo *info;
    info = (VehicleInfo *) vehicleInfo;

    Arrive(info);
    Cross(info);
    Leave(info);
    
    pthread_exit(NULL);
}


void Arrive(VehicleInfo *info){

    int id = info->id;
    VehicleType type = info->type;
    VehicleDirection direction = info->direction;
    int groupNum = info->groupNum;    


    //print arrival
    printOutVehicleStatus(arrived,info);

    //Counts vehicle status of north & south bound
    if (direction == north)
    {
        pthread_mutex_lock(&NorthBoundLock);
        addVehicleAtZone(info, FALSE);
        pthread_mutex_unlock(&NorthBoundLock);
    }
    else
    {
        pthread_mutex_lock(&SouthBoundLock);
        addVehicleAtZone(info, FALSE);
        pthread_mutex_unlock(&SouthBoundLock);
    }

    //wait until other vehicle in same group to arrive
    pthread_mutex_lock(&GroupNumLock);
    currentVehicleCount ++;
    while(currentVehicleCount < vehicleCountInGroup){
        pthread_cond_wait(&GroupNumCond,&GroupNumLock);
    }
    pthread_cond_signal(&GroupNumCond);
    pthread_mutex_unlock(&GroupNumLock);

    pthread_mutex_lock(&GroupNumLock);
    startedVehicleCount++;
    if(startedVehicleCount == vehicleCountInGroup){
        currentVehicleCount = 0;
        startedVehicleCount = 0;
    }
    pthread_mutex_unlock(&GroupNumLock);
    
//////
////RESTRICTION LOGIC STARTS
    pthread_mutex_lock(&BridgeLock);

    if (type == car){
        //Car logic
            //checking truck exist || bridge is full || bridge direction match
        while(northBoundZone.truckCount > 0 || southBoundZone.truckCount > 0 || bridge.carCount >= 3 || bridge.truckCount > 0 || bridge.direction != direction){
            (direction == north) ? pthread_cond_wait(&BridgeNorthCarCond, &BridgeLock) : pthread_cond_wait(&BridgeSouthCarCond, &BridgeLock);
            pthread_cond_signal(&BridgeNorthCarCond);
            pthread_cond_signal(&BridgeNorthTruckCond);
            pthread_cond_signal(&BridgeSouthCarCond);
            pthread_cond_signal(&BridgeSouthTruckCond);
        }
    }
    else{
        //Truck logic
        while(bridge.carCount > 0 || bridge.truckCount > 0 || bridge.direction != direction){
            (direction == north) ? pthread_cond_wait(&BridgeNorthTruckCond, &BridgeLock) : pthread_cond_wait(&BridgeSouthTruckCond, &BridgeLock);
            pthread_cond_signal(&BridgeNorthCarCond);
            pthread_cond_signal(&BridgeNorthTruckCond);
            pthread_cond_signal(&BridgeSouthCarCond);
            pthread_cond_signal(&BridgeSouthTruckCond);
        }
       
    }

    
////YOU MADE IT !!! PREPARE FOR CROSSING!
    addVehicleAtZone(info, TRUE);
    if (direction == north)
    {
        pthread_mutex_lock(&NorthBoundLock);
        subtractVehicleAtZone(info, FALSE);
        pthread_mutex_unlock(&NorthBoundLock);
    }
    else
    {
        pthread_mutex_lock(&SouthBoundLock);
        subtractVehicleAtZone(info, FALSE);
        pthread_mutex_unlock(&SouthBoundLock);
    }

    printOutVehicleStatus(crossing,info);
    printOutAllZoneStatus();

    pthread_mutex_unlock(&BridgeLock);
}
void Cross(VehicleInfo *info){
    
    sleep(2);

}

void Leave(VehicleInfo *info){
    
    int id = info->id;
    VehicleType type = info->type;
    VehicleDirection direction = info->direction;
    int groupNum = info->groupNum; 

    pthread_mutex_lock(&BridgeLock);

    subtractVehicleAtZone(info, TRUE);
    printOutVehicleStatus(exiting,info);
    printOutAllZoneStatus();
    
    ////logic after getting out
    if (type == car){
    //Car
        //switch bridge direction when there are no cars waiting at bridge direction
        if(bridge.direction == north){
            if(northBoundZone.carCount == 0){
                if(bridge.carCount == 0){
                    bridge.direction = south;
                    pthread_cond_signal(&BridgeSouthCarCond);
                }
            }
            else{
                    pthread_cond_signal(&BridgeNorthCarCond);
            }
        }
        else{
            if(southBoundZone.carCount == 0){
                if(bridge.carCount == 0){
                    bridge.direction = north;
                    pthread_cond_signal(&BridgeNorthCarCond);
                }
            }
            else{
                pthread_cond_signal(&BridgeSouthCarCond);
            }
        }
    }
    else{
    //Truck
        if (southBoundZone.truckCount > 0 || northBoundZone.truckCount > 0){
                //switch bridge direction
            if (bridge.direction == north){
                if (southBoundZone.truckCount != 0){
                    bridge.direction = south;
                    pthread_cond_signal(&BridgeSouthTruckCond);
                }
                else{
                    pthread_cond_signal(&BridgeNorthTruckCond);
                }
            }
            else{
                if (northBoundZone.truckCount != 0){
                    bridge.direction = north;
                    pthread_cond_signal(&BridgeNorthTruckCond);
                }
                else{
                    pthread_cond_signal(&BridgeSouthTruckCond);
                }
            }
        }
        else if(southBoundZone.truckCount == 0 && northBoundZone.truckCount == 0){
            if (bridge.direction == south){
                if(southBoundZone.carCount == 0){
                    bridge.direction = north;
                    pthread_cond_signal(&BridgeNorthCarCond);
                }
                else{
                    pthread_cond_signal(&BridgeSouthCarCond);
                }
            }
            else{
                if(northBoundZone.carCount == 0){
                    bridge.direction = south;
                    pthread_cond_signal(&BridgeSouthCarCond);
                }
                else{
                    pthread_cond_signal(&BridgeNorthCarCond);
                }  
            }
        }
    }
    pthread_mutex_unlock(&BridgeLock);
    
}

//// main
int main (int argc, char *argv[])
{
    //mutex&cond init
    pthread_mutex_init(&GroupNumLock, NULL);
    pthread_mutex_init(&NorthBoundLock, NULL);
    pthread_mutex_init(&SouthBoundLock, NULL);
    pthread_mutex_init(&BridgeLock, NULL);
    pthread_mutex_init(&PrintLock, NULL);
    pthread_mutex_init(&DirectionLock, NULL);
    pthread_cond_init (&GroupNumCond, NULL);
    pthread_cond_init (&NorthBoundCond, NULL);
    pthread_cond_init (&SouthBoundCond, NULL);
    pthread_cond_init (&BridgeNorthCarCond, NULL);
    pthread_cond_init (&BridgeSouthCarCond, NULL);
    pthread_cond_init (&BridgeNorthTruckCond, NULL);
    pthread_cond_init (&BridgeSouthTruckCond, NULL);
    pthread_cond_init (&PrintCond, NULL);
    pthread_cond_init (&DirectionCond, NULL);

    //launch Schedule
    int scheduleNum = getScheduleNumFromInput(argc,argv);
    launchSchedule(scheduleNum);
}

//// Methods 
int getScheduleNumFromInput(int argc, char *argv[]){
    
    if (argc < 2) {
	    fprintf(stderr,"usage: ./bridge_crossing <schedule number>.bridge_crossing\n");
	    exit(0);
    }

    int scheduleNum = atoi(argv[1]);
    return scheduleNum;
}

void launchSchedule(int scheduleNum){

    switch(scheduleNum){
        case 1 : {
            int delays[] = { 10 };
            initiateVehicles(1.0,0.0,20,10,2,delays);
            break;
        }
        case 2 : {
            int delays[] = { 10 };
            initiateVehicles(0.0,1.0,20,10,2,delays);
            break;
        }
        case 3 : {
            int delays[] = {};
            initiateVehicles(0.65,0.35,20,20,1,delays);
            break;
        }
        case 4 : {
            int delays[] = { 25 , 25 };
            initiateVehicles(0.5,0.5,30,10,3,delays);
            break;
        }
        case 5 : {
            int delays[] = { 3 , 10 };
            initiateVehicles(0.65,0.35,30,10,3,delays);
            break;
        }
        case 6 : {
            int delays[] = { 15 };
            initiateVehicles(0.75,0.25,30,20,2,delays);
            break;
        }
        default : 
        printf("scheduleNum error");
        exit(-1);
    }
}

void initiateVehicles(double carProb, double truckProb, int totalVehiclesCount, int startingVehiclesCount, int vehicleGroupCount, int delays[]){
    
    pthread_t threads[totalVehiclesCount];
    int rc;             //return value for creating thread
    int carID = 0;      //To genereate Car ID
    int truckID = 0;    //To genereate Truck ID
    int vehicleIndex = 0;       //To track vehicle index in vehicle Array for total
    int delayIndex = 0;         //To track delays in delayArray  
    int vehicleCountPerGroup[vehicleGroupCount];    

    VehicleInfo infos[totalVehiclesCount];    
    srand(time(NULL));

    //Prepare ingredient for threads(info & vehicle count for each group)
    for(int groupIndex = 0; groupIndex < vehicleGroupCount; groupIndex++){

        int vehiclesCount = 0;    
        //calculate vehicle count in group
        if (groupIndex == 0){
            vehiclesCount = startingVehiclesCount;
        }
        else{
            int remainingCount = (totalVehiclesCount - startingVehiclesCount)/(vehicleGroupCount-1);
            vehiclesCount = remainingCount;
        }

        //set vehicleCountPerGroup
        vehicleCountPerGroup[groupIndex] = vehiclesCount;

        for (int i = 0; i < vehiclesCount; i++){
         //declare vehicle type
            int randomNum = rand() % 100 + 1;
            VehicleType vehicleType = (randomNum  <= carProb * 100) ? car : truck;
          
            //declare vehicle direction
            int randomNum1 = rand() % 100 + 1;
            VehicleDirection vehicleDirection = (randomNum1 <= 0.5 * 100) ? south : north;

            //declare vehicle ID
            int ID;
            ID = (vehicleType == car) ? ++carID : ++truckID;

            //init vehicleInfo
            VehicleInfo vehicleInfo;
            vehicleInfo = (VehicleInfo) {.id = ID, .type = vehicleType, .direction = vehicleDirection, .groupNum = groupIndex + 1 };
            
            infos[vehicleIndex] = vehicleInfo;

            //increment vehicleIndex
            vehicleIndex++;

        }
    }

    vehicleIndex = 0;

    //START TRHEADING
    //iterate over each group
    for(int i = 0; i < vehicleGroupCount; i++){
        
        int vehiclesCount = vehicleCountPerGroup[i];

        pthread_mutex_lock(&GroupNumLock);
        vehicleCountInGroup = vehiclesCount;
        pthread_mutex_unlock(&GroupNumLock);

        //iterate over each vehicle in group
        for (int t = 0; t < vehiclesCount; t++)
        {  
            // create threads
            rc = pthread_create(&threads[vehicleIndex], NULL, Vehicle_Routine, (void *) &infos[vehicleIndex]);
            if (rc){
                printf("ERROR; return code from pthread_create() is %d\n", rc);
                exit(-1);
            }
  
            // increment vehicleIndex
            vehicleIndex ++;
        }

        //delay between group
        if (delayIndex < vehicleGroupCount - 1) {
            sleep(delays[delayIndex]);
            delayIndex++;
        }
    }

    //wait for all threads
    for(int i=0; i<totalVehiclesCount; i++){
       pthread_join(threads[i], NULL);
    }

}

////print mutex handled
void printOutZoneStatus(Zone zone){

    VehicleInfo *vehicleList;
    VehicleDirection direction;
    int vehicleCount;
    switch (zone)
    {
        case northBZone:   
            vehicleList = northBoundZone.vehicleList; 
            vehicleCount = northBoundZone.vehicleCount;
            direction = north;
            break;  
        case southBZone:  
            vehicleList = southBoundZone.vehicleList; 
            vehicleCount = southBoundZone.vehicleCount;
            direction = south;  
            break;   
        case bridgeZone:    
            vehicleList = bridge.vehicleList; 
            vehicleCount = bridge.vehicleCount;
            direction = bridge.direction;    
            break;
    }
    
    char *typeString;
    char *directionString = direction == north ? "northBound" : "southBound";
    char *statusString;

    if (zone == northBZone || zone == southBZone){
        if (vehicleCount == 0){
            printf("no vehicle waiting on %s\n", directionString);
        }
        else{
            if (vehicleCount == 1){
                char *typeString = vehicleList[0].type == car ? "Car" : "Truck";
                int id = vehicleList[0].id;
                printf("Waiting vehicles(%s): [%s %d]\n",directionString ,typeString, id);
            }
            else{
                for(int i = 0; i < vehicleCount; i++){
                    char *typeString = vehicleList[i].type == car ? "Car" : "Truck";
                    int id = vehicleList[i].id;
                    if (i == 0){
                        printf("Waiting vehicles(%s): [%s %d, ",directionString ,typeString, id);
                    }
                    else if (i == vehicleCount - 1){
                        printf("%s %d]\n", typeString, id);
                    }
                    else{
                         printf("%s %d, ", typeString, id);
                    }
                }
            }
        }
    }
    else{
        if (vehicleCount == 0){
            printf("no vehicle crossing on bridge\n");
        }  
        else{
             if (vehicleCount == 1){
                char *typeString = vehicleList[0].type == car ? "Car" : "Truck";
                int id = vehicleList[0].id;
                printf("Vehicles on the bridge(%s): [%s %d]\n",directionString ,typeString, id);
            }
            else{
                for(int i = 0; i < vehicleCount; i++){
                    char *typeString = vehicleList[i].type == car ? "Car" : "Truck";
                    int id = vehicleList[i].id;
                    if (i == 0){
                        printf("Vehicles on the bridge(%s): [%s %d, ",directionString ,typeString, id);
                    }
                    else if (i == vehicleCount - 1){
                        printf("%s %d]\n", typeString, id);
                    }
                    else{
                         printf("%s %d, ", typeString, id);
                    }
                }
            }
        }
    }

    
        
}

////print mutex handled 
void printOutAllZoneStatus(){
    pthread_mutex_lock(&PrintLock);
    printOutZoneStatus(northBZone);
    printOutZoneStatus(southBZone);
    printOutZoneStatus(bridgeZone);
    printf("-------------------------------------------------------------\n");
    pthread_mutex_unlock(&PrintLock);
}
    

////print mutex  NOT handled
void printOutVehicleStatus(vehicleStatus status, VehicleInfo *info){
    
    int id = info->id;
    VehicleType type = info->type;
    VehicleDirection direction = info->direction;
    int groupNum = info->groupNum; 

    char * typeString = type == car ? "Car" : "Truck";
    char * directionString = direction == north ? "northbound" : "southbound";

    pthread_mutex_lock(&PrintLock);
    printf("-------------------------------------------------------------\n");
    switch(status){
        case arrived:
            printf("Group %d %s #%d (%s) arrived.\n",groupNum,typeString,id,directionString);
            break;
        case crossing:
            printf("Group %d %s #%d (%s) CROSSING...\n",groupNum,typeString,id,directionString);
            break;
        case exiting:
            printf("Group %d %s #%d (%s) EXITING...\n",groupNum,typeString,id,directionString);
            break;
    }
    pthread_mutex_unlock(&PrintLock);
}

//mutex NOT handled. thread handles.
void addVehicleAtZone(VehicleInfo *info , int forBridge){

    VehicleType type = info->type;
    VehicleDirection direction = info->direction;

    if (forBridge == TRUE){
        (type == car) ? bridge.carCount++ : bridge.truckCount++;
        bridge.vehicleCount ++;
        bridge.vehicleList[bridge.vehicleCount - 1] = *info;
        if (bridge.carCount >= 3 || bridge.truckCount >= 1){
            bridge.isFull = TRUE;
        }
        else{
            bridge.isFull = FALSE;
        }
        bridge.direction = direction;
    }
    else{
        if (direction == north){
            (type == car) ? northBoundZone.carCount++ : northBoundZone.truckCount++;
            northBoundZone.vehicleCount++;
            northBoundZone.vehicleList[northBoundZone.vehicleCount - 1] = *info;
        }
        else{
            (type == car) ? southBoundZone.carCount++ : southBoundZone.truckCount++;
            southBoundZone.vehicleCount++;
            southBoundZone.vehicleList[southBoundZone.vehicleCount - 1] = *info;
        }
    }
   
}

void subtractVehicleAtZone(VehicleInfo *info , int forBridge){
    
    VehicleType type = info->type;
    VehicleDirection direction = info->direction;
    int id = info->id;

    if (forBridge == TRUE){
       
        int indexToDelete;
        for (int i = 0; i < MAXIMUM_SIZE; i++){
            if(bridge.vehicleList[i].id == id){
                indexToDelete = i;
                break;
            } 
        }
        for(int i = indexToDelete; i <= bridge.vehicleCount; i++){
            bridge.vehicleList[i] = bridge.vehicleList[i+1];
        }
        (type == car) ? bridge.carCount-- : bridge.truckCount--;
        bridge.vehicleCount--;

        if (bridge.carCount < 3 || bridge.truckCount < 1){
            bridge.isFull = FALSE;
        }
        else{
            bridge.isFull = TRUE;
        }
    }
    else{
         if (direction == north){
            int indexToDelete;
            for (int i = 0; i < MAXIMUM_SIZE; i++){
                if(northBoundZone.vehicleList[i].id == id){
                    if(northBoundZone.vehicleList[i].type == type){
                        indexToDelete = i;
                        break;
                    }
                }   
            }
            for(int i = indexToDelete; i <= northBoundZone.vehicleCount; i++){
                northBoundZone.vehicleList[i] = northBoundZone.vehicleList[i+1];
            }
            (type == car) ? northBoundZone.carCount-- : northBoundZone.truckCount--;
            northBoundZone.vehicleCount--;
        }
        else{
            int indexToDelete;
            for (int i = 0; i < MAXIMUM_SIZE; i++){
                if(southBoundZone.vehicleList[i].id == id){
                    if(southBoundZone.vehicleList[i].type == type){
                        indexToDelete = i;
                        break;
                    }
                }   
            }
            for(int i = indexToDelete; i <= southBoundZone.vehicleCount; i++){
                southBoundZone.vehicleList[i] = southBoundZone.vehicleList[i+1];
            }
            (type == car) ? southBoundZone.carCount-- : southBoundZone.truckCount--;
            southBoundZone.vehicleCount--;
        }
    }
}
    
