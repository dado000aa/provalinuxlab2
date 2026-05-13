import java.util.*;
import java.io.*;

public class Msf {

    // classe interna per il risultato di Kruskal
    static class RisultatoKruskal {
        long costo;
        int componenti;

        RisultatoKruskal(long costo, int componenti) {
            this.costo = costo;
            this.componenti = componenti;
        }
    }

    static int find(int i, int[] parent) {
        if (parent[i] != i)
            parent[i] = find(parent[i], parent);
        return parent[i];
    }

    static void union(int x, int y, int[] parent, int[] rank) {
        int rx = find(x, parent);
        int ry = find(y, parent);
        if (rx == ry) return;
        if (rank[rx] < rank[ry]) parent[rx] = ry;
        else if (rank[rx] > rank[ry]) parent[ry] = rx;
        else { parent[ry] = rx; rank[rx]++; }
    }

    static int leggiGrafo(String nomeFile, List<int[]> archi) throws IOException {
        BufferedReader br = new BufferedReader(new FileReader(nomeFile));
        String linea;
        int numNodi = 0;

        while ((linea = br.readLine()) != null) {
            String[] parti = linea.split(" ");
            if (parti[0].equals("c")) {
                continue;
            } else if (parti[0].equals("p")) {
                numNodi = Integer.parseInt(parti[2]) + 1;
            } else if (parti[0].equals("a")) {
                int u = Integer.parseInt(parti[1]);
                int v = Integer.parseInt(parti[2]);
                int w = Integer.parseInt(parti[3]);
                if (u > v) { int tmp = u; u = v; v = tmp; }
                archi.add(new int[]{u, v, w});
            }
        }

        br.close();
        return numNodi;
    }

    static void applicaOperazioni(String nomeFile, List<int[]> archi, int numNodi) throws IOException {
        BufferedReader br = new BufferedReader(new FileReader(nomeFile));
        String linea;

        while ((linea = br.readLine()) != null) {
            String[] parti = linea.split(" ");

            if (parti[0].equals("+")) {
                if (parti.length != 4) continue;
                int u = Integer.parseInt(parti[1]);
                int v = Integer.parseInt(parti[2]);
                int w = Integer.parseInt(parti[3]);
                if (u < 0 || u >= numNodi || v < 0 || v >= numNodi) continue;

                int minNodo = Math.min(u, v);
                int maxNodo = Math.max(u, v);

                boolean presente = false;
                for (int[] arco : archi) {
                    if (arco[0] == minNodo && arco[1] == maxNodo) {
                        presente = true;
                        break;
                    }
                }
                if (!presente) archi.add(new int[]{minNodo, maxNodo, w});

            } else if (parti[0].equals("-")) {
                if (parti.length != 3) continue;
                int u = Integer.parseInt(parti[1]);
                int v = Integer.parseInt(parti[2]);
                if (u < 0 || u >= numNodi || v < 0 || v >= numNodi) continue;

                int minNodo = Math.min(u, v);
                int maxNodo = Math.max(u, v);

                for (int i = 0; i < archi.size(); i++) {
                    int[] arco = archi.get(i);
                    if (arco[0] == minNodo && arco[1] == maxNodo) {
                        archi.remove(i);
                        break;
                    }
                }
            } else {
                continue;
            }
        }

        br.close();
    }

    static RisultatoKruskal kruskal(List<int[]> archi, int numNodi) {
        archi.sort((a, b) -> a[2] - b[2]);

        int[] parent = new int[numNodi];
        int[] rank = new int[numNodi];
        for (int i = 0; i < numNodi; i++) {
            parent[i] = i;
            rank[i] = 0;
        }

        long costoMSF = 0;
        int numComponenti = numNodi;

        for (int[] arco : archi) {
            int u = arco[0];
            int v = arco[1];
            int w = arco[2];

            if (find(u, parent) != find(v, parent)) {
                union(u, v, parent, rank);
                costoMSF += w;
                numComponenti--;
            }
        }

        return new RisultatoKruskal(costoMSF, numComponenti);
    }

    public static void main(String[] args) throws IOException {
        if (args.length != 2) {
            System.err.println("Uso: java Msf file_grafo file_archi");
            System.exit(1);
        }

        List<int[]> archi = new ArrayList<>();
        int numNodi = leggiGrafo(args[0], archi);
        applicaOperazioni(args[1], archi, numNodi);

        RisultatoKruskal risultato = kruskal(archi, numNodi);

        System.out.println(archi.size() + " " + risultato.componenti + " " + risultato.costo);
    }
}