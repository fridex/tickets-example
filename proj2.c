/*
 ***********************************************************************
 *
 *        @version  1.0
 *        @date     03/07/2014 09:38:33 PM
 *        @author   Fridolin Pokorny <fridex.devel@gmail.com>
 *
 *        @brief Ticket algorithm implementation using threads and thread
 *        conditional variables.
 *
 ***********************************************************************
 */

/*
 * Fix nanosleep() implicit declaration warning
 */
#ifndef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 200809L
#endif

#ifndef _XOPEN_SOURCE
#  define _XOPEN_SOURCE 500
#endif

#ifndef NDEBUG
#  define NDEBUG
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>

#define UNUSED(X)		((void) X)

typedef int ticket_t;
typedef unsigned threadid_t;
typedef void * (* fun_t)(void *);

/**
 * @brief  thread param
 */
typedef struct thread_param_t {
	threadid_t		id;
	int			loop_count;
} thread_param_t;

/**
 * @brief  Critical section mutex
 */
pthread_mutex_t section_mutex;

/**
 * @brief  Next ticket mutex
 */
pthread_mutex_t ticket_mutex;

/**
 * @brief  Actual ticket for critical section condition
 */
pthread_cond_t  section_cond;


/**
 * @brief  Actual ticket for critical section
 */
static
ticket_t actual_ticket = 0;


/**
 * @brief  Next ticket to be granted
 */
static
ticket_t next_ticket = 0;

/**
 * @brief  Print help for the user
 *
 * @param pname program name
 *
 * @return always EXIT_FAILURE
 */
static
int print_help(const char * pname) {
	static const char * help_begin =
		"Ticket algorithm implementation using POSIX threads\n"
		"Fridolin Pokorny, 2014 <fridex.devel@gmail.com>\n"
		"USAGE:\n"
		"\t";

	static const char * help_end =
		" THREAD_COUNT EXCL_COUNT\n"
		"\tTHREAD_COUNT\t\t- number of threads to be crated\n"
		"\tLOOP_COUNT\t\t- number of critical section entrance\n";

	fputs(help_begin, stderr);
	fputs(pname, stderr);
	fputs(help_end, stderr);

	return EXIT_FAILURE;
}

/**
 * @brief  Convert string to number and check if whole string was converted
 *
 * @param num place to store converted value
 * @param str string containing number
 *
 * @return false if string is not a number
 */
static
bool get_num(unsigned * num, const char * str) {
	char * endptr;
	*num = strtoul(str, &endptr, 10);

	if (endptr != str + (strlen(str)* sizeof(char)))
		return false;
	else
		return true;
}

/**
 * @brief  Suspend thread for <0, 0.5s>
 */
static inline
void suspend() {
	struct timespec ts;
	struct timeval  tv;

	gettimeofday(&tv, NULL);
	// we will loose some bits sometime
	unsigned seedp = (1000000 * tv.tv_sec + tv.tv_usec) + getpid();

	ts.tv_sec = 0;
	ts.tv_nsec = ((rand_r(&seedp) % 499999999) + 1);

	nanosleep(&ts, NULL);
}

/**
 * @brief  Get ticket to critical section
 *
 * @return ticket to critical section
 */
static
int getticket(void) {
	int res;

	pthread_mutex_lock(&ticket_mutex);
	res = next_ticket;
	next_ticket++;
	pthread_mutex_unlock(&ticket_mutex);

	return res;
}

/**
 * @brief  Enter critical section if available
 *
 * @param aenter ticket number
 */
static
void await(int aenter) {
	UNUSED(aenter);

	pthread_mutex_lock(&section_mutex);
	while (aenter != actual_ticket) {
		//fprintf(stderr, ">>> %d != %d\n", aenter, actual_ticket);
		pthread_cond_signal(&section_cond); // not me, signal other threads...
		pthread_cond_wait(&section_cond, &section_mutex); // suspend till change
	}
}

/**
 * @brief  Leave critical section and run next thread
 */
static
void advance(void) {
	actual_ticket++;
	pthread_cond_signal(&section_cond);
	pthread_mutex_unlock(&section_mutex);
}

/**
 * @brief  Iterate through critical section
 *
 * @param loop_count
 * @return NULL
 */
static
void * just_do_it(struct thread_param_t * param) {
	ticket_t ticket = 0;

	while ((ticket = getticket()) < param->loop_count) {
		//fprintf(stderr, "\t(%d) >>> my ticket %d\n", param->id, ticket);
		suspend();
		await(ticket);
		printf("%d\t(%d)\n", ticket, param->id); fflush(stdout);
		advance();
		suspend();
	}

	return NULL;
}

/**
 * @brief  Entry point
 *
 * @param argc argument count
 * @param argv[] argument vector
 *
 * @return EXIT_FAILURE on failure, otherwise EXIT_SUCCESS
 */
int main(int argc, char * argv[]) {
	unsigned thread_count = 0;
	unsigned loop_count = 0;
	pthread_t * threads = NULL;
	thread_param_t * params = NULL;

	if (argc != 3) {
		fputs("Invalid argument count\n", stderr);
		return print_help(argv[0]);
	}

	if (! get_num(&thread_count, argv[1]) || thread_count <= 0) {
		//fprintf(stderr, "Invalid thread count: '%s'\n", argv[1]);
		return print_help(argv[0]);
	}

	if (! get_num(&loop_count, argv[2]) || loop_count  <= 0) {
		//fprintf(stderr, "Invalid loop count: '%s'\n", argv[2]);
		return print_help(argv[0]);
	}

	threads = malloc(thread_count * sizeof(pthread_t));
	params  = malloc(thread_count * sizeof(thread_param_t));
	if (! threads || ! params) {
		fputs("Error: out of memory!\n", stderr);
		return EXIT_FAILURE;
	}

	pthread_mutex_init(&section_mutex, NULL);
	pthread_cond_init (&section_cond, NULL);

	/*
	 * Function for threads
	 */
	fun_t fun = (fun_t) just_do_it;

	/*
	 * Run ticket algorithm
	 */
	for (unsigned i = 0; i < thread_count; ++i) {
		params[i].id = i + 1;
		params[i].loop_count = (int) loop_count;
		pthread_create(&threads[i], NULL, fun, &params[i]);
	}

	/*
	 * Wait for all threads...
	 */
	for (unsigned i = 0; i < thread_count; ++i)
		pthread_join(threads[i], NULL);

	/*
	 * Clean up!
	 */
	free(threads); threads = NULL;
	free(params); params = NULL;

	pthread_mutex_destroy(&section_mutex);
	pthread_cond_destroy(&section_cond);

	return EXIT_SUCCESS;
}

