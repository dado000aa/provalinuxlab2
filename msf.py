import heapq
import sys


def leggi_grafo(NomeFile): 
    vicini = {}
    nodi = 0

    with open (NomeFile, 'r') as f:
        for linea in f:
            if linea.startswith('c'):
                continue
            if linea.startswith('p'):  # p sp 6 7  
                pezzetto = linea.split() 
                nodi = int(pezzetto[2])+1
                for i in range(nodi):
                    vicini[i]= []
            if linea.startswith('a'): # a 1 2 17
                pezzetto = linea.split()
                u = int(pezzetto[1])
                v = int(pezzetto[2])
                w = int(pezzetto[3])
                vicini[u].append([v,w])
                vicini[v].append([u,w])
    return vicini, nodi 



def operazioni(vicini, nodi, nome_file):
    with open(nome_file, 'r') as f:
        for linea in f:
            
            
            if linea.startswith('+'): # + 0 2 7
                pezzetto = linea.split()
                if len(pezzetto) != 4:
                    continue
                u = int(pezzetto[1])
                v = int(pezzetto[2])
                w = int(pezzetto[3])

                if u < 0 or u >= nodi or v < 0 or v >= nodi:
                    continue
                
                presente = 0
                for vic in vicini[u]:
                    if vic[0] == v:
                        presente = 1
                        break

                if presente == 0:
                    vicini[u].append([v,w])
                    vicini[v].append([u,w])
            


            elif linea.startswith('-'):
                pezzetto = linea.split()
                if len(pezzetto) != 3:
                    continue
                u = int(pezzetto[1])
                v = int(pezzetto[2])

                if u<0 or u>= nodi or v<0 or v>=nodi:
                    continue

                trovato = False
                elemento_daRimuovere = None

                for vic in vicini[u]:
                    if vic[0] == v:
                        trovato = True
                        elemento_daRimuovere = vic
                        break
                if trovato:
                    vicini[u].remove(elemento_daRimuovere)
                
                trovato = False
                elemento_daRimuovere = None

                for vic in vicini[v]:
                    if vic[0] == u:
                        trovato = True
                        elemento_daRimuovere = vic
                        break
                if trovato:
                    vicini[v].remove(elemento_daRimuovere)


def prim (vicini, nodi):
    visitati = set()
    costo_totale = 0
    num_componenti = 0



#decido da dove inizio trovando una componente non ancora visitata 
    for nodo_start in range(nodi):

        if nodo_start in visitati:
            continue
        num_componenti += 1
        coda = [(0,nodo_start)] # coda = [(0, 0)] Il peso è 0 perché il primo nodo non costa niente.



        while coda:
            peso , nodo = heapq.heappop (coda)
            if nodo in visitati: 
                continue

            visitati.add(nodo)
            costo_totale += peso 


            for vicino, w in vicini[nodo]:
                if vicino not in visitati:
                    heapq.heappush(coda, (w, vicino))

    return costo_totale, num_componenti


def main():
    if len(sys.argv) != 3:
        print("Uso: msf.py file_grafo file_archi", file=sys.stderr)
        sys.exit(1)

    vicini, nodi = leggi_grafo(sys.argv[1])
    operazioni(vicini, nodi, sys.argv[2])

    num_archi = 0

    for i in range(nodi):
        num_archi+= len(vicini[i])
    
    num_archi = num_archi // 2
    costo, num_componenti = prim(vicini, nodi)
    print(f"{num_archi} {num_componenti} {costo}")

main()







                    

            