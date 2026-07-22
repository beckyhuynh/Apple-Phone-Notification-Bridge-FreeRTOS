#include <Arduino.h>

int count1 = 0;
int count2 = 0;

void task1(void* parameters){
  for (;;){
    Serial.print("Task 1 counter: ");
    Serial.println(count1++);

    // how long to wait in number of seconds
    // the next iteration of for loop delayed by one sec(other task can run during that period)
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void task2(void* parameters){
  for (;;){
    Serial.print("Task 2 counter: ");
    Serial.println(count2++);

    // how long to wait in number of seconds
    // the next iteration of for loop delayed by one sec(other task can run during that period)
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(9600);

  xTaskCreate(
    task1, // function name
    "Task 1", // task name for debugging
    1000, // stack size in bytes
    NULL, // passing in parameters here if applicable
    1, // task priority, lower number means lower priority
    NULL // task handle
  );

  xTaskCreate(
    task2, // function name
    "Task 2", // task name for debugging
    1000, // stack size in bytes
    NULL, // passing in parameters here if applicable
    1, // task priority, lower number means lower priority
    NULL // task handle
  );
}

void loop() {
  // put your main code here, to run repeatedly:

}
