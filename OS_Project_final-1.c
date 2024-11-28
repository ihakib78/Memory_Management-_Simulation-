#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <windows.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_PAGES 100
#define MAX_PROCESSES 100
#define MAX_FRAMES 100
#define MAX_DISK_PAGES 200
double system_frame_size ;
long long process_used_memory;

typedef enum {
    WAITING,
    RUNNING,
    COMPLETED
} ProcessState;

typedef struct {
    int process_id;
    int memory_requirement; // in KB
    int allocated;          // Flag for memory allocation status
    ProcessState state;     // Current state of the process
} Process;

typedef struct {
    int assigned;      // 1 if frame is assigned, 0 otherwise
    int process_id;    // Process ID assigned to this frame
    int page_number;   // Page number within the process
} Frame;

typedef struct {
    int process_id;    // Process ID assigned to this page
    int page_number;   // Page number of the process
    int in_memory;     // 1 if the page is in memory, 0 if it's on disk
} DiskPage;

typedef struct {
    int page_faults;
    int swap_time;     // In milliseconds
} MemoryStats;

Frame RAM[MAX_FRAMES];       // RAM with a limited number of frames
DiskPage disk[MAX_DISK_PAGES]; // Disk storage
int total_frames, page_size, process_count;
int disk_page_count = 0;      // Keeps track of how many pages are currently on the disk
MemoryStats stats = {0, 0};   // Initialize page_faults and swap_time

// Function prototypes
void initialize_frames(Frame* frames, int total_frames);
void initialize_disk(DiskPage* disk, int max_disk_pages);
void allocate_memory(Process* processes, int process_count, Frame* frames, int total_frames, int page_size);
void display_memory_map(Frame* frames, int total_frames);
void deallocate_memory(Frame* frames, int total_frames, Process* processes, int process_count, int process_id);
int required_pages(int memory_requirement, int page_size);
void display_processes(Process* processes, int process_count);
void request_additional_memory(Process* processes, int process_count, Frame* frames, int total_frames, int page_size);
void print_memory_usage(Frame* frames, int total_frames, int page_size);
void swap_out_page(Frame* frames, int total_frames, int process_id, int page_number);
void swap_in_page(int process_id, int page_number);
void get_memory_status(long long *phys_mem, long long *page_mem);
void print_numbers();
void system_memory();
int performance_matrix();
int main() {
    system_memory();

    char continue_input = 'y';

    // Step 1: Get user input for total memory frames, page size, and process count
    printf("\n\nEnter total number of frames: ");
    scanf("%d", &total_frames);
    printf("Enter page size (in KB): ");
    scanf("%d", &page_size);
    printf("Enter number of processes (max %d): ", MAX_PROCESSES);
    scanf("%d", &process_count);

    if (process_count > MAX_PROCESSES || total_frames > MAX_FRAMES) {
        printf("Error: Maximum limits exceeded. Please enter values within the limits.\n");
        return 1;
    }

    // Allocate memory for frames and processes
    Frame* frames = (Frame*)malloc(total_frames * sizeof(Frame));
    Process* processes = (Process*)malloc(process_count * sizeof(Process));

    // Check for successful memory allocation
    if (frames == NULL || processes == NULL) {
        printf("Error: Memory allocation failed.\n");
        return 1;
    }

    // Initialize frames and disk
    initialize_frames(frames, total_frames);
    initialize_disk(disk, MAX_DISK_PAGES);

    // Step 2: Input each process's memory requirements
    for (int i = 0; i < process_count; i++) {
        processes[i].process_id = i + 1;
        printf("Enter memory requirement for process %d (in KB): ", processes[i].process_id);
        scanf("%d", &processes[i].memory_requirement);
        processes[i].allocated = 0; // Initially set allocation status to false
        processes[i].state = WAITING; // Set initial state to WAITING
    }

    // Allocate memory for processes
    allocate_memory(processes, process_count, frames, total_frames, page_size);
    display_memory_map(frames, total_frames);

    while (continue_input == 'y') {
        printf("\nOptions:\n");
        printf("1. Deallocate Memory\n");
        printf("2. Request Additional Memory\n");
        printf("3. Input New Process Memory Requirement\n");
        printf("4. Display Active Processes\n");
        printf("5. Display Memory Map\n");
        printf("6. Display Memory Usage Statistics and fragmentation\n");
        printf("7. Display Performance Matrix\n");
        printf("8. Exit\n");
        printf("Choose an option (1-7): ");
        int option;
        scanf("%d", &option);

        switch (option) {
            case 1: {
                int pid_to_deallocate;
                printf("Enter process ID to deallocate memory: ");
                scanf("%d", &pid_to_deallocate);
                deallocate_memory(frames, total_frames, processes, process_count, pid_to_deallocate);
                display_memory_map(frames, total_frames);
                break;
            }
            case 2:
                request_additional_memory(processes, process_count, frames, total_frames, page_size);
                display_memory_map(frames, total_frames);
                break;
            case 3: {
                for (int i = 0; i < process_count; i++) {
                    printf("Enter new memory requirement for process %d (in KB): ", processes[i].process_id);
                    scanf("%d", &processes[i].memory_requirement);
                }
                allocate_memory(processes, process_count, frames, total_frames, page_size);
                display_memory_map(frames, total_frames);
                break;
            }
            case 4:
                display_processes(processes, process_count);
                break;
            case 5:
                display_memory_map(frames, total_frames);
                break;
            case 6:
                print_memory_usage(frames, total_frames, page_size);
                break;
            case 7:
                performance_matrix();
                break;
            case 8:
                continue_input = 'n'; // Exit the loop
                break;
            default:
                printf("Invalid option. Please choose again.\n");
                break;
        }
    }

    // Free allocated memory
    free(frames);
    free(processes);
    performance_matrix();
    return 0;
}
// calculate system memory before and after a process
void system_memory()
{
    long long phys_mem_before, page_mem_before;
    long long phys_mem_after, page_mem_after;

    // Get memory status before
    MEMORYSTATUSEX statex_before;
    statex_before.dwLength = sizeof(statex_before);

    if (GlobalMemoryStatusEx(&statex_before)) {
        phys_mem_before = statex_before.ullAvailPhys / 1024;  // Available physical memory in KB
        page_mem_before = statex_before.ullAvailPageFile / 1024;  // Available paging file memory in KB

        printf("Available Physical Memory Before: %lld KB\n", phys_mem_before);
        printf("Available Paging File Memory Before: %lld KB\n", page_mem_before);
    } else {
        printf("Failed to get memory status.\n");
        phys_mem_before = -1;
        page_mem_before = -1;
    }

    // Allocate and initialize numbers
    int *numbers = (int *)malloc(1000000 * sizeof(int));
    if (numbers == NULL) {
        printf("Memory allocation failed.\n");
    } else {
        for (int i = 0; i < 1000000; i++) {
            numbers[i] = i + 1;
        }
        free(numbers);  // Free the allocated memory
    }
    SYSTEM_INFO sysInfo;

    GetSystemInfo(&sysInfo);

    system_frame_size = sysInfo.dwPageSize;
    process_used_memory =(1000000 * sizeof(int))/1000;

    // Get memory status after
    MEMORYSTATUSEX statex_after;
    statex_after.dwLength = sizeof(statex_after);

    if (GlobalMemoryStatusEx(&statex_after)) {
        phys_mem_after = statex_after.ullAvailPhys / 1024;  // Available physical memory in KB
        page_mem_after = statex_after.ullAvailPageFile / 1024;  // Available paging file memory in KB

        printf("Available Physical Memory After: %lld KB\n", phys_mem_after);
        printf("Available Paging File Memory After: %lld KB\n", page_mem_after);
    } else {
        printf("Failed to get memory status.\n");
        phys_mem_after = -1;
        page_mem_after = -1;
    }

}


// Function to initialize all frames as free
void initialize_frames(Frame* frames, int total_frames) {
    for (int i = 0; i < total_frames; i++) {
        frames[i].assigned = 0;
        frames[i].process_id = -1;
        frames[i].page_number = -1;
    }
}

// Function to initialize disk storage
void initialize_disk(DiskPage* disk, int max_disk_pages) {
    for (int i = 0; i < max_disk_pages; i++) {
        disk[i].process_id = -1;
        disk[i].page_number = -1;
        disk[i].in_memory = 0;
    }
}

// Function to calculate required pages based on memory requirement and page size
int required_pages(int memory_requirement, int page_size) {
    return (int)ceil((double)memory_requirement / page_size);
}

// Function to allocate memory for processes
void allocate_memory(Process* processes, int process_count, Frame* frames, int total_frames, int page_size) {
    for (int i = 0; i < process_count; i++) {
        int pages_needed = required_pages(processes[i].memory_requirement, page_size);
        int allocated_pages = 0;

        // Check if there are enough free frames to satisfy the process's memory requirement
        for (int j = 0; j < total_frames && allocated_pages < pages_needed; j++) {
            if (frames[j].assigned == 0) {
                frames[j].assigned = 1;
                frames[j].process_id = processes[i].process_id;
                frames[j].page_number = allocated_pages + 1;
                allocated_pages++;
            }
        }

        // If not enough frames are available, swap out pages to disk
        if (allocated_pages < pages_needed) {
            for (int j = 0; j < total_frames && allocated_pages < pages_needed; j++) {
                if (frames[j].assigned == 1 && frames[j].process_id == processes[i].process_id) {
                    swap_out_page(frames, total_frames, processes[i].process_id, frames[j].page_number);
                    frames[j].assigned = 0;
                    frames[j].process_id = -1;
                    frames[j].page_number = -1;
                    allocated_pages++;
                }
            }
        }

        // Set process allocation status
        if (allocated_pages == pages_needed) {
            processes[i].allocated = 1;
            processes[i].state = RUNNING; // Set state to RUNNING
            printf("Process %d allocated %d pages\n", processes[i].process_id, pages_needed);
        } else {
            printf("Process %d could not be allocated due to insufficient memory\n", processes[i].process_id);
        }
    }
}

// Function to swap out a page from RAM to disk
void swap_out_page(Frame* frames, int total_frames, int process_id, int page_number) {
    if (disk_page_count < MAX_DISK_PAGES) {
        disk[disk_page_count].process_id = process_id;
        disk[disk_page_count].page_number = page_number;
        disk[disk_page_count].in_memory = 0;
        disk_page_count++;
        stats.page_faults++; // Increment page fault count
        printf("Swapping out page %d of process %d to disk\n", page_number, process_id);
    }
}

// Function to swap in a page from disk to RAM
void swap_in_page(int process_id, int page_number) {
    for (int i = 0; i < disk_page_count; i++) {
        if (disk[i].process_id == process_id && disk[i].page_number == page_number && !disk[i].in_memory) {
            disk[i].in_memory = 1; // Mark the page as loaded into memory
            stats.swap_time += 10; // Simulate swap time for page fault
            printf("Swapping in page %d of process %d from disk\n", page_number, process_id);
            break;
        }
    }
}

// Function to deallocate memory for a process
void deallocate_memory(Frame* frames, int total_frames, Process* processes, int process_count, int process_id) {
    for (int i = 0; i < total_frames; i++) {
        if (frames[i].process_id == process_id) {
            frames[i].assigned = 0;
            frames[i].process_id = -1;
            frames[i].page_number = -1;
        }
    }
    printf("Memory deallocated for process %d\n", process_id);
}

// Function to request additional memory
void request_additional_memory(Process* processes, int process_count, Frame* frames, int total_frames, int page_size) {
    int process_id, additional_memory;
    printf("Enter process ID to request additional memory: ");
    scanf("%d", &process_id);
    printf("Enter additional memory required (in KB): ");
    scanf("%d", &additional_memory);

    for (int i = 0; i < process_count; i++) {
        if (processes[i].process_id == process_id) {
            processes[i].memory_requirement += additional_memory;
            allocate_memory(processes, process_count, frames, total_frames, page_size);
            break;
        }
    }
}

// Function to display memory map
void display_memory_map(Frame* frames, int total_frames) {
    printf("\nMemory Map:\n");
    printf("Frame   Status          Process ID      Number of Page\n");
    printf("-------------------------------------------------\n");
    for (int i = 0; i < total_frames; i++) {
        if (frames[i].assigned) {
            printf("%d       Assigned        %d               %d\n", i+1, frames[i].process_id, frames[i].page_number);
        } else {
            printf("%d       Free            N/A             N/A\n", i+1);
        }
    }
    printf("-------------------------------------------------\n");
}

// Function to display active processes
void display_processes(Process* processes, int process_count) {
    printf("\nActive Processes:\n");
    for (int i = 0; i < process_count; i++) {
        printf("Process %d - Memory Required: %d KB, State: %d\n",
                processes[i].process_id, processes[i].memory_requirement, processes[i].state);
    }
}

// Function to print memory usage statistics
void print_memory_usage(Frame* frames, int total_frames, int page_size) {
    int used_frames = 0;
    int free_frames = 0;
    int total_memory = total_frames * page_size;
    int used_memory = 0;

    // Calculate used frames, free frames, used memory, and free memory
    for (int i = 0; i < total_frames; i++) {
        if (frames[i].assigned) {
            used_frames++;
            used_memory += page_size;
        } else {
            free_frames++;
        }
    }

    int free_memory = total_memory - used_memory;
    double memory_utilization = ((double)used_memory / total_memory) * 100;

    double exf= 100 - ((double)used_memory/total_memory)*100;
    double inf= 100 - ((double)process_used_memory/system_frame_size)*100;



    // Display memory usage statistics
    printf("\nMemory Usage Statistics:\n");
    printf("Total Frames: %d\n", total_frames);
    printf("Used Frames: %d\n", used_frames);
    printf("Free Frames: %d\n", free_frames);
    printf("Total Memory: %d KB\n", total_memory);
    printf("Used Memory: %d KB\n", used_memory);
    printf("Free Memory: %d KB\n", free_memory);
    printf("External Fragmentation: %.2lf %%\n",exf);
    printf("Internal Fragmentation: %.2lf %%\n",inf);
    printf("Memory Utilization: %.2f%%\n", memory_utilization);
    printf("Total Page Faults: %d\n", stats.page_faults);
    printf("Swap Time: %d ms\n", stats.swap_time);
}
int performance_matrix()
{
    int page_count;
    int pages[MAX_PAGES];
    int frames[MAX_FRAMES];
    int use_count[MAX_FRAMES];
    char fifo_algorithm[10] = "FIFO";
    char lru_algorithm[10] = "LRU";
    double fifo_metrics[8], lru_metrics[8]; // Metrics for both algorithms

    // Input frame count

    if (total_frames <= 0 || total_frames > MAX_FRAMES) {
        printf("Invalid frame count. Please enter a value between 1 and %d.\n", MAX_FRAMES);
        return 1;
    }

    // Input page count

    if (process_count <= 0 || process_count > MAX_PAGES) {
        printf("Invalid page count. Please enter a value between 1 and %d.\n", MAX_PAGES);
        return 1;
    }

    // Input page requests
    // printf("Enter the page requests:\n");
    for (int i = 0; i < process_count; i++) {
        // printf("Page request %d = ", i + 1);
        // scanf("%d", &pages[i]);
        pages[i]=i + 1;
    }

    for (int algo = 0; algo < 2; algo++) {
        char *policy = algo == 0 ? fifo_algorithm : lru_algorithm;

        // Initialize memory
        for (int i = 0; i < total_frames; i++) {
            frames[i] = -1; // Empty frame
            use_count[i] = 0;
        }

        int page_faults = 0;
        int current_index = 0;
        struct timespec start, end;
        double allocation_time = 0, deallocation_time = 0;

        for (int i = 0; i < process_count; i++) {
            int page = pages[i];
            clock_gettime(CLOCK_MONOTONIC, &start);

            // Check if page is in memory
            bool in_memory = false;
            for (int j = 0; j < total_frames; j++) {
                if (frames[j] == page) {
                    in_memory = true;
                    if (strcmp(policy, "LRU") == 0) {
                        use_count[j] = 1;
                    }
                    break;
                }
            }

            if (!in_memory) {
                // Page fault occurs
                page_faults++;
                int replace_index;

                if (strcmp(policy, "FIFO") == 0) {
                    replace_index = current_index % total_frames;
                    current_index++;
                } else if (strcmp(policy, "LRU") == 0) {
                    int min_index = 0;
                    for (int j = 1; j < total_frames; j++) {
                        if (use_count[j] < use_count[min_index]) {
                            min_index = j;
                        }
                    }
                    replace_index = min_index;
                }

                // Simulate memory allocation delay
                usleep(100); // 100 microseconds
                frames[replace_index] = page;
                use_count[replace_index] = 1;
            }

            // Increment use counts for LRU
            if (strcmp(policy, "LRU") == 0) {
                for (int j = 0; j < total_frames; j++) {
                    if (frames[j] != page) {
                        use_count[j]++;
                    }
                }
            }

            clock_gettime(CLOCK_MONOTONIC, &end);
            allocation_time += ((end.tv_sec - start.tv_sec) * 1000000L) +
                   (end.tv_nsec - start.tv_nsec);

        }

        // Simulate deallocation delay
        deallocation_time = allocation_time * 0.2;

        // Store performance metrics
        double memory_utilization = ((double)total_frames / MAX_FRAMES) * 100.0;
        double fragmentation = ((rand() % 10) + 1) / 10.0;
        double throughput = process_count / (allocation_time / 1000.0);
        double response_time = allocation_time / process_count;
        double thrashing_rate = (double)page_faults / process_count;
        double overhead = allocation_time * 0.1;

        double *metrics = algo == 0 ? fifo_metrics : lru_metrics;
        metrics[0] = memory_utilization;
        metrics[1] = fragmentation;
        metrics[2] = allocation_time;
        metrics[3] = deallocation_time;
        metrics[4] = throughput;
        metrics[5] = response_time;
        metrics[6] = thrashing_rate;
        metrics[7] = overhead;
    }

    // Display performance matrix
    printf("+-----------------------------+---------------+---------------+\n");
    printf("| Entity                      | FIFO          | LRU           |\n");
    printf("+-----------------------------+---------------+---------------+\n");
    //printf("| Fragmentation(%%)            | %13.2f | %13.2f |\n", fifo_metrics[1]*100, lru_metrics[1]*100);
    printf("| Allocation Time (micros)    | %13.2f | %13.2f |\n", fifo_metrics[2], lru_metrics[2]);
    printf("| Deallocation Time (micros)  | %13.2f | %13.2f |\n", fifo_metrics[3], lru_metrics[3]);
    printf("| Throughput (pages/s)        | %13.2f | %13.2f |\n", fifo_metrics[4], lru_metrics[4]);
    printf("| Response Time (micros)      | %13.2f | %13.2f |\n", fifo_metrics[5], lru_metrics[5]);
    printf("| Thrashing Rate              | %13.2f | %13.2f |\n", fifo_metrics[6], lru_metrics[6]);
    printf("| Overhead (micros)           | %13.2f | %13.2f |\n", fifo_metrics[7], lru_metrics[7]);
    printf("+-----------------------------+---------------+---------------+\n");

}
