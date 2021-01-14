
#include <menu.h> // untuk enter diawal
#include <ncurses.h> // untuk tampilan
#include <stdio.h>
#include <string.h>
#include <unistd.h> 
#include <stdlib.h>
#include <pthread.h> // untuk thread
#include <time.h>   //untuk fungsi clock(),clock_t

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define CTRLD 	4

static const char word_dictionary[] = "word_dict.txt";
int game_state;
int score;
int buffer_pos;
int wait; // status tunggu word untuk ke kata berikutnya
int scoreChanged = 0; // status apakah score berubah
char local_input[15];
char local_buffer[15];

// still hardcode for timer
int hour = 0, minute = 0, second = 10, flag = 0; //global variables

char buffer[10][15]; //Buffer for WordGen Thread
FILE *read_ptr;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
WINDOW *gameWin, *timeWin, *scoreWin, *inputWin, *wordWin;
pthread_t t_game, t_wordGen, t_timer, t_word, t_score;

// pilihan untuk menu
char *choices[] = {
                        "Enter to Start Game",
                        (char *)NULL,
                  };

// prosedur membuat window baru
WINDOW *create_newwin(int height, int width, int starty, int startx);
// prosedur menghapus window jika dibutuhkan
void destroy_win(WINDOW *local_win);

// thread generator kata 
void* wordGen(void * arg);

// delay timer
void delay(int ms); //delay function
// countdown timer
int counter();
// thread countdown timer
void *countTimer(void *vargp);

// thread throw score to display
void *throwScore(void *vargp);


// menampilkan window kata , timer, dan score
void displayWord();
void displayTime();
void displayScore();

// TODO : prosedur proses input kata
// blueprint
void inputWord();

// print kata di tengah window
void print_on_middle(WINDOW *win, int starty, int startx, int width, char *string, chtype color);

// window screen game
void gameScreen(int height, int width, int starty, int startx);

// Main Program
int main()
{
	// keperluan untuk menu
   ITEM **my_items;
	
	strcpy(local_input, "");

	// penampung key yang ditekan
	int c;	

	// status tunggu true
	wait = 1;		
	
	// menu
	MENU *my_menu;

	// window menu 
   WINDOW *my_menu_win;
   
	// pilihan menu
	int n_choices, i;

	// baca data dari file "word_dictionary.txt"
   read_ptr = fopen(word_dictionary, "r");
	
	/* Initialize curses */
	initscr();
	start_color();
        cbreak();
        noecho();

	// key diperbolehkan
	keypad(stdscr, TRUE);

	// inisialisasi warna
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_BLUE, COLOR_BLACK);
	init_pair(3, COLOR_YELLOW, COLOR_BLACK);

	/* Create items */
        n_choices = ARRAY_SIZE(choices);
        my_items = (ITEM **)calloc(n_choices, sizeof(ITEM *));
        for(i = 0; i < n_choices; ++i)
         my_items[i] = new_item(choices[i], "");

	/* Create menu */
	   my_menu = new_menu((ITEM **)my_items);

	/* Create the window to be associated with the menu */
        my_menu_win = newwin(LINES-1, (COLS-3)/3 + 3, 1, 1);
        keypad(my_menu_win, TRUE);
     
	/* Set main window and sub window */
        set_menu_win(my_menu, my_menu_win);
        set_menu_sub(my_menu, derwin(my_menu_win, 4, 30, 3, 2));

	/* Set menu mark to the string " * " */
        set_menu_mark(my_menu, " >> ");

	/* Print a border around the main window and print a title */
   box(my_menu_win, 0, 0);
	print_on_middle(my_menu_win, 1, 0, (COLS-3)/3 + 3, "Fast Type Game", COLOR_PAIR(1));
	mvwaddch(my_menu_win, 2, 0, ACS_LTEE);
	mvwhline(my_menu_win, 2, 1, ACS_HLINE, (COLS-3)/3 + 1);
	mvwaddch(my_menu_win, 2, (COLS-3)/3 + 2, ACS_RTEE);
	mvprintw(LINES/2, 0, "  Notes : F2 to exit");
	refresh();
   
	/* Post the menu */
	post_menu(my_menu);
	wrefresh(my_menu_win);
	while((c = wgetch(my_menu_win)) != KEY_F(2) || game_state != 0)
	{       
		switch(c)
	   {	
			case KEY_DOWN:
				menu_driver(my_menu, REQ_DOWN_ITEM);
				break;
			case KEY_UP:
				menu_driver(my_menu, REQ_UP_ITEM);
				break;
			case 10:
			{
				// start game
				buffer_pos = 0;
            game_state = 1;
            score = 0;
				// draw gamescreen
				gameScreen(LINES-1,(COLS-3)/3 + 3, 1, 1);
				// throw timer word and score
				displayTime();
				displayWord();
				displayScore();
				while(game_state == 1){
					inputWord();
				}
				break;	
			}
		}
		wrefresh(my_menu_win);
		noecho();
	}	
		// wait until this thread finish running
		pthread_join(t_word, NULL);
		pthread_join(t_timer, NULL);
		pthread_join(t_score, NULL);
	
	// end game
	game_state = 0;
	/* Unpost and free all the memory taken up */
        unpost_menu(my_menu);
        free_menu(my_menu);
        for(i = 0; i < n_choices; ++i)
            free_item(my_items[i]);
	endwin();
}

void gameScreen(int height, int width, int starty, int startx){

	// init window
	gameWin = create_newwin(height, width, starty, startx);
	wordWin = create_newwin(6, COLS/5, 4,2);
	timeWin = create_newwin(3, COLS/8, 4,COLS/5+3);
	scoreWin = create_newwin(3, COLS/8, 7,COLS/5+3);
	inputWin = create_newwin(4, (COLS-3)/3, 10, 2);

	// receive input key
	keypad(gameWin, TRUE);

	// print header
	print_on_middle(gameWin, 1, 0, width, "Fast Type Game", COLOR_PAIR(1));
	mvwaddch(gameWin, 2, 0, ACS_LTEE);
	mvwhline(gameWin, 2, 1, ACS_HLINE, width-2);
	mvwaddch(gameWin, 2, width-1, ACS_RTEE);

	// refresh window of game
	wrefresh(gameWin);
}

void* wordGen(void * arg){
    char local_buffer[15];
    int get_word = 1;
    memset(local_buffer, 0, sizeof(local_buffer));
    if(fgets(local_buffer, sizeof(local_buffer), read_ptr) == NULL){
        rewind(read_ptr);
        fgets(local_buffer, sizeof(local_buffer), read_ptr);
    }

    while(game_state == 1){
        pthread_mutex_lock(&lock);
        if(buffer_pos < 10){
            memset(buffer[buffer_pos], 0, sizeof(buffer[buffer_pos]));
            memset(&local_buffer[(strlen(local_buffer)-3)], '\0', 1);
            strcpy(buffer[buffer_pos], local_buffer);
            buffer_pos += 1;
            pthread_mutex_unlock(&lock);
            get_word = 1;
        } else {
            pthread_mutex_unlock(&lock);
            get_word = 0;
            sleep(1);
        }

        if(get_word == 1){
            memset(local_buffer, 0, sizeof(local_buffer));
            if(fgets(local_buffer, sizeof(local_buffer), read_ptr) == NULL){
                rewind(read_ptr);
                fgets(local_buffer, sizeof(local_buffer), read_ptr);
            }
        }
    }
    return NULL;
}

// prosedur throw skor ke layar
void *throwWord(void  *vargp){
	while(game_state == 1){
		noecho();
		//Empty the buffer
		memset(local_buffer, 0, sizeof(local_buffer));

		pthread_mutex_lock(&lock);
			if(buffer_pos > 0){
				buffer_pos -= 1;
				strcpy(local_buffer,buffer[buffer_pos]);
			} else {
				strcpy(local_buffer, "layangan");
			}
		pthread_mutex_unlock(&lock);

		wattron(wordWin, COLOR_PAIR(3));
		mvwprintw(wordWin, 2, 10, "          ");
		mvwprintw(wordWin, 2, 10, "%s", local_buffer);
		wattroff(wordWin, COLOR_PAIR(3));

		pthread_mutex_lock(&lock);
		wrefresh(wordWin);
		pthread_mutex_unlock(&lock);

		if(wait == 1){
			delay(10000000);
		} else {
			wait = 1;
		}
	}
}

void displayWord(){	

	wattron(wordWin, COLOR_PAIR(1));
	mvwprintw(wordWin, 2, 3, "Word :");
	wattroff(wordWin, COLOR_PAIR(1));

	wrefresh(wordWin);
   pthread_create(&t_wordGen, NULL, wordGen, NULL);
	pthread_create(&t_word, NULL, throwWord, NULL);
}

void delay(int ms) //delay function
{
   clock_t timeDelay = ms*2 + clock(); //Step up the difference from clock delay
   while (timeDelay > clock()){
		if(scoreChanged){
			break;
		}
	} //stop when the clock is higher than time delay
	
}

int counter()
{
   while (game_state == 1)
   { 
		if(scoreChanged){
			second = 10;
			scoreChanged = 0;
		}
      if (minute < 0)
      { 
         minute = 59;
         --hour;
      }

      if (second < 0)
      { 
         second = 59;
         --minute;
      }

		mvwprintw(timeWin, 1, 11, "          ");
		mvwprintw(timeWin, 1, 11, "%d:%d:%d", hour, minute, second);

		pthread_mutex_lock(&lock);
		wrefresh(timeWin);
		pthread_mutex_unlock(&lock);
		
      delay(1000000);
      if (hour == 0 && minute == 0 && second == 0)
      {
		pthread_mutex_lock(&lock);
			if(strlen(local_input) == 0){
				game_state = 0;
			}else{
				second = 10;
			}
		pthread_mutex_unlock(&lock);
			// game_state = 0;
         // break;
      }else{
      	second -= 1;
		}
   }
}

void *countTimer(void *vargp)
{
   counter();
}

void displayTime()
{
	wattron(timeWin, COLOR_PAIR(2));
	mvwprintw(timeWin, 1, 3, "Time :  -:--:-");
	wattroff(timeWin, COLOR_PAIR(2));

	pthread_create(&t_timer, NULL, countTimer, NULL);

	wrefresh(timeWin);
}

void *throwScore(void *vargp){
	while (game_state == 1)
	{
		if(scoreChanged){
			wattron(scoreWin, COLOR_PAIR(1));
			mvwprintw(scoreWin, 1, 11, "   ");
			mvwprintw(scoreWin, 1, 11, "%d", score);
			wattroff(scoreWin, COLOR_PAIR(1));
			pthread_mutex_lock(&lock);
			wrefresh(scoreWin);
			pthread_mutex_unlock(&lock);
		}
	}
	
}
void displayScore(){

	wattron(scoreWin, COLOR_PAIR(1));
	mvwprintw(scoreWin, 1, 3, "Score : 0");
	wattroff(scoreWin, COLOR_PAIR(1));

	pthread_create(&t_score, NULL, throwScore, NULL);

	wrefresh(scoreWin);
}

void inputWord()
{
	mvwprintw(inputWin, 1, 2, "Your Type : ");
	wrefresh(inputWin);

	int flag, x = 14;
	if(game_state == 1){
		echo();
		wgetstr(inputWin, local_input);
		clrtoeol();
		noecho();

		if(!strcmp(local_input, local_buffer)){
         score += 100;
			scoreChanged = 1;
			wait = 0;
		} else {
         pthread_mutex_lock(&lock);
			game_state = 0;
         pthread_mutex_unlock(&lock);
		}	
	}
}

WINDOW *create_newwin(int height, int width, int starty, int startx)
{	
	WINDOW *local_win;

	local_win = newwin(height, width, starty, startx);
	box(local_win, 0 , 0);		/* 0, 0 gives default characters 
					 					* for the vertical and horizontal
					 					* lines			*/
	wrefresh(local_win);		/* Show that box 		*/

	return local_win;
}

void destroy_win(WINDOW *local_win)
{	
	wborder(local_win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
	wrefresh(local_win);
	delwin(local_win);
}

void print_on_middle(WINDOW *win, int starty, int startx, int width, char *string, chtype color)
{	
	int length, x, y;
	float temp;

	if(win == NULL)
		win = stdscr;

	getyx(win, y, x);

	if(startx != 0)
		x = startx;
	if(starty != 0)
		y = starty;
	if(width == 0)
		width = COLS/2-2;

	length = strlen(string);
	temp = (width - length)/ 2;
	x = startx + (int)temp;

	wattron(win, color);
	mvwprintw(win, y, x, "%s", string);
	wattroff(win, color);

	refresh();
}