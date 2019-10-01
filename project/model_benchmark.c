// Copyright (c) 2019 Xnor.ai, Inc.
//

// -std=c99 needs this macro to be defined in order to use some POSIX functions.
// eg. <sys/*>
#define _POSIX_C_SOURCE 199309L

#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>  // pid_t
#include <time.h>
#include <unistd.h>

#include "xnornet.h"

static const int CHANNELS = 3;
static const int WARM_UP_DURATION = 5;
static const int BYTES_PER_KiB = 1024;
static const int KiB_PER_MiB = 1024;
static const double NANO_PER_SEC = 1000000000;

enum print_format {
  PRINT_FORMAT_SIMPLE,
  PRINT_FORMAT_FULL,
};

typedef struct performance_results {
  double duration;
  double cpu_duration;
  int total_iterations;
  int num_threads;
  double min_latency;
} performance_results;

// Some Apple devices will have ru_opaque[14] in place of the last 14 positions
// in the ru_usage struct. This means ru_maxrss will not be defined. In order to
// avoid this, we use the Apple open source files defined in mach/task.h and
// mach/mach_init.h instead.
#ifdef __APPLE__
#include <mach/mach_init.h>
#include <mach/task.h>
#include <mach/vm_map.h>
long getrss(void) {
  task_t task = MACH_PORT_NULL;
  if (task_for_pid(mach_task_self(), getpid(), &task) != KERN_SUCCESS) {
    return 0;
  }
  struct task_basic_info t_info;
  mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

  task_info(task, TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);
  return t_info.resident_size;
}

bool read_status(pid_t pid, performance_results* stats) {
  task_t task = MACH_PORT_NULL;
  if (task_for_pid(mach_task_self(), pid, &task) != KERN_SUCCESS) {
    return false;
  }

  thread_array_t thread_list;
  mach_msg_type_number_t thread_count;
  if (task_threads(task, &thread_list, &thread_count) < 0) {
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                  sizeof(thread_list) * thread_count);
    return false;
  }
  stats->num_threads = thread_count;

  vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                sizeof(thread_list) * thread_count);
  return true;
}
#else /* defined(__APPLE__) */
// As defined in man(5) proc
#define THREAD_FIELD 20
static const int BUFSIZE = 100;
long getrss(void) {
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  long rss = usage.ru_maxrss;
  return rss * BYTES_PER_KiB;  // Convert from kilobytes to bytes
}

/**
 * Reads performance information from /proc/<pid>/stat
 * See man(5) proc for field information
 */
bool read_status(pid_t pid, performance_results* stats) {
  char buf[BUFSIZE];
  char stat_path[BUFSIZE];
  sprintf(stat_path, "/proc/%d/stat", pid);
  FILE* stat_stream = fopen(stat_path, "r");
  if (stat_stream) {
    int field = 1;
    while (fscanf(stat_stream, "%s ", buf) == 1) {  // read entire file
      switch (field) {
        case THREAD_FIELD:
          stats->num_threads = atoi(buf);
          break;
      }
      ++field;
    }
    fclose(stat_stream);
  } else {
    fprintf(stderr, "Failed to open %s\n", stat_path);
    return false;
  }

  return true;
}
#endif /* defined(__APPLE__) */

void print_performance(performance_results* stats, enum print_format format) {
  long rss = getrss();
  double cpu_percent =
      100 * stats->cpu_duration / (stats->duration * stats->num_threads);
  double average_fps = ((double)stats->total_iterations) / stats->duration;
  switch (format) {
    case PRINT_FORMAT_SIMPLE:
      printf("Duration\t");
      printf("Total frames\t");
      printf("Average FPS\t");
      printf("Num threads\t");
      printf("Avg CPU%%\t");
      printf("Minimum latency\t");
      printf("Max Resident Set\n");
      printf("%.3f\t", stats->duration);
      printf("%d\t", stats->total_iterations);
      printf("%.3f\t", average_fps);
      printf("%d\t", stats->num_threads);
      printf("%.2f%%\t", cpu_percent);
      printf("%.1f ms\t", stats->min_latency * 1000);
      if (rss != 0) {
        printf("%ld bytes\n", rss);
      } else {
        printf("Unknown\n");
      }
      break;
    case PRINT_FORMAT_FULL:
      printf("Summary\n");
      printf("  Duration:           %.3f s\n", stats->duration);
      printf("  Total frames:       %d\n", stats->total_iterations);
      printf("  Average FPS:        %.3f\n", average_fps);
      printf("  Num threads:        %d\n", stats->num_threads);
      printf("  Avg CPU%%:           %.2f %%\n", cpu_percent);
      printf("  Minimum latency:    %.1f ms\n", stats->min_latency * 1000);
      if (rss != 0) {
        printf("  Max Resident Set:   %.3f MiB\n",
               ((double)rss / (BYTES_PER_KiB * KiB_PER_MiB)));
      } else {
        printf("  Max Resident Set:   Could not be determined\n");
      }
      break;
  }
}

void print_help() {
  fprintf(stderr,
          "Usage: ./build/model_benchmark [-h]\n"
          "                               [--input_width INPUT_WIDTH]\n"
          "                               [--input_height INPUT_HEIGHT]\n"
          "                               [--warm_up_iterations "
          "WARM_UP_ITERATIONS]\n"
          "                               [--max_benchmark_iterations "
          "MAX_BENCHMARK_ITERATIONS]\n"
          "                               [--max_benchmark_duration "
          "MAX_BENCHMARK_DURATION}\n"
          "                               [--single_threaded]\n"
          "                               [--format {simple, full}]\n"
          "                               [--quiet]\n");
}

void print_extended_help() {
  print_help();
  fprintf(
      stderr,
      "\nSimple utility for benchmarking Xnor models.\n"
      "\n"
      "Evaluates a model with configurable input size and measures resource "
      "usage\n"
      "\n"
      "Optional arguments:\n"
      "  -h, --help            show this help message and exit\n"
      "  --input_width  INPUT_WIDTH\n"
      "                        Input Resolution width of the camera.\n"
      "  --input_height  INPUT_HEIGHT\n"
      "                        Input Resolution height of the camera.\n"
      "  --warm_up_iterations WARM_UP_ITERATIONS\n"
      "                        Iterations required for warming up.\n"
      "  --max_benchmark_iterations MAX_BENCHMARK_ITERATIONS\n"
      "                        Maximum iterations for benchmarking. Benchmark\n"
      "                        ends when either benchmark iterations or \n"
      "                        maximum duration is hit.\n"
      "  --max_benchmark_duration MAX_BENCHMARK_DURATION\n"
      "                        Maximum duration for benchmarking, in seconds\n"
      "                        (minimum 5). Benchmark ends when either \n"
      "                        maximum benchmark iterations or maximum \n"
      "                        duration is hit.\n"
      "  --single_threaded\n"
      "                        Run the model in single-threaded mode (default\n"
      "                        is multi-threaded)\n"
      "  --format {simple, full}\n"
      "                        Output format. 'full' has labels and tabular \n"
      "                        formatting\n"
      "  --quiet\n"
      "                        Suppress output indicating benchmark "
      "progress\n");
}

bool perform_inference_loop(xnor_model* model, xnor_input* input,
                            int max_iterations, double max_duration,
                            performance_results* stats) {
  xnor_error* error = NULL;
  xnor_evaluation_result* result = NULL;

  stats->min_latency = INFINITY;
  stats->duration = 0;
  struct timespec start_real, end_real, start_cpu, end_cpu;
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_cpu)) {
    fprintf(stderr, "Clock returned error\n");
    goto inference_loop_fail;
  }
  stats->total_iterations = max_iterations;
  for (int i = 0; i < max_iterations; ++i) {
    if (clock_gettime(CLOCK_REALTIME, &start_real)) {
      fprintf(stderr, "Clock returned error\n");
      goto inference_loop_fail;
    }
    error = xnor_model_evaluate(model, input, NULL, &result);
    if (clock_gettime(CLOCK_REALTIME, &end_real)) {
      fprintf(stderr, "Clock returned error\n");
      goto inference_loop_fail;
    }
    if (error != NULL) {
      fprintf(stderr, "%s\n", xnor_error_get_description(error));
      goto inference_loop_fail;
    }
    double latency =
        (double)(end_real.tv_sec - start_real.tv_sec) +
        ((double)(end_real.tv_nsec - start_real.tv_nsec)) / NANO_PER_SEC;
    stats->duration += latency;
    if (latency < stats->min_latency) {
      stats->min_latency = latency;
    }
    if (stats->duration > max_duration) {
      stats->total_iterations = i + 1;
      break;
    }
  }
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_cpu)) {
    fprintf(stderr, "Clock returned error\n");
    goto inference_loop_fail;
  }
  stats->cpu_duration =
      (double)(end_cpu.tv_sec - start_cpu.tv_sec) +
      ((double)(end_cpu.tv_nsec - start_cpu.tv_nsec)) / NANO_PER_SEC;

  pid_t pid = getpid();
  if (!read_status(pid, stats)) {  // Check num_threads
    goto inference_loop_fail;
  }

  xnor_evaluation_result_free(result);
  return true;
inference_loop_fail:
  // If any of these are NULL, the corresponding free() function will do nothing
  xnor_error_free(error);
  xnor_evaluation_result_free(result);
  return false;
}

int main(int argc, char* argv[]) {
  int opt;
  // Set default values
  int input_width = 448;
  int input_height = 448;
  int warm_up_iterations = 10;
  int test_iterations = 20;
  int test_duration = 5;
  bool single_threaded = false;
  enum print_format format = PRINT_FORMAT_FULL;
  bool quiet = false;

  enum option_values {
    OPTION_INPUT_WIDTH = 1,
    OPTION_INPUT_HEIGHT,
    OPTION_WARM_UP_ITERATIONS,
    OPTION_MAX_BENCHMARK_ITERATIONS,
    OPTION_MAX_BENCHMARK_DURATION,
    OPTION_SINGLE_THREADED,
    OPTION_RESULT_FORMAT,
    OPTION_QUIET,
    OPTION_HELP,
  };

  int option_index = 0;
  struct option options[] = {
      {"input_width", required_argument, 0, OPTION_INPUT_WIDTH},
      {"input_height", required_argument, 0, OPTION_INPUT_HEIGHT},
      {"warm_up_iterations", required_argument, 0, OPTION_WARM_UP_ITERATIONS},
      {"max_benchmark_iterations", required_argument, 0,
       OPTION_MAX_BENCHMARK_ITERATIONS},
      {"max_benchmark_duration", required_argument, 0,
       OPTION_MAX_BENCHMARK_DURATION},
      {"single_threaded", no_argument, 0, OPTION_SINGLE_THREADED},
      {"format", required_argument, 0, OPTION_RESULT_FORMAT},
      {"quiet", no_argument, 0, OPTION_QUIET},
      {"help", no_argument, 0, OPTION_HELP},
      {0, 0, 0, 0}};

  while ((opt = getopt_long_only(argc, argv, "h", options, &option_index)) !=
         -1) {
    switch (opt) {
      case OPTION_INPUT_WIDTH:
        input_width = atoi(optarg);
        break;
      case OPTION_INPUT_HEIGHT:
        input_height = atoi(optarg);
        break;
      case OPTION_WARM_UP_ITERATIONS:
        warm_up_iterations = atoi(optarg);
        break;
      case OPTION_MAX_BENCHMARK_ITERATIONS:
        test_iterations = atoi(optarg);
        break;
      case OPTION_MAX_BENCHMARK_DURATION:
        test_duration = atoi(optarg);
        break;
      case OPTION_SINGLE_THREADED:
        single_threaded = true;
        break;
      case OPTION_RESULT_FORMAT:
        if (strcmp(optarg, "simple") == 0) {
          format = PRINT_FORMAT_SIMPLE;
        } else if (strcmp(optarg, "full") == 0) {
          format = PRINT_FORMAT_FULL;
        } else {
          fprintf(stderr, "--format: Please pass one of {simple, full}\n");
          return EXIT_FAILURE;
        }
        break;
      case OPTION_QUIET:
        quiet = true;
        break;
      case '?':
        print_help();
        return EXIT_SUCCESS;
      case 'h':
      case OPTION_HELP:
        print_extended_help();
        return EXIT_SUCCESS;
    }
  }

  // Forward declare variables we will need to clean up later
  xnor_model* model = NULL;
  xnor_error* error = NULL;
  xnor_input* input = NULL;
  xnor_model_load_options* load_options = xnor_model_load_options_create();
  uint8_t* input_image = NULL;

  // Load the Xnor model to get a model handle. We will free this at the end of
  // main(), either via a successful return or after the fail: label.
  if (!quiet) {
    printf("Loading model...\n");
  }

  if (single_threaded) {
    error = xnor_model_load_options_set_threading_model(
        load_options, kXnorThreadingModelSingleThreaded);
  } else {
    error = xnor_model_load_options_set_threading_model(
        load_options, kXnorThreadingModelMultiThreaded);
  }
  if (error != NULL) {
    fprintf(stderr, "%s\n", xnor_error_get_description(error));
    goto fail;
  }
  error = xnor_model_load_built_in("", load_options, &model);
  if (error != NULL) {
    fprintf(stderr, "%s\n", xnor_error_get_description(error));
    goto fail;
  }

  // Get the model information
  xnor_model_info model_info;
  model_info.xnor_model_info_size = sizeof(model_info);
  error = xnor_model_get_info(model, &model_info);
  if (error != NULL) {
    fprintf(stderr, "%s\n", xnor_error_get_description(error));
    goto fail;
  }

  printf("Model: %s\n", model_info.name);
  printf("  version '%s'\n", model_info.version);

  if (!quiet) {
    printf("Generating Input...\n");
  }
  input_image = malloc(sizeof(uint8_t) * input_width * input_height * CHANNELS);
  if (!input_image) {
    fprintf(stderr, "Failed to allocate space for input image\n");
    goto fail;
  }

  srand(time(NULL));
  // Generate a random byte for each position
  // Currently generating a long int then casting down
  for (int i = 0; i < input_width; ++i) {
    for (int j = 0; j < input_height; ++j) {
      for (int k = 0; k < CHANNELS; ++k) {
        input_image[i * (input_height * CHANNELS) + j * (CHANNELS) + k] =
            (uint8_t)rand();
      }
    }
  }

  error = xnor_input_create_rgb_image(input_width, input_height, input_image,
                                      &input);

  if (!quiet) {
    printf("Warming up...\n");
  }
  performance_results warm_up_results;
  if (!perform_inference_loop(model, input, warm_up_iterations,
                              WARM_UP_DURATION, &warm_up_results)) {
    fprintf(stderr, "Warmup failure\n");
    goto fail;
  }
  if (!quiet) {
    printf("Finished warming up.\n");
    printf("Benchmarking...\n\n");
  }
  performance_results test_results;
  if (!perform_inference_loop(model, input, test_iterations, test_duration,
                              &test_results)) {
    fprintf(stderr, "Benchmarking failure\n");
    goto fail;
  }

  print_performance(&test_results, format);

  free(input_image);
  xnor_input_free(input);
  xnor_model_free(model);

  return EXIT_SUCCESS;
fail:
  if (input_image) {
    free(input_image);
  }
  // If any of these are NULL, the corresponding free() function will do nothing
  xnor_error_free(error);
  xnor_input_free(input);
  xnor_model_free(model);
  xnor_model_load_options_free(load_options);
  return EXIT_FAILURE;
}
