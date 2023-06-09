#include <stdio.h>
#include "altera_avalon_pio_regs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include <stdlib.h>
#include "sys/alt_irq.h"
#include "system.h"
#include "io.h"
#include "altera_up_avalon_video_character_buffer_with_dma.h"
#include "altera_up_avalon_video_pixel_buffer_dma.h"
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

// Constants and hardware definitions
#define THRESHOLD_FREQ 50 // Set your threshold frequency
#define THRESHOLD_ROC 5 // Set your threshold rate of change of frequency
#define MAX_LOADS 5 // maximum number of loads
#define PRIORITY_HIGH 2
#define PRIORITY_MEDIUM 1
#define PRIORITY_LOW 0
#define MIN_FREQ 45 // minimum frequency for drawing graph


// Shared resources and definitions of semaphores
SemaphoreHandle_t xMutex; // frequency
SemaphoreHandle_t system_status;

// definition for frequency plot(VGA task)
#define FREQPLT_ORI_X 101 //x axis pixel position at the plot origin
#define FREQPLT_GRID_SIZE_X 5 //pixel separation in the x axis between two data points
#define FREQPLT_ORI_Y 199.0 //y axis pixel position at the plot origin
#define FREQPLT_FREQ_RES 20.0 //number of pixels per Hz (y axis scale)

#define ROCPLT_ORI_X 101
#define ROCPLT_GRID_SIZE_X 5
#define ROCPLT_ORI_Y 259.0
#define ROCPLT_ROC_RES 0.5

#define FREQ_ARRAY 100


// global variable
float inst_freq = 0;
float roc_freq = 0;
int load_status[5] = {0};
int net_stability = 1;
int relay_state = 0;
int timing_meas[5] = {0};
char measureBuffer[50];

int load_priority[MAX_LOADS] = {PRIORITY_HIGH, PRIORITY_MEDIUM, PRIORITY_LOW, PRIORITY_LOW, PRIORITY_LOW};
int load_status[MAX_LOADS] = {0};
int relay_state = 0;
int net_stability = 0;


// Function prototypes
void task1(void *pvParameters);
void task2(void *pvParameters);
void task3(void *pvParameters);
void ISR1(void *context, alt_u32 id);
void ISR2(TimerHandle_t xTimer);

typedef struct{
	unsigned int x1;
	unsigned int y1;
	unsigned int x2;
	unsigned int y2;
}Line;


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

    while (1) {
        printf("Task 1 - Frequency measurement: \n");
        // Measure inst_freq using appropriate sensors or methods
        measured_inst_freq = IORD(FREQUENCY_ANALYSER_BASE, 0);
        printf("%f Hz\n", 16000 / (double)measured_inst_freq);

        // Calculate rate of change of frequency in Hz/s
        curr_time = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000.0; // Convert ms to s
        curr_freq = measured_inst_freq;
        if (curr_time != prev_time) { // Check if the time interval is not zero
            measured_roc_freq = (curr_freq - prev_freq) / (curr_time - prev_time);
        } else {
            measured_roc_freq = 0; // Set rate of change to 0 if time interval is zero
        }
        prev_freq = curr_freq;
        prev_time = curr_time;

        printf("Task 1 - Rate of frequency change: \n");
        printf("%f Hz\n", (double)measured_roc_freq);

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
		
		xSemaphoreTake(xMutex, portMAX_DELAY);
        int net_stability_local = net_stability;
        int relay_state_local = relay_state;
        xSemaphoreGive(xMutex);
        // ...



        // Manage loads based on network stability and relay state
        // ...
		for (int i = 0; i < MAX_LOADS; i++) {
            if (load_priority[i] == PRIORITY_HIGH && net_stability_local == 0) {
                load_status[i] = 0; // shed high priority load if network is unstable
            } else if (load_priority[i] == PRIORITY_LOW && relay_state_local == 0) {
                load_status[i] = 0; // shed low priority load if relay is off
            } else {
                load_status[i] = 1; // reconnect load if conditions are met
            }
        }

        // Update LED states according to load status and relay state
        for (int i = 0; i < MAX_LOADS; i++) {
            // Set LED state according to load status
            if (load_status[i] == 1) {
                IOWR_ALTERA_AVALON_PIO_DATA(LED_BASE, (1 << i));
            } else {
                IOWR_ALTERA_AVALON_PIO_DATA(LED_BASE, ~(1 << i));
            }
        }
		
		IOWR_ALTERA_AVALON_PIO_DATA(RED_LED_BASE, RED_LED);
    	IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LED_BASE, GREEN_LED);

        // Sleep for a while (adjust the delay as needed)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


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
	

	

	
	Line f_draw, roc_draw;

    while(1) {
        // Read shared variables
        // ...
		// Print threshold value
		double f[100], roc[100]
		//RoC = (f[i] - f[i-1]) * 2 * f[i] * f[i-1] / (f[i] + f[i-1])
		
		if (i==0) {
			roc[0] = (f[0]-f[99]) * 2.0 * f[0] * f[99] / (f[0]+f[99]);
		}
		else {
			roc[i] = (f[i] - f[i-1]) * 2.0 * f[i] * f[i-1] / (f[i] + f[i-1])
		}
		
		if (dfreq[i] > 100.0){
			dfreq[i] = 100.0;
		}
		
		i =	++i%100; // Point to the next data (oldest) to be overwritten
			//i here points to the oldest data, j loops through all the data to be drawn on VGA
			
		for(int j=0;j<99;++j){ 
			if (((int)(f[(roc+j)%100]) > MIN_FREQ) && ((int)(f[(roc+j+1)%100]) > MIN_FREQ)){
				//Calculate coordinates of the two data points to draw a line in between
				
				//Draw frequency 
				f_draw.x1 = FREQPLT_ORI_X + FREQPLT_GRID_SIZE_X * j;
				f_draw.y1 = (int)(FREQPLT_ORI_Y - FREQPLT_FREQ_RES * (f[(roc+j)%100] - MIN_FREQ));

				f_draw.x2 = FREQPLT_ORI_X + FREQPLT_GRID_SIZE_X * (j + 1);
				f_draw.y2 = (int)(FREQPLT_ORI_Y - FREQPLT_FREQ_RES * (freq[(freqIndex+j+1)%100] - MIN_FREQ));


				//Draw roc frequency
				roc_draw.x1 = ROCPLT_ORI_X + ROCPLT_GRID_SIZE_X * j;
				roc_draw.y1 = (int)(ROCPLT_ORI_Y - ROCPLT_ROC_RES * dfreq[(freqIndex+j)%100]);

				roc_draw.x2 = ROCPLT_ORI_X + ROCPLT_GRID_SIZE_X * (j + 1);
				roc_draw.y2 = (int)(ROCPLT_ORI_Y - ROCPLT_ROC_RES * dfreq[(freqIndex+j+1)%100]);



				//Draw Line
				alt_up_pixel_buffer_dma_draw_line(pixel_buf, f_draw.x1, f_draw.y1, f_draw.x2, f_draw.y2, 0x3ff << 0, 0);
				alt_up_pixel_buffer_dma_draw_line(pixel_buf, roc_draw.x1, roc_draw.y1, roc_draw.x2, roc_draw.y2, 0x3ff << 0, 0);
		
		
		sprintf(measureBuffer, "Frequency Threshold value:  %05.2f Hz", (double)THRESHOLD_FREQ);
		alt_up_char_buffer_string(char_buf, measureBuffer, 44, 40);
		sprintf(measureBuffer, "ROC Threshold value : %05.2f Hz/s", (double)THRESHOLD_ROC);
		alt_up_char_buffer_string(char_buf, measureBuffer, 44, 42);

		// Print text about status
    	alt_up_char_buffer_string(char_buf, "System status", 44, 48);
    	if(1) {
    		alt_up_char_buffer_string(char_buf, "Stable", 44, 50);
    		alt_up_pixel_buffer_dma_draw_box(pixel_buffer, 352, 365, 362, 375, 0x3ff, 0); //Green
    	}
    	else {
    		alt_up_char_buffer_string(char_buf, "Unstable", 44, 50);
    		alt_up_pixel_buffer_dma_draw_box(pixel_buffer, 352, 365, 362, 375, ((0x3ff << 20) + (0x3ff << 10) + (0x3ff)), 0); //Green
    	}
		// clear the old value and graph
		alt_up_pixel_buffer_dma_draw_box(pixel_buffer, 101, 0, 639, 199, 0, 0);
		alt_up_pixel_buffer_dma_draw_box(pixel_buffer, 101, 201, 639, 299, 0, 0);
		
        // Update VGA display with frequency relay information
        // ...

        // Sleep for a while (adjust the delay as needed)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}




void ISR1(void *context, alt_u32 id) {
    // Handle user input (slide switches and push button)
    // ...
	int switchState = IORD_ALTERA_AVALON_PIO_DATA(SLIDE_SWITCH_BASE);

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
