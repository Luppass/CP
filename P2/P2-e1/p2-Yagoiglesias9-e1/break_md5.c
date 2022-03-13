#include <sys/types.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#define PASS_LEN 6


typedef struct {
    pthread_t id;
    unsigned char md5_num[MD5_DIGEST_LENGTH];
    long comprobaciones;
    bool fin;
} Breaker;

void pinta_barra(int porcentaje, void * arg){
        Breaker * aux = arg;
		int j;
        printf("\rComprobaciones realizadas: %ld ", aux->comprobaciones);
		for(j=0; j<porcentaje/2; j++){
			printf("*");
		}
		for(; j<50; j++){
			printf("_");
		}
		printf("[%d%%]",porcentaje);
		printf("\r");
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
};

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
    unsigned char *pass = malloc((PASS_LEN + 1) * sizeof(char));
    long bound = ipow(26, PASS_LEN); // we have passwords of PASS_LEN
                                     // lowercase chars =>
                                    //     26 ^ PASS_LEN  different cases

    for(long i=0; i < bound; i++) {
        long_to_pass(i, pass);
        MD5(pass, PASS_LEN, res);
        if(0 == memcmp(res, breaker->md5_num, MD5_DIGEST_LENGTH)){
            breaker->fin = true;
            puts("\nEncontrado!\n");
            setvbuf (stdout, NULL, _IONBF, 0);
            printf("Password: %s\n", pass);
            break; // Found it!
        } 
        breaker->comprobaciones++;
    }
    return NULL;
}


void * barra_progreso(void * args){
    
    Breaker * aux = args;
    long num_max_pass = ipow(26, PASS_LEN);

    do{
        pinta_barra((aux->comprobaciones/(double)num_max_pass)*100, aux);
        sleep(1);
        
    }while (!aux->fin);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    
    if(argc < 2) {
        printf("Use: %s string\n", argv[0]);
        exit(0);
    }

    Breaker * breaker;
    Breaker * progress;
    
    progress = malloc(sizeof(Breaker));
    breaker = malloc(sizeof(Breaker));
    
    breaker->fin = false;
    hex_to_num(argv[1], breaker->md5_num);

    pthread_create(&breaker->id, NULL, break_pass, breaker);
    pthread_create(&progress->id, NULL, barra_progreso, breaker);

    pthread_join(breaker->id, NULL);
    pthread_join(progress->id, NULL);
    
    free(breaker);
    free(progress);

    return 0;
}
