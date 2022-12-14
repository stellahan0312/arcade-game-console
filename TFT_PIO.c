#include <stdio.h> //The standard C library
#include <stdlib.h> //C stdlib
#include "pico/stdlib.h" //Standard library for Pico
#include <math.h> //The standard math library
#include "hardware/gpio.h" //The hardware GPIO library
#include "pico/time.h" //The pico time library
#include "pico/multicore.h" //The pico multicore library
#include "hardware/irq.h" //The hardware interrupt library
#include "hardware/pwm.h" //The hardware PWM library
#include "hardware/pio.h" //The hardware PIO library
#include "TFTMaster.h" //The TFT Master library
#include "pt_cornell_rp2040_v1.h"


#define MENU 0;
#define PONG 1;
#define SNAKE 2;
#define MEMORY 3;

static struct pt_sem pong_animation; 
static struct pt_sem snake_animation; 
static struct pt_sem memory_animation; 
static struct pt_sem menu_animation; 

// game variables to keep track what game is running
bool pong = false; 
bool snake = false; 
bool memory = false; 
bool menu = false;
bool back_to_pong = false; 
volatile int state = 0;

//buffer to convert int to string
volatile char buffer[256];

// Menu variables 
volatile int counter = 0; 
// Button debounce
unsigned volatile long debounce = 0; 
// pong variables
volatile int lives = 2; 
//volatile int pong_counter = 0; 
int score; 

//screen center position
int center_X = ILI9340_TFTWIDTH/2; 
int center_Y = ILI9340_TFTHEIGHT/2; 

// barrier x and y values
int left_line = 25; 
int right_line = 215;
int up_line = 30;
int down_line = 290;

bool collision = false; 

//Decalare paddle variable and default x value, width, height, and velocity
int default_paddle_x = 95;
int paddle_x; 
int paddle_y;
int paddle_w = 40;
int paddle_h = 6; 
int paddle_vx = 1;

// Pong --------------------------------------------------------------------------------------------------------------------------//
//Creating a struct Ball Object
struct Ball{
	double x; 
	double y; 
	double vx; 
	double vy; 
};

typedef struct Ball Ball; 

Ball ball; 

void generatePaddle(x, y){
  tft_fillRect(x, y, paddle_w, paddle_h, ILI9340_WHITE);
}

//Generate the ball in the beginning of the game
void generateBall(Ball* ball){
	ball->x = default_paddle_x; 
	ball->y = 35;
	ball->vx = 1; 
	ball->vy = 1; 
  tft_fillCircle(ball->x, ball->y, 4, ILI9340_WHITE);

  //Draw the paddle
  paddle_x = center_X - 25; 
  paddle_y = down_line-8;
  generatePaddle(paddle_x, paddle_y);
}

void clearPaddle(x){
  tft_fillRect(x, paddle_y, paddle_w, paddle_h, ILI9340_BLACK);
}

void clearBall(x,y){
  tft_fillCircle(x, y, 4, ILI9340_BLACK);
}

// Updating the ball struct in every frame and updating the paddle whenever it is needed 
void updateBall(Ball* ball){
  //Storing the previous ball position, velocity, and size
  int prev_ball_x = ball->x; 
  int prev_ball_y = ball->y; 
  int prev_ball_vx = ball->vx; 
  int prev_ball_vy = ball->vy; 
  int radius = 4; 
  //Storing the previous score and paddle x position since the paddle only need to update the x axis
  int old_score = score;
  int prev_paddle_x = paddle_x; 

  // check whether state change from menu to pong
  if(back_to_pong){
    generatePaddle(paddle_x, paddle_y); 
    back_to_pong = false; 
  }
  
  //If the joystick is set to the left or right side it would change the paddle x value by +/- (right/left)
  if(!gpio_get(BUTTON_LEFT)){
    paddle_x -= paddle_vx; 
    if(paddle_x < left_line){ //checking if the paddle is exceeding the game borders
      paddle_x = left_line + 1; 
    }
    //redraw the paddle in that frame
    clearPaddle(prev_paddle_x);
    generatePaddle(paddle_x, paddle_y);
  }
  else if(!gpio_get(BUTTON_RIGHT)){
    paddle_x += paddle_vx; 
    if(paddle_x + paddle_w > right_line){ //checking if the paddle is exceeding the game borders
      paddle_x = right_line - paddle_w; 
    }
    //redraw the paddle in that frame
    clearPaddle(prev_paddle_x);
    generatePaddle(paddle_x, paddle_y);
  }
  
  // resetting ball velocity when it is moving too slow
  if(abs(ball->vx) < 1){
    ball->vx = 1; 
  }

  // updating ball position
  ball->x = prev_ball_x + prev_ball_vx; 
  ball->y = prev_ball_y + prev_ball_vy; 

  //checking it the ball hits the barrier
  //ball hitting left border
  if(ball->x - radius < left_line){
    ball->x = left_line + radius + 4; //+radius
    ball->vx = -prev_ball_vx; 
    collision = true; 
  }
  //ball hitting right border
  if(ball->x + radius > right_line){
    ball->x = right_line - radius - 3; 
    ball->vx = -prev_ball_vx; 
    collision = true; 
  }
  //ball hitting up border
  if(ball->y - radius < up_line){
    ball->y = up_line + radius + 4; // +radius
    ball->vy = -prev_ball_vy; 
    ball->vx = prev_ball_vx;  
  }
  //detecting that the ball hit the paddle, would set the velocity to the opposite direction and increase score
  if((ball->y + radius >= paddle_y) && (ball->x - radius > paddle_x) && (ball->x + radius < paddle_x + paddle_w)){
    ball->vy = -prev_ball_vy; 
    ball->vx = -prev_ball_vx;
    ball->y = paddle_y - 5 - radius; //
    ball->vx += (ball->x - (paddle_x + paddle_w/2))/10; 
    score += 10; 
    //update the score on the screen
    tft_setCursor(35,0);
    tft_fillRect(35, 0, 15, 25, ILI9340_BLACK);
    tft_setTextColor(ILI9340_WHITE);
    tft_writeString(itoa(score, buffer, 10));
  }
  //detecting that the ball miss the paddle and reset the ball and paddle position and lose a live 
  if(ball->y + radius > down_line){
    ball->x = center_X; 
    ball->y = 35; 
    ball->vx = 1;
    ball->vy = 1; 
    //redraw the paddle ! 
    clearPaddle(prev_paddle_x);
    paddle_x = default_paddle_x; 
    //score = 0; 
    generatePaddle(default_paddle_x, paddle_y);
    //clear the lives 
    if(lives == 2){
      tft_fillTriangle(234, 310, 230, 316, 238, 316, ILI9340_BLACK);
    }
    else if(lives == 1){
      tft_fillTriangle(224, 310, 220, 316, 228, 316, ILI9340_BLACK);
    }
    else{ // lose the game when losing all the lives 
      tft_fillTriangle(214, 310, 210, 316, 218, 316, ILI9340_BLACK);
      pong = false; 
      //asking player what to do 
      tft_setTextSize(1);
      tft_setTextColor(ILI9340_WHITE);
      tft_setCursor(95,150);
      tft_writeString("CONTINUE");
      tft_setCursor(75,165);
      tft_writeString("GO BACK TO MAIN");
    }
    lives--; 

  }

  //Clear ball and redraw ball
  clearBall(prev_ball_x, prev_ball_y); 
  tft_fillCircle(ball->x, ball->y, radius, ILI9340_WHITE);
  //Redraw the boundaries
  tft_drawRect(left_line, up_line, 191, 260, ILI9340_WHITE);
}

//reset all the variables for pong game 
void pong_restart_from_menu(){
  tft_fillScreen(ILI9340_BLACK); 
  pong = false; 
  lives = 2; 
  score = 0; 
  back_to_pong = true; 
}

//Main function of Pong game that continuously running until lose or changes state 
void run_pong(){
  // PT_SEM_WAIT(pt, &pong_animation);
  while(1){
    //ball gets updated every frame
    if(pong){
      updateBall(&ball); 
      sleep_us(3000);
    }
    else{ // restart the game 
      if(gpio_get(BUTTON1)){
        tft_fillScreen(ILI9340_BLACK); 
        //pong = true; 
        lives = 2; 
        score = 0; 
        // pong_barrier();
        state = PONG;
        break;
      } // going back to menu; need to clear screen and reset the pong game state variables 
      else if(gpio_get(BUTTON2)){
        pong_restart_from_menu();
        state = MENU;
        break;
      }
  }
  }
}

// SNAKE --------------------------------------------------------------------------------------------------------------------

//Defining the macros to indicate the direction of the snake facing 
#define UP 0 
#define DOWN 1
#define LEFT 2
#define RIGHT 3

//create a struct call Snake with x,y, and pointer to the next node elements 
struct Snake{
	short x; 
	short y; 
  struct Snake *next; // next node of the snake (body)
};

typedef struct Snake Snake; 

Snake *head = NULL; // head of the snake 
Snake *tail = NULL; // tail of the snake 

short apple[2]; // the apple that snake has to chase for (x,y axis)
volatile char dir = UP; // snake will start upward 
char alive = 1; 
volatile char reset = 1; 
volatile char back_to_menu = 0; 
volatile int snake_score; 

int randomGenerator(int min, int max){ // to generate the random apple (x,y) position on the screen 
  return(rand() % (max - min)) + min; 
}

// does the graphic when the game is over 
void dead(){
  alive = 0;
  tft_fillRect(head->x, head->y, 8, 8, ILI9340_BLUE);
  tft_setTextColor(ILI9340_WHITE); 
  tft_setTextSize(2); 
  tft_setCursor(center_X-80, center_Y-40);
  tft_writeString("You Scored:"); 
  tft_setTextColor(ILI9340_RED);
  tft_writeString(itoa(snake_score, buffer, 10));
  tft_setCursor(center_X-70, center_Y);
  tft_setTextColor(ILI9340_WHITE);
  tft_setTextSize(1);
  tft_writeString("Move Joystick to Replay!");
  tft_setCursor(35, 200); 
  tft_writeString("Press Bottom Button for Menu");
}


void food(){
  //generate apple and check whether the snake is at the same poistion as apple
  //if overlaps, regenerate apple 
  generate: 
    tft_fillCircle(apple[0], apple[1], 4, ILI9340_BLACK);
    apple[0] = randomGenerator(5, ILI9340_TFTWIDTH-5); //x coordinate from 5 to 235
    apple[1] = randomGenerator(5, ILI9340_TFTHEIGHT-5); //y coordinate from 5 to 315 

    //set the current pointer to the head
    Snake *current = head; 
    current = current -> next; 

    while(current != NULL){
      //when the positions overlapt with the new generated apple 
      int z = sqrt(pow((current->x + 4 - apple[0]),2)+pow((current->y + 4 - apple[1]),2));
      if(z < 9){
        goto generate; //make the apple again 
      }
      current = current->next; 
    }

    tft_fillCircle(apple[0], apple[1], 4, ILI9340_RED); // generate a red circle in the random axis  
}

//Increasing the snake's length by looping through the snake and changing the tail and head position. 
void expand(short x, short y){
  if(head == NULL){
    head = (Snake *)malloc(sizeof(Snake)); 
    head->x = x; 
    head->y = y; 
    head->next = NULL;
    tft_fillRect(x, y, 8, 8, ILI9340_BLUE); 
  }
  else{ 
    Snake *current = head; 
    while(current->next != NULL){
      current = current->next; 
    }
    current->next = (Snake *)malloc(sizeof(Snake)); 
    current->next->x = x; 
    current->next->y = y; 
    current->next->next = NULL;
    tft_fillRect(x, y, 8, 8, ILI9340_GREEN); // increase by 8 pixels on the display screen 
  }
}


void move(short x, short y){ //Function to move the snake such that the new coordinates of the head are x, y
  tft_fillRect(x, y, 8, 8, ILI9340_BLUE);
  Snake *current = head; 
  if(current->next != NULL){
    tft_fillRect(current->next->x, current->next->y, 8, 8, ILI9340_GREEN); 
  }
  while(current != NULL){
    if(current->next == NULL){
      tft_fillRect(current->x, current->y, 8, 8, ILI9340_BLACK); //clear the previous snake positions 
    }
    swap(current->x, x);
    swap(current->y, y);
    current = current->next; 
    if(current != NULL){
      if(current->x == head->x && current->y == head->y){ //snake ate itself 
        dead(); 
        break;  
      }
    }
  }
}

//check if the snake consumed the apple while it is moving 
void ate_the_apple(short x, short y){
  snake_score += 10; 
  tft_fillRect(x, y, 8, 8, ILI9340_BLUE);
  Snake *current = head; 
  while(current->next != NULL){
    tft_fillRect(current->x, current->y, 8, 8, ILI9340_GREEN); 
    swap(current->x, x);
    swap(current->y, y);
    current = current -> next; 
  }
  current -> next = (Snake *)malloc(sizeof(Snake)); //setting the size of the snake 
  tft_fillRect(current->x, current->y, 8, 8, ILI9340_GREEN);
  //changing the x and y coordinates with previous value and current values 
  swap(current->x, x);
  swap(current->y, y);
  current = current -> next; 
  current->x = x; 
  current->y = y; 
  current->next = NULL;  
}

// snake moving up when up direction is pressed 
void moveUp(){
  //distance from snake head to apple 
  int z = sqrt(pow((head->x + 4 - apple[0]),2)+pow((head->y + 4 - apple[1]),2));
  if(z < 8){//overlapped distance 
    ate_the_apple(head->x, head->y); 
    food(); 
  }
  else{
    //check if snake hit the screen border 
    if(head->y <= 0){
      dead(); 
    }
    else{ //update the snake y position for 8 pixels 
      move(head->x, head->y - 8); 
    }
  }
}

// snake moving down when down direction is pressed 
void moveDown(){
  int z = sqrt(pow((head->x + 4 - apple[0]),2)+pow((head->y + 4 - apple[1]),2));
  if(z < 8){
    ate_the_apple(head->x, head->y+1); 
    food(); 
  }
  else{
    //check if snake hit the screen border 
    if(head->y >= ILI9340_TFTHEIGHT-8){
      dead();
    }
    else{ //update the snake y position for 8 pixels 
      move(head->x, head->y + 8); 
    }
  }
}


void moveLeft(){
  int z = sqrt(pow((head->x + 4 - apple[0]),2)+pow((head->y + 4 - apple[1]),2));
  if(z < 8){
    ate_the_apple(head->x-1, head->y); 
    food(); 
  }
  else{
    if(head->x <= 2){
      dead();
    }
    else{
      move(head->x-8, head->y); //update the snake x position for 8 pixels
    }
  }
}


void moveRight(){
  int z = sqrt(pow((head->x + 4 - apple[0]),2)+pow((head->y + 4 - apple[1]),2));
  if(z < 8){
    ate_the_apple(head->x+1, head->y); 
    food(); 
  }
  else{
    if(head->x >= ILI9340_TFTWIDTH-8){
      dead();
    }
    else{
      move(head->x+8, head->y); //update the snake y position for 8 pixels
    }
  }
}


void changeDir(uint gpio, uint32_t events) { //The change direction interrupt handler
    if(alive){ //If the snake is alive
        switch(gpio){ //Switch based on the GPIO
            case BUTTON_UP: dir = (dir == DOWN ? DOWN : UP); //If the command is to go UP and the snake isn't going DOWN, go UP
                        break;
            case BUTTON_DOWN: dir = (dir == UP ? UP : DOWN); //If the command is to go DOWN and the snake isn't going UP, go DOWN
                        break;
            case BUTTON_LEFT: dir = (dir == RIGHT ? RIGHT : LEFT); //If the command is to go LEFT and the snake isn't going RIGHT, go LEFT
                        break;
            case BUTTON_RIGHT: dir = (dir == LEFT ? LEFT : RIGHT); //If the command is to go RIGHT and the snake isn't going LEFT, go RIGHT
                        break;
            default: break;
        }
    }
    else{ //If the snake is dead
        reset = 1; //Set the reset flag
        snake_score = 0;
    }
}


void snake_restart_from_menu(){
  tft_fillScreen(ILI9340_BLACK);
  snake = false; 
  reset = 1; 
  score = 0; 
}


//Main function for running the snake game
void run_snake(){
  srand(time_us_32()); //generate the random seed as the time 
  gpio_set_irq_enabled_with_callback(BUTTON_UP, GPIO_IRQ_EDGE_FALL, true, &changeDir); //Attach the callback to the specified GPIO
  gpio_set_irq_enabled_with_callback(BUTTON_DOWN, GPIO_IRQ_EDGE_FALL, true, &changeDir); //Attach the callback to the specified GPIO
  gpio_set_irq_enabled_with_callback(BUTTON_LEFT, GPIO_IRQ_EDGE_FALL, true, &changeDir); //Attach the callback to the specified GPIO
  gpio_set_irq_enabled_with_callback(BUTTON_RIGHT, GPIO_IRQ_EDGE_FALL, true, &changeDir); //Attach the callback to the specified GPIO
  while(1){
    //create the snake object 
    if(reset){
      tft_fillScreen(ILI9340_BLACK); 
      alive = 1; 
      Snake *tmp; 
      while(head != NULL){
        tmp = head; 
        head= head->next; 
        free(tmp); 
      }
      //generate length of 5 
      for(int i = 0; i < 5; i++){
        expand((ILI9340_TFTWIDTH/2)-i, ILI9340_TFTHEIGHT/2); 
      }
      //set the direction and generate a random apple
      dir = RIGHT; 
      food(); 
      reset = 0; 
    }
    //continue moving when snake is still alive 
    while(alive){
      switch(dir){
        case UP: moveUp(); 
                break; 
        case DOWN: moveDown(); 
                break; 
        case LEFT: moveLeft(); 
                break; 
        case RIGHT: moveRight(); 
                break; 
        default: break; 
      }
      //update it every 75 ms
      sleep_ms(75);
    }
    //restart the game 
    if(gpio_get(BUTTON1)){
      reset = 1; 
      score = 0; 
      state = SNAKE; 
      break;
    }
    //return back to menu 
    else if(gpio_get(BUTTON2)){
      snake_restart_from_menu();
      state = MENU;
      break;
    }
  }
}



// Memory game ----------------------------------------------------------------------------------------------------

//Defining the macros to indicate the arrows 
#define UP_ARROW 1 
#define DOWN_ARROW 2
#define LEFT_ARROW 3
#define RIGHT_ARROW 4

char start = 0; //Begin the game until it pressed a button 

//create an array of 50 for computer and player (big enough for the game)
volatile int computer[50]; 
volatile int player[50]; 
bool computerTurn = true; 
unsigned int clicked = 0; //player index 
unsigned int level = 1; 
char clicked_dir = NULL; 
volatile int arrow = 0; //default value for arrow so it won't draw anything
bool allCorrect = true;

int wait_time = 750; 

//randomly display the arrows that generated by the system
void computerDisplay(char computerInput){
  tft_fillScreen(ILI9340_BLACK);
  tft_setCursor(center_X - 40, center_Y);
  if(computerTurn){
    if(computerInput == 1){ //up
      tft_fillRect(center_X-18, center_Y-25, 38, 60, ILI9340_YELLOW);
      tft_fillTriangle(center_X+40, center_Y-25, center_X-40, center_Y-25, center_X, center_Y-70, ILI9340_YELLOW);
      
    }
    if(computerInput == 2){ //down
      tft_fillRect(center_X-18, center_Y-25, 38, 60, ILI9340_CYAN);
      tft_fillTriangle(center_X+40, center_Y+35, center_X-40, center_Y+35, center_X, center_Y+80, ILI9340_CYAN);
    }
    if(computerInput == 3){ //left
      tft_fillTriangle(center_X-35, center_Y-30, center_X-80, center_Y+5, center_X-35, center_Y+40, ILI9340_MAGENTA);
      tft_fillRect(center_X-34, center_Y-12, 60, 38, ILI9340_MAGENTA);
    }
    if(computerInput == 4){ //right 
      tft_fillTriangle(center_X+35, center_Y-30, center_X+80, center_Y+5, center_X+35, center_Y+40, ILI9340_GREEN);
      tft_fillRect(center_X-24, center_Y-12, 60, 38, ILI9340_GREEN);
    }
  }
}


//setting the direction that the player moved to on the joystick and set click_dir and arrow to the value
void checkPlayer(uint gpio, uint32_t events){
   if(!computerTurn){ //If the snake is alive
      switch(gpio){ //Switch based on the GPIO
        case BUTTON_UP: clicked_dir = UP_ARROW; //If the command is to go UP and the snake isn't going DOWN, go UP
                        arrow = 1; 
                    break;
        case BUTTON_DOWN: clicked_dir = DOWN_ARROW; //If the command is to go DOWN and the snake isn't going UP, go DOWN
                          arrow = 2; 
                    break;
        case BUTTON_LEFT: clicked_dir = LEFT_ARROW; //If the command is to go LEFT and the snake isn't going RIGHT, go LEFT
                          arrow = 3; 
                    break;
        case BUTTON_RIGHT:  clicked_dir = RIGHT_ARROW; //If the command is to go RIGHT and the snake isn't going LEFT, go RIGHT
                            arrow = 4; 
                    break;
        default: break;
      }
      player[clicked] = arrow; //update player array index as the input it gets 
    }
}

//display the player's input onto the screen when it moved the joystick 
void display_Player(){
  switch(clicked_dir){
    case UP_ARROW:  tft_fillRect(center_X-18, center_Y-25, 40, 60, ILI9340_YELLOW);
                    tft_fillTriangle(center_X+40, center_Y-25, center_X-40, center_Y-25, center_X, center_Y-70, ILI9340_YELLOW);
                  break;
    case DOWN_ARROW:  tft_fillRect(center_X-18, center_Y-25, 40, 60, ILI9340_CYAN);
                      tft_fillTriangle(center_X+40, center_Y+35, center_X-40, center_Y+35, center_X, center_Y+80, ILI9340_CYAN);
                  break;
    case LEFT_ARROW:  tft_fillTriangle(center_X-35, center_Y-30, center_X-80, center_Y+5, center_X-35, center_Y+40, ILI9340_MAGENTA);
                      tft_fillRect(center_X-34, center_Y-12, 60, 38, ILI9340_MAGENTA);
                  break;
    case RIGHT_ARROW: tft_fillTriangle(center_X+35, center_Y-30, center_X+80, center_Y+5, center_X+35, center_Y+40, ILI9340_GREEN);
                      tft_fillRect(center_X-24, center_Y-12, 60, 38, ILI9340_GREEN);
                  break;
    default: break; 
  }
}

//create graphic when player could not completely memorize the sequence
void levelFail(int level){
  tft_setTextSize(3);
  tft_setTextColor(ILI9340_RED);
  tft_setCursor(60,70);
  tft_writeString("LEVEL ");
  tft_writeString(itoa(level, buffer, 10));
  tft_setCursor(68,150);
  tft_setTextColor(ILI9340_YELLOW);
  tft_writeString("FAILED");
  tft_setCursor(center_X-80, center_Y + 40); 
  tft_setTextSize(1); 
  tft_setTextColor(ILI9340_WHITE);
  tft_writeString("Press Top Button to Restart"); 
  tft_setCursor(25, center_Y + 60); 
  tft_writeString("Press Bottom Button Back to Menu"); 
  //}
}

//Create graphic when player cleared the current level 
void levelClear(int level){
  tft_setTextSize(3);
  tft_setTextColor(ILI9340_RED);
  tft_setCursor(60,70);
  tft_writeString("LEVEL ");
  tft_writeString(itoa(level, buffer, 10));
  tft_setCursor(70,150);
  tft_setTextColor(ILI9340_GREEN);
  tft_writeString("CLEAR!");
  tft_setTextColor(ILI9340_WHITE);
  tft_setTextSize(1);
  tft_setCursor(35,205);
  tft_writeString("Press");
  tft_setTextColor(ILI9340_BLUE);
  tft_writeString(" Blue ");
  tft_setTextColor(ILI9340_WHITE);
  tft_writeString("Button to Move On");
  tft_setCursor(25, 225); 
  tft_writeString("Press Bottom Button Back to Menu");
}

void memory_restart_from_menu(){
  tft_fillScreen(ILI9340_BLACK); 
  memory = false; 
  level = 1;
  allCorrect = true;
  computerTurn = true;
}

//Main function running the memory game 
void run_memory(){
  srand(time_us_32()); //randomly each time 
  gpio_set_irq_enabled_with_callback(BUTTON_UP, GPIO_IRQ_EDGE_FALL, true, &checkPlayer); //Attach the callback to the specified GPIO
  gpio_set_irq_enabled_with_callback(BUTTON_DOWN, GPIO_IRQ_EDGE_FALL, true, &checkPlayer); //Attach the callback to the specified GPIO
  gpio_set_irq_enabled_with_callback(BUTTON_LEFT, GPIO_IRQ_EDGE_FALL, true, &checkPlayer); //Attach the callback to the specified GPIO
  gpio_set_irq_enabled_with_callback(BUTTON_RIGHT, GPIO_IRQ_EDGE_FALL, true, &checkPlayer); //Attach the callback to the specified GPIO

  //starting graphic of the game 
  tft_setCursor(center_X - 96, center_Y - 55); 
  tft_setTextSize(3);
  tft_setTextColor(ILI9340_RED); 
  tft_writeString("M");
  tft_setTextColor(ILI9340_MAGENTA);
  tft_writeString("e");
  tft_setTextColor(ILI9340_GREEN);
  tft_writeString("m");
  tft_setTextColor(ILI9340_YELLOW);
  tft_writeString("o");
  tft_setTextColor(ILI9340_CYAN);
  tft_writeString("r");
  tft_setTextColor(ILI9340_RED);
  tft_writeString("y");
  tft_setTextColor(ILI9340_WHITE);
  tft_writeString(" Game");
  tft_setTextSize(2);
  tft_setCursor(center_X - 92, center_Y-5); 
  tft_writeString("Test Your Brain!"); 
  tft_setCursor(center_X-83, center_Y+25);
  tft_setTextSize(1);
  tft_setTextColor(ILI9340_WHITE);
  tft_writeString("Memorize Sequences of Arrows"); 
  tft_setTextSize(1);
  tft_setCursor(center_X - 77, center_Y+50); 
  tft_setTextColor(ILI9340_WHITE); 
  tft_writeString("Press ");
  tft_setTextColor(ILI9340_BLUE);
  tft_writeString("Blue");
  tft_setTextSize(1);
  tft_setTextColor(ILI9340_WHITE);
  tft_writeString(" Button to Start");

  //won't start until the arcade button is pressed 
  while(start == 0){
    if(!gpio_get(BUTTON_SELECT)){
      start = 1; 
    }
  }

  //keep running until the player loses the game or restart or go back to menu 
  while(1){
    //generate computer arrows for memory 
    if(start){
      if(computerTurn){
        for (int i = 0; i < level; i++){
          computer[i] = randomGenerator(1,5); //4 arrows will be drawn 
          } 
      }
      //generate the number of arrows based on the level 
      for(int i = 0; i < level; i++){ 
        computerDisplay(computer[i]); 
        sleep_ms(wait_time); 
      }

      //telling the player that it is their turn 
      computerTurn = false; 
      tft_fillScreen(ILI9340_BLACK);
      tft_setCursor(center_X-20, center_Y-60);
      tft_setTextSize(10);
      tft_setTextColor(ILI9340_RED);
      tft_writeString("?");
      tft_setTextSize(2); 
      tft_setTextColor(ILI9340_WHITE);
      tft_setCursor(center_X-55, center_Y+40);
      tft_writeString("Your Turn!");
      start = 0;
      
      //check if the player pressed the number of arrows for the level 
      while(clicked < level){
        if(!gpio_get(BUTTON_UP) || !gpio_get(BUTTON_DOWN) || !gpio_get(BUTTON_LEFT) || !gpio_get(BUTTON_RIGHT)){
          tft_fillScreen(ILI9340_BLACK);
          display_Player();
          if(player[clicked] != computer[clicked]){//check if the arrows matches with the computer based on the current index 
            allCorrect = false;
            break; 
          }
          clicked_dir = NULL;
          clicked++; 
          sleep_ms(250);
          
        }
      }
      tft_fillScreen(ILI9340_BLACK);
    }
    //check if player clear the level or failed the level 
    if (allCorrect){
      levelClear(level);
      for (int i = 0; i < level; i++){ //reset all the values in the array and all the game variables 
        computer[i] = 0;
        player[i] = 0;
      }
      clicked = 0; 
      clicked_dir = NULL; 
      if (!gpio_get(BUTTON_SELECT)){ //next level and update level 
        allCorrect = true;
        start = 1; 
        level++; 
        computerTurn = true; 
        tft_fillScreen(ILI9340_BLACK);
      }
      else if (gpio_get(BUTTON2)){ //back to menu 
        memory_restart_from_menu();
        state = MENU;
        break;
      }
    }
    else {
      levelFail(level); // fail and reset all the variables 
      start = 0; 
      for (int i = 0; i < level; i++){
        computer[i] = 0;
        player[i] = 0;
      }
      clicked = 0; 
      clicked_dir = NULL; 
      if (gpio_get(BUTTON1)){ //restart the game 
        start = 0;
        level = 1;
        allCorrect = true;
        computerTurn = true;
        tft_fillScreen(ILI9340_BLACK);
        state = MEMORY;
        break;
      }
      else if (gpio_get(BUTTON2)){ //go back to menu 
        memory_restart_from_menu();
        state = MENU;
        break;
      }
    }
  }
}





//First time calling pong 
void pong_func(){
  tft_setTextSize(1);
  tft_setTextColor(ILI9340_WHITE);
  tft_setCursor(0,0);
  tft_writeString("Score:");
  tft_writeString(itoa(score, buffer, 10));
  tft_setCursor(174, 310);
  tft_writeString("Lives:"); 
  tft_fillTriangle(214, 310, 210, 316, 218, 316, ILI9340_RED);
  tft_fillTriangle(224, 310, 220, 316, 228, 316, ILI9340_RED);
  tft_fillTriangle(234, 310, 230, 316, 238, 316, ILI9340_RED);
  //Making the Pong Barriers 
  tft_drawRect(left_line, up_line, 191, 260, ILI9340_WHITE);
  generateBall(&ball); 
  run_pong();
}

//Main function for the menu 
void menu_loop(){
  menu = true; 
  pong = false; 
  snake = false; 
  memory = false; 
  
  //deciding what game the player want to select 
  while(menu){
    tft_setCursor(center_X-87, center_Y-85); 
    tft_setTextSize(3); 
    tft_setTextColor(ILI9340_CYAN);
    tft_writeString("Z"); 
    //tft_setCursor(center_X-62, center_Y-100);
    tft_setTextColor(ILI9340_WHITE);
    tft_writeString("&");
    tft_setTextColor(ILI9340_BLUE);
    tft_writeString("S"); 
    //tft_setCursor(center_X-50, center_Y-100);
    tft_setTextColor(ILI9340_WHITE);
    tft_writeString(" Arcade"); 
    tft_setTextSize(2);
    tft_setTextColor(ILI9340_WHITE);

    tft_setCursor(center_X-28, center_Y);
    tft_writeString("Pong"); 
    tft_fillRect(center_X+21, center_Y+13, 10, 3, ILI9340_WHITE);
    tft_fillCircle(center_X+25, center_Y+7, 2, ILI9340_WHITE);

    tft_setCursor(center_X-30, center_Y+30);
    tft_writeString("Snake");

    tft_setCursor(center_X-35, center_Y+60);
    tft_writeString("Memory"); 

  if(counter%3==0){ // display the selection is on pong right now 
    tft_fillRect(center_X-67, center_Y+65, 15, 7, ILI9340_BLACK);
    tft_fillTriangle(center_X-53, center_Y+74, center_X-43, center_Y+68, center_X-53, center_Y+62, ILI9340_BLACK);
    tft_fillRect(center_X-64, center_Y+35, 15, 7, ILI9340_BLACK);
    tft_fillTriangle(center_X-50, center_Y+44, center_X-40, center_Y+38, center_X-50, center_Y+32, ILI9340_BLACK);
    tft_fillRect(center_X-61, center_Y+4, 15, 7, ILI9340_CYAN);
    tft_fillTriangle(center_X-47, center_Y+13, center_X-37, center_Y+7, center_X-47, center_Y+1, ILI9340_CYAN);

    if(!gpio_get(BUTTON_SELECT)){ //set the game state 
      state = PONG;
      break;
    }
  }
  if(counter%3==1){// display the selection is on snake right now 
    tft_fillRect(center_X-61, center_Y+4, 15, 7, ILI9340_BLACK);
    tft_fillTriangle(center_X-47, center_Y+13, center_X-37, center_Y+7, center_X-47, center_Y+1, ILI9340_BLACK);
    tft_fillRect(center_X-67, center_Y+65, 15, 7, ILI9340_BLACK);
    tft_fillTriangle(center_X-53, center_Y+74, center_X-43, center_Y+68, center_X-53, center_Y+62, ILI9340_BLACK);
    tft_fillRect(center_X-64, center_Y+35, 15, 7, ILI9340_CYAN);
    tft_fillTriangle(center_X-50, center_Y+44, center_X-40, center_Y+38, center_X-50, center_Y+32, ILI9340_CYAN);

    if(!gpio_get(BUTTON_SELECT)){ //set the game state
      state = SNAKE;
      break;
    }
  }
  if(counter%3==2){// display the selection is on memory right now 
    tft_fillRect(center_X-64, center_Y+35, 15, 7, ILI9340_BLACK);
    tft_fillTriangle(center_X-50, center_Y+44, center_X-35, center_Y+38, center_X-50, center_Y+32, ILI9340_BLACK);
    tft_fillRect(center_X-61, center_Y+4, 15, 7, ILI9340_BLACK);
    tft_fillTriangle(center_X-47, center_Y+13, center_X-37, center_Y+7, center_X-47, center_Y+1, ILI9340_BLACK);
    tft_fillRect(center_X-67, center_Y+65, 15, 7, ILI9340_CYAN);
    tft_fillTriangle(center_X-53, center_Y+74, center_X-43, center_Y+68, center_X-53, center_Y+62, ILI9340_CYAN);

    if(!gpio_get(BUTTON_SELECT)){//set the game state
      state = MEMORY;
      break;
    }
  }
  sleep_ms(250);
  }
}


//main thread that does the state machine to indicate the which main function to run depending on the state it is in 
static PT_THREAD(protothread_main(struct pt *pt)){
  PT_BEGIN(pt);
  while (1){
    switch (state) {
      case 0: //menu state 
        menu_loop();
        break;

      case 1: //pong state 
        tft_fillScreen(ILI9340_BLACK);
        pong = true;
        menu = false;
        pong_func();
        break;

      case 2: //snake state 
        tft_fillScreen(ILI9340_BLACK);
        snake = true; 
        menu = false; 
        run_snake();
        break;
      
      case 3: //memory state 
        tft_fillScreen(ILI9340_BLACK);
        memory = true; 
        menu = false;
        run_memory();
        break;
      
      default:
        break;
    }
  }
  PT_END(pt);
}


//core 1
void core1_main(){
  pt_add_thread(protothread_main);
  pt_schedule_start;
}

//gpio interrupt for game selection on the menu state 
void gpio_callback(){
  if(menu && gpio_get(!BUTTON_DOWN)){
    if(time_us_64() < debounce){
      return;
    }
    counter++;
    debounce = time_us_64() + 300000;
  }
}

int main(){
  stdio_init_all(); //Initialize all of the present standard stdio types that are linked into the binary
  tft_init_hw(); //Initialize the hardware for the TFT
  tft_begin(); //Initialize the TFT
  tft_setRotation(0); //Set TFT rotation
  tft_fillScreen(ILI9340_BLACK); //Fill the entire screen with black colour

  gpio_set_irq_enabled_with_callback(BUTTON_DOWN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

  //Launch core 1 that does all the animation
  multicore_reset_core1();
  multicore_launch_core1(&core1_main);
  pt_schedule_start;
}