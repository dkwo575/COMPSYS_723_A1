#include <stdio.h>
#include "altera_avalon_pio_regs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"

// Constants and hardware definitions
#define THRESHOLD_FREQ 50 // Set your threshold frequency
#define THRESHOLD_ROC 5 // Set your threshold rate of change of frequency

// Shared resources and definitions of semaphores
SemaphoreHandle_t xMutex; // frequency
SemaphoreHandle_t system_status;



// global variable
float inst_freq = 0;
float roc_freq = 0;
int load_status[5] = {0};
int net_stability = 1;
int relay_state = 0;
int timing_meas[5] = {0};
char measureBuffer[50];

float threshold_freq;
float threshold_roc;

// Function prototypes
void task1(void *pvParameters);
void task2(void *pvParameters);
void task3(void *pvParameters);
void ISR1(void *context, alt_u32 id);
void ISR2(TimerHandle_t xTimer);

// local function
void drawThresholds(float, float);
void drawStatus();

int main() {
    // Initialize hardware, peripherals, and shared resources
    // ...

    // Create the tasks
    xTaskCreate(task1, "Monitor frequency and RoC", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(task2, "Manage loads", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(task3, "Update VGA display", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    // Create the mutex for shared resources
    xMutex = xSemaphoreCreateMutex();

    // Set up timer interrupt for ISR2
    TimerHandle_t xTimer = xTimerCreate("TimerISR", pdMS_TO_TICKS(200), pdTRUE, 0, ISR2);
    xTimerStart(xTimer, 0);

    // Set up user input interrupt for ISR1
    // ...

    // Start the scheduler
    vTaskStartScheduler();

    // The program should not reach this point, as the scheduler takes control
    while(1);
    return 0;
}

void task1(void *pvParameters) {

	// Variables for frequency measurement and rate of change calculation
    float prev_freq = 0;
    float curr_freq = 0;
    float prev_time = 0;
    float curr_time = 0;
    float measured_inst_freq;
    float measured_roc_freq;

    while(1) {
        printf("Task 1 - Frequency measurement: \n");
        // Measure inst_freq using appropriate sensors or methods
        measured_inst_freq = IORD(FREQUENCY_ANALYSER_BASE, 0);
        printf("%f Hz\n", 16000 / (double)measured_inst_freq);

        // Calculate rate of change of frequency in Hz/s
        curr_time = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000.0; // Convert ms to s
        curr_freq = measured_inst_freq;
        measured_roc_freq = (curr_freq - prev_freq) / (curr_time - prev_time);
        prev_freq = curr_freq;
        prev_time = curr_time;

        printf("Task 1 - Rate of frequency change: \n");
        printf("%f Hz/s\n", (double)measured_roc_freq);

        // Update shared resources
        xSemaphoreTake(xMutex, portMAX_DELAY);
        inst_freq = measured_inst_freq;
        roc_freq = measured_roc_freq;
        net_stability = (inst_freq >= THRESHOLD_FREQ) && (abs(roc_freq) <= THRESHOLD_ROC);
        xSemaphoreGive(xMutex);

        // Sleep for a while (adjust the delay as needed)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void task2(void *pvParameters) {
	// this task is for
    while(1) {

        // Check network stability and relay state
        // ...
//		xSemaphoreTake(xMutex, portMAX_DELAY);
//		xSemaphoreTake(system_status, portMAX_DELAY);


        // Manage loads based on network stability and relay state
        // ...

        // Update LED states according to load status and relay state
        // ...

        // Sleep for a while (adjust the delay as needed)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
typedef struct{
	unsigned int x1;
	unsigned int y1;
	unsigned int x2;
	unsigned int y2;
}Line;

void task3(void *pvParameters) {
	// display shared variable on VGA display
	// initialise VGA controller
	// for plot graph
	alt_up_pixel_buffer_dma_dev *pixel_buffer;
	pixel_buffer = alt_up_pixel_buffer_dma_open_dev(VIDEO_PIXEL_BUFFER_DMA_NAME);
	if(pixel_buffer == NULL){
		printf("can't find pixel buffer device\n");
	}
	alt_up_pixel_buffer_dma_clear_screen(pixel_buffer, 0);
	
	// ....
	alt_up_char_buffer_dev *char_buf;
	char_buf = alt_up_char_buffer_open_dev("/dev/video_character_buffer_with_dma");
	if(char_buf == NULL){
		printf("Error: could not open character buffer device\n");
	}
	else{
		printf("Opened character buffer device \n");
	}
	alt_up_char_buffer_clear(char_buf);
	
	//Set up plot axes
	alt_up_pixel_buffer_dma_draw_hline(pixel_buffer, 100, 590, 200, ((0x3ff << 20) + (0x3ff << 10) + (0x3ff)), 0);
	alt_up_pixel_buffer_dma_draw_hline(pixel_buffer, 100, 590, 300, ((0x3ff << 20) + (0x3ff << 10) + (0x3ff)), 0);
	alt_up_pixel_buffer_dma_draw_vline(pixel_buffer, 100, 50, 200, ((0x3ff << 20) + (0x3ff << 10) + (0x3ff)), 0);
	alt_up_pixel_buffer_dma_draw_vline(pixel_buffer, 100, 220, 300, ((0x3ff << 20) + (0x3ff << 10) + (0x3ff)), 0);
	
	// set up plot variable
	alt_up_char_buffer_string(char_buf, "Frequency(Hz)", 4, 4);
	alt_up_char_buffer_string(char_buf, "52", 10, 7);
	alt_up_char_buffer_string(char_buf, "50", 10, 12);
	alt_up_char_buffer_string(char_buf, "48", 10, 17);
	alt_up_char_buffer_string(char_buf, "46", 10, 22);

	alt_up_char_buffer_string(char_buf, "df/dt(Hz/s)", 4, 26);
	alt_up_char_buffer_string(char_buf, "60", 10, 28);
	alt_up_char_buffer_string(char_buf, "30", 10, 30);
	alt_up_char_buffer_string(char_buf, "0", 10, 32);
	alt_up_char_buffer_string(char_buf, "-30", 9, 34);
	alt_up_char_buffer_string(char_buf, "-60", 9, 36);
	
	// Print text about status
	
	drawStatus();
	
	// Print threshold value
	drawThresholds(threshold_freq, threshold_roc);
	
	
	Line freq_line, roc_line;

    while(1) {
        // Read shared variables
        // ...


		// clear the old value and graph
        // Update VGA display with frequency relay information
        // ...

        // Sleep for a while (adjust the delay as needed)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void drawThresholds(float frequency_val, float roc_val){
	// Print threshold value
	sprintf(measureBuffer, "Frequency Threshold value:  %05.2f Hz", frequency_val);
	alt_up_char_buffer_string(char_buf, measureBuffer, 44, 40);
	sprintf(measureBuffer, "ROC Threshold value : %05.2f Hz/s", roc_val);
	alt_up_char_buffer_string(char_buf, measureBuffer, 44, 42);
}

void drawStatus(){
	
	
}


void ISR1(void *context, alt_u32 id) {
    // Handle user input (slide switches and push button)
    // ...

    // Update shared resources (load_status and relay_state)
    xSemaphoreTakeFromISR(xMutex, 0);
    // Update load_status based on the slide switches
    // Update relay_state based on the push button
    xSemaphoreGiveFromISR(xMutex, 0);
}

void ISR2(TimerHandle_t xTimer) {
    // Measure the time intervals and update timing measurements
    // ...

    // Update shared resources (timing_meas)
    xSemaphoreTakeFromISR(xMutex, 0);
    // Update timing_meas array
    xSemaphoreGiveFromISR(xMutex, 0);
}
