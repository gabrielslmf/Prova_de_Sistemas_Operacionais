# Avaliação de Sistemas Operacionais — INF0336

Simulação de transferências PIX

## Ideia do programa

O programa simula vários usuários enviando R$ 1,00 cada para uma mesma conta. A conta de destino começa com saldo zero, e cada conta de origem começa com R$ 1,00. Os valores ficam guardados em variáveis e em um vetor compartilhados pelas threads; não existe conexão com bancos nem com o PIX verdadeiro.

Usei uma thread pool com tamanho fixo. Cada thread pega o próximo pedido da fila, espera um tempo para simular o atendimento e realiza a transferência. Como os pedidos são retirados do começo da fila, o atendimento começa na ordem de chegada (FCFS). Depois que termina, a thread fica disponível para pegar outro pedido.

Nesta avaliação, N:1 significa N contas enviando dinheiro para uma conta de destino. Não é o modelo de mapeamento N:1 entre threads de usuário e do kernel: para a implementação foi usada a biblioteca Pthreads apresentada na aula.

## Compilação e execução

No terminal do Linux, dentro da pasta da avaliação:

```bash
gcc pix.c -o pix -pthread
./pix 10 2 0 5
./pix 10 2 1 5
```

Os quatro números são: quantidade de envios (N), tamanho da thread pool, proteção (0 sem mutex e 1 com mutex) e tempo de cada atendimento em milissegundos.

Para enxergar melhor o funcionamento da fila e do sinal:

```bash
./pix 6 2 1 700
```

Enquanto o programa estiver rodando, pressionar `Ctrl+C` mostra os pedidos aguardando, o pedido de cada thread e quantos já terminaram. O programa continua executando depois disso.

## 1. Condição de corrida

O recurso compartilhado é o conjunto de saldos das contas, principalmente o saldo da conta de destino. A seção crítica é o trecho que lê o saldo de destino, desconta da origem e atualiza o destino. Sem mutex, duas threads podem ler o mesmo saldo antigo e escrever valores que não consideram a transferência da outra.

Por exemplo, se o destino tem R$ 0,00, duas threads podem ler esse valor, cada uma somar R$ 1,00 e ambas gravarem R$ 1,00. O resultado correto seria R$ 2,00.

O programa permite executar sem a proteção para demonstrar o problema. Nesse modo, existe uma pausa de 5 ms entre ler e atualizar o saldo da conta de destino. Essa pausa facilita reproduzir a condição de corrida. É uma simulação propositalmente incorreta usada apenas no teste.

Os testes foram feitos com uma pool de até duas threads e 5 ms de atendimento por envio. Cada linha foi testada dez vezes em cada modo.

| N | Pool | Saldo esperado | Saldo sem mutex (10 execuções) | Testes com erro | Saldo com mutex (10 execuções) |
|---|---|---|---|---|---|
| 1 | 1 | R$ 1,00 | R$ 1,00 | 0/10 | R$ 1,00 |
| 2 | 2 | R$ 2,00 | R$ 1,00 | 10/10 | R$ 2,00 |
| 3 | 2 | R$ 3,00 | R$ 2,00 | 10/10 | R$ 3,00 |
| 4 | 2 | R$ 4,00 | R$ 2,00 | 10/10 | R$ 4,00 |
| 5 | 2 | R$ 5,00 | R$ 3,00 | 10/10 | R$ 5,00 |
| 10 | 2 | R$ 10,00 | R$ 5,00 | 10/10 | R$ 10,00 |
| 20 | 2 | R$ 20,00 | R$ 10,00 | 10/10 | R$ 20,00 |

Nesses testes, a falha apareceu a partir de N = 2, porque duas threads podiam atualizar a mesma conta. Isso não significa que o sistema sempre dará exatamente esses saldos: a ordem de execução das threads pode variar. Os dados de todas as execuções estão em `experimentos.csv`.

## 2. Proteção e FCFS

Para corrigir o problema dos saldos, usei `pthread_mutex_lock` antes da seção crítica e `pthread_mutex_unlock` ao sair dela. Assim, só uma thread por vez altera os valores das contas. Outro mutex protege as informações da fila e o contador de pedidos concluídos. Um semáforo avisa às threads quando existe um pedido para atender.

Para demonstrar o FCFS, usei seis pedidos e duas threads. Os pedidos 1 e 2 são retirados primeiro. Enquanto os dois estão em execução, os pedidos 3, 4, 5 e 6 aguardam. Quando há uma vaga, o primeiro pedido da fila é retirado. Depois vêm os pedidos 4, 5 e 6, nessa ordem. As threads podem terminar os pedidos em momentos diferentes, mas a retirada respeita a ordem da fila.

No comando `./pix 6 2 1 700`, cada pedido dura por volta de 700 ms. Isso permite enxergar os dois atendimentos simultâneos e os quatro pedidos que estão esperando.

## 3. Debug com sinal

O programa trata o sinal `SIGINT`, gerado ao pressionar `Ctrl+C`. O tratador do sinal marca que é necessário mostrar a fila. Em seguida, o programa principal imprime os dados enquanto segura o mutex da fila, para não pegar informações no meio de uma atualização.

Em um teste com seis pedidos, duas threads e 700 ms por pedido, o estado exibido foi:

```text
--- ESTADO DA FILA ---
Concluidos: 0 de 6
Thread 1: atendendo pedido 2
Thread 2: atendendo pedido 1
Aguardando (4): 3 4 5 6
----------------------
```

Depois disso, o programa continuou e o saldo final foi R$ 6,00. A saída completa desse teste está em `saida_debug.txt`.

## Materiais da disciplina usados

- Aula 4, sinais e tratadores de sinais.
- Aula 5, Pthreads, concorrência e thread pools.
- Aula 6, escalonamento FCFS e fila FIFO.
- Aula 7, condição de corrida, seção crítica e mutex POSIX.
- Aula 8, semáforos POSIX.
