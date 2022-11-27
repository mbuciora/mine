#include <stdlib.h>	
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <ncurses.h>

#define powers_num 3				// liczba elektrowni
#define timer 1000000				// taktowanie zegara w sekundach

// ************************ Definicja struktur ***************************
typedef struct mine_t
{
	int capability;					// aktualne wydobycie
	int max_cap;					// maksymalne wydobycie
	int state;						// stan magazynu
	int size;						// rozmiar magazynu
	pthread_mutex_t state_mutex;	// blokada na magazynie
	pthread_t thread_id;			// wątek kopalni	DEFINIOWANIE ID WATKU
	pthread_cond_t state_cond;		// sygnał zmiany na magazynie    ZMIENNA WARUNKOWA
	int x;
	int y;
};

typedef struct power_t
{
	int lvl;						// poziom "rozpalenia"
	int consumption;				// aktualne zużycie
	int state;						// stan magazynu
	int size;						// rozmiar magazynu
	int order;						// zamówienie
	pthread_mutex_t state_mutex;
	pthread_t thread_id;
	pthread_cond_t state_cond;
	int x;
	int y;
};

typedef struct train_t
{
	int coach_num;					// czy zwolniony pociag
	int vol;						// zawartosc
	pthread_t thread_id;
	int x;
	int y;
};

typedef struct base_t
{
	int coach_num;
	int coach_vol;
	pthread_mutex_t coach_mutex;
	pthread_cond_t coach_cond;
	int x;
	int y;
};

// ****************** zmienne globalne **************************

	int height = 0;
	int width = 0;
	int run = 1;

	// kopalnia
	struct mine_t mine;

	// tablica elektrowni
	struct power_t powers[powers_num];

	// tablica lokomotyw
	struct train_t trains[powers_num];

	// baza zawierająca nieużywane wagony
	struct base_t base;

	int steps = 10;

	//INICJALIZACJA MUTEXOW
	pthread_mutex_t paint_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t paint_cond = PTHREAD_COND_INITIALIZER;
	bool changed = FALSE;

	pthread_t stop_thread;


// ****************** wątki *************************************
void* power_function(void* arg)
{
	int id = (int)arg;
	int i;

	// wartości początkowe
	powers[id].lvl = 5;
	powers[id].consumption = 1;
	powers[id].size = 100;
	powers[id].state = 10;
	powers[id].order = 2;
	pthread_mutex_init(&powers[id].state_mutex, NULL);
	pthread_cond_init(&powers[id].state_cond, NULL);
	powers[id].y = 4 + id * (height / powers_num) - 3;
	powers[id].x = width - 20;

	//printf("Utworzono elektrownie nr: %d\n", id);

	while(run)
	{
		pthread_mutex_lock(&powers[id].state_mutex);	// lock
			if(powers[id].state > powers[id].consumption)
			{
				powers[id].state -= powers[id].consumption;
				if ( powers[id].lvl < 100 )
					powers[id].lvl++;
			}
			else
			{
				powers[id].state = 0;
				if ( powers[id].lvl > 0 )
					powers[id].lvl--;
			}
			if(powers[id].state < 0.4 * powers[id].size)
			{
				powers[id].order = powers[id].size - powers[id].state;
				pthread_mutex_unlock(&powers[id].state_mutex);
				pthread_cond_broadcast(&powers[id].state_cond);
			}
			else
			{
				powers[id].order = 0;
				pthread_mutex_unlock(&powers[id].state_mutex);
			}
		powers[id].consumption = (powers[id].lvl/10 + 1) * 20;
		changed = true;
		pthread_cond_broadcast(&paint_cond);		//unlock
		//printf("Dane elektrowni nr %d:\n %d %d %d %d %d\n", id, powers[id].lvl, powers[id].consumption, powers[id].state,
		//				powers[id].size, powers[id].order);
		usleep(timer);
	}
	//pthread_cond_broadcast(&paint_cond);
	//pthread_cond_broadcast(&powers[id].state_cond);

	return NULL;
}

void* mine_function(void* arg)
{
	int i;
	// wartości początkowe
	mine.max_cap = 20;
	mine.capability = 30;
	mine.size = 400;
	mine.state = 0;
	pthread_mutex_init(&mine.state_mutex, NULL);
	pthread_cond_init(&mine.state_cond, NULL);
	mine.x = 3;
	mine.y = height / 2 - 3;
	//printf("Utworzono kopalnie\n");
	initscr();

	while(run)
	{
		pthread_mutex_lock(&mine.state_mutex);
			if ( mine.size > mine.state + mine.capability )
			{
				mine.state += mine.capability;
				if (mine.state < 0.5 * mine.size)
				{
					mine.capability = mine.max_cap;
				}
				else
				{
					mine.capability = mine.max_cap * 2 * ( 1.0 - (float)mine.state/mine.size );
				}
			}
			else
			{
				mine.state = mine.size;
				mine.capability = 0;
			}
		pthread_mutex_unlock(&mine.state_mutex);

		pthread_cond_broadcast(&mine.state_cond);

		changed = true;
		pthread_cond_broadcast(&paint_cond);

		//printf("Dane kopalni: %d %d %d\n", mine.capability, mine.state, mine.size);
		usleep(timer);
	}
	//pthread_cond_broadcast(&mine.state_cond);
	//pthread_cond_broadcast(&paint_cond);

	return NULL;
}

void* train_function(void* arg)
{
	int id = (int)arg;
	int i, n;
	trains[id].x = base.x + id;
	trains[id].y = base.y;

	//printf("Utworzono lokomotywę nr: %d\n", id);

	while(run)
	{
		pthread_mutex_lock(&powers[id].state_mutex);
		if( powers[id].order == 0 )
		{
			trains[id].x = base.x;
			trains[id].y = base.y;
			pthread_cond_wait(&powers[id].state_cond, &powers[id].state_mutex);
		}
		else
		{
			trains[id].vol = powers[id].order;
			pthread_mutex_unlock(&powers[id].state_mutex);
			n = ceil((float)trains[id].vol / base.coach_vol);

			trains[id].x = base.x;
			trains[id].y = base.y + 1;

			pthread_mutex_lock(&base.coach_mutex);
				do
				{
					if( n <= base.coach_num)
					{
						trains[id].x = base.x;
						trains[id].y = base.y + 2;
						base.coach_num -= n;
						trains[id].coach_num = n;
						changed = true;
						pthread_cond_broadcast(&paint_cond);
						break;
					}
					else
					{
						//printf(" oczekiwanie na wagony %d\n", id);
						pthread_cond_wait(&base.coach_cond, &base.coach_mutex);
					}
				}
				while(1);
			pthread_mutex_unlock(&base.coach_mutex);

			// pobranie węgla w kopalni
			pthread_mutex_lock(&mine.state_mutex);
				do
				{
					if( trains[id].vol <= mine.state)
					{
						trains[id].y++;
						mine.state -= trains[id].vol;
						// załadunek
						usleep(trains[id].coach_num * 100000);
						changed = true;
						pthread_cond_broadcast(&paint_cond);
						break;
					}
					else
					{
						//printf("oczekiwanie na węgiel %d\n", id);
						pthread_cond_wait(&mine.state_cond, &mine.state_mutex);
					}
				}
				while(1);
			pthread_mutex_unlock(&mine.state_mutex);

			//podróż
			while(trains[id].x != powers[id].x -1 || trains[id].y != powers[id].y)
			{
				if ( trains[id].x < powers[id].x -1)
					trains[id].x++;
				else if ( trains[id].x > powers[id].x -1)
					trains[id].x--;
				if ( trains[id].y < powers[id].y )
				{
					trains[id].y++;
				}
				else if ( trains[id].y > powers[id].y )
				{
					trains[id].y--;
				}
				changed = true;
				pthread_cond_broadcast(&paint_cond);
				usleep(50000);
			}

			// zdanie węgla do magazynu elektrowni
			pthread_mutex_lock(&powers[id].state_mutex);
				powers[id].state += trains[id].vol;
				trains[id].vol = 0;
				changed = true;
				pthread_cond_broadcast(&paint_cond);
				// rozładunek
				usleep(trains[id].coach_num * 100000);
			pthread_mutex_unlock(&powers[id].state_mutex);

			// powrót
			while(trains[id].x != base.x || trains[id].y != base.y)
			{
				if ( trains[id].x < base.x)
					trains[id].x++;
				else if ( trains[id].x > base.x)
					trains[id].x--;
				if ( trains[id].y < base.y )
				{
					trains[id].y++;
				}
				else if ( trains[id].y > base.y )
				{
					trains[id].y--;
				}
				changed = true;
				pthread_cond_broadcast(&paint_cond);
				usleep(50000);
			}
			//usleep(1000);

			// zwolnienie wagonów
			pthread_mutex_lock(&base.coach_mutex);
				base.coach_num += trains[id].coach_num;
				trains[id].coach_num = 0;
			pthread_mutex_unlock(&base.coach_mutex);
			//printf("zwrócenie wagonów aktualnie: %d\n", base.coach_num);
			pthread_cond_broadcast(&base.coach_cond);
			//break;
		}
	}
	//pthread_cond_broadcast(&paint_cond);
	//pthread_cond_broadcast(&base.coach_cond);

	return NULL;
}

void* stop_lisner(void* arg)
{
	char c;
	c = wgetch(stdscr);
	if ( c == 'q')
	{
		run = 0;
	}
	return NULL;
}
// ****************** Funkcje pomocnicze ************************
void initialization()
{
	int i, status;

	// utworzenie wątku kopalni
	pthread_create(&stop_thread, NULL, stop_lisner, NULL);
	status = pthread_create(&(mine.thread_id), NULL, mine_function, NULL);

	// utworzenie wątków elektrownii i lokomotyw
	for (i=0; i < powers_num; i++)
	{
		status = pthread_create(&(powers[i].thread_id), NULL, power_function, i);
		if ( status )
		{
			//printf("Błąd utworzenia elektrowni nr: %d\n", i);
		}
		status = pthread_create(&trains[i].thread_id, NULL, train_function, i);
		if ( status )
		{
			//printf("Błąd utworzenia lokomotywy nr: %d\n", i);
		}
	}
	// inicjalizacja bazy
	pthread_mutex_init(&base.coach_mutex, NULL);
	pthread_cond_init(&base.coach_cond, NULL);
	base.coach_num = 50;
	base.coach_vol = 10;
	base.x = mine.x + 20;
	base.y = mine.y - 2;

}

void end()
{
	int i, status;
	/* oczekiwanie na zakończenie wszystkich wątków */
		for (i=0; i < powers_num; i++)
		{
			status = pthread_join(trains[i].thread_id, NULL);
			if (status)
			{
				printf("Błąd zamknięcia lokomotywy\n");
			}
			else
				printf("zamknięcie lokomotywy %d\n", i );
			status = pthread_join(powers[i].thread_id, NULL);
			if (status)
			{
				printf("Błąd zamknięcia elektrowni\n");
			}
			else
				printf("zamknięcie eleketrowni %d\n", i );
			pthread_mutex_destroy(&powers[i].state_mutex);
			pthread_cond_destroy(&powers[i].state_cond);
		}
		status = pthread_join(mine.thread_id, NULL);
		if (status)
		{
			printf("Błąd zamknięcia kopalni\n");
		}else
			printf("zamknięcie kopalni\n" );
		pthread_join(stop_thread, NULL);			// oczekiwanie na zakończenie wątku (gdy poprawny zwraca 0)

		pthread_mutex_destroy(&paint_mutex);
		pthread_cond_destroy(&paint_cond);

		pthread_mutex_destroy(&mine.state_mutex);
		pthread_cond_destroy(&mine.state_cond);

		pthread_mutex_destroy(&base.coach_mutex);
		pthread_cond_destroy(&base.coach_cond);
}
//------------------------------------------------------------------------
int main()
{
	int i;
	char c;
	// *****************	initialization	*****************
	initscr();

	//Pobieranie wartości okna do zmiennych
	getmaxyx( stdscr, height, width ); //1
	initialization();


	// *************************** Główna część programu **************************

	//wyswietlanie tekstu
	while(run)
	{
		pthread_mutex_lock(&paint_mutex);
		do{
			if ( changed)
			{
				changed = FALSE;
				break;
			}
			else
				pthread_cond_wait(&paint_cond, &paint_mutex);
		}while(1);
		pthread_mutex_unlock(&paint_mutex);

		clear();
		mvprintw( mine.y, mine.x, "Kopalnia" );
		mvprintw( mine.y + 1, mine.x, "Wydajnosc: " );
		printw("%d", mine.capability);
		mvprintw( mine.y + 2, mine.x, "Stan: " );
		printw("%d", mine.state);

		for ( i = 0; i < powers_num; i++)
		{
			mvprintw( powers[i].y, powers[i].x, "Elektrownia" );
			mvprintw( powers[i].y + 1, powers[i].x, "Zuzycie: " );
			printw("%d", powers[i].consumption);
			mvprintw( powers[i].y + 2, powers[i].x, "Stan: " );
			printw("%d", powers[i].state);
			mvprintw( powers[i].y + 3, powers[i].x, "Poziom: " );
			printw("%d", powers[i].lvl);
			// rysowanie pociagów
			mvprintw( trains[i].y, trains[i].x - 6, "o[%d]o", i+1);
		}
		refresh();
	}


	// *************************** Zakończenie ************************************
	end();
	endwin();
	printf("Zakończenie aplikacji");
	return EXIT_SUCCESS;
}
