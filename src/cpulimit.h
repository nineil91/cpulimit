/**
 *
 * cpulimit - a CPU limiter for Linux
 *
 * Copyright (C) 2005-2012, by:  Angelo Marletta <angelo dot marletta at gmail dot com> 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 **************************************************************
 *
 * This is a simple program to limit the cpu usage of a process
 * If you modify this code, send me a copy please
 *
 * Get the latest version at: http://github.com/opsengine/cpulimit
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <dirent.h>
#include <pthread.h>

#ifdef __APPLE__ || __FREEBSD__
#include <libgen.h>
#endif

#ifdef HAVE_SYS_SYSINFO_H
#include <sys/sysinfo.h>
#endif

#ifdef __APPLE__
#include "memrchr.c"
#endif

//some useful macro
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

//control time slot in microseconds
//each slot is splitted in a working slice and a sleeping slice
//TODO: make it adaptive, based on the actual system load
#define TIME_SLOT 100000

#define MAX_PRIORITY -10


//cpulimit2부분
#ifndef _CPULIMIT2_H
#define _CPULIMIT2_H

//=======================DEFINITIONS=======================================

// Kernel time resolution (inverse of one jiffy interval) in Hertz.
#define HZ 100

// Define a default correction to be added to the limit user gives us.
// This can be positive or negative but should be in the range -1 to 1.
#define PER_CORR -0.04

// Time quantum in microseconds. it's split in a working period and a sleeping one.
#define period 100000

// Some useful macros...
#define min(a,b) (a<b?a:b)
#define max(a,b) (a>b?a:b)

#define OPTBLOCKDONE(o) ((o->pid > 0 && o->percLimit > 0) || ((o->bExactName ^ o->bStartsWith ^ o->bExactPath) && o->text != NULL && o->percLimit > 0))

//=====================STRUCTURES AND DATATYPES============================

// Holds info related to each process being managed.
typedef struct {
   int pid;      // Holds the process' pid.
   pthread_t threadid;   // Holds the thread id of the thread who is managing the process.
   float limit;  // Usage limit [0,1].
   void *next;
} process;

typedef struct {
   process *head;
} proc_list;

typedef struct{
   int percLimit;
   int pid;

   int bExactName;
   int bExactPath;
   int bStartsWith;
   char *text;
   void *next;
} OptionBlock;

typedef struct{
   OptionBlock *head;
} OptionList;

// Process instant photo
struct process_screenshot {
   struct timespec when;   //timestamp
   int jiffies;   //jiffies count of the process
   int cputime;   //microseconds of work from previous screenshot to current
};

// Extracted process statistics
struct cpu_usage {
   float pcpu;
   float workingrate;
};

typedef struct {
   struct process_screenshot *samples;
   int size;
   int samplesUsed;
   int front;
   int tail;

   float cpu_percent;  // How much of the cpu we use.
   float workcycle_percent; // What percent of the time we are given to work.
   float working_percent;  // Out of the amount of time given to work.

} cpuSamples;

//=========================GLOBAL VARIABLES====================================

// If user specifies a single pid in cmd line, this is it.
int pid;

OptionList *optList;

// Linked list of processes to be controlled.
proc_list *plist;
pthread_mutex_t listLock;

int noMatchingPids;
int noStaticPids;

// Executable file name
char *program_name;
// Verbose mode
int verbose;
// Lazy mode
int lazy;
// CPU usage limit.
float limit;
// Whether we should look for exact program name, or only a string
// that begins with the name given by user.
int exactname;

//========================FUNCTION DEFINITIONS===================================
void checkOptions();
proc_list *newProcList();
OptionList *newOptList();
OptionBlock *newOptBlock();
process *newProc();
void addToProcList( proc_list *list, process *newproc );
void addToOptList( OptionList *list, OptionBlock *optBlock );
void removeFromProcList( proc_list *list, int pid );
void removeFromOptionList( OptionList *list, OptionBlock *item );
process *searchProcList( proc_list *list, int pid );
void printProcList( proc_list *list );
int waitforpid( int pid );
float percLimitToFloat( int percLimit );
void *manage_list( void *data );
void *slow_down_process( void *process_parm );
int getpidof(const char *process);
void *find_procs(void *procname);
void quit(int sig);
void errorQuit(char *message);
int getjiffies(int pid);
int compute_cpu_usage(int pid,int last_working_quantum,struct cpu_usage *pusage,
                      struct process_screenshot *ps, int *front, int *tail);
int GetCpuUsage( int pid, int last_working_quantum, cpuSamples *c );
void print_caption();
void usageAndExit(FILE *stream,int exit_code);

#endif // _CPULIMIT2_H



//list.h부분

#ifndef __LIST__
#define __LIST__

#ifndef  TRUE
    #define TRUE 1
    #define FALSE 0
#endif

struct list_node {
    //pointer to the content of the node
    void *data;
    //pointer to previous node
    struct list_node *previous;
    //pointer to next node
    struct list_node *next;
};

struct list {
    //first node
    struct list_node *first;
    //last node
    struct list_node *last;
    //size of the search key in bytes
    int keysize;
    //element count
    int count;
};

/*
 * Initialize a list, with a specified key size
 */
void init_list(struct list *l,int keysize);

/*
 * Add a new element at the end of the list
 * return the pointer to the new node
 */
struct list_node *add_elem(struct list *l,void *elem);

/*
 * Delete a node
 */
void delete_node(struct list *l,struct list_node *node);

/*
 * Delete a node from the list, even the content pointed by it
 * Use only when the content is a dynamically allocated pointer
 */
void destroy_node(struct list *l,struct list_node *node);

/*
 * Check whether a list is empty or not
 */
int is_empty_list(struct list *l);

/*
 * Return the element count of the list
 */
int get_list_count(struct list *l);

/*
 * Return the first element (content of the node) from the list
 */
void *first_elem(struct list *l);

/*
 * Return the first node from the list
 */
struct list_node *first_node(struct list *l);

/*
 * Return the last element (content of the node) from the list
 */
void *last_elem(struct list *l);

/*
 * Return the last node from the list
 */
struct list_node *last_node(struct list *l);

/*
 * Search an element of the list by content
 * the comparison is done from the specified offset and for a specified length
 * if offset=0, the comparison starts from the address pointed by data
 * if length=0, default keysize is used for length
 * if the element is found, return the node address
 * else return NULL
 */
struct list_node *xlocate_node(struct list *l,void *elem,int offset,int length);

/*
 * The same of xlocate_node(), but return the content of the node
 */
void *xlocate_elem(struct list *l,void *elem,int offset,int length);

/*
 * The same of calling xlocate_node() with offset=0 and length=0
 */
struct list_node *locate_node(struct list *l,void *elem);

/*
 * The same of locate_node, but return the content of the node
 */
void *locate_elem(struct list *l,void *elem);

/*
 * Delete all the elements in the list
 */
void clear_list(struct list *l);

/*
 * Delete every element in the list, and free the memory pointed by all the node data
 */
void destroy_list(struct list *l);

#endif

//process_group.h
ifndef __PROCESS_GROUP_H

#define __PROCESS_GROUP_H

#define PIDHASH_SZ 1024
#define pid_hashfn(x) ((((x) >> 8) ^ (x)) & (PIDHASH_SZ - 1))

struct process_group
{
	//hashtable with all the processes (array of struct list of struct process)
	struct list *proctable[PIDHASH_SZ];
	struct list *proclist;
	pid_t target_pid;
	int include_children;
	struct timeval last_update;
};

int init_process_group(struct process_group *pgroup, int target_pid, int include_children);

void update_process_group(struct process_group *pgroup);

int close_process_group(struct process_group *pgroup);

int find_process_by_pid(pid_t pid);

int find_process_by_name(const char *process_name);

int remove_process(struct process_group *pgroup, int pid);

#endif

//process_iterator.h
#ifndef __PROCESS_ITERATOR_H

#define __PROCESS_ITERATOR_H

#include <unistd.h>
#include <limits.h>
#include <dirent.h>

//USER_HZ detection, from openssl code
#ifndef HZ
# if defined(_SC_CLK_TCK) \
     && (!defined(OPENSSL_SYS_VMS) || __CTRL_VER >= 70000000)
#  define HZ ((double)sysconf(_SC_CLK_TCK))
# else
#  ifndef CLK_TCK
#   ifndef _BSD_CLK_TCK_ /* FreeBSD hack */
#    define HZ  100.0
#   else /* _BSD_CLK_TCK_ */
#    define HZ ((double)_BSD_CLK_TCK_)
#   endif
#  else /* CLK_TCK */
#   define HZ ((double)CLK_TCK)
#  endif
# endif
#endif

#ifdef __FreeBSD__
#include <kvm.h>
#endif

// process descriptor
struct process {
	//pid of the process
	pid_t pid;
	//ppid of the process
	pid_t ppid;
	//start time (unix timestamp)
	int starttime;
	//cputime used by the process (in milliseconds)
	int cputime;
	//actual cpu usage estimation (value in range 0-1)
	double cpu_usage;
	//absolute path of the executable file
	char command[PATH_MAX+1];
};

struct process_filter {
	int pid;
	int include_children;
	char program_name[PATH_MAX+1];
};

struct process_iterator {
#ifdef __linux__
	DIR *dip;
	int boot_time;
#elif defined __FreeBSD__
	kvm_t *kd;
	struct kinfo_proc *procs;
	int count;
	int i;
#elif defined __APPLE__
	int i;
	int count;
	int *pidlist;
#endif
	struct process_filter *filter;
};

int init_process_iterator(struct process_iterator *i, struct process_filter *filter);

int get_next_process(struct process_iterator *i, struct process *p);

int close_process_iterator(struct process_iterator *i);

#endif