#define _GNU_SOURCE
#include "msf.h"
#include <limits.h>

// Union-Find con path compression
int find(int i, int parent[]) {
    if (parent[i] != i) {
        parent[i] = find(parent[i], parent);
    }
    return parent[i];
}

// Unione di due insiemi con union by rank
void unione(int x, int y, int parent[], int rank[]) {
    int radiceX = find(x, parent);
    int radiceY = find(y, parent);

    if (rank[radiceX] < rank[radiceY]) {
        parent[radiceX] = radiceY;
    } else if (rank[radiceX] > rank[radiceY]) {
        parent[radiceY] = radiceX;
    } else {
        parent[radiceY] = radiceX;
        rank[radiceX]++;
    }
}

// Comparatore archi per ordinamento per peso crescente
int confrontaArchi(const void *a, const void *b) {
    arco *arcoA = (arco *)a;
    arco *arcoB = (arco *)b;
    return (arcoA->weight - arcoB->weight);
}

// Legge il grafo da file .gr e restituisce array di archi ordinati per peso
arco *leggiArco(int *numNodi, int *numArchi, const char *nomeFile) {
    FILE *fileGrafo = fopen(nomeFile, "r");
    if (fileGrafo == NULL) {
        fprintf(stderr, "Errore nell'apertura del file.\n");
        return NULL;
    }

    int i = 0;
    char linea_letta[256];
    arco *arrayArchi = NULL;
    int u, v, peso;

    while (fgets(linea_letta, sizeof(linea_letta), fileGrafo) != NULL) {
        if (linea_letta[0] == 'c') continue;

        if (linea_letta[0] == 'p') {
            sscanf(linea_letta, "p %*s %d %d", numNodi, numArchi);
            arrayArchi = (arco *)malloc(*numArchi * sizeof(arco));
            if (arrayArchi == NULL) {
                fprintf(stderr, "Errore nell'allocazione della memoria per gli archi.\n");
                fclose(fileGrafo);
                return NULL;
            }
        }

        if (linea_letta[0] == 'a') {
            sscanf(linea_letta, "a %d %d %d", &u, &v, &peso);
            if (u < v) {
                arrayArchi[i].u = u;
                arrayArchi[i].v = v;
            } else {
                arrayArchi[i].u = v;
                arrayArchi[i].v = u;
            }
            arrayArchi[i].weight = peso;
            arrayArchi[i].msf = false;
            arrayArchi[i].next = NULL;
            i++;
        }
    }

    fclose(fileGrafo);

    if (arrayArchi != NULL) {
        qsort(arrayArchi, *numArchi, sizeof(arco), confrontaArchi);
    }
    return arrayArchi;
}

// Calcola l'indice hash per l'arco (u,v)
int hashArco(int u, int v, int hashsize) {
    return (u * 31 + v) % hashsize;
}

// Inserisce un nodo nella lista di adiacenza ordinata per id crescente
void inserisci_vicini(elemento **testa, int id, int w, bool ismsf) {
    elemento *nuovoNodo = malloc(sizeof(elemento));
    if (nuovoNodo == NULL) {
        fprintf(stderr, "Errore nell'allocazione di un nuovo nodo nella lista di adiacenza.\n");
        exit(EXIT_FAILURE);
    }
    nuovoNodo->id = id;
    nuovoNodo->w = w;
    nuovoNodo->msf = ismsf;
    nuovoNodo->next = NULL;

    if (*testa == NULL || (*testa)->id > id) {
        nuovoNodo->next = *testa;
        *testa = nuovoNodo;
        return;
    }

    elemento *cur = *testa;
    while (cur->next != NULL && cur->next->id < id) {
        cur = cur->next;
    }
    nuovoNodo->next = cur->next;
    cur->next = nuovoNodo;
}

// Cerca un arco nella hash table
arco *cercaArco(arco **gHash, int u, int v, int hashsize) {
    int hashIndex = hashArco(u, v, hashsize);
    arco *cur = gHash[hashIndex];
    while (cur != NULL) {
        if (cur->u == u && cur->v == v) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

// Rimuove un nodo dalla lista di adiacenza
void rimuoviDaVicini(elemento **testa, int id) {
    elemento *cur = *testa;
    elemento *prec = NULL;

    while (cur != NULL) {
        if (cur->id == id) {
            if (prec == NULL) {
                *testa = cur->next;
            } else {
                prec->next = cur->next;
            }
            free(cur);
            return;
        }
        prec = cur;
        cur = cur->next;
    }
}

// DFS iterativa sugli archi MSF, esclude un nodo dal percorso
void dfs(grafo *g, int nodoStart, int nodoEscluso, bool *visitati) {
    int *stack = malloc(g->numNodiTotali * sizeof(int));
    if (stack == NULL) {
        fprintf(stderr, "Errore allocazione stack DFS.\n");
        exit(EXIT_FAILURE);
    }
    int numElementi = 0;
    stack[numElementi++] = nodoStart;
    visitati[nodoStart] = true;

    while (numElementi > 0) {
        int nodoCorrente = stack[--numElementi];
        elemento *cur = g->vicini[nodoCorrente];
        while (cur != NULL) {
            if (cur->msf && !visitati[cur->id] && cur->id != nodoEscluso) {
                visitati[cur->id] = true;
                stack[numElementi++] = cur->id;
            }
            cur = cur->next;
        }
    }

    free(stack);
}

// Cancella un arco dal grafo, gestendo eventuale split della componente
void cancellaArco(grafo *g, int u, int v) {
    if (u < 0 || u >= g->numNodiTotali || v < 0 || v >= g->numNodiTotali) {
        pthread_mutex_lock(&g->mutexStato);
        printf("- %d %d 0\n", u, v);
        pthread_mutex_unlock(&g->mutexStato);
        return;
    }

    int h = hashArco(u, v, g->dimensioneHash);
    pthread_mutex_lock(&g->mutexHash[h % g->numMutex]);

    arco *arcotrovato = cercaArco(g->gHash, u, v, g->dimensioneHash);
    if (arcotrovato == NULL) {
        pthread_mutex_unlock(&g->mutexHash[h % g->numMutex]);
        pthread_mutex_lock(&g->mutexStato);
        printf("- %d %d 0\n", u, v);
        pthread_mutex_unlock(&g->mutexStato);
        return;
    }

    int pesoArcoEliminato = arcotrovato->weight;
    bool eraMSF = arcotrovato->msf;

    arco *cur = g->gHash[h];
    arco *precedente = NULL;
    while (cur != NULL) {
        if (cur->u == u && cur->v == v) {
            if (precedente == NULL) {
                g->gHash[h] = cur->next;
            } else {
                precedente->next = cur->next;
            }
            free(cur);
            break;
        }
        precedente = cur;
        cur = cur->next;
    }

    pthread_mutex_unlock(&g->mutexHash[h % g->numMutex]);

    rimuoviDaVicini(&g->vicini[u], v);
    rimuoviDaVicini(&g->vicini[v], u);
    pthread_mutex_lock(&g->mutexStato);
    g->numArchi--;
    pthread_mutex_unlock(&g->mutexStato);

    if (eraMSF == false) {
        pthread_mutex_lock(&g->mutexStato);
        printf("- %d %d %d %d %ld\n", u, v, g->numArchi, g->numCoCo, g->costoMSF);
        pthread_mutex_unlock(&g->mutexStato);
        return;
    }

    bool *Lu = malloc(g->numNodiTotali * sizeof(bool));
    bool *Lv = malloc(g->numNodiTotali * sizeof(bool));
    if (Lu == NULL || Lv == NULL) {
        fprintf(stderr, "Errore allocazione Lu/Lv.\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < g->numNodiTotali; i++) {
        Lu[i] = false;
        Lv[i] = false;
    }

    dfs(g, u, v, Lu);
    dfs(g, v, u, Lv);

    int pesoArcoConnessione = INT_MAX;
    int nodoLu = -1;
    int nodoLv = -1;

    for (int i = 0; i < g->numNodiTotali; i++) {
        if (Lu[i]) {
            elemento *curE = g->vicini[i];
            while (curE != NULL) {
                if (Lv[curE->id] && curE->w < pesoArcoConnessione) {
                    pesoArcoConnessione = curE->w;
                    nodoLu = i;
                    nodoLv = curE->id;
                }
                curE = curE->next;
            }
        }
    }

    if (nodoLu == -1) {
        int minLu = -1, minLv = -1;
        for (int i = 0; i < g->numNodiTotali; i++) {
            if (Lu[i] && minLu == -1) minLu = i;
            if (Lv[i] && minLv == -1) minLv = i;
        }

        pthread_mutex_lock(&g->mutexComponenti);
        if (minLu < minLv) {
            for (int i = 0; i < g->numNodiTotali; i++) {
                if (Lv[i]) g->cCon[i] = minLv;
            }
        } else {
            for (int i = 0; i < g->numNodiTotali; i++) {
                if (Lu[i]) g->cCon[i] = minLu;
            }
        }
        pthread_mutex_unlock(&g->mutexComponenti);

        pthread_mutex_lock(&g->mutexStato);
        g->numCoCo++;
        g->costoMSF -= pesoArcoEliminato;
        pthread_mutex_unlock(&g->mutexStato);

    } else {
        int minNodo = (nodoLu < nodoLv) ? nodoLu : nodoLv;
        int maxNodo = (nodoLu < nodoLv) ? nodoLv : nodoLu;

        int h2 = hashArco(minNodo, maxNodo, g->dimensioneHash);
        pthread_mutex_lock(&g->mutexHash[h2 % g->numMutex]);

        arco *nuovoMsf = cercaArco(g->gHash, minNodo, maxNodo, g->dimensioneHash);
        if (nuovoMsf != NULL) nuovoMsf->msf = true;

        pthread_mutex_unlock(&g->mutexHash[h2 % g->numMutex]);

        elemento *curU = g->vicini[nodoLu];
        while (curU != NULL) {
            if (curU->id == nodoLv) { curU->msf = true; break; }
            curU = curU->next;
        }
        elemento *curV = g->vicini[nodoLv];
        while (curV != NULL) {
            if (curV->id == nodoLu) { curV->msf = true; break; }
            curV = curV->next;
        }

        pthread_mutex_lock(&g->mutexStato);
        g->costoMSF -= pesoArcoEliminato;
        g->costoMSF += pesoArcoConnessione;
        pthread_mutex_unlock(&g->mutexStato);
    }

    pthread_mutex_lock(&g->mutexStato);
    printf("- %d %d %d %d %ld\n", u, v, g->numArchi, g->numCoCo, g->costoMSF);
    pthread_mutex_unlock(&g->mutexStato);

    free(Lu);
    free(Lv);
}

// Aggiunge un arco al grafo, aggiornando MSF e componenti se necessario
void aggiungiArco(grafo *g, int u, int v, int w) {
    if (u < 0 || u >= g->numNodiTotali || v < 0 || v >= g->numNodiTotali) {
        pthread_mutex_lock(&g->mutexStato);
        printf("+ %d %d %d 0\n", u, v, w);
        pthread_mutex_unlock(&g->mutexStato);
        return;
    }

    int minNodo, maxNodo;
    if (u < v) {
        minNodo = u;
        maxNodo = v;
    } else {
        minNodo = v;
        maxNodo = u;
    }

    int h = hashArco(minNodo, maxNodo, g->dimensioneHash);
    pthread_mutex_lock(&g->mutexHash[h % g->numMutex]);

    arco *trovato = cercaArco(g->gHash, minNodo, maxNodo, g->dimensioneHash);
    if (trovato != NULL) {
        pthread_mutex_unlock(&g->mutexHash[h % g->numMutex]);
        pthread_mutex_lock(&g->mutexStato);
        printf("+ %d %d %d 0\n", u, v, w);
        pthread_mutex_unlock(&g->mutexStato);
        return;
    }

    arco *nuovoArco = malloc(sizeof(arco));
    if (nuovoArco == NULL) {
        pthread_mutex_unlock(&g->mutexHash[h % g->numMutex]);
        fprintf(stderr, "Errore allocazione arco.\n");
        exit(EXIT_FAILURE);
    }
    nuovoArco->u = minNodo;
    nuovoArco->v = maxNodo;
    nuovoArco->weight = w;
    nuovoArco->msf = false;

    nuovoArco->next = g->gHash[h];
    g->gHash[h] = nuovoArco;

    pthread_mutex_unlock(&g->mutexHash[h % g->numMutex]);

    inserisci_vicini(&g->vicini[u], v, w, false);
    inserisci_vicini(&g->vicini[v], u, w, false);
    pthread_mutex_lock(&g->mutexStato);
    g->numArchi++;
    pthread_mutex_unlock(&g->mutexStato);

    //Caso A: L'arco connette due componenti separate
    if (g->cCon[u] != g->cCon[v]) {
        pthread_mutex_lock(&g->mutexHash[h % g->numMutex]);
        nuovoArco->msf = true;
        pthread_mutex_unlock(&g->mutexHash[h % g->numMutex]);

        //AGGIORNO ARCO MSF  = TRUE IN LISTA VICINI U
        elemento *curU = g->vicini[u];
        while (curU != NULL) {
            if (curU->id == v) { curU->msf = true; break; }
            curU = curU->next;
        }

        //AGGIORNO ARCO MSF  = TRUE IN LISTA VICINI V
        elemento *curV = g->vicini[v];
        while (curV != NULL) {
            if (curV->id == u) { curV->msf = true; break; }
            curV = curV->next;
        }

        pthread_mutex_lock(&g->mutexComponenti);

        int compU = g->cCon[u];
        int compV = g->cCon[v];
        int nuovoId   = (compU < compV) ? compU : compV;
        int vecchioId = (compU < compV) ? compV : compU;

        for (int i = 0; i < g->numNodiTotali; i++) {
            if (g->cCon[i] == vecchioId) {
                g->cCon[i] = nuovoId;
            }
        }

        pthread_mutex_unlock(&g->mutexComponenti);

        pthread_mutex_lock(&g->mutexStato);
        g->numCoCo--;
        g->costoMSF += w;
        printf("+ %d %d %d %d %d %ld\n", u, v, w, g->numArchi, g->numCoCo, g->costoMSF);
        pthread_mutex_unlock(&g->mutexStato);

    } else { //Caso B: u e v  stessa componente connessa
        int  *padre    = malloc(g->numNodiTotali * sizeof(int));
        bool *visitato = malloc(g->numNodiTotali * sizeof(bool));
        int  *stack    = malloc(g->numNodiTotali * sizeof(int));
        if (padre == NULL || visitato == NULL || stack == NULL) {
            fprintf(stderr, "Errore allocazione.\n");
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < g->numNodiTotali; i++) {
            padre[i] = -1;
            visitato[i] = false;
        }

        int numElementi = 0;
        stack[numElementi++] = u;
        visitato[u] = true;
        bool trovato_v = false;

        while (numElementi > 0 && !trovato_v) {
            int nodoCorrente = stack[--numElementi];
            elemento *cur = g->vicini[nodoCorrente];
            while (cur != NULL) {
                if (cur->msf && !visitato[cur->id]) {
                    visitato[cur->id] = true;
                    padre[cur->id] = nodoCorrente;
                    if (cur->id == v) {
                        trovato_v = true;
                        break;
                    }
                    stack[numElementi++] = cur->id;
                }
                cur = cur->next;
            }
        }
        free(stack);
        free(visitato);

        int pesoMax  = INT_MIN;
        int nodoMaxU = -1;
        int nodoMaxV = -1;

        int nodoCorrente = v;
        while (nodoCorrente != u) {
            int nodoPadre = padre[nodoCorrente];
            elemento *cur = g->vicini[nodoPadre];
            while (cur != NULL) {
                if (cur->id == nodoCorrente && cur->msf) {
                    if (cur->w > pesoMax) {
                        pesoMax = cur->w;
                        if (nodoPadre < nodoCorrente) {
                            nodoMaxU = nodoPadre;
                            nodoMaxV = nodoCorrente;
                        } else {
                            nodoMaxU = nodoCorrente;
                            nodoMaxV = nodoPadre;
                        }
                    }
                    break;
                }
                cur = cur->next;
            }
            nodoCorrente = nodoPadre;
        }

        if (pesoMax > w) {
            int h2 = hashArco(nodoMaxU, nodoMaxV, g->dimensioneHash);
            int idx_h  = h  % g->numMutex;
            int idx_h2 = h2 % g->numMutex;

            // lock dei due bucket in ordine crescente di indice di mutex
            if (idx_h == idx_h2) {
                // stesso mutex: un solo lock
                pthread_mutex_lock(&g->mutexHash[idx_h]);
            } else if (idx_h < idx_h2) {
                pthread_mutex_lock(&g->mutexHash[idx_h]);
                pthread_mutex_lock(&g->mutexHash[idx_h2]);
            } else {
                pthread_mutex_lock(&g->mutexHash[idx_h2]);
                pthread_mutex_lock(&g->mutexHash[idx_h]);
            }

            arco *arcoMax = cercaArco(g->gHash, nodoMaxU, nodoMaxV, g->dimensioneHash);
            if (arcoMax != NULL) arcoMax->msf = false;

            nuovoArco->msf = true;

            // unlock (in ordine inverso, per simmetria; con mutex non ricorsivi l'ordine non è critico)
            if (idx_h == idx_h2) {
                pthread_mutex_unlock(&g->mutexHash[idx_h]);
            } else {
                pthread_mutex_unlock(&g->mutexHash[idx_h]);
                pthread_mutex_unlock(&g->mutexHash[idx_h2]);
            }

            elemento *curU = g->vicini[nodoMaxU];
            while (curU != NULL) {
                if (curU->id == nodoMaxV) { curU->msf = false; break; }
                curU = curU->next;
            }
            elemento *curV = g->vicini[nodoMaxV];
            while (curV != NULL) {
                if (curV->id == nodoMaxU) { curV->msf = false; break; }
                curV = curV->next;
            }

            elemento *curNU = g->vicini[u];
            while (curNU != NULL) {
                if (curNU->id == v) { curNU->msf = true; break; }
                curNU = curNU->next;
            }
            elemento *curNV = g->vicini[v];
            while (curNV != NULL) {
                if (curNV->id == u) { curNV->msf = true; break; }
                curNV = curNV->next;
            }

            pthread_mutex_lock(&g->mutexStato);
            g->costoMSF -= pesoMax;
            g->costoMSF += w;
            pthread_mutex_unlock(&g->mutexStato);
        }

        free(padre);

        pthread_mutex_lock(&g->mutexStato);
        printf("+ %d %d %d %d %d %ld\n", u, v, w, g->numArchi, g->numCoCo, g->costoMSF);
        pthread_mutex_unlock(&g->mutexStato);
    }
}

// Thread consumatore: estrae operazioni dal buffer e le esegue
void *consumatore(void *args) {
    datiThread *dati = (datiThread *)args;
    grafo *g = dati->g;

    while (true) {
        sem_wait(&g->operazioni_In_Buffer);
        pthread_mutex_lock(&g->mutexBuffer);
        operazione op = g->buffer[g->indiceConsumatore % DIM_BUFFER];
        g->indiceConsumatore++;
        pthread_mutex_unlock(&g->mutexBuffer);
        sem_post(&g->slotsLiberi);

        if (op.tipo == '\0') break;

        if (op.u < 0 || op.u >= g->numNodiTotali ||
            op.v < 0 || op.v >= g->numNodiTotali) {
            pthread_mutex_lock(&g->mutexStato);
            if (op.tipo == '+') {
                printf("+ %d %d %d 0\n", op.u, op.v, op.w);
            } else {
                printf("- %d %d 0\n", op.u, op.v);
            }
            pthread_mutex_unlock(&g->mutexStato);
            continue;
        }

        pthread_mutex_lock(&g->mutexComponenti);
        while (g->componenteOccupata[g->cCon[op.u]] ||
               g->componenteOccupata[g->cCon[op.v]]) {
            pthread_cond_wait(&g->componenteLibera, &g->mutexComponenti);
        }
        g->componenteOccupata[g->cCon[op.u]] = true;
        g->componenteOccupata[g->cCon[op.v]] = true;
        int compU = g->cCon[op.u];
        int compV = g->cCon[op.v];
        pthread_mutex_unlock(&g->mutexComponenti);

        if (op.tipo == '+') {
            aggiungiArco(g, op.u, op.v, op.w);
        } else {
            cancellaArco(g, op.u, op.v);
        }

        pthread_mutex_lock(&g->mutexComponenti);
        g->componenteOccupata[compU] = false;
        g->componenteOccupata[compV] = false;
        pthread_cond_broadcast(&g->componenteLibera);
        pthread_mutex_unlock(&g->mutexComponenti);
    }

    return NULL;
}