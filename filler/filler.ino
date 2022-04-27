#include <Wire.h>
#include <HX711.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>

#define SCREEN_WIDTH  128// OLED display width,  in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define WIDTH_OF_TEXT_B 16
#define WIDTH_OF_TEXT_C 18

//#define DEBUG // comment this line to remove debug

//pin definitions
const byte ENCODE_PIN_A = 18; // pin with interrupt
const byte ENCODE_PIN_B = 17; //pin without interrupt
const byte ENCODE_BUTTON = 19; // pin with interrupt
// HX711 circuit wiring
const byte LOADCELL_DOUT_PIN = 8;
const byte LOADCELL_SCK_PIN = 9;
// touch button
const byte FILL_BUTTON = 10;
const byte TARE_AND_FILL_BUTTON = 11;
//pwm control pin
const byte VIBRATOR_PIN = 12;
const byte SERVO_PIN = 7;

float measured_weight=0; //to store measured weight
float set_weight = 5; //default set weight

//states of machine
enum class Machine_State : byte {
  FIRST_PAGE,
  EDIT_WHOLE,
  EDIT_DECIMAL,
  LOOKUP_TABLE
};

//pid
boolean pid_reset_flag = true;
//encoder
volatile int acceleration_constant=5; // for encoder to feel responsive
volatile float encoderPos = 0;
boolean encoder_update_flag = false;
volatile long encoder_update_time=0; //for use within ISR
unsigned long encode_last_update=0; //??
//encoder button
boolean encoder_button_update_flag = false;
volatile long encoder_button_update_time=0;//for use within ISR
Machine_State current_edit_state=Machine_State::FIRST_PAGE;
//OLED
static boolean edit_blink_state=true;
//powder filling
boolean powder_filling_flag = false; 

//lookup table definitions 
#define NUM_OF_TABLES 5
struct VALUE_STRUCT{
  float WEIGHT;
  byte PWM;
  byte SERVO;
};
#define SIZE_OF_VALUE_STRUCT 6 //float+byte+byte = 4+1+1
struct TABLE_STRUCT {
  struct VALUE_STRUCT* STORED_TABLE;
  byte TABLE_SIZE;
  char TABLE_NAME[9]; //need to put one char more than required
};
struct MAIN_STRUCT{
  struct TABLE_STRUCT* LK_TABLE;
};

//lookup table values
//table A
static VALUE_STRUCT LOOKUPTABLE_A[20] ={ 
  {20,220,30},   //weight, pwm, servo
  {3.5,200,32},
  {3,195,38},
  {2.5,190,40},
  {2,172,45},
  {1.5,165,50},
  {1.2,162,52},
  {1,160,55},
  {.8,158,58},
  {.5,152,60},
  {.35,150,65},
  {0.3,146,72},
  {0.2,144,90},
  {0.1,125,105},
  {0.05,120,107},
  {0.03,116,108},
  {0.015,112,110},
  {0.01,105,112},
  {0.005,100,113},
  {0,0,114}
};
static struct TABLE_STRUCT STRUCT_OF_TABLE_WITH_SIZE_A[1]={
  LOOKUPTABLE_A,
  sizeof(LOOKUPTABLE_A)/SIZE_OF_VALUE_STRUCT,
  "preset 1"
};

//table B
static VALUE_STRUCT LOOKUPTABLE_B[6] ={
  {20,230,20},
  {1,220,23},
  {.5,200,26},
  {.1,190,29},
  {.01,180,30},
  {0,0,125}
};
static struct TABLE_STRUCT STRUCT_OF_TABLE_WITH_SIZE_B[1]={
  LOOKUPTABLE_B,
  sizeof(LOOKUPTABLE_B)/SIZE_OF_VALUE_STRUCT,
  "preset 2"
};

//table C
static struct TABLE_STRUCT STRUCT_OF_TABLE_WITH_SIZE_C[1]={
  LOOKUPTABLE_B,
  sizeof(LOOKUPTABLE_B)/SIZE_OF_VALUE_STRUCT,
  "preset 3"
};

//table D
static struct TABLE_STRUCT STRUCT_OF_TABLE_WITH_SIZE_D[1]={
  LOOKUPTABLE_B,
  sizeof(LOOKUPTABLE_B)/SIZE_OF_VALUE_STRUCT,
  "preset 4"
};

//table E
static struct TABLE_STRUCT STRUCT_OF_TABLE_WITH_SIZE_E[1]={
  LOOKUPTABLE_B,
  sizeof(LOOKUPTABLE_B)/SIZE_OF_VALUE_STRUCT,
  "preset 5"
};

//all tables
static struct MAIN_STRUCT TOP_STRUCT[NUM_OF_TABLES]={
  STRUCT_OF_TABLE_WITH_SIZE_A,
  STRUCT_OF_TABLE_WITH_SIZE_B,
  STRUCT_OF_TABLE_WITH_SIZE_C,
  STRUCT_OF_TABLE_WITH_SIZE_D,
  STRUCT_OF_TABLE_WITH_SIZE_E,
};
int current_table=0; //stores which lookup table is being currently used

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // declare an SSD1306 display object connected to I2C
HX711 scale;
Servo lift_servo;

//some fuction needs to be declared for this to be compiled
void measure_weight(int level=1);

void setup() {
  const float CALIBRATION_FACTOR = 15085; //for my load cell
  Serial.begin(115200);
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);// initialize OLED display
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.tare();
  scale.set_scale(CALIBRATION_FACTOR);
  
  pinMode(ENCODE_PIN_A, INPUT);
  pinMode(ENCODE_PIN_B, INPUT);
  pinMode(ENCODE_BUTTON, INPUT);
  pinMode(FILL_BUTTON, INPUT);
  pinMode(TARE_AND_FILL_BUTTON, INPUT);
  pinMode(VIBRATOR_PIN, OUTPUT);
  pinMode(SERVO_PIN, OUTPUT);
  
  lift_servo.attach(SERVO_PIN);
  attachInterrupt(digitalPinToInterrupt(ENCODE_PIN_A),encoderISR, RISING);
  attachInterrupt(digitalPinToInterrupt(ENCODE_BUTTON),encoderButtonISR, RISING);
  turn_off_motors();
  
  //startup page
  oled.setTextSize(1);          // text size
  oled.setTextColor(WHITE);     // text color
  oled.setCursor(10, 30);        // position to display
  oled.println("Powder filler v0.1"); // text to display
  oled.display();               // show on OLED
  delay(500);
}

void loop() {
  oled.clearDisplay();//OLED
  switch(current_edit_state){
    case Machine_State::FIRST_PAGE:
      measure_weight();
      if(powder_filling_flag){
        fillPowder();
      }
      displayWeights();  // show first page
      break;
    case Machine_State::EDIT_WHOLE:
    case Machine_State::EDIT_DECIMAL:
      readUserWeightInput(); 
      setWeightPage();//OLED
      break;
    case Machine_State::LOOKUP_TABLE:
      show_lookup_table_names();//OLED
      break;
    default:
      break;
  }
  oled.display();// to change display
  touchButtonWatcher();
  encoderButtonWatcher();
}

void measure_weight(int level=1){ //if weight jumps suddenly due to electrical noise, it is weighed again
  static float previous_weight=0;
  static const float tolerance = 2;
  measured_weight = scale.get_units(1); // measure weight once
  if (abs(measured_weight - previous_weight)+1>(pow(tolerance,level))){ //this ignore natural weight chages
    measure_weight(level+1);
    return;
  }
  previous_weight=measured_weight;
  return;
}

boolean filling_motor_started(boolean reset_flag = false){
  static unsigned long filling_start_last_update_time=0;
  static boolean new_run=true;
  if(reset_flag){
    new_run=true;
    pid_reset_flag=true;
  }
  if(new_run && !reset_flag){ //don't update values during reset
    filling_start_last_update_time=millis();
    new_run=false;
  }
  if(millis() - filling_start_last_update_time <500){ //wait for motor to start up for 0.5 sec
     return false;
  } else{
    return true;
  }
  return false;
}

void turn_off_motors(){
  analogWrite(VIBRATOR_PIN,0);
  lift_servo.write(125);
  filling_motor_started(true); //resetting flag
  powder_filling_flag=false;
}

byte current_table_size(){
  return((TOP_STRUCT+current_table)->LK_TABLE->TABLE_SIZE);
}

void lookup_with_pid(float x,byte *pwm,byte *servo){
  const float KP = .50;
  const float KI = .00002; 
  const float KD = 180;
  
  unsigned long current_time, elapsed_time;
  static unsigned long previous_time;
  float error,rate_error,new_x;
  static float cumulative_error,last_error;
  static boolean skip_first_error;
  if(pid_reset_flag){ //flag is set to reset pid to startup value
    previous_time = millis();
    cumulative_error = 0;
    last_error=0;
    lookup(x,*pwm,*servo); //first value without pid
    skip_first_error=true;
    pid_reset_flag = false;
    return;
  }
  current_time = millis();
  elapsed_time = current_time - previous_time;
  error = set_weight - measured_weight;
  if(error>.75){ //to prevent cumuative to climb higher
    cumulative_error += error * (elapsed_time)-(cumulative_error/50);
  } else{
    cumulative_error += error * (elapsed_time)*9;//additional help in reaching goal
  }
  rate_error = (error - last_error)/(elapsed_time);
  if(skip_first_error){ // last error is not present
    rate_error =0;
    skip_first_error=false;
  }
  new_x = KP * error + KI * cumulative_error + KD * rate_error;
  previous_time = current_time;
  last_error = error;

//debug
#ifdef DEBUG
  Serial.print(elapsed_time);
  Serial.print("\t");
  Serial.print(error);
  Serial.print("\t");
  Serial.print(KP * error);
  Serial.print("\t");
  Serial.print(KI * cumulative_error);
  Serial.print("\t");
  Serial.print(KD * rate_error);
  Serial.print("  |  ");
  lookup(x,pwm,servo);
  Serial.print(x);
  Serial.print("\t");
  Serial.print(*pwm);
  Serial.print("\t");
  Serial.print(*servo);
  Serial.print("  |  ");
#endif
  
  lookup(new_x,pwm,servo);
  
//debug
#ifdef DEBUG
  Serial.print(new_x);
  Serial.print("\t");
  Serial.print(*pwm);
  Serial.print("\t");
  Serial.println(*servo);
#endif
  
  return;
}

void lookup(float x,byte *pwm,byte *servo){
  struct VALUE_STRUCT* lookup_table=(TOP_STRUCT+current_table)->LK_TABLE->STORED_TABLE;
  byte table_size = current_table_size();
  if(x<=0){
    //default values to be passed when lookup fails
    *pwm=0;
    *servo=121;
    return;
  }
  for(byte i=0;i<table_size;i++){ //linear interpolation from lookup table
    if(((lookup_table+i)->WEIGHT>=x) && ((lookup_table+i+1)->WEIGHT <=x)){  
      //yp = y0 + (y1-y0) * (xp - x0) / (x1-x0)
      //calculating  (xp - x0) / (x1-x0) seperatly
      byte intermediate_variable = (x - (lookup_table+i+1)->WEIGHT)/(((lookup_table+i)->WEIGHT)-((lookup_table+i+1)->WEIGHT));
      *pwm= (((lookup_table+i+1)->PWM)+(((lookup_table+i)->PWM)-(lookup_table+i+1)->PWM)* intermediate_variable);
      // pwm = base pwm + (high pwm- base pwm) * (weight read - weight base) / (weight high - weight base)
      *servo= (((lookup_table+i+1)->SERVO)+(((lookup_table+i)->SERVO)-(lookup_table+i+1)->SERVO)* intermediate_variable);
      // servo = base servo + (high servo- base servo) * (weight read - weight base) / (weight high - weight base)
      return;
    }
  }
  //default values to be passed when lookup fails
  *pwm=0;
  *servo=125;
  return;
}

void fillPowder(){
  static const byte FILING_MOTOR_START_PWM=220; // start speed //adjustable
  if(set_weight <= measured_weight || (set_weight - measured_weight)>99.99){ //don't try to fill negative or too large amounts
    turn_off_motors();
    return;
  }
  if(!filling_motor_started()){ //get the motor starting
    analogWrite(VIBRATOR_PIN,FILING_MOTOR_START_PWM);
    return;
  }
  
  if(measured_weight<set_weight){
    byte pwm;
    byte servo;
    lookup_with_pid(set_weight-measured_weight,&pwm,&servo);
    analogWrite(VIBRATOR_PIN,pwm);
    constrain(servo,30,125);
    lift_servo.write(servo);
    return;
  }
  else{
    if(scale.get_units(1)>set_weight){ //checking weight again to avoid false positive
      turn_off_motors();
    }
  }
}

void touchButtonWatcher(){
  static unsigned long touch_fill_last_update_time=0;
  if(digitalRead(FILL_BUTTON)){
    current_edit_state = Machine_State::FIRST_PAGE;
    if(millis() - touch_fill_last_update_time >600){ //debounce delay
      touch_fill_last_update_time = millis();
      powder_filling_flag = !powder_filling_flag;
      if(!powder_filling_flag){ //reset motor started flag false when stopped
        turn_off_motors();
      }
    }
  }
  
  if(digitalRead(TARE_AND_FILL_BUTTON)){
    scale.tare();
    powder_filling_flag = !powder_filling_flag; //need to fix this logic
  }
}

void show_lookup_table_names(){
  //adding from encoder variable
  current_table+=encoderPos;
  current_table=constrain(current_table,0,NUM_OF_TABLES-1);
  encoderPos=0; //clearing encoder;
  
  //display headings
  oled.setTextSize(2);          // text size
  oled.setCursor(1.4*WIDTH_OF_TEXT_B,0);
  oled.println("PRESET");
  oled.setCursor(0,.6*WIDTH_OF_TEXT_B);
  oled.println("----------");
  oled.setTextSize(1);
  oled.setCursor(0,WIDTH_OF_TEXT_B*2.6); //
  oled.println(">>                 <<");
  oled.setTextSize(2);
  
  //display table names
  int iter_counter = current_table - 1; //-1 is there as preceding table name is displayed
  byte cursor_pos=WIDTH_OF_TEXT_B*1.2;
  for(int i=0;i<3;i++){ // three rows are available for display
    if(iter_counter>=0 && iter_counter<NUM_OF_TABLES){ 
      oled.setCursor(WIDTH_OF_TEXT_B*1,cursor_pos);
      oled.println((TOP_STRUCT+iter_counter)->LK_TABLE->TABLE_NAME);
    }
    iter_counter++;
    cursor_pos+=WIDTH_OF_TEXT_B*1.1;
  }
}

void displayWeights(){
  oled.setTextSize(3);          // text size
  
  //set weights
  oled.setCursor(WIDTH_OF_TEXT_C, WIDTH_OF_TEXT_C/2);        // position to display
  char str[5];
  dtostrf(set_weight,5,2,str);
  if(set_weight<10){ //I have no idea why <10 wasn't working for sometime which caused it to display "00.00" for 10.00
    str[0]='0'; //padding zero
  }
  oled.println(str);
  
  //actual weight
  if(measured_weight<0){//for negative
    oled.setCursor(0, WIDTH_OF_TEXT_C*2);
    oled.println('-');
  }
  oled.setCursor(WIDTH_OF_TEXT_C, WIDTH_OF_TEXT_C*2);
  if(abs(measured_weight)<10){//for padding
    oled.println('0');
    oled.setCursor(WIDTH_OF_TEXT_C*2, WIDTH_OF_TEXT_C*2);
  }
  oled.println(abs(measured_weight));
}


void setWeightPage(){
  static const int OLED_BLINK_INTERVAL = 800; //ms
  static unsigned long oled_last_blink=0;
  
  //page heading
  oled.setTextSize(2);          // text size
  oled.setCursor(0,0);
  oled.println("SET WEIGHT"); 
  oled.setCursor(0,.6*WIDTH_OF_TEXT_B);
  oled.println("----------");

  //display weights
  oled.setTextSize(3);          // text size
  oled.setCursor(32-WIDTH_OF_TEXT_B, 2*WIDTH_OF_TEXT_B);        // position to display
  char str[5];
  dtostrf(set_weight,5,2,str);
  if(set_weight<10){
    str[0]='0'; //padding zero
  }
  //blinking using '_'
  if(edit_blink_state){ //replace whole or decimal part by "--" when blinking
    if(current_edit_state==Machine_State::EDIT_WHOLE){ 
      str[0]=str[1]='_';
    } else {
    str[3]=str[4]='_';
    }
  }
  oled.println(str);
  
  //changes blink interval -> vairable interval for ON and OFF states
  if((millis()-oled_last_blink) > OLED_BLINK_INTERVAL){
    edit_blink_state =!edit_blink_state;
    if(edit_blink_state){ 
      oled_last_blink = millis()-OLED_BLINK_INTERVAL*9/10; //lowering turn off time by reducing delay 
    }else{
      oled_last_blink = millis();
    }
  }
}

void encoderButtonWatcher(){
  if(encoder_button_update_flag){
    encoderPos=0;//flushes current value in buffer 
    turn_off_motors(); //turns off motor and servo
    //intialize when button is pressed
    switch(current_edit_state){
      case Machine_State::FIRST_PAGE:
        current_edit_state = Machine_State::EDIT_WHOLE;
        break;
      case Machine_State::EDIT_WHOLE :
        current_edit_state = Machine_State::EDIT_DECIMAL;
        acceleration_constant=5;
        break;
      case Machine_State::EDIT_DECIMAL:
        current_edit_state = Machine_State::LOOKUP_TABLE;
        acceleration_constant=10; //more acceleration for decimal part
        break;
      case Machine_State::LOOKUP_TABLE:
        current_edit_state = Machine_State::FIRST_PAGE;
        acceleration_constant=2;
        break;
      default:
        current_edit_state = Machine_State::FIRST_PAGE;
        break;
    }
    encoder_button_update_flag=false;
  }
}

void readUserWeightInput(){
  float temp_fractional_storage = set_weight - int(set_weight);// addtional calculations to prevent number from going below 0 and over 99.99
  if(current_edit_state== Machine_State::EDIT_WHOLE){ // whole number edit
    set_weight+=encoderPos;
    if(set_weight<0){
      set_weight=temp_fractional_storage; //if whole part is edited, lowest is fractional part
    }
    if(set_weight>99.99){
      set_weight=99+temp_fractional_storage; //if whole part is edited, highest is 99 + fractional
    }
  } else { //since the fuction is called when current_edit_state is either EDIT_WHOLE or EDIT_DECIMAL
    set_weight+= encoderPos/100; // adding fraction to weight
    if(set_weight>99.99){
      set_weight=99.99; //if fractional part is edited, highest is 99.99
    }
  }
  if(set_weight<0.01){ // this is done outside if else to avoid going to 0.00
    set_weight=0.01; //if fractional part is edited, lowest is 0.01
  }
  encoderPos=0;  
}

void encoderISR(){
  int multiplier = ((millis() - encoder_update_time) <50)? acceleration_constant:1; //acceleration
  if (digitalRead(ENCODE_PIN_B)){
    encoderPos+=multiplier;
  }else{
    encoderPos-=multiplier;
  }
  encoder_update_time=millis();
}

void encoderButtonISR(){
  if(millis() - encoder_button_update_time >250){
    encoder_button_update_flag =true;
    encoder_button_update_time = millis();
  }
}
