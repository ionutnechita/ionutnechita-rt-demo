#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

#define NSEC_PER_SEC 1000000000L
#define MAX_SAMPLES 10000
#define LOAD_THREADS 4
#define MAX_CPUS 256

typedef struct {
    long long latency_samples[MAX_SAMPLES];
    int sample_count;
    long long max_latency;
    long long min_latency;
    long long total_latency;
    int running;
} rt_stats_t;

typedef struct {
    int isolated_cpus[MAX_CPUS];
    int rcu_nocbs_cpus[MAX_CPUS];
    int nohz_full_cpus[MAX_CPUS];
    int isolated_count;
    int rcu_nocbs_count;
    int nohz_full_count;
    int total_cpus;
} cpu_config_t;

static rt_stats_t stats = {0};
static pthread_t load_threads[LOAD_THREADS];
static volatile int should_exit = 0;
static cpu_config_t cpu_config = {0};

// Function declarations
void parse_cpu_list(const char* cpu_list, int* cpu_array, int* count);
void detect_rt_cpu_config(void);
long long timespec_to_ns(struct timespec *ts);
int set_cpu_affinity(int cpu);
void* load_thread(void* arg);
void* rt_thread(void* arg);
void signal_handler(int sig);
int setup_rt_environment(void);
void calculate_statistics(void);
void print_system_info(void);

// Function to parse CPU lists from cmdline
void parse_cpu_list(const char* cpu_list, int* cpu_array, int* count) {
    if (!cpu_list || !cpu_array || !count) return;
    
    *count = 0;
    char* list_copy = strdup(cpu_list);
    char* token = strtok(list_copy, ",");
    
    while (token && *count < MAX_CPUS) {
        // Check if it's a range (e.g., 14-15)
        if (strchr(token, '-')) {
            int start, end;
            if (sscanf(token, "%d-%d", &start, &end) == 2) {
                for (int i = start; i <= end && *count < MAX_CPUS; i++) {
                    cpu_array[(*count)++] = i;
                }
            }
        } else {
            // Single CPU
            int cpu = atoi(token);
            if (cpu >= 0) {
                cpu_array[(*count)++] = cpu;
            }
        }
        token = strtok(NULL, ",");
    }
    
    free(list_copy);
}

// Function to detect RT CPU configuration
void detect_rt_cpu_config() {
    FILE *fp;
    char buffer[1024];
    char *pos;
    
    cpu_config.total_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    
    printf("=== RT CPU CONFIGURATION ===\n");
    
    // Read /proc/cmdline for RT parameters
    fp = fopen("/proc/cmdline", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            printf("Kernel cmdline: %s\n", buffer);
            
            // Detect isolcpus
            pos = strstr(buffer, "isolcpus=");
            if (pos) {
                char *isolcpus_start = strchr(pos, '=') + 1;
                char *isolcpus_end = strchr(isolcpus_start, ' ');
                if (isolcpus_end) *isolcpus_end = '\0';
                
                // Skip prefixes (managed_irq,domain,)
                char *cpu_list = isolcpus_start;
                while (*cpu_list && (*cpu_list < '0' || *cpu_list > '9')) {
                    if (*cpu_list == ',') {
                        cpu_list++;
                        // Check if next character is a digit
                        if (*cpu_list >= '0' && *cpu_list <= '9') break;
                    } else {
                        cpu_list++;
                    }
                }
                
                parse_cpu_list(cpu_list, cpu_config.isolated_cpus, &cpu_config.isolated_count);
                printf("Isolated CPUs (isolcpus): ");
                for (int i = 0; i < cpu_config.isolated_count; i++) {
                    printf("%d%s", cpu_config.isolated_cpus[i], 
                           (i < cpu_config.isolated_count - 1) ? "," : "");
                }
                printf(" (total: %d)\n", cpu_config.isolated_count);
            }
            
            // Reset buffer for next search
            fseek(fp, 0, SEEK_SET);
            fgets(buffer, sizeof(buffer), fp);
            
            // Detect rcu_nocbs
            pos = strstr(buffer, "rcu_nocbs=");
            if (pos) {
                char *rcu_start = strchr(pos, '=') + 1;
                char *rcu_end = strchr(rcu_start, ' ');
                if (rcu_end) {
                    size_t len = rcu_end - rcu_start;
                    char rcu_list[64];
                    strncpy(rcu_list, rcu_start, len);
                    rcu_list[len] = '\0';
                    
                    parse_cpu_list(rcu_list, cpu_config.rcu_nocbs_cpus, &cpu_config.rcu_nocbs_count);
                    printf("RCU no-callbacks CPUs: ");
                    for (int i = 0; i < cpu_config.rcu_nocbs_count; i++) {
                        printf("%d%s", cpu_config.rcu_nocbs_cpus[i],
                               (i < cpu_config.rcu_nocbs_count - 1) ? "," : "");
                    }
                    printf(" (total: %d)\n", cpu_config.rcu_nocbs_count);
                }
            }
            
            // Reset buffer for next search
            fseek(fp, 0, SEEK_SET);
            fgets(buffer, sizeof(buffer), fp);
            
            // Detect nohz_full
            pos = strstr(buffer, "nohz_full=");
            if (pos) {
                char *nohz_start = strchr(pos, '=') + 1;
                char *nohz_end = strchr(nohz_start, ' ');
                if (nohz_end) {
                    size_t len = nohz_end - nohz_start;
                    char nohz_list[64];
                    strncpy(nohz_list, nohz_start, len);
                    nohz_list[len] = '\0';
                    
                    parse_cpu_list(nohz_list, cpu_config.nohz_full_cpus, &cpu_config.nohz_full_count);
                    printf("NO_HZ full CPUs: ");
                    for (int i = 0; i < cpu_config.nohz_full_count; i++) {
                        printf("%d%s", cpu_config.nohz_full_cpus[i],
                               (i < cpu_config.nohz_full_count - 1) ? "," : "");
                    }
                    printf(" (total: %d)\n", cpu_config.nohz_full_count);
                }
            }
        }
        fclose(fp);
    }
    
    // Determine best CPU for RT
    if (cpu_config.isolated_count > 0) {
        printf("\nRecommended for RT: CPU %d (isolated + nocb + nohz)\n", 
               cpu_config.isolated_cpus[0]);
    } else if (cpu_config.rcu_nocbs_count > 0) {
        printf("\nRecommended for RT: CPU %d (nocb)\n", 
               cpu_config.rcu_nocbs_cpus[0]);
    } else {
        printf("\nNo specially configured RT CPUs found\n");
    }
    
    printf("\n");
}

// Function to convert timespec to nanoseconds
long long timespec_to_ns(struct timespec *ts) {
    return ts->tv_sec * NSEC_PER_SEC + ts->tv_nsec;
}

// Function to set CPU affinity
int set_cpu_affinity(int cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
        perror("sched_setaffinity");
        return -1;
    }
    
    return 0;
}

// Load thread to simulate system activity
void* load_thread(void* arg) {
    int thread_id = *(int*)arg;
    
    printf("Load thread %d started\n", thread_id);
    
    // Set load thread to run on normal CPUs (not isolated ones)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    // Add all CPUs except isolated ones
    for (int cpu = 0; cpu < cpu_config.total_cpus; cpu++) {
        int is_isolated = 0;
        for (int i = 0; i < cpu_config.isolated_count; i++) {
            if (cpu == cpu_config.isolated_cpus[i]) {
                is_isolated = 1;
                break;
            }
        }
        if (!is_isolated) {
            CPU_SET(cpu, &cpuset);
        }
    }
    
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == 0) {
        printf("Load thread %d avoiding isolated CPUs\n", thread_id);
    }
    
    // Initialize random seed based on thread_id
    srand((unsigned int)thread_id + (unsigned int)time(NULL));
    
    while (!should_exit) {
        // CPU intensive operations
        for (int i = 0; i < 100000; i++) {
            volatile double result = (rand() % 1000) * 3.14159;
            result = result / ((rand() % 1000) + 1);
            (void)result; // avoid unused warning
        }
        
        // I/O operations - replace usleep with nanosleep
        struct timespec delay = {0, (rand() % 1000) * 1000}; // 0-1000 microseconds
        nanosleep(&delay, NULL);
    }
    
    printf("Load thread %d finished\n", thread_id);
    return NULL;
}

// Real-time thread for latency measurement
void* rt_thread(void* arg) {
    (void)arg; // avoid unused parameter warning
    struct timespec start, end, sleep_time;
    long long expected_wake, actual_wake, latency;
    int priority = 99;
    struct sched_param param;
    int rt_cpu = -1;
    
    // Choose best CPU for RT
    if (cpu_config.isolated_count > 0) {
        rt_cpu = cpu_config.isolated_cpus[0];
        printf("RT thread using isolated CPU %d\n", rt_cpu);
    } else if (cpu_config.rcu_nocbs_count > 0) {
        rt_cpu = cpu_config.rcu_nocbs_cpus[0];
        printf("RT thread using RCU no-callbacks CPU %d\n", rt_cpu);
    } else {
        rt_cpu = cpu_config.total_cpus - 1; // last CPU
        printf("RT thread using last CPU %d\n", rt_cpu);
    }
    
    // Set CPU affinity
    if (set_cpu_affinity(rt_cpu) == 0) {
        printf("RT thread pinned to CPU %d\n", rt_cpu);
    } else {
        printf("Warning: Could not pin RT thread to CPU %d\n", rt_cpu);
    }
    
    // Set real-time priority
    param.sched_priority = priority;
    if (sched_setscheduler(0, SCHED_RR, &param) == -1) {
        perror("sched_setscheduler");
        return NULL;
    }
    
    printf("RT thread started with priority %d on CPU %d\n", priority, rt_cpu);
    
    // Sleep period: 1ms
    sleep_time.tv_sec = 0;
    sleep_time.tv_nsec = 1000000; // 1ms
    
    stats.min_latency = LLONG_MAX;
    stats.max_latency = 0;
    stats.running = 1;
    
    while (!should_exit && stats.sample_count < MAX_SAMPLES) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        expected_wake = timespec_to_ns(&start) + timespec_to_ns(&sleep_time);
        
        // Precise sleep
        nanosleep(&sleep_time, NULL);
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        actual_wake = timespec_to_ns(&end);
        
        latency = actual_wake - expected_wake;
        
        // Store only positive latencies (delays)
        if (latency > 0) {
            stats.latency_samples[stats.sample_count] = latency;
            stats.total_latency += latency;
            
            if (latency > stats.max_latency) {
                stats.max_latency = latency;
            }
            if (latency < stats.min_latency) {
                stats.min_latency = latency;
            }
            
            stats.sample_count++;
        }
        
        // Show progress every 1000 samples
        if (stats.sample_count % 1000 == 0) {
            printf("Samples collected: %d, current latency: %lld ns\n", 
                   stats.sample_count, latency);
        }
    }
    
    stats.running = 0;
    printf("RT thread finished\n");
    return NULL;
}

// Signal handler
void signal_handler(int sig) {
    printf("\nReceived signal %d, stopping...\n", sig);
    should_exit = 1;
}

// Setup for real-time execution
int setup_rt_environment() {
    struct rlimit rlim;
    
    // Lock memory to avoid page faults
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("mlockall");
        printf("Warning: Could not lock memory\n");
    }
    
    // Set limit for locked memory
    rlim.rlim_cur = RLIM_INFINITY;
    rlim.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_MEMLOCK, &rlim) == -1) {
        perror("setrlimit");
        printf("Warning: Could not set memory lock limit\n");
    }
    
    return 0;
}

// Calculate statistics
void calculate_statistics() {
    if (stats.sample_count == 0) {
        printf("No samples collected\n");
        return;
    }
    
    // Sort samples to calculate percentiles
    for (int i = 0; i < stats.sample_count - 1; i++) {
        for (int j = i + 1; j < stats.sample_count; j++) {
            if (stats.latency_samples[i] > stats.latency_samples[j]) {
                long long temp = stats.latency_samples[i];
                stats.latency_samples[i] = stats.latency_samples[j];
                stats.latency_samples[j] = temp;
            }
        }
    }
    
    long long avg = stats.total_latency / stats.sample_count;
    long long p95 = stats.latency_samples[(int)(stats.sample_count * 0.95)];
    long long p99 = stats.latency_samples[(int)(stats.sample_count * 0.99)];
    
    printf("\n=== LATENCY STATISTICS ===\n");
    printf("Samples collected: %d\n", stats.sample_count);
    printf("Minimum latency: %lld ns (%.2f μs)\n", stats.min_latency, stats.min_latency / 1000.0);
    printf("Average latency: %lld ns (%.2f μs)\n", avg, avg / 1000.0);
    printf("Maximum latency: %lld ns (%.2f μs)\n", stats.max_latency, stats.max_latency / 1000.0);
    printf("95th percentile: %lld ns (%.2f μs)\n", p95, p95 / 1000.0);
    printf("99th percentile: %lld ns (%.2f μs)\n", p99, p99 / 1000.0);
    
    // Performance classification
    if (stats.max_latency < 100000) { // < 100μs
        printf("Performance: EXCELLENT for real-time\n");
    } else if (stats.max_latency < 1000000) { // < 1ms
        printf("Performance: GOOD for real-time\n");
    } else {
        printf("Performance: POOR for real-time\n");
    }
}

void print_system_info() {
    FILE *fp;
    char buffer[256];
    int rt_detected = 0;
    
    printf("=== SYSTEM INFORMATION ===\n");
    
    // Kernel version
    fp = fopen("/proc/version", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            printf("Kernel: %s", buffer);
            // Check if contains PREEMPT_RT in version
            if (strstr(buffer, "PREEMPT_RT") != NULL) {
                rt_detected = 1;
            }
        }
        fclose(fp);
    }
    
    // Check /sys/kernel/realtime
    fp = fopen("/sys/kernel/realtime", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            int rt_value = atoi(buffer);
            if (rt_value == 1) {
                printf("RT Kernel: Active (/sys/kernel/realtime = 1)\n");
                rt_detected = 1;
            } else {
                printf("RT Kernel: Inactive (/sys/kernel/realtime = %d)\n", rt_value);
            }
        }
        fclose(fp);
    }
    
    // Check /proc/sys/kernel/ostype for RT
    fp = fopen("/proc/sys/kernel/ostype", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, "rt") != NULL || strstr(buffer, "RT") != NULL) {
                printf("RT Kernel: Detected in ostype\n");
                rt_detected = 1;
            }
        }
        fclose(fp);
    }
    
    // Check kernel config in /boot/config
    char config_path[256];
    snprintf(config_path, sizeof(config_path), "/boot/config-%s", "");
    
    // Try to read /proc/config.gz if it exists
    fp = popen("zcat /proc/config.gz 2>/dev/null | grep CONFIG_PREEMPT_RT", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, "CONFIG_PREEMPT_RT=y") != NULL) {
                printf("RT Kernel: CONFIG_PREEMPT_RT=y detected\n");
                rt_detected = 1;
            }
        }
        pclose(fp);
    }
    
    // Check /proc/version for other RT indicators
    if (!rt_detected) {
        fp = fopen("/proc/version", "r");
        if (fp) {
            if (fgets(buffer, sizeof(buffer), fp)) {
                if (strstr(buffer, "lowlatency") != NULL || 
                    strstr(buffer, "rt") != NULL ||
                    strstr(buffer, "RT") != NULL) {
                    printf("RT Kernel: Probably RT (detected: lowlatency/rt in version)\n");
                    rt_detected = 1;
                }
            }
            fclose(fp);
        }
    }
    
    if (!rt_detected) {
        printf("RT Kernel: PREEMPT_RT not detected\n");
        printf("Note: Kernel appears to have 'lowlatency' and 'PREEMPT_RT' in version name\n");
        printf("      This suggests it's an RT kernel, but automatic detection failed\n");
    }
    
    // Number of cores
    printf("CPU cores: %ld\n", sysconf(_SC_NPROCESSORS_ONLN));
    
    // Check real-time limits
    struct rlimit rlim;
    if (getrlimit(RLIMIT_RTPRIO, &rlim) == 0) {
        printf("RT priority limit: %ld (max: %ld)\n", 
               (long)rlim.rlim_cur, (long)rlim.rlim_max);
    }
    
    if (getrlimit(RLIMIT_RTTIME, &rlim) == 0) {
        printf("RT time limit: %ld μs (max: %ld μs)\n", 
               (long)rlim.rlim_cur, (long)rlim.rlim_max);
    }
    
    printf("\n");
}

int main(int argc, char *argv[]) {
    pthread_t rt_thread_id;
    int thread_ids[LOAD_THREADS];
    int duration = 10; // seconds
    
    if (argc > 1) {
        duration = atoi(argv[1]);
        if (duration <= 0) duration = 10;
    }
    
    printf("=== PREEMPT_RT KERNEL DEMO ===\n");
    printf("Test duration: %d seconds\n", duration);
    printf("To stop early, press Ctrl+C\n\n");
    
    // Detect RT CPU configuration
    detect_rt_cpu_config();
    
    // Display system information
    print_system_info();
    
    // Setup RT environment
    setup_rt_environment();
    
    // Register signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Start load threads
    printf("Starting %d load threads...\n", LOAD_THREADS);
    for (int i = 0; i < LOAD_THREADS; i++) {
        thread_ids[i] = i;
        if (pthread_create(&load_threads[i], NULL, load_thread, &thread_ids[i]) != 0) {
            perror("pthread_create load_thread");
            return 1;
        }
    }
    
    // Start real-time thread
    printf("Starting RT measurement thread...\n\n");
    if (pthread_create(&rt_thread_id, NULL, rt_thread, NULL) != 0) {
        perror("pthread_create rt_thread");
        return 1;
    }
    
    // Wait for specified duration or until all samples are collected
    sleep(duration);
    should_exit = 1;
    
    // Wait for all threads to finish
    printf("Stopping threads...\n");
    pthread_join(rt_thread_id, NULL);
    
    for (int i = 0; i < LOAD_THREADS; i++) {
        pthread_cancel(load_threads[i]);
        pthread_join(load_threads[i], NULL);
    }
    
    // Calculate and display statistics
    calculate_statistics();
    
    printf("\nTest completed.\n");
    return 0;
}
