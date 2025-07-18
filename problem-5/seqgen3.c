// Sam Siewert, December 2020
//
// Edited by Gerritt Luoma, June 2025
// Sequencer - 2 Hz
//                   [gives semaphores to all other services]
// Service_1 - 1 Hz, every other Sequencer loop - Calculates random "satellite data" with a timestamp
// Service_2 - 0.1 Hz, every 20th Sequencer loop - Ingests the random "satellite data" and outputs with a timestamp
//
// With the above, priorities by RM policy would be:
//
// Sequencer = RT_MAX	@ 2   Hz
// Servcie_1 = RT_MAX-1	@ 1   Hz
// Service_2 = RT_MAX-2	@ 0.1 Hz
//

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>

#include <syslog.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <errno.h>

#include <signal.h>
#include <math.h>

#define USEC_PER_MSEC (1000)
#define NANOSEC_PER_MSEC (1000000)
#define NANOSEC_PER_SEC (1000000000)
#define NUM_CPU_CORES (4)
#define TRUE (1)
#define FALSE (0)

#define NUM_THREADS (3)

// Of the available user space clocks, CLOCK_MONONTONIC_RAW is typically most precise and not subject to 
// updates from external timer adjustments
//
// However, some POSIX functions like clock_nanosleep can only use adjusted CLOCK_MONOTONIC or CLOCK_REALTIME
//
//#define MY_CLOCK_TYPE CLOCK_REALTIME
//#define MY_CLOCK_TYPE CLOCK_MONOTONIC
#define MY_CLOCK_TYPE CLOCK_MONOTONIC_RAW
//#define MY_CLOCK_TYPE CLOCK_REALTIME_COARSE
//#define MY_CLOCK_TYPE CLOCK_MONTONIC_COARSE

int abortTest=FALSE;
int abortS1=FALSE, abortS2=FALSE, abortS3=FALSE;
sem_t semS1, semS2;
struct timespec start_time_val;
double start_realtime;
unsigned long long sequencePeriods;

static timer_t timer_1;
static struct itimerspec itime = {{1,0}, {1,0}};
static struct itimerspec last_itime;

static unsigned long long seqCnt=0;

typedef struct
{
    int threadIdx;
} threadParams_t;

typedef struct {
    double lat, lon, alt;
    double roll, pitch, yaw;
    double sample_time;
} nav_state_t;

nav_state_t shared_state;
pthread_mutex_t shared_state_mutex;
pthread_mutex_t service_3_mutex;

void Sequencer(int id);

void *Service_1(void *threadp);
void *Service_2(void *threadp);
void *Service_3(void *threadp);

double getTimeMsec(void);
double realtime(struct timespec *tsptr);
void print_scheduler(void);

void main(void)
{
    struct timespec current_time_val, current_time_res;
    double current_realtime, current_realtime_res;

    int i, rc, scope, flags=0;

    cpu_set_t threadcpu;
    cpu_set_t allcpuset;

    pthread_t threads[NUM_THREADS];
    threadParams_t threadParams[NUM_THREADS];
    pthread_attr_t rt_sched_attr[NUM_THREADS];
    int rt_max_prio, rt_min_prio, cpuidx;

    struct sched_param rt_param[NUM_THREADS];
    struct sched_param main_param;

    pthread_attr_t main_attr;
    pid_t mainpid;

    printf("Starting Low Rate Sequencer Demo\n");
    clock_gettime(MY_CLOCK_TYPE, &start_time_val); start_realtime=realtime(&start_time_val);
    clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
    clock_getres(MY_CLOCK_TYPE, &current_time_res); current_realtime_res=realtime(&current_time_res);
    printf("START Low Rate Sequencer @ sec=%6.9lf with resolution %6.9lf\n", (current_realtime - start_realtime), current_realtime_res);
    syslog(LOG_CRIT, "START Low Rate Sequencer @ sec=%6.9lf with resolution %6.9lf\n", (current_realtime - start_realtime), current_realtime_res);

    printf("System has %d processors configured and %d available.\n", get_nprocs_conf(), get_nprocs());

    CPU_ZERO(&allcpuset);

    for(i=0; i < NUM_CPU_CORES; i++)
        CPU_SET(i, &allcpuset);

    printf("Using CPUS=%d from total available.\n", CPU_COUNT(&allcpuset));


    // initialize the sequencer semaphores
    //
    if (sem_init (&semS1, 0, 0)) { printf ("Failed to initialize S1 semaphore\n"); exit (-1); }
    if (sem_init (&semS2, 0, 0)) { printf ("Failed to initialize S2 semaphore\n"); exit (-1); }

    mainpid=getpid();

    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);

    rc=sched_getparam(mainpid, &main_param);
    main_param.sched_priority=rt_max_prio;
    rc=sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
    if(rc < 0) perror("main_param");
    print_scheduler();

    pthread_attr_getscope(&main_attr, &scope);

    if(scope == PTHREAD_SCOPE_SYSTEM)
      printf("PTHREAD SCOPE SYSTEM\n");
    else if (scope == PTHREAD_SCOPE_PROCESS)
      printf("PTHREAD SCOPE PROCESS\n");
    else
      printf("PTHREAD SCOPE UNKNOWN\n");

    printf("rt_max_prio=%d\n", rt_max_prio);
    printf("rt_min_prio=%d\n", rt_min_prio);

    for(i=0; i < NUM_THREADS; i++)
    {
        // run even all threads on core 2
        CPU_ZERO(&threadcpu);
        cpuidx=(2);
        CPU_SET(cpuidx, &threadcpu);

        rc=pthread_attr_init(&rt_sched_attr[i]);
        rc=pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
        rc=pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
        rc=pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu);

        rt_param[i].sched_priority=rt_max_prio-i;
        pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

        threadParams[i].threadIdx=i;
    }

    printf("Service threads will run on %d CPU cores\n", CPU_COUNT(&threadcpu));

    // Create Service threads which will block awaiting release for:
    //

    // Servcie_1 = RT_MAX-1	@ 1 Hz
    //
    rt_param[0].sched_priority=rt_max_prio-1;
    pthread_attr_setschedparam(&rt_sched_attr[0], &rt_param[0]);
    rc=pthread_create(&threads[0],               // pointer to thread descriptor
                      &rt_sched_attr[0],         // use specific attributes
                      //(void *)0,               // default attributes
                      Service_1,                 // thread function entry point
                      (void *)&(threadParams[0]) // parameters to pass in
                     );
    if(rc < 0)
        perror("pthread_create for service 1");
    else
        printf("pthread_create successful for service 1\n");


    // Service_2 = RT_MAX-2	@ 0.1 Hz
    //
    rt_param[1].sched_priority=rt_max_prio-2;
    pthread_attr_setschedparam(&rt_sched_attr[1], &rt_param[1]);
    rc=pthread_create(&threads[1], &rt_sched_attr[1], Service_2, (void *)&(threadParams[1]));
    if(rc < 0)
        perror("pthread_create for service 2");
    else
        printf("pthread_create successful for service 2\n");

    // Service_3 = RT_MAX-3 @ Unknown frequency
    // Will be blocking for up to 10 seconds at a time on its own mutex
    // and outputting "No new data" when the timeout occurs
    rt_param[2].sched_priority=rt_max_prio-3;
    pthread_attr_setschedparam(&rt_sched_attr[2], &rt_param[2]);

    // Acquire the mutex before starting the thread so that it always times out for 10 seconds
    pthread_mutex_lock(&service_3_mutex);
    rc=pthread_create(&threads[2], &rt_sched_attr[2], Service_3, (void *)&(threadParams[2]));
    if(rc < 0)
        perror("pthread_create for service 3");
    else
        printf("pthread_create successful for service 3\n");

    // Wait for service threads to initialize and await relese by sequencer.
    //
    // Note that the sleep is not necessary of RT service threads are created with
    // correct POSIX SCHED_FIFO priorities compared to non-RT priority of this main
    // program.
    //
    // sleep(1);

    // Create Sequencer thread, which like a cyclic executive, is highest prio
    printf("Start sequencer\n");
    sequencePeriods=360; // Sequencer is running at 2Hz and needs to run for 180 seconds

    // Sequencer = RT_MAX	@ 2 Hz
    //
    /* set up to signal SIGALRM if timer expires */
    timer_create(CLOCK_REALTIME, NULL, &timer_1);

    signal(SIGALRM, (void(*)()) Sequencer);

    /* arm the interval timer */
    itime.it_interval.tv_sec = 0;
    itime.it_interval.tv_nsec = 500000000; // 0.5 second timer period
    itime.it_value.tv_sec = 0;
    itime.it_value.tv_nsec = 500000000;

    timer_settime(timer_1, flags, &itime, &last_itime);


    for(i=0;i<NUM_THREADS;i++)
    {
        if(rc=pthread_join(threads[i], NULL) < 0)
		perror("main pthread_join");
	else
		printf("joined thread %d\n", i);
    }

   printf("\nTEST COMPLETE\n");
}

void Sequencer(int id)
{
    struct timespec current_time_val;
    double current_realtime;
    int rc, flags=0;

    //clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
    //printf("Sequencer on core %d for cycle %llu @ sec=%6.9lf\n", sched_getcpu(), seqCnt, current_realtime-start_realtime);
    //syslog(LOG_CRIT, "Sequencer on core %d for cycle %llu @ sec=%6.9lf\n", sched_getcpu(), seqCnt, current_realtime-start_realtime);

    // Release each service at a sub-rate of the generic sequencer rate
    seqCnt++;

    // Servcie_1 = RT_MAX-1	@ 1 Hz
    if((seqCnt % 2) == 0) sem_post(&semS1);

    // Service_2 = RT_MAX-2	@ 0.1 Hz
    if((seqCnt % 20) == 0) sem_post(&semS2);


    if(abortTest || (seqCnt >= sequencePeriods))
    {
        // disable interval timer
        itime.it_interval.tv_sec = 0;
        itime.it_interval.tv_nsec = 0;
        itime.it_value.tv_sec = 0;
        itime.it_value.tv_nsec = 0;
        timer_settime(timer_1, flags, &itime, &last_itime);
	    printf("Disabling sequencer interval timer with abort=%d and %llu of %lld\n", abortTest, seqCnt, sequencePeriods);

	    // shutdown all services
        sem_post(&semS1); sem_post(&semS2);
        abortS1=TRUE; abortS2=TRUE; abortS3=TRUE;
    }

}

void *Service_1(void *threadp)
{
    struct timespec current_time_val;
    double current_realtime;
    unsigned long long S1Cnt=0;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    // Start up processing and resource initialization
    clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
    syslog(LOG_CRIT, "S1 thread @ sec=%6.9lf\n", current_realtime-start_realtime);
    printf("S1 thread @ sec=%6.9lf\n", current_realtime-start_realtime);

    while(!abortS1) // check for synchronous abort request
    {
	    // wait for service request from the sequencer, a signal handler or ISR in kernel
        sem_wait(&semS1);

        S1Cnt++;

        nav_state_t new_state;
        new_state.lat = 50.0 + (1.0 * S1Cnt);
        new_state.lon = -40.0 - (2.0 * S1Cnt);
        new_state.alt = 150.0 - (0.05 * S1Cnt);

        new_state.roll = sin((double)S1Cnt);
        new_state.pitch = cos((double)S1Cnt * (double)S1Cnt);
        new_state.yaw = cos((double)S1Cnt);

        clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
        new_state.sample_time = current_realtime;

        pthread_mutex_lock(&shared_state_mutex);
        shared_state = new_state;
	    // Shouldn't do additional proc like logging in mutex lock but keep locked to print current vals
        syslog(LOG_CRIT,
               "S1 1 Hz on core %d for release  %3llu: lat=%6.5lf, lon=%6.5lf, alt=%6.5lf, roll=%6.5lf, pitch=%6.5lf, yaw=%6.5lf @ sec=%6.9lf\n",
               sched_getcpu(),
               S1Cnt,
               shared_state.lat,
               shared_state.lon,
               shared_state.alt,
               shared_state.roll,
               shared_state.pitch,
               shared_state.yaw,
               shared_state.sample_time
        );
        pthread_mutex_unlock(&shared_state_mutex);

    }

    // Resource shutdown here
    //
    pthread_exit((void *)0);
}


void *Service_2(void *threadp)
{
    struct timespec current_time_val;
    double current_realtime;
    unsigned long long S2Cnt=0;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
    syslog(LOG_CRIT, "S2 thread @ sec=%6.9lf\n", current_realtime-start_realtime);
    printf("S2 thread @ sec=%6.9lf\n", current_realtime-start_realtime);

    while(!abortS2)
    {
        sem_wait(&semS2);
        S2Cnt++;

        pthread_mutex_lock(&shared_state_mutex);
	    // Shouldn't do additional proc like logging in mutex lock but keep locked to print current vals
        syslog(LOG_CRIT,
               "S2 0.1 Hz on core %d for release %2llu: lat=%6.5lf, lon=%6.5lf, alt=%6.5lf, roll=%6.5lf, pitch=%6.5lf, yaw=%6.5lf @ sec=%6.9lf\n",
               sched_getcpu(),
               S2Cnt,
               shared_state.lat,
               shared_state.lon,
               shared_state.alt,
               shared_state.roll,
               shared_state.pitch,
               shared_state.yaw,
               shared_state.sample_time
        );
        pthread_mutex_unlock(&shared_state_mutex);
    }

    pthread_exit((void *)0);
}

void *Service_3(void *threadp)
{
    struct timespec timeout;
    int rc;
    double current_realtime = 0;
    struct timespec current_time_val;

    while (!abortS3)
    {
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 10; // wait up to 10 seconds

        rc = pthread_mutex_timedlock(&service_3_mutex, &timeout);
        if (rc == ETIMEDOUT)
        {
            clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
            syslog(LOG_CRIT, "No new data available at %6.9lf\n", current_realtime);
        }
        else if (rc == 0)
        {
            // Accessed shared state successfully, immediately unlock
            pthread_mutex_unlock(&service_3_mutex);
        }
    }

    pthread_exit(NULL);
}

double getTimeMsec(void)
{
  struct timespec event_ts = {0, 0};

  clock_gettime(MY_CLOCK_TYPE, &event_ts);
  return ((event_ts.tv_sec)*1000.0) + ((event_ts.tv_nsec)/1000000.0);
}


double realtime(struct timespec *tsptr)
{
    return ((double)(tsptr->tv_sec) + (((double)tsptr->tv_nsec)/1000000000.0));
}


void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
       case SCHED_FIFO:
           printf("Pthread Policy is SCHED_FIFO\n");
           break;
       case SCHED_OTHER:
           printf("Pthread Policy is SCHED_OTHER\n"); exit(-1);
         break;
       case SCHED_RR:
           printf("Pthread Policy is SCHED_RR\n"); exit(-1);
           break;
       //case SCHED_DEADLINE:
       //    printf("Pthread Policy is SCHED_DEADLINE\n"); exit(-1);
       //    break;
       default:
           printf("Pthread Policy is UNKNOWN\n"); exit(-1);
   }
}

