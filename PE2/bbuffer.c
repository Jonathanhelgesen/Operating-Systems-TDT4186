#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "sem.h"
#include "bbuffer.h"

typedef struct SEM // Burde unngå å definere denne to steder...
{
    int count; // Trenger man waiting/wakeups? Ser det er implementert noen steder.
    pthread_mutex_t *mutex;
    pthread_cond_t *cv; // Conditional value
} SEM;

typedef struct BNDBUF
{
    pthread_mutex_t *lock;
    pthread_cond_t *bufferCV;
    SEM *empty;
    SEM *full;      // Used for checking if the buffer is full/empty
    int *fds;               // File descriptors
    int size, num_entries, next_add, next_get;     // Keeping track of positions and size of buffer
} BNDBUF;

/* Creates a new Bounded Buffer.
 *
 * This function creates a new bounded buffer and all the helper data
 * structures required by the buffer, including semaphores for
 * synchronization. If an error occurs during the initialization the
 * implementation shall free all resources already allocated by then.
 *
 * Parameters:
 *
 * size     The number of integers that can be stored in the bounded buffer.
 *
 * Returns:
 *
 * handle for the created bounded buffer, or NULL if an error occured.
 */
BNDBUF *bb_init(unsigned int size)
{
    BNDBUF *bndbuf = malloc(sizeof(BNDBUF));
    int *fds_pointer = malloc(size * sizeof(int));
    SEM *empty = malloc(sizeof(SEM));
    SEM *full = malloc(sizeof(SEM));
    pthread_mutex_t *lock = malloc(sizeof(pthread_mutex_t));

    if (bndbuf == NULL || fds_pointer == NULL || empty == NULL || full == NULL || lock == NULL)
    {
        printf("Not able to allocate memory\n");
        if (bndbuf != NULL)
            free(bndbuf);
        if (fds_pointer != NULL)
            free(fds_pointer);
        if (empty != NULL)
            free(empty);
        if (full != NULL)
            free(full);
        if (lock != NULL)
            free(lock);
        return NULL;
    }

    if (pthread_mutex_init(lock, NULL) != 0)
    {
        printf("Error when making mutex\n");
        free(bndbuf);
        free(fds_pointer);
        free(empty);
        free(full);
        free(lock);
        return NULL;
    }

    bndbuf->fds = fds_pointer;
    bndbuf->empty = sem_init(size);
    bndbuf->full = sem_init(0);
    bndbuf->lock = lock;
    //bndbuf->position = 0;
    bndbuf->size = size;
    bndbuf->next_add = 0;
    bndbuf->next_get = 0;
    bndbuf->num_entries = 0;
    bndbuf->bufferCV = malloc(sizeof(pthread_cond_t));
    pthread_cond_init(bndbuf->bufferCV, NULL);
    return bndbuf;
}

/* Destroys a Bounded Buffer.
 *
 * All resources associated with the bounded buffer are released.
 *
 * Parameters:
 *
 * bb       Handle of the bounded buffer that shall be freed.
 */
void bb_del(BNDBUF *bb)
{
    free(bb->empty);
    free(bb->full);
    free(bb->fds);
    free(bb->lock);
    free(bb);
}

/* Add an element to the bounded buffer.
 *
 * This function adds an element to the bounded buffer. If the bounded
 * buffer is full, the function blocks until an element is removed from
 * the buffer.
 *
 * Parameters:
 *
 * bb     Handle of the bounded buffer.
 * fd     Value that shall be added to the buffer.
 *
 * Returns:
 *
 * the int element
 */
void bb_add(BNDBUF *bb, int fd)
{
    // Make empty-semaphore wait
    P(bb->empty);

    pthread_mutex_lock(bb->lock);
    // Entering critical section
    // conditional while løkke som gjør at den venter til det er plass i bufferen her?

    while (bb->num_entries == bb->size){ //Tror ikke vi trenger cond var
        pthread_cond_wait(bb->bufferCV, bb->lock);
    }
    bb->fds[bb->next_add] = fd;
    bb->num_entries++;
    bb->next_add++;
    if (bb->next_add == bb->size) {
        bb->next_add = 0;
    }
    pthread_cond_signal(bb->bufferCV);
    pthread_mutex_unlock(bb->lock);
    // Send signal to full-semaphore
    V(bb->full);

    printf("Added %d\n", fd);
}

/* Retrieve an element from the bounded buffer.
 *
 * This function removes an element from the bounded buffer. If the bounded
 * buffer is empty, the function blocks until an element is added to the
 * buffer.
 *
 * Parameters:
 *
 * bb         Handle of the bounded buffer.
 *
 * Returns:
 *
 * the int element
 */
int bb_get(BNDBUF *bb)
{
    // Make full-semaphore wait
    P(bb->full);
    pthread_mutex_lock(bb->lock);

    // Entering critical section
    while (bb->num_entries == 0){
        pthread_cond_wait(bb->bufferCV, bb->lock);
    }
    int fd = bb->fds[bb->next_get];
    bb->num_entries--;
    bb->next_get++;
    if (bb->next_get == bb->size) {
        bb->next_get = 0;
    }
    pthread_cond_signal(bb->bufferCV);
    pthread_mutex_unlock(bb->lock);

    // Send signal to empty-semaphore
    V(bb->empty);
    printf("Got %d\n", fd);
    return fd;
}

struct readThreadParams
{
    int fd;
    BNDBUF *bb;
};

void *parse_bb_add(void *parameters)
{
    struct readThreadParams *params = parameters;
    bb_add(params->bb, params->fd);
}

void *parse_bb_get(void *pbb)
{
    BNDBUF *bb =(BNDBUF*) pbb;
    bb_get(bb);
}

int main()
{
    BNDBUF *bb = bb_init(4);
    pthread_mutex_t mutex;
    // struct readThreadParams r1;
    // r1.bb = bb;
    // r1.fd = 1;
    // parse_bb_add((void*) &r1);
    // struct readThreadParams r2;
    // r2.bb = bb;
    // r2.fd = 2;
    // parse_bb_add((void*) &r2);
    // parse_bb_get((void*) bb);
    // struct readThreadParams r3;
    // r3.bb = bb;
    // r3.fd = 3;
    // parse_bb_add((void*) &r3);
    // struct readThreadParams r4;
    // r4.bb = bb;
    // r4.fd = 4;
    // parse_bb_add((void*) &r4);
    // parse_bb_get(bb);
    // parse_bb_get(bb);

    //sleep(10);

    pthread_mutex_init(&mutex, NULL);
    int THREAD_NUM = 8;
    pthread_t th[THREAD_NUM];
    int numbers[THREAD_NUM];
    int i;
    for (i = 0; i < THREAD_NUM; i++) {
        numbers[i] = i * 10;
    }
    int counter = 0;

    while (counter < THREAD_NUM - 1)
    {
        pthread_mutex_lock(&mutex);
        struct readThreadParams readParams;
        readParams.bb = bb;
        readParams.fd = numbers[counter];
        counter++;
        pthread_mutex_unlock(&mutex);

        if (counter % 2 == 0 || counter % 3 == 0)
        {
            printf("counter in add at: %d\n", counter);

            if (pthread_create(&th[counter-1], NULL, &parse_bb_add, &readParams) != 0)
            {
                perror("Failed to create thread");
            }
        }
        else
        {
            printf("counter in get at: %d\n", counter);
            if (pthread_create(&th[counter-1], NULL, &parse_bb_get, bb) != 0)
            {
                perror("Failed to create thread");
            }
        }
    }




    for (i = 0; i < THREAD_NUM; i++)
    {
        if (pthread_join(th[i], NULL) != 0)
        {
            perror("Failed to join thread");
        }
    }
    bb_del(bb);
    return 0;
}
