// #include <Arduino.h>
#define LED1_PIN 2
#define BUTTON_PIN 23
#define LED2_PIN 4

// declare task handle, a variable that points to freeRTOS task
TaskHandle_t Task1Handle = NULL;
TaskHandle_t Task2Handle = NULL;

// Volatile variables for ISR
volatile bool taskSuspended = false; // determine whether task is suspended or not

// to debounce the pushbutton
volatile uint32_t lastInterruptTime = 0; 
const uint32_t debounceDelay = 100; // debounce period

// creating an interrupt service routine to run on RAM
void IRAM_ATTR buttonISR(){
  // Debounce
  uint32_t currentTime = millis();
  if (currentTime - lastInterruptTime < debounceDelay) {
    return;
  }
  lastInterruptTime = currentTime;

  // Toggle task state
  taskSuspended = !taskSuspended; 

  if (taskSuspended) {
    vTaskSuspend(Task1Handle);
  }

  // if taskSuspended variable is false, resume execution
  else {
    vTaskResume(Task1Handle);
  }
}

// create a task function
// freertos tasks has to return void and accept a single argument
void Task1(void* parameter) {
  pinMode(LED1_PIN, OUTPUT);
  for (;;){ // infinite loop

  digitalWrite(LED1_PIN, HIGH);
  Serial.println("Task1: LED1 ON");
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  digitalWrite(LED1_PIN, LOW);
  Serial.println("Task1: LED1 OFF");
  vTaskDelay(1000/portTICK_PERIOD_MS);

  Serial.print("task1 running on core");
  Serial.println(xPortGetCoreID()); // shows which core task is running on
  
   Serial.printf("Task1 Stack Free: %u bytes\n", uxTaskGetStackHighWaterMark(NULL));
  }
}

void Task2(void* parameter) {
  pinMode(LED2_PIN, OUTPUT);
  for (;;){ // infinite loop

  digitalWrite(LED2_PIN, HIGH);
  Serial.println("Task2: LED2 ON");
  vTaskDelay(333 / portTICK_PERIOD_MS);

  digitalWrite(LED2_PIN, LOW);
  Serial.println("Task2: LED2 OFF");
  vTaskDelay(333/portTICK_PERIOD_MS);

  Serial.print("task2 running on core");
  Serial.println(xPortGetCoreID()); // shows which core task is running on
  
  Serial.printf("Task2 Stack Free: %u bytes\n", uxTaskGetStackHighWaterMark(NULL));
  
  }
}

void setup() {
  Serial.begin(115200);

  delay(1000);

  Serial.printf("Starting FreeRTOS: Memory Usage\nInitial Free Heap: %u bytes\n", xPortGetFreeHeapSize());


  // internal pull up resistor
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // declare pushbutton as interrupt
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

  // creating the task, choosing a specific core
  xTaskCreatePinnedToCore(
    Task1, // taskfunction
    "Task1", //task name
    10000, //stack size(bytes)
    NULL, //parameters
    1,  // priority
    &Task1Handle, // Task handle
    1 // Core 1
  );

  xTaskCreatePinnedToCore(
    Task2, // taskfunction
    "Task2", //task name
    10000, //stack size(bytes)
    NULL, //parameters
    1,  // priority
    &Task2Handle, // Task handle
    0 // Core 0
  );
}

void loop() {
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    Serial.printf("Free Heap: %u bytes\n", xPortGetFreeHeapSize());
    lastCheck = millis();
  }

}
