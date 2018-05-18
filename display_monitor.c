/* LC-3 Emulator
 *
 * Date: May 2018
 *
 * This a terminal-based program that emulates the low-level functions of the 16-bit LC-3
 * machine based a finite state machine (FSM) interpretation of its operations.
 */

#include <curses.h>
#include <menu.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include "slc3.h"
#include "display_monitor.h"

#define REG 0
#define MEM 1
#define CPU 2

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define CTRLD 4

#define REG_PANEL_WIDTH 36
#define REG_PANEL_HEIGHT 12
#define MEM_PANEL_WIDTH 30
#define MEM_PANEL_HEIGHT 22
#define CPU_PANEL_WIDTH 36
#define CPU_PANEL_HEIGHT 12
#define IO_PANEL_HEIGHT 5
#define HEIGHT_PADDING 2

#define NUM_CPU_ELEMENTS 10

static const char MSG_CPU_HALTED[] =        "CPU halted :*)";
static const char MSG_LOAD[] =              "1) Enter a program to load >> ";
static const char MSG_LOADED[] =            "1) Loaded %s";
static const char MSG_FILE_NOT_LOADED[] =   "1) File not found. Please try again.";
static const char MSG_STEP[] =              "3) Stepped";
static const char MSG_STEP_NO_FILE[] =      "3) No file loaded yet!";
static const char MSG_RUNNING_CODE[] =      "4) Running code";
static const char MSG_RUN_NO_FILE[] =       "4) No file loaded yet!";
static const char MSG_DISPLAY_MEM[] =       "5) Enter the hex address to jump to >> ";
static const char MSG_EDIT_MEM_PROMPT_ADDR[] = "6) Enter the hex address to edit >> ";
static const char MSG_EDIT_MEM_PROMPT_DATA[] = "6) Enter the hex data to push to %s >> ";
static const char MSG_EDIT_MEM_NO_FILE[] =  "6) No file loaded yet!";
static const char MSG_SET_UNSET_BRKPT[] =   "8) Enter the hex address to set/unset breakpoint >> ";
static const char MSG_SET_UNSET_BRKPT_NO_FILE[] = "8) No file loaded yet!";
static const char MSG_CPU_HALTED_STEP[] =   "3) Cannot step: CPU halted";
static const char MSG_CPU_HALTED_RUN[] =    "4) Cannot run: CPU halted";

typedef struct MenuString
{
    char label[31];
    char description[31];
} MenuString;

int init_display_monitor(CPU_p);
int free_display_monitor();
int display_monitor_destroy();
void print_message(const char *, char *);
void clear_message();
int update_display_monitor(CPU_p);
void clear_line(int line);
void print_title(WINDOW *, int, char *, chtype);
void draw_io_window(WINDOW *, char *);
void print_window_titles();
unsigned int get_mem_address(char *);
unsigned int get_mem_data(char *);

/** Ensure these are at least as big (or equal to) the actual vaues in the CPU. These have
 * +1 because in addition to the actual string representing data in the LC-3, a null
 * value must also be present to signify the end of the array. */
MenuString reg_strings[NUM_OF_REGISTERS + 1];
MenuString mem_strings[NUM_OF_MEM_BANKS + 1];
MenuString cpu_strings[NUM_CPU_ELEMENTS + 1];

WINDOW *menu_windows[3];
WINDOW *input_window, *output_window;
MENU *menus[3];
ITEM **menu_list_items[3];
int item_counts[3];
int saved_menu_index[3];
int active_window = MEM;
int c, i;
char output_console[36];
char output_console_ptr = 0;


/** Initializes the debug monitor with variables needed throughout the execution of the
 * LC-3 simulator. */
int display_monitor_init(CPU_p cpu)
{
    /* Initialize ncurses */
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_CYAN, COLOR_BLACK);

    for (i = 0; i < NUM_OF_MEM_BANKS; i++) {
        has_breakpoint[i] = false;
    }

    /* Stores the size of each array */
    item_counts[REG] = NUM_OF_REGISTERS;
    item_counts[MEM] = NUM_OF_MEM_BANKS;char display_mem_input[6];
    item_counts[CPU] = NUM_CPU_ELEMENTS;

    saved_menu_index[REG] = 0;
    saved_menu_index[MEM] = 0;
    saved_menu_index[CPU] = 0;

    menu_list_items[REG] = (ITEM **)calloc(item_counts[REG] + 1, sizeof(ITEM *));
    menu_list_items[MEM] = (ITEM **)calloc(item_counts[MEM] + 1, sizeof(ITEM *));
    menu_list_items[CPU] = (ITEM **)calloc(item_counts[CPU] + 1, sizeof(ITEM *));

    /* Create the window instances to be associated with the menus */
    menu_windows[REG] = newwin(REG_PANEL_HEIGHT, REG_PANEL_WIDTH, HEIGHT_PADDING, 4);
    menu_windows[MEM] = newwin(MEM_PANEL_HEIGHT, MEM_PANEL_WIDTH, HEIGHT_PADDING, CPU_PANEL_WIDTH + 8);
    menu_windows[CPU] = newwin(CPU_PANEL_HEIGHT, CPU_PANEL_WIDTH, REG_PANEL_HEIGHT + HEIGHT_PADDING, 4);

    input_window = newwin(IO_PANEL_HEIGHT, MEM_PANEL_WIDTH + REG_PANEL_WIDTH + 4, MEM_PANEL_HEIGHT + HEIGHT_PADDING + 3, 4);
    draw_io_window(input_window, "Input");
    output_window = newwin(IO_PANEL_HEIGHT, MEM_PANEL_WIDTH + REG_PANEL_WIDTH + 4, MEM_PANEL_HEIGHT + IO_PANEL_HEIGHT + HEIGHT_PADDING + 3, 4);
    draw_io_window(output_window, "Output");

    display_monitor_update(cpu);

    return 0;
}

/** Frees the memory associated with windows within the LC-3 debug monitor and prepares
 * the debug monitor for a refresh of displayed contents. */
int display_monitor_free()
{
    int j;
    for (i = 0; i < 3; i++)
    {
        /* Unpost and free all the memory taken up */
        unpost_menu(menus[i]);
        free_menu(menus[i]);
        for (j = 0; j < item_counts[i] + 1; ++j)
        {
            free_item(menu_list_items[i][j]);
        }
    }

    return 0;
}

/** Frees all memory associated with Ncurses and prepares the LC-3 simulator for a complete
 * shutdown. */
int display_monitor_destroy()
{
    display_monitor_free();
    for (i = 0; i < 3; i++)
    {
        delwin(menu_windows[i]);
    }
    return endwin();
}

/** Prints a message pertaining to a user operation. It could be a prompt if the user just
 * selected an operation, the outcome of an operation, or additional information about the
 * state of the LC-3 */
void print_message(const char *message, char *arg)
{
    clear_line(MEM_PANEL_HEIGHT + HEIGHT_PADDING + 1);
    /** Positions the message just below the memory window (the tallest window) with a
     * left padding of 4 */
    attron(COLOR_PAIR(2));
    mvprintw(MEM_PANEL_HEIGHT + HEIGHT_PADDING + 1, 4, message, arg);
    attroff(COLOR_PAIR(2));
    refresh();
}

/** Clears the message displayed just below the list of available user operations */
void clear_line(int line)
{
    move(line, 0);
    clrtoeol();
}

/** Prints output resulting from the LC-3 */
void display_monitor_print_output(char ch)
{
    if (ch != '\n')
    {
        output_console[output_console_ptr] = ch;
        output_console[++output_console_ptr] = '\0';
        /** Position the message */
        attron(COLOR_PAIR(2));
        mvprintw(MEM_PANEL_HEIGHT + IO_PANEL_HEIGHT * 2 + HEIGHT_PADDING, 6, "%s", output_console);
        attroff(COLOR_PAIR(2));
    }
    else
    {
        output_console_ptr = 0;
        output_console[output_console_ptr] = '\0';
        clear_line(MEM_PANEL_HEIGHT + HEIGHT_PADDING + 1);
        draw_io_window(output_window, "Output");
    }
}

char display_monitor_get_input()
{
    int prev_active_window = active_window;

    /* Draw all other windows as inactive, draw input as active */
    active_window = -1;
    print_window_titles();
    print_title(input_window, 0, "Input", COLOR_PAIR(2));
    wrefresh(input_window);

    mvprintw(MEM_PANEL_HEIGHT + IO_PANEL_HEIGHT + HEIGHT_PADDING, 6, ">> ");
    move(MEM_PANEL_HEIGHT + IO_PANEL_HEIGHT + HEIGHT_PADDING, 9);
    echo();
    char input_ch = getch();
    noecho();
    clear_line(MEM_PANEL_HEIGHT + IO_PANEL_HEIGHT + HEIGHT_PADDING);

    draw_io_window(input_window, "Input");
    active_window = prev_active_window;
    print_window_titles();
    return input_ch;
}

void print_window_titles()
{
    /** Print a border around the windows and print a title */
    print_title(menu_windows[REG], 1, "Registers", (active_window == REG) ? COLOR_PAIR(2) : COLOR_PAIR(1));
    mvwaddch(menu_windows[REG], 2, 0, ACS_LTEE);
    mvwhline(menu_windows[REG], 2, 1, ACS_HLINE, 38);
    /** mvwaddch(menu_windows[REG], 2, REG_PANEL_WIDTH - 1, ACS_RTEE);
        box(reg_menu_win, 0, 0); */

    print_title(menu_windows[MEM], 1, "Memory", (active_window == MEM) ? COLOR_PAIR(2) : COLOR_PAIR(1));
    mvwaddch(menu_windows[MEM], 2, 0, ACS_LTEE);
    mvwhline(menu_windows[MEM], 2, 1, ACS_HLINE, 38);
    /** mvwaddch(menu_windows[MEM], 2, MEM_PANEL_WIDTH - 1, ACS_RTEE);
        box(mem_menu_win, 0, 0); */

    print_title(menu_windows[CPU], 1, "CPU Registers", (active_window == CPU) ? COLOR_PAIR(2) : COLOR_PAIR(1));
    mvwaddch(menu_windows[CPU], 2, 0, ACS_LTEE);
    mvwhline(menu_windows[CPU], 2, 1, ACS_HLINE, 38);
    /** mvwaddch(menu_windows[CPU], 2, CPU_PANEL_WIDTH - 1, ACS_RTEE);
        box(cpu_menu_win, 0, 0); */

    /* Refresh the menus */
    for (i = 0; i < 3; i++)
    {
        wrefresh(menu_windows[i]);
    }

    refresh();
}

/** Prints the title of a window */
void print_title(WINDOW *win, int y, char *string, chtype color)
{
    if (win == NULL)
        win = stdscr;
    wattron(win, color);
    mvwprintw(win, y, 4, "%s", string);
    wattroff(win, color);
    refresh();
}

/** Draws the input and output windows with title. This method is called when the display
 *  monitor is first created, and every time input or output is erased from the screen */
void draw_io_window(WINDOW *window, char *title)
{
    box(window, 0, 0);
    print_title(window, 0, title, COLOR_PAIR(1));
    wrefresh(window);
}

void save_menu_indicies()
{
    for (i = 0; i < 3; i++)
    {
        ITEM *item = (ITEM *)current_item(menus[i]);
        if (item != NULL) {
            saved_menu_index[i] = item_index(item);
        } else {
            saved_menu_index[i] = 0;
        }
    }
}

void restore_menu_indicies()
{
    for (i = 0; i < 3; i++)
    {
        set_current_item(menus[i], menu_list_items[i][saved_menu_index[i]]);
    }
    refresh();
}

/** Updates the Ncurses window each time this function is called. The arrays containing
 *  menu data are all rebuilt, the menus are reinstantiated, positioned, and posted to the
 *  windows. */
void display_monitor_update(CPU_p cpu)
{
    save_menu_indicies();
    display_monitor_free();

    for (i = 0; i < item_counts[REG]; ++i)
    {
        sprintf(reg_strings[i].label, "R%d:", i);
        sprintf(reg_strings[i].description, "x%04X", cpu->registers[i]);
        menu_list_items[REG][i] = new_item(reg_strings[i].label, reg_strings[i].description);
    }
    menu_list_items[REG][i] = new_item((char *)NULL, (char *)NULL);

    /* Create the items for the memory */
    for (i = 0; i < item_counts[MEM]; ++i)
    {
        sprintf(mem_strings[i].label, "x%04X:", starting_address + i); /* So to start at x3000 */
        /* If this memory location has a breakpoint we will display a small square next to it. */
        if (has_breakpoint[i])
        {
            sprintf(mem_strings[i].description, "x%04X [x]", memory[i]);
        }
        else
        {
            sprintf(mem_strings[i].description, "x%04X    ", memory[i]);
        }
        
        menu_list_items[MEM][i] = new_item(mem_strings[i].label, mem_strings[i].description);
    }
    menu_list_items[MEM][i] = new_item((char *)NULL, (char *)NULL);

    /* Create items for the CPU */
    sprintf(cpu_strings[0].label, "PC:");
    sprintf(cpu_strings[0].description, "x%04X", cpu->pc);
    sprintf(cpu_strings[1].label, "IR:");
    sprintf(cpu_strings[1].description, "x%04X", cpu->ir);
    sprintf(cpu_strings[2].label, "ALU A:");
    sprintf(cpu_strings[2].description, "x%04X", cpu->alu_a);
    sprintf(cpu_strings[3].label, "ALU B:");
    sprintf(cpu_strings[3].description, "x%04X", cpu->alu_b);
    sprintf(cpu_strings[4].label, "MAR:");
    sprintf(cpu_strings[4].description, "x%04X", cpu->mar);
    sprintf(cpu_strings[5].label, "MDR:");
    sprintf(cpu_strings[5].description, "x%04X", cpu->mdr);
    sprintf(cpu_strings[6].label, "CC N:");
    sprintf(cpu_strings[6].description, "%d", cpu->ccN);
    /* Inserting a blank one so the CC stuff looks better */
    sprintf(cpu_strings[7].label, " ");
    sprintf(cpu_strings[7].description, " ");
    sprintf(cpu_strings[8].label, "CC Z:");
    sprintf(cpu_strings[8].description, "%d", cpu->ccZ);
    sprintf(cpu_strings[9].label, "CC P:");
    sprintf(cpu_strings[9].description, "%d", cpu->ccP);

    for (i = 0; i < NUM_CPU_ELEMENTS; i++)
    {
        menu_list_items[CPU][i] = new_item(cpu_strings[i].label, cpu_strings[i].description);
    }
    menu_list_items[CPU][i] = new_item((char *)NULL, (char *)NULL);

    /* Create menu instances from the lists */
    for (i = 0; i < 3; i++)
    {
        menus[i] = new_menu((ITEM **)menu_list_items[i]);
    }

    /* Set the initially active window */
    keypad(menu_windows[active_window], TRUE);

    /* Set menus to their windows */
    for (i = 0; i < 3; i++)
    {
        set_menu_win(menus[i], menu_windows[i]);
    }

    /* The the menu sub ??? and its format menu_format is number of rows, columns for the
     * list's visible contents and scrolls the rest of the list. */
    set_menu_sub(menus[REG], derwin(menu_windows[REG], REG_PANEL_HEIGHT - 4, REG_PANEL_WIDTH - 4, 3, 1));
    set_menu_format(menus[REG], REG_PANEL_HEIGHT - 4, 1);
    /* Memories... */
    set_menu_sub(menus[MEM], derwin(menu_windows[MEM], MEM_PANEL_HEIGHT - 4, MEM_PANEL_WIDTH - 4, 3, 1));
    set_menu_format(menus[MEM], MEM_PANEL_HEIGHT - 4, 1);
    /* CPU... */
    set_menu_sub(menus[CPU], derwin(menu_windows[CPU], CPU_PANEL_HEIGHT - 4, CPU_PANEL_WIDTH - 4, 3, 1));
    set_menu_format(menus[CPU], MEM_PANEL_HEIGHT - 4, 2);

    /* Set menu mark to the string " * " */
    for (i = 0; i < 3; i++)
    {
        set_menu_mark(menus[i], " * ");
    }

    print_window_titles();

    /* Post the menus */
    for (i = 0; i < 3; i++)
    {
        post_menu(menus[i]);
        wrefresh(menu_windows[i]);
    }

    /* Print labels on the window */
    attron(COLOR_PAIR(2));
    mvprintw(0, 4, "Welcome to the LC-3 Simulator Simulator!");
    mvprintw(MEM_PANEL_HEIGHT + 2, 4, "1) Load, 3) Step, 4) Run, 5) Show Mem, 6) Edit 8) Set Brkpt 9) Exit");
    mvprintw(LINES - 2, 0, "Use Tab (\\t) to switch active panels");
    mvprintw(LINES - 1, 0, "Arrow Keys to navigate (9 to Exit)");
    attroff(COLOR_PAIR(2));

    restore_menu_indicies();
}

/** The main logic loop for the debug monitor. Listens for user keystrokes and performs
 *  debugging operations */
int display_monitor_loop(CPU_p cpu)
{
    /* Set selected item in memory to be current PC */
    set_current_item(menus[MEM], menu_list_items[MEM][cpu->pc]);        
    display_monitor_update(cpu);
    
    /* Input array used for 5) Show Mem, 8) Set/Unset breakpoint */
    char mem_addr_input[6];
    char mem_data_input[6];
    unsigned short mem_addr_index;
    unsigned short mem_data;

    /** This variable is used to return information about the user's selection back to the
     *  LC-3 */
    char monitor_return = MONITOR_UPDATE;

    if (is_halted) {
        print_message(MSG_CPU_HALTED, NULL);
    }

    /* If the user selected 9) to quit this while loop will exit */
    while ((c = wgetch(menu_windows[active_window])) != 57)
    {
        switch (c)
        {
        case 9:
            /*　User pressed Tab to change active window */
            active_window++;
            active_window = active_window % 3;
            keypad(menu_windows[active_window], TRUE);
            print_window_titles();
            break;
        case 49:
            /* User selected 1) to load a file */
            print_message(MSG_LOAD, NULL);
            /** Move the cursor, turn on echo mode so the user can see their input
             *  then turn it back on after capturing file name input */
            move(MEM_PANEL_HEIGHT + HEIGHT_PADDING + 1, strlen(MSG_LOAD) + 4);
            echo();
            getnstr(input_file_name, 80);
            noecho();          
            monitor_return = MONITOR_LOAD;
            break;
        case 51:
            /* User selected 3) to step through code */
            if (!file_loaded)
            {
                print_message(MSG_STEP_NO_FILE, NULL);
                monitor_return = MONITOR_NO_RETURN;
            } else if (is_halted) {
                print_message(MSG_CPU_HALTED_STEP, NULL);
                monitor_return = MONITOR_NO_RETURN;
            } else {
                print_message(MSG_STEP, NULL);
                monitor_return = MONITOR_STEP;
            }
            break;
        case 52:
            /* User selected 4) to run code */
            if (!file_loaded)
            {
                print_message(MSG_RUN_NO_FILE, NULL);
                monitor_return = MONITOR_NO_RETURN;
            } else if (is_halted) {
                print_message(MSG_CPU_HALTED_RUN, NULL);
                monitor_return = MONITOR_NO_RETURN;
            } else {
                print_message(MSG_RUNNING_CODE, NULL);
                monitor_return = MONITOR_RUN;
            }
            break;
        case 53:
            /* User selected 5) to display a specific memory address */
            print_message(MSG_DISPLAY_MEM, NULL);
            /** Move the cursor, turn on echo mode so the user can see their input
             *  then turn it back on after capturing file name input */
            move(MEM_PANEL_HEIGHT + HEIGHT_PADDING + 1, strlen(MSG_DISPLAY_MEM) + 4);
            echo();
            getstr(mem_addr_input);
            noecho();
            mem_addr_index = get_mem_address(mem_addr_input);
            set_current_item(menus[MEM], menu_list_items[MEM][mem_addr_index]);     
            monitor_return = MONITOR_NO_RETURN;
            break;
        case 54:
            if (!file_loaded)
            {
                print_message(MSG_EDIT_MEM_NO_FILE, NULL);
                monitor_return = MONITOR_NO_RETURN;
            }
            /* User selected 6) to edit a memory location */
            print_message(MSG_EDIT_MEM_PROMPT_ADDR, NULL);
            /** Move the cursor, turn on echo mode so the user can see their input
             *  then turn it back on after capturing address */
            move(MEM_PANEL_HEIGHT + HEIGHT_PADDING + 1, strlen(MSG_EDIT_MEM_PROMPT_ADDR) + 4);
            echo();
            getstr(mem_addr_input);
            noecho();
            mem_addr_index = get_mem_address(mem_addr_input);
            print_message(MSG_EDIT_MEM_PROMPT_DATA, mem_addr_input);
            /** Move the cursor, turn on echo mode so the user can see their input
             *  then turn it back on after capturing the data */
            move(MEM_PANEL_HEIGHT + HEIGHT_PADDING + 1, strlen(MSG_EDIT_MEM_PROMPT_DATA) + strlen(mem_addr_input) + 4);
            echo();
            getstr(mem_data_input);
            noecho();
            mem_data = get_mem_data(mem_data_input);
            memory[mem_addr_index] = mem_data;
            monitor_return = MONITOR_UPDATE;
            break;            
        case 56:
            /* User selected 8) to set/unset a breakpoint */
            if (!file_loaded)
            {
                print_message(MSG_SET_UNSET_BRKPT_NO_FILE, NULL);
                monitor_return = MONITOR_NO_RETURN;
            }
            else 
            {
                print_message(MSG_SET_UNSET_BRKPT, NULL);
                /** Move the cursor, turn on echo mode so the user can see their input
                 *  then turn it back on after capturing file name input */
                move(MEM_PANEL_HEIGHT + HEIGHT_PADDING + 1, strlen(MSG_SET_UNSET_BRKPT) + 4);
                echo();
                getstr(mem_addr_input);
                noecho();
                mem_addr_index = get_mem_address(mem_addr_input);
                has_breakpoint[mem_addr_index] = !has_breakpoint[mem_addr_index];
                save_menu_indicies();
                display_monitor_update(cpu);
                restore_menu_indicies();
            }
            monitor_return = MONITOR_UPDATE;
            break;
        case KEY_DOWN:
            menu_driver(menus[active_window], REQ_DOWN_ITEM);
            monitor_return = MONITOR_NO_RETURN;
            break;
        case KEY_UP:
            menu_driver(menus[active_window], REQ_UP_ITEM);
            monitor_return = MONITOR_NO_RETURN;
            break;
        case KEY_LEFT:
            menu_driver(menus[active_window], REQ_LEFT_ITEM);
            monitor_return = MONITOR_NO_RETURN;
            break;
        case KEY_RIGHT:
            menu_driver(menus[active_window], REQ_RIGHT_ITEM);
            monitor_return = MONITOR_NO_RETURN;
            break;
        case KEY_NPAGE:
            menu_driver(menus[active_window], REQ_SCR_DPAGE);
            monitor_return = MONITOR_NO_RETURN;
            break;
        case KEY_PPAGE:
            menu_driver(menus[active_window], REQ_SCR_UPAGE);
            monitor_return = MONITOR_NO_RETURN;
            break;
        }

        /* Shows the last inputted character as Ncurses recognizes it */
        attron(COLOR_PAIR(1));
        mvprintw(LINES - 3, 0, "Debug: %d", c, c);
        attroff(COLOR_PAIR(1));

        refresh();

        if (monitor_return != MONITOR_NO_RETURN)
        {
            return monitor_return;
        }

    }

    monitor_return = MONITOR_QUIT;
    return monitor_return;
}

unsigned int get_mem_address(char *mem_string) {
    /* A little bit of edge case handling...
     * x3002 + strlen("x3002") - 4 = 3002
     * 3002 + strlen("3002") - 4 = 3002 */
    unsigned short address = strtol(mem_string + strlen(mem_string) - 4, NULL, 16);
    return translate_memory_address(address);
}

unsigned int get_mem_data(char *mem_string) {
    /* x3002 + strlen("x3002") - 4 = 3002
     * 3002 + strlen("3002") - 4 = 3002 */
    return strtol(mem_string + strlen(mem_string) - 4, NULL, 16);
}

/*
 * This function will allow the opening of files
 */
FILE *open_file(char *input_file_name)
{
    /* Attempt to open file. If file isn't found or otherwise null, allow user to press enter to
	return to main program of the menu. */
    FILE *input_file_pointer;
    input_file_pointer = fopen(input_file_name, "r");
    //input_file_pointer = fopen("hex/HW3.hex", "r");
    if (input_file_pointer == NULL)
    {        
        clear_line(MEM_PANEL_HEIGHT + HEIGHT_PADDING + 1);
        print_message(MSG_FILE_NOT_LOADED, NULL);
        file_loaded = 0;
    } else {
        clear_line(MEM_PANEL_HEIGHT + HEIGHT_PADDING + 1);
        print_message(MSG_LOADED, input_file_name);
        file_loaded = 1;
    }
    return input_file_pointer;
}