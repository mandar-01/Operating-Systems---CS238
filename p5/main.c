/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * main.c
 */

#include <signal.h>
#include "system.h"

/**
 * Needs:
 *   signal() -> to handle ctrl_c
 */

/* done is set if ctrl_c pressed*/
static volatile int done;

static void
_signal_(int signum)
{
	assert( SIGINT == signum );

	done = 1;
}

double
cpu_util(const char *s)
{
	static unsigned sum_, vector_[7];
	unsigned sum, vector[7];
	const char *p;
	double util;
	uint64_t i;

	/*
		these are what the 7 integers represent, cpu utilization by each of below category
	  user
	  nice
	  system
	  idle
	  iowait
	  irq
	  softirq
	*/
	
	/*read 7 integers from 1st line of proc/stat*/
	/* strstr sets p to first unsigned integer, since the first word is cpu and scanf would fail to read integer there.*/
	if (!(p = strstr(s, " ")) ||
	    (7 != sscanf(p,
			 "%u %u %u %u %u %u %u",
			 &vector[0],
			 &vector[1],
			 &vector[2],
			 &vector[3],
			 &vector[4],
			 &vector[5],
			 &vector[6]))) {
		return 0;
	}

	/* total cpu utilization is sum of all 7 categories*/
	sum = 0.0;
	for (i=0; i<ARRAY_SIZE(vector); ++i) {
		sum += vector[i];
	}
	/*calculate actual cpu util differentiating previous sum and current sum*/
	util = (1.0 - (vector[3] - vector_[3]) / (double)(sum - sum_)) * 100.0;
	sum_ = sum;
	for (i=0; i<ARRAY_SIZE(vector); ++i) {
		vector_[i] = vector[i];
	}
	return util;
}

void network_stats(const char *interface_name)
{
    FILE *file = fopen("/proc/net/dev", "r");
    char line[1024];

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (strstr(line, interface_name) != NULL)
        {
            unsigned long long packets_received, packets_transmitted, bytes_received, bytes_transmitted;
            sscanf(line + strcspn(line, ":") + 1, "%llu %llu %*u %*u %*u %*u %*u %*u %llu %llu",
                   &bytes_received, &packets_received, &bytes_transmitted, &packets_transmitted);
            printf("Transmitted packets: %llu | Received packets: %llu | Transmitted bytes: %llu | Received bytes: %llu\n", packets_transmitted, packets_received, bytes_transmitted, bytes_received);
            fflush(stdout);
            fclose(file);
            return;
        }
    }
    fclose(file);
}

void io_stats(const char *device_name) {
    char line[1024];
    FILE *file;
    file = fopen("/proc/diskstats", "r");
    while (fgets(line, sizeof(line), file) != NULL) {
        unsigned int major, minor;
        char dev_name[20];
        unsigned long long reads_completed, reads_merged, sectors_read, read_time,
            writes_completed, writes_merged, sectors_written, write_time;
        if (sscanf(line, "%u %u %s %llu %llu %llu %llu %llu %llu %llu %llu",
                   &major, &minor, dev_name,
                   &reads_completed, &reads_merged, &sectors_read, &read_time,
                   &writes_completed, &writes_merged, &sectors_written, &write_time) == 11) {

            if (strcmp(dev_name, device_name) == 0) {
                printf("Reads: %llu | Writes: %llu\n", reads_completed, writes_completed);
                fflush(stdout);
                return;
            }
        }
    }
    fclose(file);
}

double memory_util()
{	
	const char * const MEMINFO = "/proc/meminfo";
    FILE *meminfo_file = fopen(MEMINFO, "r");

    char line[1024];
    unsigned long mem_total = 0, mem_free = 0, buffers = 0, cached = 0;

    while (fgets(line, sizeof(line), meminfo_file))
    {
        unsigned long value;
        if (sscanf(line, "MemTotal: %lu kB", &value) == 1)
        {
            mem_total = value;
        }
        else if (sscanf(line, "MemFree: %lu kB", &value) == 1)
        {
            mem_free = value;
        }
        else if (sscanf(line, "Buffers: %lu kB", &value) == 1)
        {
            buffers = value;
        }
        else if (sscanf(line, "Cached: %lu kB", &value) == 1)
        {
            cached = value;
        }
    }

    fclose(meminfo_file);

    unsigned long memory_used = mem_total - mem_free - buffers - cached;
    double memory_used_percentage = ((double)memory_used / mem_total) * 100.0;

    return memory_used_percentage;
}

int
main(int argc, char *argv[])
{
	const char * const PROC_STAT = "/proc/stat";
	char line[1024];
	FILE *file;

	UNUSED(argc);
	UNUSED(argv);

	if (SIG_ERR == signal(SIGINT, _signal_)) {
		TRACE("signal()");
		return -1;
	}
	while (!done) {
		if (!(file = fopen(PROC_STAT, "r"))) {
			TRACE("fopen()");
			return -1;
		}
		if (fgets(line, sizeof (line), file)) {
			printf("CPU Utilization: %5.1f%%\n", cpu_util(line));
			fflush(stdout);
		}
		printf("Memory Utilization: %5.1f%%\n", memory_util());
		network_stats("wlp0s20f3");
		io_stats("nvme0n1");
		printf("==========================================================\n");
		fflush(stdout);
		
		us_sleep(500000);
		fclose(file);
	}
	printf("\rDone!   \n");
	return 0;
}
