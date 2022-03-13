#include <sys/types.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#define GREEN   "\033[32m"      /* Green */
#define RED     "\033[31m"      /* Red */
#define RESET   "\033[0m"       /* Normal */
#define PASS_LEN 6

typedef struct {
    pthread_t id;
    unsigned char ** md5_num;
    bool * hashes_found;
    long comprobaciones;
    long comb_ini;
    long comb_fin;
    int n_threads;
    int n_hashes;
    int id_hilo;
    long inicio_intervalo;
    long fin_intervalo;
    bool * STOP;
    char ** printHash;
    long * totalcombs;
} Breaker;

typedef struct {
    Breaker * breakers;
} Progress;


void pinta_barra(int porcentaje, long totales, long persec){

    if (porcentaje >= 100){
        fprintf(stderr, "\n\n" RED "All cases of length 6 have been tested, no match. exiting..." RESET "\n\n");
        exit(0);
    } 
   
    int j;
    printf("\rComprobaciones totales: %ld ", totales);
    for(j=0; j<porcentaje/2; j++){
	    printf("*");
	}
	for(; j<50; j++){
	    printf("_");
	}
	printf("[%d%%]",porcentaje);
    printf(" %ld combs/s", (persec) );
	fflush(stdout);   
}

long ipow(long base, int exp)
{
    long res = 1;
    for (;;)
    {
        if (exp & 1)
            res *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }

    return res;
}

long pass_to_long(char *str) {
    long res = 0;

    for(int i=0; i < PASS_LEN; i++)
        res = res * 26 + str[i]-'a';

    return res;
}

void long_to_pass(long n, unsigned char *str) {  // str should have size PASS_SIZE+1
    for(int i=PASS_LEN-1; i >= 0; i--) {
        str[i] = n % 26 + 'a';
        n /= 26;
    }
    str[PASS_LEN] = '\0';
}

int hex_value(char c) {
    if (c>='0' && c <='9')
        return c - '0';
    else if (c>= 'A' && c <='F')
        return c-'A'+10;
    else if (c>= 'a' && c <='f')
        return c-'a'+10;
    else return 0;
}

void hex_to_num(char *str, unsigned char *hex) {
    for(int i=0; i < MD5_DIGEST_LENGTH; i++)
        hex[i] = (hex_value(str[i*2]) << 4) + hex_value(str[i*2 + 1]);
}

void * break_pass(void * args) {
    Breaker * breaker = args;
    unsigned char res[MD5_DIGEST_LENGTH];
    //long bound = ipow(26, PASS_LEN); // we have passwords of PASS_LEN
                                     // lowercase chars =>
                                    //     26 ^ PASS_LEN  different cases
    
    unsigned char *pass = malloc((PASS_LEN + 1) * sizeof(char));
    unsigned char *pass_ini = malloc((PASS_LEN + 1) * sizeof(char));
    unsigned char *pass_fin = malloc((PASS_LEN + 1) * sizeof(char));

    long_to_pass(breaker->inicio_intervalo, pass_ini);
    long_to_pass(breaker->fin_intervalo, pass_fin);

    for(long i = breaker->inicio_intervalo; i < breaker->fin_intervalo && !*breaker->STOP; i++) {
        
        long_to_pass(i, pass);
        MD5(pass, PASS_LEN, res);

        for(int j = 0; j < breaker->n_hashes; j++){
            
            if(breaker->hashes_found[j]) continue; // Si ya ha sido descubierta no hace falta volver a comparar

            if(0 == memcmp(res, breaker->md5_num[j], MD5_DIGEST_LENGTH)){
                
                fprintf(stderr, "\n\n" GREEN "Found it!" RESET " \n");          
                printf("Hash: %s -> %s\nFounded by thread Nº: %d\n\n", breaker->printHash[j+1], pass, breaker->id_hilo+1);

                breaker->hashes_found[j] = true;

                int total_decrypt = 0;
                for(int k = 0; k < breaker->n_hashes; k++){
                    if(breaker->hashes_found[k]) total_decrypt++;
                }

                if(total_decrypt == breaker->n_hashes-1){
                    *breaker->STOP = true;
                    fprintf(stderr, "\n\n" GREEN "All hashes have been decrypted!" RESET " \n");          
                    fclose(stdout);
                    break; // Found them!
                }
            }
        }
        breaker->comprobaciones++;
    }
    return NULL;
}

void * barra_progreso(void * args){
    
    Progress * aux = args;
    long num_max_pass = ipow(26, PASS_LEN);

    while (!*aux->breakers->STOP){
        long total_combs = 0;
        long comp_totales_ini = 0, comp_totales_fin = 0;

        for (int i = 0; i < aux->breakers->n_threads; i++){
            comp_totales_ini += aux->breakers[i].comprobaciones;
        }
        sleep(1);
        for (int i = 0; i < aux->breakers->n_threads; i++){ 
            comp_totales_fin += aux->breakers[i].comprobaciones;
        }
        for (int i = 0; i < aux->breakers->n_threads; i++){
            total_combs += aux->breakers[i].comprobaciones;
            pinta_barra(((aux->breakers[i].comprobaciones)/(double)num_max_pass)*100*aux->breakers->n_threads, total_combs, comp_totales_fin - comp_totales_ini);        
            fflush(stdout);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    
    if(argc < 3) {
        printf("%d", argc);
        printf("Use: %s string and number of threads\n", argv[0]);
        exit(0);
    } 

    int i;
    int n_threads = atoi(argv[argc-1]);
    long max_pass = ipow(26, PASS_LEN);
    long intervalo =  max_pass / n_threads;
    bool STOP = false;
    long totalcombs;
    
    Breaker * breakers;
    Progress progress;
    pthread_t progressThread;
    
    breakers = malloc(sizeof(Breaker) * n_threads);
    printf("\n");

    unsigned char **md5s = malloc((argc - 1) * sizeof(unsigned char*)); // Estructura de hashes introducidos por argumentos
	bool * hashes_encontrados = malloc((argc - 1) * sizeof(bool));

    for(int i = 0; i < argc - 1; i++){ // Creamos el array de hashes
        md5s[i] = malloc(MD5_DIGEST_LENGTH * sizeof(unsigned char));
        hex_to_num(argv[i+1], md5s[i]);
        hashes_encontrados[i] = false;
    }  

    for(i = 0; i < n_threads; i++){

        breakers[i].md5_num = md5s;       
        breakers[i].hashes_found = hashes_encontrados;
        breakers[i].id_hilo = i;
        breakers[i].comprobaciones = 0;
        breakers[i].inicio_intervalo = intervalo * i;
        breakers[i].n_threads = n_threads;
        breakers[i].n_hashes = argc - 1;
        breakers[i].STOP = &STOP;
        breakers[i].totalcombs = &totalcombs;
        breakers[i].printHash = argv;
        if((n_threads - 1) == i) breakers[i].fin_intervalo = ipow(26, PASS_LEN);
        else breakers[i].fin_intervalo = (intervalo * (i+1));

        printf("Creando hilo %d con info: intervalo inicial %ld , intervalo final %ld\n", breakers[i].id_hilo + 1, breakers[i].inicio_intervalo, breakers[i].fin_intervalo);
        pthread_create(&breakers[i].id, NULL, break_pass, &breakers[i]);
    }

    printf("\n");

    progress.breakers = breakers; // Meter lista de threads breakers
    
    pthread_create(&progressThread, NULL, barra_progreso, &progress);

    for(int i = 0; i < n_threads; i++) pthread_join(breakers[i].id, NULL);
    
    pthread_join(progressThread, NULL);

    free(breakers);

    return 0;
}