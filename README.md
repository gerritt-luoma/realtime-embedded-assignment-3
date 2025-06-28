# RTES Assignment 3

Gerritt Luoma


## Exercise 1

> Read Sha, Rajkumar, et al paper, "Priority Inheritance Protocols: An Approach to Real-Time Synchronization".

> A: Summarize 3 main key points the paper makes. Read my summary paper on the topic as well, which might be easier to understand.

The paper "Priority Inheritance Protocols: An Approach to Real-Time Synchronization" by Sha, Rajkumar, et al is explaining the issue in realtime systems known as Priority Inversion and approaches for how to solve the problem.  The first key point in the paper is the definition of the Priority Inversion problem.  The section explains how the problem arises when a lower priority task is able to revent a higher priority task from running, typically through taking a shared resource and being unable to release the resource due to a higher priority task.  If this inversion is unbounded, it can lead to missing deadlines causing catastrophic consequences in critical systems.  The second main point of the paper is the introduction of Priority Inheritance Protocols which are solutions to the Priority Inversion problem.  The base Priority Inheritance Protocol proposes having lower priority tasks that are blocking a higher priority task inherit the priority of the higher priority task for the duration of the task block so that priority inversion is unable to occur.  By doing this, the lower priority task can never be preempted by middle priority tasks causing a potential unbounded inversion.  Section 4 of the paper also introduces the concept of the Priority Ceiling Protocol which builds upon the Priority Inheritance Protocol to also prevent potential issues like deadlocks.  The third and final key point made in the paper is the introduction of Schedulability Analysis which sets a "set of sufficient conditions under which a set of periodic tasks using the priority ceiling protocol can be scheduled by the rate monotonic algorithm.

> B: Read the historical positions of Linux Torvalds as described by Jonathan Corbet and Ingo Molnar and Thomas Gleixner on this topic as well. Note that more recently Red Hat has added support for priority ceiling emulation and priority inheritance and has a great overview on use of these POSIX real-time extension features here – general real-time overview. The key systems calls are pthread_mutexattr_setprotocol, pthread_mutexattr_getprotocol as well as pthread_mutex_setpriorityceiling and pthread_mutex_getpriorityceiling. Why did some of the Linux community resist addition of priority ceiling and/or priority inheritance until more recently (Linux has been distributed since the early 1990’s and the problem and solutions were know before this).

The Linux communicty resisted addition of priority ceiling/priority inheritance because the Linux Kernel maintainers prioritize simplicity and general purpose performance.  Many of the priority inversion solutions added significant performance overhead to the kernal reducing performance.  Even Linus was very against priority inheritance going as far as saying "If you really need [priority inheritance], your system is broken anyway.".  It wasn't until Ingo introduced his solution for his futex that the first form of realtime priority inheritance was added into the kernel.  Within the comments people are mentioning that Ingo was able to do this despite heavy resistance is because he "writes sane patches and doesn't irritate people needlessly" which is key for working with others and finding ways to get what they want.

> C: Given that there are two solutions to unbounded priority inversion, priority ceiling emulation and priority inheritance, based upon your reading on support for both by POSIX real-time extensions, which do you think is best to use and why?

Based off of my reading and personal experience, I believe that priority inheritance is the way to go for the majority of scenarios.  Based off the simplicity and minimal performance overhead it seems like the obvious choice.

## Exercise 2

> Review the terminology guide (glossary in the textbook).

> A: Describe clearly what it means to write "thread safe" functions that are "re-entrant".

Functions that are "re-entrant" so that they are "thread safe" are written in such a way that concurrent calls to the function will not result in the corruption of data.  To put it simply, the function must be able to be called, be paused at any point in its execution, a new instance of the function runs, and then it can resume with no changed data or behavior.  The most threadsafe/reentrant functions are functions that use exclusively the stack and local variables.  If global variables must be used, synchronization primitives must be used to prevent data races or the corruption of data.

> B: There are generally three main ways to do this: 1) pure functions that use only stack and have no global memory, 2) functions which use thread indexed global data, and 3)  functions which use shared memory global data, but synchronize access to it using a MUTEX semaphore critical section wrapper. Describe each method and how you would code it and how it would impact real-time threads/tasks. (E.g., if you were to create a simple function to average 2 numbers and return the average, how would you make sure this function is thread safe using each of the three methods).

The easiest and most consistent of the three ways of making a funciton re-entrant is by keeping the function pure and only use the stack.  For the case of averaging two numbers, you would take the two numbers as inputs and return the result as such:

```c
double pure_solution(double a, double b)
{
    return (a + b) / 2.0;
}
```

This function is pure because it does not reference any global variables and only uses the stack.  This function could be called by an infinite number of threads all at the same time and there would be no blocking or performance overhead introduced by interthread synchronization.

The second option is through the use of thread indexed global data.  This can be achieved through the use of a global key and the `pthread_getspecific()`/`pthread_setspecific()` functions.  When using these functions in tandem with a key, threads are able to store and retrieve data that is reserved for use by the calling thread.  This example can be seen below:

```c
pthread_key_t key; // Assume key has been initialized in the main or calling thread

void* average_thread_func(void* arg)
{
    double* data = malloc(sizeof(double));
    *data = 0;
    pthread_setspecific(key, data); // Set the data in the key store

    thread_arg_t* thread_args = (thread_arg_t*)arg; // Assume thread_arg_t is defined
    *data = (thread_args->a + thread_args->b) / 2.0;

    // ...

    double* retrieve_data = pthread_getspecific(key); // Grab the stored pointer later in the thread

    return NULL;
}
```

This method seems to require a lot of setup and will require extra memory allocation since each thread can allocate its own data to be stored.

The final method is through the use of synchronization primitives like a mutex which allow exclusive access to a shared resource.  If a mutex has already been locked, then any other threads that want access to the shared resource will block on the acquisition of the mutex.  This method can be seen below:

```c
double global_data = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void* average_thread_func(void* arg)
{
    // Assume that thread_arg_t is defined
    thread_arg_t* thread_args = (thread_arg_t*)args;
    double avg = (thread_args->a + thread_args->b) / 2.0;

    // Can block if another thread has already locked the mutex
    pthread_mutex_lock(&lock);
    global_data = avg;
    pthread_mutex_unlock(&lock);

    return NULL;
}
```

> C: Now, using a MUTEX, provide an example using RT-Linux Pthreads that does a threadsafe update of a complex state (3 or more numbers – e.g., Latitude, Longitude and Altitude of a location) with a timestamp (pthread_mutex_lock). Your code should include two threads and one should update a timespec structure contained in a structure that includes a double precision position and attitude state of {Lat, Long, Altitude and Roll, Pitch, Yaw at Sample_Time} and the other should read it and never disagree on the values as function of time. You can just make up values for the navigational state usin gmath library function generators (e.g., use simple periodic functions for Roll, Pitch, Yaws in(x), cos(x2), and cos(x), where x=time and linear functions for Lat, Long, Alt) and see http://linux.die.net/man/3/clock_gettime for how to add a precision timestamp. The second thread should read the times-stamped state without the possibility of data corruption (partial update of one of the 6 floating point values). There should be no disagreement between the functions and the state reader for any point in time. Run this for 180 seconds with a 1 Hz update rate and a 0.1 Hz read rate. Make sure the 18 values read are correct.

I have added my program fulfilling the requirements of this problem along with its output to the directory `problem-2/` of this repository.  I used Sam Siewert's provided `seqgen3.c` program as the base which can be found in the [RTES-ECEE-5623](https://github.com/siewertsmooc/RTES-ECEE-5623/blob/main/sequencer_generic/seqgen3.c) repository.  I have modified the program to only have the two required services, the writer and the reader, which are running at 1Hz and 0.1Hz respectively being released by a timer sequencer operating at 2Hz.  Instrucitons to run the program can be found below:

```bash
// First terminal in the problem-2 directory - builds and runs the program
$ make clean
$ make
$ sudo ./seqgen3

// Second terminal in any directory - outputs the syslogs of the program
$ journalctl -f
```

I am using practically randomized data in the writer thread, Service_1, to "simulate" the Attitude and SCLK of a satellite.  If any satellite has the true ATD of this satellite, you will want to say goodbye to it because it is spinning like a top and dropping fast.  The logic of the writer thread running at 1Hz is found below:

```c
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
```

The second thread, Service_2, runs at 0.1Hz simply reading the shared state and printing it to the syslogs.  Its code is found below:

```c
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
```

As mentioned before, the full output of the program can be found in the `problem-2` directory but the main task of this problem was to ensure the mutex synchronization was preventing any data corruption for all 18 times both of the threads access the shared state.  After analyzing the logs, I can confirm that all 18 occurrences result in no data corruption.  The writer thread, having the higher priority, first updates the state and then the reader thread outputs it.  The output of all 18 of these occurrences can be found below:

```bash
Jun 28 09:24:20 arthur seqgen3[31547]: S1 1 Hz on core 2 for release   10: lat=60.00000, lon=-60.00000, alt=149.50000, roll=-0.54402, pitch=0.86232, yaw=-0.83907 @ sec=1728732.540957039
Jun 28 09:24:20 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release  1: lat=60.00000, lon=-60.00000, alt=149.50000, roll=-0.54402, pitch=0.86232, yaw=-0.83907 @ sec=1728732.540957039

Jun 28 09:24:30 arthur seqgen3[31547]: S1 1 Hz on core 2 for release   20: lat=70.00000, lon=-80.00000, alt=149.00000, roll=0.91295, pitch=-0.52530, yaw=0.40808 @ sec=1728742.541062533
Jun 28 09:24:30 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release  2: lat=70.00000, lon=-80.00000, alt=149.00000, roll=0.91295, pitch=-0.52530, yaw=0.40808 @ sec=1728742.541062533

Jun 28 09:24:40 arthur seqgen3[31547]: S1 1 Hz on core 2 for release   30: lat=80.00000, lon=-100.00000, alt=148.50000, roll=-0.98803, pitch=0.06625, yaw=0.15425 @ sec=1728752.541168657
Jun 28 09:24:40 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release  3: lat=80.00000, lon=-100.00000, alt=148.50000, roll=-0.98803, pitch=0.06625, yaw=0.15425 @ sec=1728752.541168657

Jun 28 09:24:50 arthur seqgen3[31547]: S1 1 Hz on core 2 for release   40: lat=90.00000, lon=-120.00000, alt=148.00000, roll=0.74511, pitch=-0.59836, yaw=-0.66694 @ sec=1728762.541265651
Jun 28 09:24:50 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release  4: lat=90.00000, lon=-120.00000, alt=148.00000, roll=0.74511, pitch=-0.59836, yaw=-0.66694 @ sec=1728762.541265651

Jun 28 09:25:00 arthur seqgen3[31547]: S1 1 Hz on core 2 for release   50: lat=100.00000, lon=-140.00000, alt=147.50000, roll=-0.26237, pitch=0.75983, yaw=0.96497 @ sec=1728772.541368367
Jun 28 09:25:00 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release  5: lat=100.00000, lon=-140.00000, alt=147.50000, roll=-0.26237, pitch=0.75983, yaw=0.96497 @ sec=1728772.541368367

Jun 28 09:25:10 arthur seqgen3[31547]: S1 1 Hz on core 2 for release   60: lat=110.00000, lon=-160.00000, alt=147.00000, roll=-0.30481, pitch=0.96505, yaw=-0.95241 @ sec=1728782.541465065
Jun 28 09:25:10 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release  6: lat=110.00000, lon=-160.00000, alt=147.00000, roll=-0.30481, pitch=0.96505, yaw=-0.95241 @ sec=1728782.541465065

Jun 28 09:25:20 arthur seqgen3[31547]: S1 1 Hz on core 2 for release   70: lat=120.00000, lon=-180.00000, alt=146.50000, roll=0.77389, pitch=0.63365, yaw=0.63332 @ sec=1728792.541573429
Jun 28 09:25:20 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release  7: lat=120.00000, lon=-180.00000, alt=146.50000, roll=0.77389, pitch=0.63365, yaw=0.63332 @ sec=1728792.541573429

Jun 28 09:25:30 arthur seqgen3[31547]: S1 1 Hz on core 2 for release   80: lat=130.00000, lon=-200.00000, alt=146.00000, roll=-0.99389, pitch=-0.83878, yaw=-0.11039 @ sec=1728802.541682201
Jun 28 09:25:30 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release  8: lat=130.00000, lon=-200.00000, alt=146.00000, roll=-0.99389, pitch=-0.83878, yaw=-0.11039 @ sec=1728802.541682201

Jun 28 09:25:40 arthur seqgen3[31547]: S1 1 Hz on core 2 for release   90: lat=140.00000, lon=-220.00000, alt=145.50000, roll=0.89400, pitch=0.56188, yaw=-0.44807 @ sec=1728812.541776806
Jun 28 09:25:40 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release  9: lat=140.00000, lon=-220.00000, alt=145.50000, roll=0.89400, pitch=0.56188, yaw=-0.44807 @ sec=1728812.541776806

Jun 28 09:25:50 arthur seqgen3[31547]: S1 1 Hz on core 2 for release  100: lat=150.00000, lon=-240.00000, alt=145.00000, roll=-0.50637, pitch=-0.95216, yaw=0.86232 @ sec=1728822.541879652
Jun 28 09:25:50 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release 10: lat=150.00000, lon=-240.00000, alt=145.00000, roll=-0.50637, pitch=-0.95216, yaw=0.86232 @ sec=1728822.541879652

Jun 28 09:26:00 arthur seqgen3[31547]: S1 1 Hz on core 2 for release  110: lat=160.00000, lon=-260.00000, alt=144.50000, roll=-0.04424, pitch=0.15526, yaw=-0.99902 @ sec=1728832.541985998
Jun 28 09:26:00 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release 11: lat=160.00000, lon=-260.00000, alt=144.50000, roll=-0.04424, pitch=0.15526, yaw=-0.99902 @ sec=1728832.541985998

Jun 28 09:26:10 arthur seqgen3[31547]: S1 1 Hz on core 2 for release  120: lat=170.00000, lon=-280.00000, alt=144.00000, roll=0.58061, pitch=0.48824, yaw=0.81418 @ sec=1728842.542088511
Jun 28 09:26:10 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release 12: lat=170.00000, lon=-280.00000, alt=144.00000, roll=0.58061, pitch=0.48824, yaw=0.81418 @ sec=1728842.542088511

Jun 28 09:26:20 arthur seqgen3[31547]: S1 1 Hz on core 2 for release  130: lat=180.00000, lon=-300.00000, alt=143.50000, roll=-0.93011, pitch=-0.19640, yaw=-0.36729 @ sec=1728852.542189560
Jun 28 09:26:20 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release 13: lat=180.00000, lon=-300.00000, alt=143.50000, roll=-0.93011, pitch=-0.19640, yaw=-0.36729 @ sec=1728852.542189560

Jun 28 09:26:30 arthur seqgen3[31547]: S1 1 Hz on core 2 for release  140: lat=190.00000, lon=-320.00000, alt=143.00000, roll=0.98024, pitch=-0.92239, yaw=-0.19781 @ sec=1728862.542293554
Jun 28 09:26:30 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release 14: lat=190.00000, lon=-320.00000, alt=143.00000, roll=0.98024, pitch=-0.92239, yaw=-0.19781 @ sec=1728862.542293554

Jun 28 09:26:40 arthur seqgen3[31547]: S1 1 Hz on core 2 for release  150: lat=200.00000, lon=-340.00000, alt=142.50000, roll=-0.71488, pitch=0.99625, yaw=0.69925 @ sec=1728872.542394289
Jun 28 09:26:40 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release 15: lat=200.00000, lon=-340.00000, alt=142.50000, roll=-0.71488, pitch=0.99625, yaw=0.69925 @ sec=1728872.542394289

Jun 28 09:26:50 arthur seqgen3[31547]: S1 1 Hz on core 2 for release  160: lat=210.00000, lon=-360.00000, alt=142.00000, roll=0.21943, pitch=-0.66855, yaw=-0.97563 @ sec=1728882.542497764
Jun 28 09:26:50 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release 16: lat=210.00000, lon=-360.00000, alt=142.00000, roll=0.21943, pitch=-0.66855, yaw=-0.97563 @ sec=1728882.542497764

Jun 28 09:27:00 arthur seqgen3[31547]: S1 1 Hz on core 2 for release  170: lat=220.00000, lon=-380.00000, alt=141.50000, roll=0.34665, pitch=-0.88272, yaw=0.93799 @ sec=1728892.542616666
Jun 28 09:27:00 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release 17: lat=220.00000, lon=-380.00000, alt=141.50000, roll=0.34665, pitch=-0.88272, yaw=0.93799 @ sec=1728892.542616666

Jun 28 09:27:10 arthur seqgen3[31547]: S1 1 Hz on core 2 for release  180: lat=230.00000, lon=-400.00000, alt=141.00000, roll=-0.80115, pitch=-0.72830, yaw=-0.59846 @ sec=1728902.542711160
Jun 28 09:27:10 arthur seqgen3[31547]: S2 0.1 Hz on core 2 for release 18: lat=230.00000, lon=-400.00000, alt=141.00000, roll=-0.80115, pitch=-0.72830, yaw=-0.59846 @ sec=1728902.542711160

```

## Exercise 3

> Download example-sync-updated-2/ and review, build and run it.

> A: Describe both the issues of deadlock and unbounded priority inversion and the root cause for both in the example code.

TODO:

> B: Fix the deadlock so that it does not occur by using a random back-off scheme to resolve. For the unbounded inversion, is there a real fix in Linux – if not, why not?

TODO:

> C: What about a patch for the Linux kernel? For example, Linux Kernel.org recommends the RT_PREEMPT Patch, also discussed by the Linux Foundation Realtime Start and this blog, but would this really help?

TODO:

> D: Read about the patch and describe why think it would or would not help with unbounded priority inversion. Based on inversion, does it make sense to simply switch to an RTOS and not use Linux at all for both HRT and SRT services?

TODO:

## Exercise 4

> Review POSIX-Examples-Updated and POSIX_MQ_LOOP and build the code related to POSIX message queues and run them to learn about basic use.

> A: First, re-write the simple message queue demonstration code so that it uses RT-Linux Pthreads (FIFO) instead of SCHED_OTHER, and then write a brief paragraph describing how the use of a heap message queue and a simple message queue for applications are similar and how they are different.

TODO:

> B: Message queues are often suggested as a better way to avoid MUTEX priority inversion. Would you agree that this really circumvents the problem of unbounded inversion? If so why, if not, why not?

TODO:

## Exercise 5

> For this problem, consider the Linux manual page on the watchdog daemon - https://linux.die.net/man/8/watchdog [you can read more about Linux watchdog timers, timeouts and timer services in this overview of the Linux Watchdog Daemon].

> A: Describe how it might be used if software caused an indefinite deadlock.

TODO:

> B: Next, to explore timeouts, use your code from #2 and create a thread that waits on a MUTEX semaphore for up to 10 seconds and then un-blocks and prints out “No new data available at <time>” and then loops back to wait for a data update again. Use a variant of the pthread_mutex_lock called pthread_mutex_timedlock to solve this programming problem.

TODO: