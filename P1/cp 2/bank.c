/*
AUTORES: Carolina Grille  carolina.grille@udc.es
Yago Iglesias santiago.iglesias4@udc.es
GRUPO: 2.3
EJ 2
*/
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"

#define MAX_AMOUNT 20

struct bank {
	int num_accounts;        // number of accounts
	int *accounts;           // balance array
	pthread_mutex_t *mutex;  //ARRAY MUTEX (CADA CUENTA TIENE UNO)

};

struct d_args {
	int		thread_num;         // application defined thread #
	int		delay;			        // delay between operations
	int		iterations;         // number of operations
	int     net_total;        // total amount deposited by this thread
	struct bank *bank;        // pointer to the bank (shared with other threads)
};

struct tr_args {
	int		thread_num;       
	int		delay;			 
	int		iterations;       
	struct bank *bank;        
};

struct d_thread_info {
	pthread_t    id;        // id returned by pthread_create()
	struct d_args *args;      // pointer to the arguments

};

struct tr_thread_info {
	pthread_t    		 id;        
	struct tr_args *args;     
};

// Threads run on this function
void *deposit(void *ptr)
{
	struct d_args *args =  ptr;
	int amount, account, balance;

	while(args->iterations--) {
		amount  = rand() % MAX_AMOUNT;
		account = rand() % args->bank->num_accounts;


		printf("Thread %d depositing %d on account %d\n",
			args->thread_num, amount, account);
			
		//SECCION CRITICA
		pthread_mutex_lock(&args->bank->mutex[account]);

		balance = args->bank->accounts[account];
		if(args->delay) usleep(args->delay); // Force a context switch

		balance += amount;
		if(args->delay) usleep(args->delay);

		args->bank->accounts[account] = balance;
		if(args->delay) usleep(args->delay);

		args->net_total += amount;
		pthread_mutex_unlock(&args->bank->mutex[account]);
		//finaliza seccion critica
	}
	return NULL;
}


//HACER TRANSFERENCIAS
void *transfer(void *ptr){
	
	struct tr_args *args =  ptr;
	
	int transfer_amount, origin_account, destination_account, origin_balance, destination_balance;

	while(args->iterations--) {		
		
		do{
			origin_account = rand() % args->bank->num_accounts;
			destination_account = rand() % args->bank->num_accounts;
		}while(origin_account==destination_account);

		if(origin_account < destination_account){
			pthread_mutex_lock(&(args->bank->mutex[origin_account]));
			pthread_mutex_lock(&(args->bank->mutex[destination_account]));
		}else{		
			pthread_mutex_lock(&(args->bank->mutex[destination_account]));
			pthread_mutex_lock(&(args->bank->mutex[origin_account]));		
		}	
		
		if(args->bank->accounts[origin_account] != 0)
			transfer_amount  = rand() % args->bank->accounts[origin_account];
		else
			transfer_amount = 0;
		
		printf("Transfer thread %d transfering %d from account %d to account %d\n",
			args->thread_num, transfer_amount, origin_account, destination_account);
			
		origin_balance = args->bank->accounts[origin_account];
		destination_balance = args->bank->accounts[destination_account];
		if(args->delay) usleep(args->delay); // Force a context switch

		origin_balance -= transfer_amount;
		destination_balance += transfer_amount;
		if(args->delay) usleep(args->delay);

		args->bank->accounts[origin_account] = origin_balance;
		args->bank->accounts[destination_account] = destination_balance;
		if(args->delay) usleep(args->delay);
		
		pthread_mutex_unlock(&(args->bank->mutex[origin_account]));
		pthread_mutex_unlock(&(args->bank->mutex[destination_account]));
		
	}
	return NULL;
}



// start opt.num_threads threads running on deposit.
struct d_thread_info *start_threads(struct options opt, struct bank *bank)
{
	int i;
	struct d_thread_info *threads;

	printf("creating %d threads\n", opt.num_threads * 2);
	threads = malloc(sizeof(struct d_thread_info) * (opt.num_threads * 2));

	if (threads == NULL) {
		printf("Not enough memory\n");
		exit(1);
	}

	// Create num_thread threads running deposit()
    for (i = 0; i < opt.num_threads; i++) {
        threads[i].args = malloc(sizeof(struct d_args));

        threads[i].args -> thread_num = i;
        threads[i].args -> net_total  = 0;
        threads[i].args -> bank       = bank;
        threads[i].args -> delay      = opt.delay;
        threads[i].args -> iterations = opt.iterations;

        if (0 != pthread_create(&threads[i].id, NULL, deposit, threads[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }

    return threads;
}

struct tr_thread_info *start_transfer_threads(struct options opt, struct bank *bank){
	int i;
	struct tr_thread_info *threads;

	printf("creating %d transfer_threads\n", opt.num_threads);
	threads = malloc(sizeof(struct tr_thread_info) * opt.num_threads);

	if (threads == NULL) {
		printf("Not enough memory\n");
		exit(1);
	}

	// Creamos num_thread threads running transfer()
	for (i = 0; i < opt.num_threads; i++) {
		threads[i].args = malloc(sizeof(struct tr_args));

		threads[i].args -> thread_num = i;
		threads[i].args -> bank       = bank;
		threads[i].args -> delay      = opt.delay;
		threads[i].args -> iterations = opt.iterations;

		if (0 != pthread_create(&threads[i].id, NULL, transfer, threads[i].args)) {
			printf("Could not create thread #%d", i);
			exit(1);
		}
	}

	return threads;
}

// Print the final balances of accounts and threads
void print_balances(struct bank *bank, struct d_thread_info *thrs, int num_threads) {
	int total_deposits=0, bank_total=0;
	printf("\nNet deposits by thread\n");

	for(int i=0; i < num_threads; i++) {
		printf("%d: %d\n", i, thrs[i].args->net_total);
		total_deposits += thrs[i].args->net_total;
	}
	printf("Total: %d\n", total_deposits);

    printf("\nAccount balance\n");
	for(int i=0; i < bank->num_accounts; i++) {
		printf("%d: %d\n", i, bank->accounts[i]);
		bank_total += bank->accounts[i];
	}
	printf("Total: %d\n", bank_total);

}


void wait_d_threads(struct options opt, struct bank *bank, struct d_thread_info *threads) {
	// Wait for the threads to finish
	for (int i = 0; i < opt.num_threads; i++)
		pthread_join(threads[i].id, NULL);
}


void wait_tr_threads(struct options opt, struct bank *bank, struct tr_thread_info *threads) {
	// Wait for the threads to finish
	for (int i = 0; i < opt.num_threads; i++)
		pthread_join(threads[i].id, NULL);
}

// Reservamos memoria, inicializamos las cuentas a 0 e inicializamos los mutexes de las cuentas
void init_accounts(struct bank *bank, int num_accounts) {
	bank->num_accounts = num_accounts;
	bank->accounts     = malloc(bank->num_accounts * sizeof(int)); //Reserva de memoria para las cuentas
	bank->mutex     = malloc(bank->num_accounts * sizeof(pthread_mutex_t)); //Reserva de memoria para los mutexes

	for(int i=0; i < bank->num_accounts; i++){
		bank->accounts[i] = 0;
		pthread_mutex_init(&(bank->mutex[i]),NULL); //Inicialización de los mutexes
	}
}


void acc_free(struct bank *bank, int num_accounts) {
	int i=0;
	while(bank->num_accounts){
		pthread_mutex_destroy(&(bank->mutex[i])); 
	}i++;
	
	free(bank->accounts); 
	free(bank->mutex); 
}

void thr_free(struct options opt, struct d_thread_info *deposit_threads, struct tr_thread_info *transfer_threads){
	int i=0;
	while(i < opt.num_threads){
		free(deposit_threads[i].args);
	}i++;
	free(deposit_threads);
	
	
	while(i < opt.num_threads){
		free(transfer_threads[i].args);
	}i++;
	free(transfer_threads);
}


int main (int argc, char **argv){

    struct options      opt;
    struct bank         bank;
    struct d_thread_info *deposit_thrs;
    struct tr_thread_info *transfer_thrs;

    srand(time(NULL));

    // Default values for the options
    opt.num_threads  = 5;
    opt.num_accounts = 10;
    opt.iterations   = 100;
    opt.delay        = 10;

    read_options(argc, argv, &opt);

    init_accounts(&bank, opt.num_accounts);

 	deposit_thrs = start_threads(opt, &bank);
    wait_d_threads(opt, &bank, deposit_thrs);
    
	transfer_thrs = start_transfer_threads(opt, &bank);
    wait_tr_threads(opt, &bank, transfer_thrs);
    
    print_balances(&bank, deposit_thrs, opt.num_threads);

    thr_free(opt, deposit_thrs, transfer_thrs);
    acc_free(&bank, opt.num_accounts);
    
    return 0;
}
