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

> C: Now, using a MUTEX, provide an example using RT-Linux Pthreads that does a threadsafe update of a complex state (3 or more numbers – e.g., Latitude, Longitude and Altitude of a location) with a timestamp (pthread_mutex_lock). Your code should include two threads and one should update a timespec structure contained in a structure that includes a double precision position and attitude state of {Lat, Long, Altitude and Roll, Pitch, Yaw at Sample_Time} and the other should read it and never disagree on the values as function of time. You can just make up values for the navigational state usin gmath library function generators (e.g., use simple periodic functions for Roll, Pitch, Yaws in(x), cos(x2), and cos(x), where x=time and linear functions for Lat, Long, Alt) and see http://linux.die.net/man/3/clock_gettime for how to add a precision timestamp. The second thread should read the times-stamped state without the possibility of data corruption (partial update of one of the 6 floating point values). There should be no disagreement between the functions and the state reader for any point in time. Run thisfor 180 seconds with a 1 Hz update rate and a 0.1 Hz read rate. Make sure the 18 values read are correct.

TODO:

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