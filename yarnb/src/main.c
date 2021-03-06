/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file).

Before you say anything:
- Yes, this code is crap
- Yes, I need to rewrite it
- Yes, I will do it in the near future
- No, I'm not just saying that
- Yes, I really mean it
Now leave me alone...
 */

#include <yarn.h>
#include <yarn/timer.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>


struct task {
  size_t i;
  size_t n;

  yarn_word_t thread_count;
  
  yarn_time_t wait_time;

  size_t array_size;
  yarn_word_t array[];

};

#define TIME_END_NS 1000000
#define TIME_STEP_NS 100

#define SPEEDUP_MIN 0.5
#define SPEEDUP_STEP 0.5

#define DEBUG "DEBUG - "
#define INFO  "INFO  - "
#define WARN  "WARN  - "
#define ERROR "ERROR - "

#define CSV_SEP ";"

double get_speedup(yarn_time_t wait_time, yarn_word_t thread_count);

int speedup_search (const double target_speedup,
		    const yarn_time_t start_time, 
		    const yarn_time_t end_time, 
		    yarn_time_t* speedup_time, 
		    yarn_word_t thread_count);

void run_normal (struct task* t);
enum yarn_ret run_speculative (const yarn_word_t pool_id, void* task, yarn_word_t indvar);



static double g_max_speedup;
static double g_min_speedup;
static bool g_use_log;
static FILE* g_log_file;


inline int comp_speedup (double a, double b) {
  static const double epsilon = 0.1;

  if (fabs(a - b) < epsilon) {
    return 0;
  }

  return a > b ? 1 : -1;
}
inline int comp_time (yarn_time_t a, yarn_time_t b) {
  static const yarn_time_t epsilon = 100;

  if (llabs(a - b) < epsilon) {
    return 0;
  }

  return a > b ? 1 : -1;
}


static double calc_reference (void) {
#define N 100 // yes, I'll burn in hell for this... Shut it.
  static double a[N];
  static double b[N];
  static double c[N];
  for (int i = 0; i < N; ++i) {
    a[i] = (double)1 / (double)i;
    b[i] = (double)1 - a[i];
  }
  
  // Actual bench
  // This could be optimized in a million different ways so it only gives a rough idea.
  // Might not be a super good idea.
  yarn_time_t start = yarn_timer_sample_thread();
  for (int i = 0; i < N; ++i) {
    c[i] = a[i] / b[i];
  }
  yarn_time_t div_diff = yarn_timer_diff(start, yarn_timer_sample_thread());

  start = yarn_timer_sample_thread();
  for (int i = 0; i < N; ++i) {
    c[i] = a[i] * b[i];
  }
  yarn_time_t mul_diff = yarn_timer_diff(start, yarn_timer_sample_thread());
  

  printf(INFO "\n");
  printf(INFO "Reference point:\n");
  printf(INFO "\tOp count: %d\n", N);  
  printf(INFO "\tFP Mul: %zuns\n", mul_diff);
  printf(INFO "\tFP Div: %zuns\n", div_diff);
  
  // to kill any dead-code optimizations.
  double acc;
  for (int i = 0; i < N; ++i) {
    acc += c[i];
  }
  return acc;
}

static void warm_up (void) {
  printf(INFO "\n");
  printf(INFO "Warming up...\n");
  fflush(stdout);
  for (int i = 0; i < 10; ++i) {
    (void) get_speedup(0, 1);
  }
}

int main (int argc, char** argv) {
  
  if (argc > 1) {
    g_use_log = true;
    g_log_file = fopen(argv[1], "w");
    if(!g_log_file) {
      perror(__FUNCTION__);
      printf(ERROR "Invalid log file.");
      return 1;
    }
  }


  bool ret = yarn_init();
  if (!ret) goto yarn_error;

  // Get from command line?
  yarn_time_t start_time = 0;
  const yarn_time_t end_time = TIME_END_NS;

  const double min_speedup = SPEEDUP_MIN;
  const double speedup_step = SPEEDUP_STEP;

  printf(INFO "Yarn Benchmark\n");
  printf(INFO "\tSpeculative threads = %zu\n", yarn_thread_count());
  printf(INFO "\tTime range = [0ns, %zuns]\n", end_time);
  printf(INFO "\tSpeedup min = %2.2f\n", min_speedup);
  printf(INFO "\tSpeedup delta = %2.2f\n", speedup_step);
  if (g_use_log) {
    printf(INFO "\tLog file = %s\n", argv[1]);
    fprintf(g_log_file, "Threads"CSV_SEP"Speedup"CSV_SEP"Time\n");
  }

  calc_reference();
  warm_up();

  printf(INFO "\n");
  printf(INFO "Executing the benchmark...\n");
  fflush(stdout);  
 
  for (yarn_word_t threads = 1; threads <= yarn_thread_count(); ++threads) {
    start_time = 0;

    g_max_speedup = get_speedup(end_time, threads);
    g_min_speedup = get_speedup(start_time, threads);
    printf(INFO "(%2zu / %2zu) = [%3f, %3f]\n", 
	   threads, yarn_thread_count(), g_min_speedup, g_max_speedup);    
    fflush(stdout);

    // Theorical limit for speedups is the number of threads involved.
    const double max_speedup = threads;
    
    for (double speedup = min_speedup; speedup <= max_speedup; speedup += speedup_step) {
      yarn_time_t speedup_time;
      int ret = speedup_search(speedup, start_time, end_time, &speedup_time, threads);
      if (ret > 0) {
	break;
      }
      else if (ret < 0) {
	printf(INFO "(%2zu, %2.2f) = SKIPPED\n", threads, speedup);
	fflush(stdout);
	continue;
      }

      //    speedup_time = speedup_lower_bound(speedup, start_time, time_step);
      start_time = speedup_time;

      if (g_use_log) {
	fprintf(g_log_file, "%zu"CSV_SEP"%.1f"CSV_SEP"%zu\n", 
		threads, speedup, speedup_time);
      }
      printf(INFO "(%2zu, %2.2f) = %9zuns\n", threads, speedup, speedup_time);
      fflush(stdout);
    }

    if (g_use_log) {
      fflush(g_log_file);
    }
  }

  if (g_use_log) {
    fclose(g_log_file);
  }

  yarn_destroy();
  return 0;

  yarn_destroy();
 yarn_error:
  perror(__FUNCTION__);
  return 1;
}

int speedup_search (const double target_speedup,
		    yarn_time_t start_time, 
		    yarn_time_t end_time, 
		    yarn_time_t* speedup_time,
		    yarn_word_t thread_count) 
{
  //  printf(DEBUG "searching for => %f\n", target_speedup);

  double start_speedup = g_min_speedup;
  if (comp_speedup(start_speedup, target_speedup) > 0) {
    printf(WARN "target=%3f < min=%3f)\n", target_speedup, start_speedup);
    return -1;
  }

  double end_speedup = g_max_speedup;
  if (comp_speedup(end_speedup, target_speedup) < 0) {
    printf(WARN "target=%3f > max=%3f)\n", target_speedup, end_speedup);
    return 1;
  }

  
  // interpolation search.
  double speedup;
  yarn_time_t time;
  int speedup_comp;
  do {

    double slope = (end_speedup - start_speedup) / ((double)(end_time - start_time));
    time = start_time + ((target_speedup - start_speedup) / slope);

    if (time > TIME_END_NS) {
      time = TIME_END_NS;
    }
    /*
    printf(DEBUG "search - time=%zu, slope=%f (speedup=[%f, %f], time=[%zu, %zu])\n", 
	   time, slope, start_speedup, end_speedup, start_time, end_time); 
    */
    speedup = get_speedup(time, thread_count);
    
    speedup_comp = comp_speedup(speedup, target_speedup);
    if (speedup_comp < 0) {
      start_time = time;
      start_speedup = speedup;
    }
    else if (speedup_comp > 0) {
      end_time = time;
      end_speedup = speedup;
    }

  } while (speedup_comp != 0);

  //printf(DEBUG "Target time => %zuns\n", time);

  *speedup_time = time;
  return 0;
}



struct task* create_task (yarn_time_t wait_time, yarn_word_t thread_count) {
  size_t array_size = yarn_thread_count();

  size_t t_size = sizeof(struct task) + sizeof(yarn_word_t)*array_size;
  struct task* t = (struct task*) malloc(t_size);
  if (!t) goto alloc_error;


  t->i = 0;
  t->wait_time = wait_time;
  t->thread_count = thread_count;
  t->array_size = array_size;
  t->n = t->array_size * t->array_size;

  for (size_t i = 0; i < array_size; ++i) {
    t->array[i] = 0;
  }

  return t;
 
 alloc_error:
  perror(__FUNCTION__);
  return NULL;
}


typedef void (*exec_func_t) (struct task*);


void exec_normal (struct task* t) {
  run_normal(t);
}

void exec_speculative (struct task* t) {
  bool ret = yarn_exec_simple(run_speculative, (void*) t, 
			      t->thread_count, t->array_size, 0);
  assert(ret);
}


yarn_time_t time_exec (exec_func_t exec_func, 
		       yarn_time_t wait_time, 
		       yarn_word_t thread_count) 
{
  static const int n = 10;

  yarn_time_t time_sum = 0;
  yarn_time_t min_time = UINT64_MAX;
  yarn_time_t max_time = 0;

  for (int i = 0; i < n; ++i) {
    struct task* t = create_task(wait_time, thread_count);
    yarn_time_t start = yarn_timer_sample_system();

    (*exec_func)(t);

    yarn_time_t end = yarn_timer_sample_system();
    yarn_time_t diff_time = yarn_timer_diff(start, end);

    time_sum += diff_time;
    if (diff_time < min_time) {
      min_time = diff_time;
    }
    if (diff_time > max_time) {
      max_time = diff_time;
    }

    free(t);
  }

  time_sum -= min_time;
  time_sum -= max_time;

  return time_sum / (n-2);  
}



// Could rerun each a couple of time to make sure that we're within our epsilon.
double get_speedup(yarn_time_t wait_time, yarn_word_t thread_count) {
  assert(wait_time <= TIME_END_NS);

  yarn_time_t base_time = time_exec(exec_normal, wait_time, thread_count);
  yarn_time_t speculative_time = time_exec(exec_speculative, wait_time, thread_count);

  double speedup = (double) base_time / (double) speculative_time;
  /*
  printf(DEBUG "time=%zu => speedup=%f (base=%zu, spec=%zu)\n", 
	 wait_time, speedup, base_time, speculative_time);
  */

  return speedup;
}




void look_busy (yarn_word_t* value, yarn_time_t wait_time) {
  yarn_time_t start = yarn_timer_sample_thread();
  yarn_time_t elapsed;
  
  do {
    yarn_time_t sample = yarn_timer_sample_thread();
    elapsed = yarn_timer_diff(start, sample);
  } while (elapsed <= wait_time);

  *value = elapsed;
}


void run_normal (struct task* t) {
  for (size_t i = 0; i < t->n; ++i) {
    size_t src = i % t->array_size;
    size_t dest = (src+1) % t->array_size;
    
    yarn_word_t value = t->array[src];
    look_busy(&value, t->wait_time);
    t->array[dest] = value;
  }
}

enum yarn_ret run_speculative (const yarn_word_t pool_id, void* task, yarn_word_t indvar) {
  struct task* t = (struct task*) task;

  if (indvar >= t->n) {
    yarn_dep_store(pool_id, &indvar, &t->i);
    return yarn_ret_break;
  }

  size_t src = indvar % t->array_size;
  //  size_t dest = (src+1) % t->array_size;    
  size_t dest = src;
  yarn_word_t value;

  yarn_dep_load(pool_id, &t->array[src], &value);
  look_busy(&value, t->wait_time);  
  yarn_dep_store(pool_id, &value, &t->array[dest]);

  return yarn_ret_continue;
}

