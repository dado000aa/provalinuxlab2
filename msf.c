#define _GNU_SOURCE  
#include "msf.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

    // =========================================================
    // 1. PARSING ARGOMENTI DA LINEA DI COMANDO
    // =========================================================
    int numThreads = 3;
    int hashsize = 100000;
    int nmutex = 1000;

    int opt;
    while ((opt = getopt(argc, argv, "t:H:M:")) != -1) {
        switch (opt) {
        case 't':
            numThreads = atoi(optarg);
            if (numThreads <= 0) {
                fprintf(stderr, "Errore: il numero di thread deve essere positivo.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'H':
            hashsize = atoi(optarg);
            if (hashsize <= 0) {
                fprintf(stderr, "Errore: la dimensione della hash deve essere positiva.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'M':
            nmutex = atoi(optarg);
            if (nmutex <= 0) {
                fprintf(stderr, "Errore: il numero di mutex deve essere positivo.\n");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            fprintf(stderr, "Uso: %s file_grafo file_archi [-t threads] [-H hashsize] [-M nmutex]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind + 2 > argc) {
        fprintf(stderr, "Uso: %s file_grafo file_archi [-t threads] [-H hashsize] [-M nmutex]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *fileGrafo = argv[optind];
    const char *fileArchi = argv[optind + 1];

    // =========================================================
    // 2. LETTURA GRAFO E KRUSKAL
    // =========================================================
    int numNodi = 0, numArchi = 0;
    arco *archi = leggiArco(&numNodi, &numArchi, fileGrafo);
    if (archi == NULL) {
        fprintf(stderr, "Errore nella lettura del grafo.\n");
        exit(EXIT_FAILURE);
    }

    int numNodiTotali = numNodi + 1;

    int *parent = malloc(numNodiTotali * sizeof(int));
    int *rank = malloc(numNodiTotali * sizeof(int));
    if (parent == NULL || rank == NULL) {
        fprintf(stderr, "Errore allocazione parent/rank.\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < numNodiTotali; i++) {
        parent[i] = i;
        rank[i] = 0;
    }

    long costoComponentiConnesse = 0;
    int numComponenti = numNodiTotali;

    for (int i = 0; i < numArchi; i++) {
        int u = archi[i].u;
        int v = archi[i].v;
        if (find(u, parent) != find(v, parent)) {
            unione(u, v, parent, rank);
            archi[i].msf = true;
            costoComponentiConnesse += archi[i].weight;
            numComponenti--;
        } else {
            archi[i].msf = false;
        }
    }

    // =========================================================
    // 3. COSTRUZIONE STRUCT GRAFO
    // =========================================================
    grafo *mioGrafo = malloc(sizeof(grafo));
    if (mioGrafo == NULL) {
        fprintf(stderr, "Errore allocazione grafo.\n");
        exit(EXIT_FAILURE);
    }

    mioGrafo->numCoCo = numComponenti;
    mioGrafo->costoMSF = costoComponentiConnesse;
    mioGrafo->numNodiTotali = numNodiTotali;
    mioGrafo->dimensioneHash = hashsize;
    mioGrafo->numMutex = nmutex;
    mioGrafo->numArchi = numArchi;

    // =========================================================
    // 4. COSTRUZIONE cCon
    // =========================================================
    mioGrafo->cCon = malloc(numNodiTotali * sizeof(int));
    if (mioGrafo->cCon == NULL) {
        fprintf(stderr, "Errore allocazione cCon.\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < numNodiTotali; i++) {
        mioGrafo->cCon[i] = i;
    }
    for (int i = 0; i < numNodiTotali; i++) {
        int radice = find(i, parent);
        if (i < mioGrafo->cCon[radice]) {
            mioGrafo->cCon[radice] = i;
        }
    }
    for (int i = 0; i < numNodiTotali; i++) {
        mioGrafo->cCon[i] = mioGrafo->cCon[find(i, parent)];
    }

    // stampa terna iniziale
    printf("%d %d %ld\n", numArchi, numComponenti, costoComponentiConnesse);

    // =========================================================
    // 5. COSTRUZIONE gHash
    // =========================================================
    mioGrafo->gHash = malloc(hashsize * sizeof(arco *));
    if (mioGrafo->gHash == NULL) {
        fprintf(stderr, "Errore allocazione gHash.\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < hashsize; i++) {
        mioGrafo->gHash[i] = NULL;
    }
    for (int i = 0; i < numArchi; i++) {
        arco *nuovoArco = malloc(sizeof(arco));
        if (nuovoArco == NULL) {
            fprintf(stderr, "Errore allocazione arco nella hash.\n");
            exit(EXIT_FAILURE);
        }
        nuovoArco->u = archi[i].u;
        nuovoArco->v = archi[i].v;
        nuovoArco->weight = archi[i].weight;
        nuovoArco->msf = archi[i].msf;
        int hashIndex = hashArco(nuovoArco->u, nuovoArco->v, hashsize);
        nuovoArco->next = mioGrafo->gHash[hashIndex];
        mioGrafo->gHash[hashIndex] = nuovoArco;
    }

    // =========================================================
    // 6. COSTRUZIONE vicini
    // =========================================================
    mioGrafo->vicini = malloc(numNodiTotali * sizeof(elemento *));
    if (mioGrafo->vicini == NULL) {
        fprintf(stderr, "Errore allocazione vicini.\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < numNodiTotali; i++) {
        mioGrafo->vicini[i] = NULL;
    }
    for (int i = 0; i < numArchi; i++) {
        int u = archi[i].u;
        int v = archi[i].v;
        int w = archi[i].weight;
        bool isMsf = archi[i].msf;
        inserisci_vicini(&mioGrafo->vicini[u], v, w, isMsf);
        inserisci_vicini(&mioGrafo->vicini[v], u, w, isMsf);
    }

    // =========================================================
    // 7. INIZIALIZZAZIONE STRUTTURE PER MULTITHREADING
    // =========================================================

    // mutex per la hash table
    mioGrafo->mutexHash = malloc(nmutex * sizeof(pthread_mutex_t));
    if (mioGrafo->mutexHash == NULL) {
        fprintf(stderr, "Errore allocazione mutexHash.\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < nmutex; i++) {
        pthread_mutex_init(&mioGrafo->mutexHash[i], NULL);
    }

    // componenti connesse occupate
    mioGrafo->componenteOccupata = malloc(numNodiTotali * sizeof(bool));
    if (mioGrafo->componenteOccupata == NULL) {
        fprintf(stderr, "Errore allocazione componenteOccupata.\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < numNodiTotali; i++) {
        mioGrafo->componenteOccupata[i] = false;
    }
    pthread_mutex_init(&mioGrafo->mutexComponenti, NULL);
    pthread_cond_init(&mioGrafo->componenteLibera, NULL);
    pthread_mutex_init(&mioGrafo->mutexStato, NULL);


    // buffer produttore-consumatore
    mioGrafo->indiceProduttore = 0;
    mioGrafo->indiceConsumatore = 0;
    pthread_mutex_init(&mioGrafo->mutexBuffer, NULL);
    sem_init(&mioGrafo->slotsLiberi, 0, DIM_BUFFER);
    sem_init(&mioGrafo->operazioni_In_Buffer, 0, 0);

    // =========================================================
    // AVVIO THREAD CONSUMATORI
    // =========================================================
    pthread_t *threads = malloc(numThreads * sizeof(pthread_t));
    datiThread *datiThreads = malloc(numThreads * sizeof(datiThread));
    if (threads == NULL || datiThreads == NULL) {
        fprintf(stderr, "Errore allocazione thread.\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < numThreads; i++) {
        datiThreads[i].g = mioGrafo;
        datiThreads[i].id = i;
        pthread_create(&threads[i], NULL, consumatore, &datiThreads[i]);
    }

    // =========================================================
    // PRODUTTORE: LEGGI FILE OPERAZIONI E METTI NEL BUFFER
    // =========================================================
    FILE *fileOperazioni = fopen(fileArchi, "r");
    if (fileOperazioni == NULL) {
        fprintf(stderr, "Errore apertura file operazioni.\n");
        exit(EXIT_FAILURE);
    }

    char linea_op[256];
    int u, v, peso;

    while (fgets(linea_op, sizeof(linea_op), fileOperazioni) != NULL) {
        operazione op;
        if (linea_op[0] == '+') {
            if (sscanf(linea_op, "+ %d %d %d", &u, &v, &peso) != 3) continue;
            op.tipo = '+'; 
            op.u = u; 
            op.v = v; 
            op.w = peso;
        } else if (linea_op[0] == '-') {
            if (sscanf(linea_op, "- %d %d", &u, &v) != 2) continue;
            op.tipo = '-'; 
            op.u = u; 
            op.v = v; 
            op.w = 0;
        } else {
            continue;
        }
        sem_wait(&mioGrafo->slotsLiberi);
        pthread_mutex_lock(&mioGrafo->mutexBuffer);
        mioGrafo->buffer[mioGrafo->indiceProduttore % DIM_BUFFER] = op;
        mioGrafo->indiceProduttore++;
        pthread_mutex_unlock(&mioGrafo->mutexBuffer);
        sem_post(&mioGrafo->operazioni_In_Buffer);
    }
    fclose(fileOperazioni);

    // =========================================================
    //  SEGNALE DI TERMINAZIONE AI THREAD
    // =========================================================
    for (int i = 0; i < numThreads; i++) {
        operazione fine;
        fine.tipo = '\0'; fine.u = 0; fine.v = 0; fine.w = 0;
        sem_wait(&mioGrafo->slotsLiberi);
        pthread_mutex_lock(&mioGrafo->mutexBuffer);
        mioGrafo->buffer[mioGrafo->indiceProduttore % DIM_BUFFER] = fine;
        mioGrafo->indiceProduttore++;
        pthread_mutex_unlock(&mioGrafo->mutexBuffer);
        sem_post(&mioGrafo->operazioni_In_Buffer);
    }

    // aspetta che tutti i thread finiscano
    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Operazioni terminate\n");

    // ricalcolo finale scansionando gHash
    int numArchiFinale = 0;
    long costoMSFfinale = 0;
    int posizioniNonVuote = 0;
    int lunghezzaMax = 0;
    long lunghezzaTotale = 0;

    for (int i = 0; i < hashsize; i++) {
        int lunghezza = 0;
        arco *cur = mioGrafo->gHash[i];
        while (cur != NULL) {
            numArchiFinale++;
            if (cur->msf) costoMSFfinale += cur->weight;
            lunghezza++;
            cur = cur->next;
        }
        if (lunghezza > 0) {
            posizioniNonVuote++;
            lunghezzaTotale += lunghezza;
            if (lunghezza > lunghezzaMax) lunghezzaMax = lunghezza;
        }
    }

    // conta componenti connesse distinte in cCon
    int numComponentiFinale = 0;
    bool *componenteContata = calloc(numNodiTotali, sizeof(bool));
    if (componenteContata == NULL) {
        fprintf(stderr, "Errore allocazione componenteContata.\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < numNodiTotali; i++) {
        int idComp = mioGrafo->cCon[i];
        if (!componenteContata[idComp]) {
            componenteContata[idComp] = true;
            numComponentiFinale++;
        }
    }
    free(componenteContata);

    // stampa statistiche e terna finale
    printf("Numero posizioni non vuote: %d\n", posizioniNonVuote);
    printf("Lunghezza media liste: %.7f\n", 
           posizioniNonVuote > 0 ? (double)lunghezzaTotale / posizioniNonVuote : 0.0);
    printf("Lunghezza massima liste: %d\n", lunghezzaMax);
    printf("%d %d %ld\n", numArchiFinale, numComponentiFinale, costoMSFfinale);


    // =========================================================
    // CLEANUP E FREE
    // =========================================================

    // distruggi mutex e semafori
    for (int i = 0; i < nmutex; i++) {
        pthread_mutex_destroy(&mioGrafo->mutexHash[i]);
    }
    free(mioGrafo->mutexHash);
    pthread_mutex_destroy(&mioGrafo->mutexComponenti);
    pthread_cond_destroy(&mioGrafo->componenteLibera);
    pthread_mutex_destroy(&mioGrafo->mutexStato);
    free(mioGrafo->componenteOccupata);
    sem_destroy(&mioGrafo->slotsLiberi);
    sem_destroy(&mioGrafo->operazioni_In_Buffer);
    pthread_mutex_destroy(&mioGrafo->mutexBuffer);
    free(threads);
    free(datiThreads);

    // libera gHash
    for (int i = 0; i < hashsize; i++) {
        arco *cur = mioGrafo->gHash[i];
        while (cur != NULL) {
            arco *tmp = cur;
            cur = cur->next;
            free(tmp);
        }
    }
    free(mioGrafo->gHash);

    // libera vicini
    for (int i = 0; i < numNodiTotali; i++) {
        elemento *cur = mioGrafo->vicini[i];
        while (cur != NULL) {
            elemento *tmp = cur;
            cur = cur->next;
            free(tmp);
        }
    }
    free(mioGrafo->vicini);

    free(mioGrafo->cCon);
    free(mioGrafo);
    free(parent);
    free(rank);
    free(archi);
    return 0;
}