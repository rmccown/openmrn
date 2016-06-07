/** \copyright
 * Copyright (c) 2012, Stuart W Baker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 * 
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file os.c
 * This file represents a C language abstraction of common operating
 * system calls.
 *
 * @author Stuart W. Baker
 * @date 13 August 2012
 */

// Forces one definition of each inline function to be compliled.
#define OS_INLINE extern

#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#if !defined (GCC_MEGA_AVR)
#include <unistd.h>
#endif
#if defined (__FreeRTOS__)
#include "devtab.h"
#include "FreeRTOS.h"
#include "task.h"
#else
#if defined(__WIN32__)
#include <winsock2.h>
#include <ws2tcpip.h> /* socklen_t */
#else
#include <sys/select.h>
#include <sched.h>
#endif
#include <time.h>
#include <signal.h>
#endif

#include "nmranet_config.h"

#include "utils/macros.h"
#include "os/os.h"

/** default stdin */
extern const char *STDIN_DEVICE;

/** default stdout */
extern const char *STDOUT_DEVICE;

/** default stderr */
extern const char *STDERR_DEVICE;


#if defined (__FreeRTOS__)
/** Task list entriy */
typedef struct task_list
{
    xTaskHandle task; /**< list entry data */
    char * name; /**< name of task */
    size_t unused; /**< number of bytes left unused in the stack */
    struct task_list *next; /**< next link in the list */
} TaskList;

/** List of all the tasks in the system */
static TaskList taskList;

/** Mutex for os_thread_once. */
static os_mutex_t onceMutex = OS_MUTEX_INITIALIZER;

/** Default hardware initializer.  This function is defined weak so that
 * a given board can stub in an intiailization specific to it.
 */
void hw_init(void) __attribute__ ((weak));
void hw_init(void)
{
}

__attribute__ ((weak))
struct _reent* allocate_reent(void)
{
    struct _reent* data = malloc(sizeof(struct _reent));
    _REENT_INIT_PTR(data);
    return data;
}

/** One time intialization routine
 * @param once one time instance
 * @param routine method to call once
 * @return 0 upon success
 */
int os_thread_once(os_thread_once_t *once, void (*routine)(void))
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        /* The scheduler has started so we should use locking */
        os_mutex_lock(&onceMutex);
        if (once->state == OS_THREAD_ONCE_NEVER)
        {
            once->state = OS_THREAD_ONCE_INPROGRESS;
            os_mutex_unlock(&onceMutex);
            routine();
            os_mutex_lock(&onceMutex);
            once->state = OS_THREAD_ONCE_DONE;
        }

        while (once->state == OS_THREAD_ONCE_INPROGRESS)
        {
            /* avoid dead lock waiting for PTHREAD_ONCE_DONE state */
            os_mutex_unlock(&onceMutex);
            usleep(MSEC_TO_USEC(10));
            os_mutex_lock(&onceMutex);
        }
        os_mutex_unlock(&onceMutex);
    }
    else
    {
        /* this is for static constructures before the scheduler is started */
        if (once->state == OS_THREAD_ONCE_NEVER)
        {
            once->state = OS_THREAD_ONCE_INPROGRESS;
            routine();
            once->state = OS_THREAD_ONCE_DONE;
        }
   }

    return 0;
}
#elif defined (__WIN32__)
/** Windows does not support pipes, so we made our own with a pseudo socketpair.
 * @param fildes fildes[0] is open for reading, filedes[1] is open for writing
 * @return 0 upon success, else -1 with errno set to indicate error
 */
int pipe(int fildes[2])
{
    struct sockaddr_in addr;  
    int listener, connector, acceptor;
    socklen_t addrlen = sizeof(addr);

    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
    {
        errno = EMFILE;
        return -1;
    }
    if ((connector = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
    {
        closesocket(listener);
        errno = EMFILE;
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; 

    int reuse = 0;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, 
                   (char*)&reuse, (socklen_t)sizeof(reuse)) < 0)
    {
        closesocket(listener);
        closesocket(connector);
        errno = EMFILE;
        return -1;
    }

    if (bind(listener, (const struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        closesocket(listener);
        closesocket(connector);
        errno = EMFILE;
        return -1;
    }
    
    if  (getsockname(listener, (struct sockaddr*)&addr, &addrlen) < 0)
    {
        closesocket(listener);
        closesocket(connector);
        errno = EMFILE;
        return -1;
    }

    if (listen(listener, 1) < 0)
    {
        closesocket(listener);
        closesocket(connector);
        errno = EMFILE;
        return -1;
    }

    if (connect(connector, (const struct sockaddr*)&addr, addrlen) < 0)
    {
        closesocket(listener);
        closesocket(connector);
        errno = EMFILE;
        return -1;
    }
   
    if ((acceptor = accept(listener, NULL, NULL)) < 0)
    {
        closesocket(listener);
        closesocket(connector);
        errno = EMFILE;
        return  -1;
    }

    int flag = 1;
    setsockopt(connector, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
    setsockopt(acceptor, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

    fildes[0] = connector;
    fildes[1] = acceptor;
    closesocket(listener);
    return 0;
}
#endif

#if defined (__FreeRTOS__)
extern const void* stack_malloc(unsigned long length);

#endif  // FreeRTOS

/** Entry point to a thread.
 * @param metadata for entering the thread
 */
#if defined (__FreeRTOS__)
static void os_thread_start(void *arg)
{
    ThreadPriv *priv = arg;
    vTaskSetApplicationTaskTag(NULL, arg);
    _impure_ptr = priv->reent;
    (*priv->entry)(priv->arg);

    TaskList *current = &taskList;
    TaskList *last = &taskList;
    vTaskSuspendAll();
    while (current->task != xTaskGetCurrentTaskHandle())
    {
        last = current;
        current = current->next;
        HASSERT(current);
    }

    last->next = current->next;
    free(current);
    xTaskResumeAll();

    free(priv->reent);
    free(priv);
    vTaskDelete(NULL);
}
#endif

#if !defined (__EMSCRIPTEN__)

#if defined(__FreeRTOS__)
#if (configSUPPORT_STATIC_ALLOCATION == 1)
/** Static memory allocators for idle system thread.
 * @param pxIdleTaskTCBBuffer pointer to pointer to TCB
 * @param pxIdelTaskStackBuffer pointer to pointer to Stack
 * @param ulIdleTaskStackSize pointer to stack size
 */

void vApplicationGetIdleTaskMemory(StaticTask_t **pxIdleTaskTCBBuffer,
                                   StackType_t **pxIdleTaskStackBuffer,
                                   uint32_t *ulIdleTaskStackSize);

void vApplicationGetIdleTaskMemory(StaticTask_t **pxIdleTaskTCBBuffer,
                                   StackType_t **pxIdleTaskStackBuffer,
                                   uint32_t *ulIdleTaskStackSize)
{
    const uint32_t stksz = configMINIMAL_STACK_SIZE*sizeof(StackType_t);
    *pxIdleTaskTCBBuffer = (StaticTask_t *) malloc(sizeof(StaticTask_t));
    HASSERT(*pxIdleTaskTCBBuffer);
    *pxIdleTaskStackBuffer = (StackType_t *) malloc(stksz);
    HASSERT(*pxIdleTaskStackBuffer);
    *ulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

 * @param pxTimerTaskTCBBuffer pointer to pointer to TCB
 * @param pxIdelTaskStackBuffer pointer to pointer to Stack
 * @param ulTimerTaskStackSize pointer to stack size
 */

void vApplicationGetTimeraskMemory(StaticTask_t **pxTimerTaskTCBBuffer,
                                   StackType_t **pxTimerTaskStackBuffer,
                                   uint32_t *ulTimerTaskStackSize);

void vApplicationGetTimerTaskMemory(StaticTask_t **pxTimerTaskTCBBuffer,
                                   StackType_t **pxTimerTaskStackBuffer,
                                   uint32_t *ulTimerTaskStackSize)
{
    const uint32_t stksz = configMINIMAL_STACK_SIZE*sizeof(StackType_t);
    *pxTimerTaskTCBBuffer = (StaticTask_t *) malloc(sizeof(StaticTask_t));
    HASSERT(*pxTimerTaskTCBBuffer);
    *pxTimerTaskStackBuffer = (StackType_t *) malloc(stksz);
    HASSERT(*pxTimerTaskStackBuffer);
    *ulTimerTaskStackSize = configMINIMAL_STACK_SIZE;
}
#endif // configSUPPORT_STATIC_ALLOCATION
#endif // FreeRTOS


/** Create a thread.
 * @param thread handle to the created thread
 * @param name name of thread, NULL for an auto generated name
 * @param priority priority of created thread, 0 means default,
 *        lower numbers means lower priority, higher numbers mean higher priority
 * @param stack_size size in bytes of the created thread's stack
 * @param start_routine entry point of the thread
 * @param arg entry parameter to the thread
 * @return 0 upon success or error number upon failure
 */
int os_thread_create(os_thread_t *thread, const char *name, int priority,
                     size_t stack_size,
                     void *(*start_routine) (void *), void *arg)
{
    static unsigned int count = 0;
    char auto_name[10];

    if (name == NULL)
    {
        strcpy(auto_name, "thread.");
        auto_name[9] = '\0';
        auto_name[8] = '0' + (count % 10);
        auto_name[7] = '0' + (count / 10);
        count++;
        name = auto_name;
    }

#if defined (__FreeRTOS__)
    ThreadPriv *priv = malloc(sizeof(ThreadPriv));
    
    priv->entry = start_routine;
    priv->selectEventBit = 0;
    priv->arg = arg;
    priv->reent = allocate_reent();
    
    if (priority == 0)
    {
        priority = configMAX_PRIORITIES / 2;
    }
    else if (priority >= configMAX_PRIORITIES)
    {
        priority = configMAX_PRIORITIES - 1;
    }
    
    if (stack_size == 0)
    {
        stack_size = 2048;
    }

    TaskList *current = &taskList;
    vTaskSuspendAll();
    while (current->next != NULL)
    {
        current = current->next;
    }
    
    TaskList *task_new = malloc(sizeof(TaskList));
    task_new->task = NULL;
    task_new->next = NULL;
    task_new->unused = stack_size;
    current->next = task_new;
    xTaskResumeAll();
    
#if (configSUPPORT_STATIC_ALLOCATION == 1)
    if (thread)
    {
        *thread = xTaskCreateStatic(os_thread_start,
                                        priv,
                                        priority,
                           (const char *const)name,
                           stack_size/sizeof(portSTACK_TYPE),
                           priv,
                           priority,
                           (xTaskHandle*)thread,
                           (long unsigned int*)stack_malloc(stack_size),
                           NULL);
        task_new->task = *thread;
        task_new->name = (char*)pcTaskGetTaskName(*thread);
    }
    else
    {
        xTaskHandle task_handle;
        xTaskGenericCreate(os_thread_start,
                           (const char *const)name,
                           stack_size/sizeof(portSTACK_TYPE),
                           priv,
                           priority,
                           (xTaskHandle*)&task_handle,
                           (long unsigned int*) stack_malloc(stack_size),
                           NULL);
        task_new->task = task_handle;
        task_new->name = (char*)pcTaskGetTaskName(task_handle);
    }

    return 0;
#else // not freertos
    pthread_attr_t attr;

    int result = pthread_attr_init(&attr);
    if (result != 0)
    {
        return result;
    }
    result = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (result != 0)
    {
        return result;
    }

#if !defined(__linux__) && !defined(__MACH__) /* Linux allocates stack as needed */
    struct sched_param sched_param;
    result = pthread_attr_setstacksize(&attr, stack_size);
    if (result != 0)
    {
        return result;
    }

    sched_param.sched_priority = 0; /* default priority */
    result = pthread_attr_setschedparam(&attr, &sched_param);
    if (result != 0)
    {
        return result;
    }

    result = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (result != 0)
    {
        return result;
    }

    result = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    if (result != 0)
    {
        return result;
    }
#endif // not linux and not mac
    result = pthread_create(thread, &attr, start_routine, arg);

#if !defined (__MINGW32__) && !defined (__MACH__)
    if (!result) pthread_setname_np(*thread, name);
#endif

    return result;
#endif // freertos or not
}
#endif // __EMSCRIPTEN__

long long os_get_time_monotonic(void)
{
    static long long last = 0;
    long long time;
#if defined (__FreeRTOS__)
    portTickType tick = xTaskGetTickCount();
    time = ((long long)tick) << NSEC_TO_TICK_SHIFT;
#elif defined (__MACH__)
    /* get the timebase info */
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    
    /* get the timestamp */
    time = (long long)mach_absolute_time();
    
    /* convert to nanoseconds */
    time *= info.numer;
    time /= info.denom;
#elif defined (__WIN32__)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time = ((long long)tv.tv_sec * 1000LL * 1000LL * 1000LL) +
           ((long long)tv.tv_usec * 1000LL);
#else
    struct timespec ts;
#if defined (__nuttx__)
    clock_gettime(CLOCK_REALTIME, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    time = ((long long)ts.tv_sec * 1000000000LL) + ts.tv_nsec;
    
#endif
    /* This logic ensures that every successive call is one value larger
     * than the last.  Each call returns a unique value.
     */
    if (time <= last)
    {
        last++;
    }
    else
    {
        last = time;
    }

    return last;
}

#if defined(__EMSCRIPTEN__)
int os_thread_once(os_thread_once_t *once, void (*routine)(void))
{
    if (once->state == OS_THREAD_ONCE_NEVER)
    {
        once->state = OS_THREAD_ONCE_INPROGRESS;
        routine();
        once->state = OS_THREAD_ONCE_DONE;
    }
    else if (once->state == OS_THREAD_ONCE_INPROGRESS)
    {
        DIE("Recursive call to os_thread_once.");
    }
    return 0;
}
#endif

#if defined (__FreeRTOS__)
/* standard C library hooks for multi-threading */

/** Lock access to malloc.
 */
void __malloc_lock(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        vTaskSuspendAll();
    }
}

/** Unlock access to malloc.
 */
void __malloc_unlock(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        xTaskResumeAll();
    }
}

#if defined (_REENT_SMALL)
void *__real__malloc_r(size_t size);
void __real__free_r(void *address);

/** malloc() wrapper for newlib-nano
 * @param size size of malloc in bytes
 * @return pointer to newly malloc'd space
 */
void *__wrap__malloc_r(size_t size)
{
    void *result;
    __malloc_lock();
    result = __real__malloc_r(size);
    __malloc_unlock();
    return result;
}

/** free() wrapper for newlib-nano
 * @param address pointer to previously malloc'd address
 */
void __wrap__free_r(void *address)
{
    __malloc_lock();
    __real__free_r(address);
    __malloc_unlock();
}
#endif

/** Implementation of standard sleep().
 * @param seconds number of seconds to sleep
 */
unsigned sleep(unsigned seconds)
{
    vTaskDelay(seconds * configTICK_RATE_HZ);
    return 0;
}

/** Implementation of standard usleep().
 * @param usec number of microseconds to sleep
 */
int usleep(useconds_t usec)
{
    long long nsec = usec;
    nsec *= 1000;
    vTaskDelay(nsec >> NSEC_TO_TICK_SHIFT);
    return 0;
}

void abort(void)
{
#if defined(TARGET_LPC2368) || defined(TARGET_LPC11Cxx) || defined(TARGET_LPC1768) || defined(GCC_ARMCM3) || defined (GCC_ARMCM0) || defined(TARGET_PIC32MX)
    diewith(BLINK_DIE_ABORT);
#endif
    for (;;)
    {
    }
}

extern char *heap_end;
char *heap_end = 0;
void* _sbrk_r(struct _reent *reent, ptrdiff_t incr)
{
    /** @todo (Stuart Baker) change naming to remove "cs3" convention */
    extern char __cs3_heap_start;
    extern char __cs3_heap_end; /* Defined by the linker */
    char *prev_heap_end;
    if (heap_end == 0)
    {
        heap_end = &__cs3_heap_start;
    }
    prev_heap_end = heap_end;
    if (heap_end + incr > &__cs3_heap_end)
    {
        /* Heap and stack collistion */
        diewith(BLINK_DIE_OUTOFMEM);
        return 0;
    }
    heap_end += incr;
    return (caddr_t) prev_heap_end;
}

xTaskHandle volatile overflowed_task = 0;
signed portCHAR * volatile overflowed_task_name = 0;

/** This method is called if a stack overflows its boundries.
 * @param task task handle for violating task
 * @param name name of violating task
 */
void vApplicationStackOverflowHook(xTaskHandle task, signed portCHAR *name)
{
    overflowed_task = task;
    overflowed_task_name = name;
    diewith(BLINK_DIE_STACKOVERFLOW);
}

/** This method will be called repeatedly from the idle task. If needed, it can
 * be overridden in hw_init.c.
 */
void hw_idle_hook(void) __attribute__((weak));

void hw_idle_hook(void)
{
}

/** Here we will monitor the other tasks.
 */
void vApplicationIdleHook( void )
{
    vTaskSuspendAll();
    xTaskResumeAll();
    hw_idle_hook();
    for (TaskList *tl = &taskList; tl != NULL; tl = tl->next)
    {
        if (tl->task)
        {
            tl->name = (char*)pcTaskGetTaskName(tl->task);
            tl->unused = uxTaskGetStackHighWaterMark(tl->task) * sizeof(portSTACK_TYPE);
        }
    }
}

/** Entry point to the main thread.
 * @param arg unused argument
 * @return NULL;
 */
void main_thread(void *arg)
{
    ThreadPriv *priv = arg;
    char *argv[2] = {"nmranet", NULL};
    vTaskSetApplicationTaskTag(NULL, arg);
    _impure_ptr = priv->reent;

    /* setup the monitoring entries for the timer and idle tasks */
#if configUSE_TIMERS
    taskList.next = malloc(sizeof(TaskList)*2);
    taskList.next->task = xTimerGetTimerDaemonTaskHandle();
    taskList.next->unused = uxTaskGetStackHighWaterMark(taskList.next->task);
    taskList.next->next = taskList.next + 1;
    taskList.next->next->task = xTaskGetIdleTaskHandle();
    taskList.next->next->unused = uxTaskGetStackHighWaterMark(taskList.next->next->task);
    taskList.next->next->next = NULL;
#else
    taskList.next = malloc(sizeof(TaskList));
    taskList.next->task = xTaskGetIdleTaskHandle();
    taskList.next->unused = uxTaskGetStackHighWaterMark(taskList.next->task);
    taskList.next->next = NULL;
#endif

    /* Allow any library threads to run that must run ahead of main */
    taskYIELD();

    appl_main(1, argv);
    // If the main thread returns, FreeRTOS usually crashes the CPU in a
    // hard-to-debug state. Let's avoid that.
    abort();
}
#endif

/** This function does nothing. It can be used to alias other symbols to it via
 * linker flags, such as atexit(). */
int ignore_fn(void)
{
    return 0;
}

#if !defined (__MINGW32__)
int main(int argc, char *argv[]) __attribute__ ((weak));
#endif

/** Entry point to program.
 * @param argc number of command line arguments
 * @param argv array of command line aguments
 * @return 0, should never return
 */
int main(int argc, char *argv[])
{
#if defined (__FreeRTOS__)
    /* initialize the processor hardware */
    hw_init();

    ThreadPriv *priv = malloc(sizeof(ThreadPriv));
    xTaskHandle task_handle;
    int priority;
    priv->reent = _impure_ptr;
    priv->selectEventBit = 0;
    priv->entry = NULL;
    priv->arg = NULL;
    
    if (config_main_thread_priority() == 0xdefa01)
    {
        priority = configMAX_PRIORITIES / 2;
    }
    else
    {
        priority = config_main_thread_priority();
    }
    
#ifndef TARGET_LPC11Cxx
    /* stdin */
    if (open(STDIN_DEVICE, O_RDWR) < 0)
    {
        open("/dev/null", O_RDWR);
    }
    /* stdout */
    if (open(STDOUT_DEVICE, O_RDWR) < 0)
    {
        open("/dev/null", O_RDWR);
    }
    /* stderr */
    if (open(STDERR_DEVICE, O_WRONLY) < 0)
    {
        open("/dev/null", O_WRONLY);
    }
#endif

    /* start the main thread */
    xTaskGenericCreate(
        main_thread, (char *)"thread.main",
        config_main_thread_stack_size() / sizeof(portSTACK_TYPE), priv,
        priority, &task_handle,
        (long unsigned int *)stack_malloc(config_main_thread_stack_size()),
        NULL);
    taskList.task = task_handle;
    taskList.unused = config_main_thread_stack_size();
    taskList.name = "thread.main";

    vTaskStartScheduler();
#else
#if defined (__WIN32__)
    /* enable Windows networking */
    WSADATA wsa_data;
    WSAStartup(WINSOCK_VERSION, &wsa_data);
#endif
    return appl_main(argc, argv);
#endif
}
