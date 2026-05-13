#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define DIM_BUFFER 32

typedef struct arco {
    int u, v;
    int weight;
    bool msf;
    struct arco *next;
} arco;

typedef struct elemento {
    int id;
    int w;
    bool msf;
    struct elemento *next;
} elemento;

// operazione deve stare PRIMA di grafo perché grafo la usa
typedef struct {
    char tipo;
    int u, v, w;
} operazione;

typedef struct {
    // dati del grafo
    arco **gHash;
    elemento **vicini;
    int *cCon;
    int numCoCo;
    long costoMSF;
    int numArchi;

    // info per i thread
    int numNodiTotali;
    int dimensioneHash;
    int numMutex;

    // mutex per proteggere i bucket della hash table
    pthread_mutex_t *mutexHash;

    // gestione componenti connesse occupate
    bool *componenteOccupata;
    pthread_mutex_t mutexComponenti;
    pthread_cond_t componenteLibera;

    // mutex per contatori e printf
    pthread_mutex_t mutexStato;

    // buffer produttore-consumatore
    operazione buffer[DIM_BUFFER];
    int indiceProduttore;
    int indiceConsumatore;
    pthread_mutex_t mutexBuffer;
    sem_t slotsLiberi;
    sem_t operazioni_In_Buffer;
} grafo;



typedef struct {
    grafo *g;
    int id;
} datiThread;

// prototipi
int find(int i, int parent[]);
void unione(int x, int y, int parent[], int rank[]);
int confrontaArchi(const void *a, const void *b);
arco* leggiArco(int *numNodi, int *numArchi, const char *nomeFile);
int hashArco(int u, int v, int dimensioneHash);
void inserisci_vicini(elemento **testa, int id, int w, bool ismsf);
arco* cercaArco(arco **gHash, int u, int v, int dimensioneHash);
void *consumatore(void *arg);
void cancellaArco(grafo *g, int u, int v) ;
void rimuoviDaVicini(elemento **testa, int id);
void dfs(grafo *g, int nodoStart, int nodoEscluso, bool *visitati);
void aggiungiArco(grafo *g, int u, int v, int w); 