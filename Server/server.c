#include <stdio.h>

#include <unistd.h>

#include <time.h>

#include <stdlib.h>

#include<sys/types.h>

#include<sys/stat.h>

#include<sys/errno.h>

#include<string.h>

#include <pthread.h>

#include <fcntl.h>

#include <sys/sem.h>

#include <semaphore.h>

#include <sys/wait.h>

#define PERMS 0666

//Fontes
#define NORMAL "\x1B[0m"
#define BOLD "\x1B[1m"

//Cores
#define RED  "\x1B[31m"
#define GREEN  "\x1B[32m"
#define YELLOW  "\x1B[33m"
#define BLUE  "\x1B[34m"
#define MAGENTA  "\x1B[35m"
#define CYAN  "\x1B[36m"
#define WHITE "\x1B[37m"

sem_t s;

int nr_mensagens=0; //numero de mensagens no servidor

typedef struct PEDIDO{ //A struct pedido serve para o servidor saber que tipo de pedido está a ser feito (1- Ficheiro, 2 - Criar Fila, etc..)
    int cliente; //cliente que faz o pedido
    int tipo; //tipo de pedido
    char diretoria[1024]; //DPATH do cliente
    char nome_ficheiro[1024]; //PATH para o ficheiro a pedir
    char mensagem[1024]; //Mensagem a enviar
    int eliminar;
}PEDIDO;

typedef struct RESPOSTA{ //Struct util apenas para uma ou duas situações
    int nr_mensagens; //armazena o numero de mensagems
    int sucesso; //flag que indica o sucesso da operação
    int cliente; //cliente a que respondo
}RESPOSTA;

typedef struct MENSAGEM{ //A struct mensagem serve para armazenar as mensagens
    int id; //id do mensagem
    int id_cliente; //id do cliente que enviou a mensagem
    char conteudo[1024]; //conteudo da mensagem
    struct MENSAGEM *nseg; //apontador para o mensagem seguinte da lista "mensagem"
    struct MENSAGEM *nant; //apontador para o mensagem anterior da lista "mensagem"
}MENSAGEM;

typedef struct FICHEIRO{ //Struct usada para a transferência de ficheiros
    char conteudo[4096]; //conteudo do ficheiro
    int sucesso; //sucesso da operação
}FICHEIRO;

//função que copia o conteudo de um ficheiro para uma variável do tipo FICHEIRO e a retorna
FICHEIRO socp(char *fonte, char *destino)
{
    int fdIn = open(fonte, O_RDWR);
    int fdOut = open(destino, O_WRONLY);
    FICHEIRO fich;
    if (fdIn<0)
    {
        goto fim;
    }

     if (fdOut<0)
    {
        fdOut=creat(destino, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    }
    int ch;
    char buffer[1];
    strcpy(fich.conteudo,"");
    while((ch=read(fdIn,buffer,1))>0)
    {
        strcat(fich.conteudo,buffer);
    }
    close(fdIn);
    close(fdOut);
    fim:
    return fich;
}
//--------------------------------------------
//incializa uma variável do tipo mensagem com valores padrão
MENSAGEM *makemensagem(int id,int cliente, char conteudo[1024])
{
            MENSAGEM *nv=(MENSAGEM *) malloc (sizeof(MENSAGEM));
            nv->id=id;
            nv->id_cliente=cliente;
            strcpy(nv->conteudo,conteudo);
            nv->nant=NULL;
            nv->nseg=NULL;
            return nv;
}


//insere um nó no fim duma lista de mensagems
MENSAGEM *insertLastmensagem(MENSAGEM *B,MENSAGEM *nv){
	MENSAGEM *aux = B;
	if(B==NULL){
		return nv;
	}
	while(aux->nseg!=NULL){
		aux=aux->nseg;
	}
	aux->nseg = nv;
	nv->nant = aux;
	return B;
}

//retorna um nodo de uma lista de mensagens onde o id é o enviado como argumento
MENSAGEM *retornamensagem (MENSAGEM *n,int id)
{
    MENSAGEM *aux=n;
    while(aux!=NULL)
    {
        if(aux->id==id)
        {
            return aux;
        }
        aux=aux->nseg;
    }
    return NULL;
}

//remove um nó duma lista de mensagems
MENSAGEM *removeNode(MENSAGEM *L,int id)
{
    MENSAGEM *R=retornamensagem(L,id);
    if(R==NULL)
    {
        return R;
    }
    if(L==R)
    {
        L=L->nseg;
    }
    if(R->nant!=NULL)
    {
        R->nant->nseg=R->nseg;
    }
    if(R->nseg!=NULL)
    {
        R->nseg->nant=R->nant;
    }
    R->nant=NULL;
    R->nseg=NULL;
    return L;
}

void *server ()
{
    umask(0);
    int i,allfifos[256];
    char clientdirectory[100],serverdirectory[1000],filedirectory[1000],toreceive[100],fifostring[20],fifochar[4];
    //criação do mensagem global
    MENSAGEM *msg=NULL;
    //criação do pedido global
    PEDIDO P;
    //just a file pointer
    FILE *file;
    //parent id
    int pid;
    //criação dos fifos (256 fifos)
    for(i=0;i<256;i++)
    {
        strcpy(fifostring,"/tmp/fifo.");
        snprintf(fifochar, 12, "%d", i); 
        strcat(fifostring,fifochar);
        mkfifo(fifostring, S_IFIFO | PERMS);
    }
    for(i=0;i<256;i++)
    {
        strcpy(fifostring,"/tmp/fifo.");
        snprintf(fifochar, 12, "%d", i); 
        strcat(fifostring,fifochar);
        if(i==0)
        {
            allfifos[i]=open(fifostring,0);
        }
        else
        {
            allfifos[i]=open(fifostring,1);
        }     
        strcpy(fifostring,"");
    }
    while(1)
    {
        pid=fork(); //criação de um processo filho
        if(pid==0) //se o processo for filho
        {
            read (allfifos[0], &P, sizeof(PEDIDO)); //leitura do pedido
            if(P.tipo==0) //tipo 0
            {
                RESPOSTA R;
                //preparação da resposta
                R.sucesso=1;
                R.nr_mensagens=nr_mensagens;
                //----------------------
                write (allfifos[P.cliente], &R, sizeof(RESPOSTA)); //escrita da resposta
                printf("%s%s[INFO]> %sO Cliente %s%d%s conectou-se%s\n",BOLD,BLUE,WHITE,YELLOW,P.cliente,WHITE,NORMAL);
                goto espera; //salto para a label espera
            }
            if(P.tipo==1) //tipo 1
            {
                char serverdirectory[200],clientdirectory[200];
                RESPOSTA R;
                FICHEIRO fich;
                getcwd(serverdirectory,sizeof(serverdirectory));
                strcat(serverdirectory,"/");
                strcat(serverdirectory,P.nome_ficheiro);
                strcpy(clientdirectory,P.diretoria);
                strcat(clientdirectory,P.nome_ficheiro);
                file = fopen(P.nome_ficheiro,"r+"); //abertura do ficheiro 
                if(file==NULL) //falha ao abrir o ficheiro
                {
                    fich.sucesso=0;
                }
                else
                { 
                    fich=socp(serverdirectory,clientdirectory); //copia do ficheiro
                    fich.sucesso=1;  
                }
                
                write (allfifos[P.cliente], &fich, sizeof(FICHEIRO)); //escrita da resposta (envio de uma variável ficheiro)
                printf("%s%s[INFO]> %sO Cliente %s%d%s pediu um ficheiro%s\n",BOLD,BLUE,WHITE,YELLOW,P.cliente,WHITE,NORMAL);
                goto espera; //salto para a label espera
            } 
            if(P.tipo==2) //tipo 2
            {
                sem_wait(&s);
                RESPOSTA R;
                MENSAGEM *nv;
                nv=makemensagem(nr_mensagens,P.cliente,P.mensagem); //cria um mensagem
                msg=insertLastmensagem(msg,nv); //insere o mensagem no final da lista n
                nr_mensagens++; //incrementa o nr de mensagems
                //prepara a resposta
                R.nr_mensagens=nr_mensagens;
                R.sucesso=1;
                write (allfifos[P.cliente], &R, sizeof(RESPOSTA)); //escreve a resposta
                sem_post(&s);
                printf("%s%s[INFO]> %sO Cliente %s%d%s enviou uma mensagem%s\n",BOLD,BLUE,WHITE,YELLOW,P.cliente,WHITE,NORMAL);
                goto espera; //salto para a label "espera"
            } 
            if(P.tipo==3) //tipo 3
            {
                MENSAGEM responde[nr_mensagens];
                MENSAGEM *swapp=msg;
                int i=0;;
                //copia dos nós da lista de n para um array do tipo mensagem
                while(i<nr_mensagens)
                {   
                responde[i].id=swapp->id;
                responde[i].id_cliente=swapp->id_cliente;
                strcpy(responde[i].conteudo,swapp->conteudo);
                    swapp=swapp->nseg;
                i++;
                }
                write (allfifos[P.cliente], &responde, nr_mensagens*sizeof(MENSAGEM)); //devolve o array como resposta
                goto espera; //salto para a label espera
            } 
            if(P.tipo==4) //tipo 4
            {
                sem_wait(&s);
                RESPOSTA R;
                msg=removeNode(msg,P.eliminar); //Remove o mensagem obtido
                nr_mensagens--; //decremente o numero de mensagems
                R.nr_mensagens=nr_mensagens;
                R.sucesso=1;
                write (allfifos[P.cliente], &R,sizeof(RESPOSTA)); //resposta
                sem_post(&s);
                printf("%s%s[INFO]> %sO Cliente %s%d%s removeu uma mensagem%s\n",BOLD,BLUE,WHITE,YELLOW,P.cliente,WHITE,NORMAL);
                goto espera; //salto para a label espera
            }   
            if(P.tipo==5) //tipo 5
            {
                RESPOSTA R;
                //preparação da resposta
                R.nr_mensagens=nr_mensagens;
                R.sucesso=1;
                //----------------------
                write(allfifos[P.cliente],&R,sizeof(RESPOSTA)); //resposta
                goto espera; //salto para a label espera
            } 
            if(P.tipo==6) //tipo 6
            {
                RESPOSTA R;
                //preparação da resposta
                R.sucesso=1;
                R.nr_mensagens=nr_mensagens;
                //----------------------
                write (allfifos[P.cliente], &R, sizeof(RESPOSTA)); //escrita da resposta
                printf("%s%s[INFO]> %sO Cliente %s%d%s desconectou-se%s\n",BOLD,BLUE,WHITE,YELLOW,P.cliente,WHITE,NORMAL);
                goto espera; //salto para a label espera
            }  
        }
        else //processo pai
        {
            espera: //label espera

            wait(NULL); //impede o servidor de desligar quando fica sem clientes
        }    
    }
    for(i=0;i<256;i++)
    {
        close(allfifos[i]);
    }
}

int main()
{
    umask(0);
    sem_init(&s,1,1);
    pthread_t limite;
    printf("%s%s[AVISO]> %s O servidor estará ligado por 15min, apoós esse tempo, será DESLIGADO%s\n\n",BOLD,YELLOW,WHITE,NORMAL);
    pthread_create(&limite,NULL,server,NULL); //cria uma thread que trata de todo o processo do servidor
    sleep(900); //espera 300 segundos aka 5min
    exit(EXIT_SUCCESS); //termina o processo
    return 0;
}
