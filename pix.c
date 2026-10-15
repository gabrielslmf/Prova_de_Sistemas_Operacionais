#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>

typedef struct {
    int numero, origem, valor;
} Pedido;

typedef struct {
    int numero;
} Trabalhador;

Pedido *fila;
Trabalhador *dados;
pthread_t *threads;
int *saldos, *ocupados;
int quantidade, tamanho_pool, protegido, tempo_ms;
int proximo = 0, concluidos = 0, encerrar = 0, saldo_destino = 0;
volatile sig_atomic_t mostrar_fila = 0;
pthread_mutex_t mutex_fila = PTHREAD_MUTEX_INITIALIZER, mutex_contas = PTHREAD_MUTEX_INITIALIZER;
sem_t pedidos_disponiveis;

void pedir_estado(int sinal) {
    (void)sinal;
    mostrar_fila = 1;
}

void mostrar_estado(void) {
    pthread_mutex_lock(&mutex_fila);
    printf("\n--- ESTADO DA FILA ---\n");
    printf("Concluidos: %d de %d\n", concluidos, quantidade);
    for (int i = 0; i < tamanho_pool; i++) {
        if (ocupados[i] == 0)
            printf("Thread %d: livre\n", i + 1);
        else
            printf("Thread %d: atendendo pedido %d\n", i + 1, ocupados[i]);
    }
    printf("Aguardando (%d):", quantidade - proximo);
    for (int i = proximo; i < quantidade; i++)
        printf(" %d", fila[i].numero);
    printf("\n----------------------\n\n");
    fflush(stdout);
    pthread_mutex_unlock(&mutex_fila);
}

void *atender(void *argumento) {
    Trabalhador *t = argumento;
    int indice = t->numero;

    while (1) {
        sem_wait(&pedidos_disponiveis);
        pthread_mutex_lock(&mutex_fila);

        if (proximo >= quantidade) {
            int terminou = encerrar;
            pthread_mutex_unlock(&mutex_fila);
            if (terminou)
                break;
            continue;
        }

        Pedido p = fila[proximo];
        proximo++;
        ocupados[indice] = p.numero;
        printf("Thread %d iniciou pedido %d\n", indice + 1, p.numero);
        fflush(stdout);
        pthread_mutex_unlock(&mutex_fila);

        usleep(tempo_ms * 1000);

        if (protegido)
            pthread_mutex_lock(&mutex_contas);

        if (saldos[p.origem] >= p.valor) {
            int saldo_lido = saldo_destino;
            if (!protegido)
                usleep(5000);
            saldos[p.origem] -= p.valor;
            saldo_destino = saldo_lido + p.valor;
        }

        if (protegido)
            pthread_mutex_unlock(&mutex_contas);

        pthread_mutex_lock(&mutex_fila);
        ocupados[indice] = 0;
        concluidos++;
        printf("Thread %d terminou pedido %d\n", indice + 1, p.numero);
        fflush(stdout);
        pthread_mutex_unlock(&mutex_fila);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s N POOL PROTECAO TEMPO_MS\n", argv[0]);
        printf("PROTECAO: 0 = sem mutex, 1 = com mutex\n");
        return 1;
    }

    quantidade = atoi(argv[1]);
    tamanho_pool = atoi(argv[2]);
    protegido = atoi(argv[3]);
    tempo_ms = atoi(argv[4]);

    if (quantidade < 1 || quantidade > 10000 || tamanho_pool < 1 ||
        tamanho_pool > quantidade || (protegido != 0 && protegido != 1) ||
        tempo_ms < 1 || tempo_ms > 10000) {
        printf("Parametros invalidos.\n");
        return 1;
    }

    fila = malloc(quantidade * sizeof(Pedido));
    saldos = malloc(quantidade * sizeof(int));
    ocupados = calloc(tamanho_pool, sizeof(int));
    threads = malloc(tamanho_pool * sizeof(pthread_t));
    dados = malloc(tamanho_pool * sizeof(Trabalhador));

    if (!fila || !saldos || !ocupados || !threads || !dados) {
        printf("Erro ao reservar memoria.\n");
        return 1;
    }

    for (int i = 0; i < quantidade; i++) {
        saldos[i] = 100;
        fila[i].numero = i + 1;
        fila[i].origem = i;
        fila[i].valor = 100;
    }

    sem_init(&pedidos_disponiveis, 0, 0);
    signal(SIGINT, pedir_estado);
    printf("SIMULACAO PIX: %d envios, pool de %d threads, protecao %d\n",
           quantidade, tamanho_pool, protegido);
    printf("Cada usuario envia R$ 1,00 para a mesma conta.\n");
    printf("Pressione Ctrl+C para ver o estado da fila.\n");
    fflush(stdout);

    for (int i = 0; i < tamanho_pool; i++) {
        dados[i].numero = i;
        if (pthread_create(&threads[i], NULL, atender, &dados[i]) != 0) {
            printf("Erro ao criar thread.\n");
            return 1;
        }
    }
    for (int i = 0; i < quantidade; i++)
        sem_post(&pedidos_disponiveis);

    while (1) {
        if (mostrar_fila) {
            mostrar_fila = 0;
            mostrar_estado();
        }

        pthread_mutex_lock(&mutex_fila);
        int pronto = concluidos == quantidade;
        pthread_mutex_unlock(&mutex_fila);
        if (pronto)
            break;
        usleep(10000);
    }

    pthread_mutex_lock(&mutex_fila);
    encerrar = 1;
    pthread_mutex_unlock(&mutex_fila);

    for (int i = 0; i < tamanho_pool; i++)
        sem_post(&pedidos_disponiveis);
    for (int i = 0; i < tamanho_pool; i++)
        pthread_join(threads[i], NULL);

    printf("\nSaldo esperado no destino: R$ %d,00\n", quantidade);
    printf("Saldo encontrado no destino: R$ %d,00\n", saldo_destino / 100);
    printf("Valor perdido na simulacao: R$ %d,00\n",
           (quantidade * 100 - saldo_destino) / 100);

    sem_destroy(&pedidos_disponiveis);
    pthread_mutex_destroy(&mutex_fila);
    pthread_mutex_destroy(&mutex_contas);
    free(fila);
    free(saldos);
    free(ocupados);
    free(threads);
    free(dados);
    return 0;
}
