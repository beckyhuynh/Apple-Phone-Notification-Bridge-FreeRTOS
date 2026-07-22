// #include <Arduino.h>
#define LED_PIN 2
#define BUTTON_PIN 23

// declare task handle, a variable that points to freeRTOS task
TaskHandle_t BlinkTaskHandle = NULL;

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
    vTaskSuspend(BlinkTaskHandle);
  }

  // if taskSuspended variable is false, resume execution
  else {
    vTaskResume(BlinkTaskHandle);
  }
}

// create a task function
// freertos tasks has to return void and accept a single argument
void BlinkTask(void* parameter) {
  for (;;){ // infinite loop

  digitalWrite(LED_PIN, HIGH);
  Serial.println("BlinkTask: LED ON");
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  digitalWrite(LED_PIN, LOW);
  Serial.println("BlinkTask: LED OFF");
  vTaskDelay(1000/portTICK_PERIOD_MS);

  Serial.print("BlinkTask running on core");
  Serial.println(xPortGetCoreID()); // shows which core task is running on
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // internal pull up resistor
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // declare pushbutton as interrupt
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

  // creating the task, choosing a specific core
  xTaskCreatePinnedToCore(
    BlinkTask, // taskfunction
    "BlinkTask", //task name
    10000, //stack size(bytes)
    NULL, //parameters
    1,  // priority
    &BlinkTaskHandle, // Task handle
    1 // Core 1
  );
}

void loop() {
  // put your main code here, to run repeatedly:

}
