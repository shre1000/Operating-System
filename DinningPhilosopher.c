
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include "msecond.h"
#include "random_int.h"

#define NUM_PHILOSOPHERS	5	
#define MEAN_THINK_TIME		1000	
#define MEAN_EAT_TIME		750	


float total_time_spent_waiting	= 0.0;
int   total_number_of_meals	= 0;

/*--------------------------------------------------------------------------*/


#define semaphore_create(s,v)	sem_init( &s, 0, v )
#define semaphore_wait(s)	sem_wait( &s )
#define semaphore_signal(s)	sem_post( &s )
#define semaphore_release(s)	sem_destroy( &s )
typedef sem_t semaphore;


semaphore chopstick[NUM_PHILOSOPHERS];
semaphore screen;
semaphore mutex;


int  screen_row[NUM_PHILOSOPHERS] = {  6,  2,  2,  6, 10 };
int  screen_col[NUM_PHILOSOPHERS] = { 31, 36, 44, 49, 40 };
int  chopstick_row[5] = {  9,  4,  3,  4,  9 }; 
int  chopstick_col[5] = { 35, 33, 40, 47, 45 };
char chopstick_sym[5] = { '/', '\\', '|', '/', '\\' };


#define cls()			printf( "\033[H\033[J" )
#define position(row,col)	printf( "\033[%d;%dH", (row), (col) )
#define position_flush(row,col)	printf( "\033[%d;%dH\n", (row), (col) )

void init_screen( void )

{
    int i;

    cls();
    position( 6, 37 );
    printf( "\\     /" );
    position( 7, 38 );
    printf( "\\___/" );

    
    for ( i = 0; i < NUM_PHILOSOPHERS; i++ )
    {
	position( screen_row[i], screen_col[i] );
	printf( "%d", i );
	position( screen_row[i] + 1, screen_col[i] - 1 );
	printf( "(%d)", 0 );
	position( chopstick_row[i], chopstick_col[i] );
	printf( "%c", chopstick_sym[i] );
    }
    position_flush( 13, 1 );
}

void draw_thinking( int n )

{
    semaphore_wait( screen );
    position( screen_row[n], screen_col[n] );
    printf( "\033[33mT\033[0m" );
    position_flush( 13, 1 );
    semaphore_signal( screen );
}

void draw_hungry( int n )
{
    semaphore_wait( screen );
    position( screen_row[n], screen_col[n] );
    printf( "\033[34mH\033[0m" );
    position_flush( 13, 1 );
    semaphore_signal( screen );
}

void draw_eating( int n, int eat_count )
{
    semaphore_wait( screen );
    position( screen_row[n], screen_col[n] );
    printf( "\033[31mE\033[0m" );
    position( screen_row[n] + 1, screen_col[n] - 1 );
    printf( "(%d)", eat_count );
    position_flush( 13, 1 );
    semaphore_signal( screen );
}

void draw_chopstick_up( int n )
{
    semaphore_wait( screen );
    position( chopstick_row[n], chopstick_col[n] );
    printf( "%c", ' ' );
    position_flush( 13, 1 );
    semaphore_signal( screen );
}

void draw_chopstick_down( int n )
{
    semaphore_wait( screen );
    position( chopstick_row[n], chopstick_col[n] );
    printf( "\033[32m%c\033[0m", chopstick_sym[n] );
    position_flush( 13, 1 );
    semaphore_signal( screen );
}

void draw_done( int n )
{
    semaphore_wait( screen );
    position( screen_row[n], screen_col[n] );
    printf( "D" );
    position_flush( 13, 1 );
    semaphore_signal( screen );
}

/*--------------------------------------------------------------------------*/

void obtain_chopsticks( int n )

{
    if ( n % 2 == 0 ) {
	/* Even number: Left, then right */
	semaphore_wait( chopstick[(n+1) % NUM_PHILOSOPHERS] );
	draw_chopstick_up( (n+1) % NUM_PHILOSOPHERS );
	semaphore_wait( chopstick[n] );
	draw_chopstick_up( n );
    } else {
	/* Odd number: Right, then left */
	semaphore_wait( chopstick[n] );
	draw_chopstick_up( n );
	semaphore_wait( chopstick[(n+1) % NUM_PHILOSOPHERS] );
	draw_chopstick_up( (n+1) % NUM_PHILOSOPHERS );
    }
}

void release_chopsticks( int n )

{
    draw_chopstick_down( n );
    semaphore_signal( chopstick[n] );
    draw_chopstick_down( (n+1) % NUM_PHILOSOPHERS );
    semaphore_signal( chopstick[(n+1) % NUM_PHILOSOPHERS] );
}

/*--------------------------------------------------------------------------*/

void philosopher( int *philosopher_data )

{
    int start_time;
    int eat_count = 0;
    int total_hungry_time = 0;
    int became_hungry_time;

    int n = philosopher_data[0];
    int duration = philosopher_data[1];

    
    start_time = msecond();

    while( msecond() - start_time < duration * 1000 )
    {
	/* Hungry */

	became_hungry_time = msecond();
	draw_hungry( n );
        obtain_chopsticks( n );
	
	/* Eating */

	total_hungry_time += ( msecond() - became_hungry_time );
	eat_count++;
	draw_eating( n, eat_count );
	usleep( 1000L * random_int( MEAN_EAT_TIME ) );	/* microseconds */
	release_chopsticks( n );

	/* Think */

	draw_thinking( n );
	usleep( 1000L * random_int( MEAN_THINK_TIME ) );/* microseconds */
    }

    /* Done */
    
    draw_done( n );

    /* Update the shared variable database */

    semaphore_wait( mutex );
    total_number_of_meals += eat_count;
    total_time_spent_waiting += ( total_hungry_time / 1000.0 );
    semaphore_signal( mutex );
    
    pthread_exit( NULL );
}

/*==========================================================================*/

int main( int argc, char *argv[] )
{
    pthread_t phil[NUM_PHILOSOPHERS];
    int philosopher_data[NUM_PHILOSOPHERS][2];
    int duration;
    int i;

    duration = ( argc > 1 ? atoi( argv[1] ) : 10 );


    for ( i = 0; i < NUM_PHILOSOPHERS; i++ )
    {
	if ( semaphore_create( chopstick[i], 1 ) < 0 ) {
	    fprintf( stderr, "cannot create chopstick semaphore\n" );
	    exit( 1 );
	}
    }

    if ( semaphore_create( screen, 1 ) < 0 ) {
	fprintf( stderr, "cannot create screen semaphore\n" );
	exit( 1 );
    }

    if ( semaphore_create( mutex, 1 ) < 0 ) {
	fprintf( stderr, "cannot create mutex semaphore\n" );
	exit( 1 );
    }

 
    init_screen();
    for ( i = 0; i < NUM_PHILOSOPHERS; i++ )
    {
	philosopher_data[i][0] = i;
	philosopher_data[i][1] = duration;
	if ( pthread_create( &phil[i], NULL, (void *(*)(void *)) &philosopher,
			     &philosopher_data[i] ) != 0 ) {
	    fprintf( stderr, "cannot create thread for philosopher %d\n", i );
	    exit( 1 );
	}
    }

  
    for ( i = 0; i < NUM_PHILOSOPHERS; i++ )
    {
	pthread_join( phil[i], NULL );
    }

  
    for ( i = 0; i < NUM_PHILOSOPHERS; i++ )
    {
	semaphore_release( chopstick[i] );
    }
    semaphore_release( screen );
    semaphore_release( mutex );

    position( 13, 1 );    
    printf( "Total meals served = %d\n", total_number_of_meals );
    printf( "Average hungry time = %f\n",
	total_time_spent_waiting / total_number_of_meals );

    return 0;
}
