
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

int cliente=0; //id do cliente
int nr_mensagens=0; //nr de mensagens no servidor

struct stat st = {0};

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
    struct mensagem *nseg; //apontador para o mensagem seguinte da lista "mensagem"
    struct mensagem *nant; //apontador para o mensagem anterior da lista "mensagem"
}MENSAGEM;

typedef struct FICHEIRO{ //Struct usada para a transferência de ficheiros
    char conteudo[4096]; //conteudo do ficheiro
    int sucesso; //sucesso da operação
}FICHEIRO;

//Menus

void menu_login()
{
    printf("%s%sMenu Login?%s\n",MAGENTA,BOLD,NORMAL);
    printf(" %s%s1-%s Registar\n %s%s2-%s Login\n %s%s0-%s Sair\n\n",MAGENTA,BOLD,NORMAL,MAGENTA,BOLD,NORMAL,MAGENTA,BOLD,NORMAL);
}

void menu()
{
    printf("%s%sO que deseja fazer?%s\n",MAGENTA,BOLD,NORMAL);
    printf(" %s%s1-%s Pedir ficheiros\n %s%s2-%s Aceder ao sistema de mensagens\n %s%s0-%s Sair\n\n",MAGENTA,BOLD,NORMAL,MAGENTA,BOLD,NORMAL,MAGENTA,BOLD,NORMAL);
}

void menu_PedirFicheiro()
{
    printf(" %s%sPedir ficheiros%s\n",MAGENTA,BOLD,NORMAL);
    printf("  %s%s1-%s Pedir\n  %s%s0-%s Sair\n\n",MAGENTA,BOLD,NORMAL,MAGENTA,BOLD,NORMAL);
}

void menu_Mensagem()
{
    printf(" %s%sSistema De Mensagens%s\n",MAGENTA,BOLD,NORMAL);
    printf("  %s%s1-%s Enviar Mensagens\n  %s%s2-%s Eliminar Mensagens\n  %s%s3-%s Atualizar\n  %s%s0-%s Sair\n\n",MAGENTA,BOLD,NORMAL,MAGENTA,BOLD,NORMAL,MAGENTA,BOLD,NORMAL,MAGENTA,BOLD,NORMAL);
}

//---------------------------------------------------------------------------------------------

void conectado(int readfd, int writefd)
{
    PEDIDO P;
    RESPOSTA R;
    P.cliente=cliente;
    P.tipo=0;
    write (writefd, &P, sizeof(PEDIDO)); //pedido
    read (readfd, &R, sizeof(RESPOSTA)); //resposta
    nr_mensagens=R.nr_mensagens;
}

void desconectado(int readfd, int writefd)
{
    PEDIDO P;
    RESPOSTA R;
    P.cliente=cliente;
    P.tipo=6;
    write (writefd, &P, sizeof(PEDIDO)); //pedido
    read (readfd, &R, sizeof(RESPOSTA)); //resposta
    nr_mensagens=R.nr_mensagens;
}

void atualizarMensagens(int readfd, int writefd)
{
    PEDIDO P;
    RESPOSTA R;
    P.cliente=cliente;
    P.tipo=5;
    write (writefd, &P, sizeof(PEDIDO)); //pedido
    read (readfd, &R, sizeof(RESPOSTA)); //resposta
    nr_mensagens=R.nr_mensagens;
}

void pedirFicheiro(int readfd, int writefd)
{
    char clientdirectory[200],nome[200],*secret,password[200],aux[200],content;
    PEDIDO P;
    FICHEIRO R;
    int i=0,j=0,fdOut;
    //----------------------------
    getcwd(clientdirectory,sizeof(clientdirectory)); //obtém a diretoria do cliente que fez o pedido
    strcat(clientdirectory,"/");
    strcpy(P.diretoria,clientdirectory);
    printf("%sEscreva o nome do %sficheiro%s a pedir%s\n",BOLD,MAGENTA,WHITE,NORMAL);
    fgets(nome,202,stdin);
    strtok(nome, "\n");
    printf("Nome: %s\n",nome);
    strcpy(aux,nome);
    secret=strtok(nome,"/"); //no caso da diretoria incluir uma subpasta
    if(strcmp(secret,"secreta")==0) //se o utilizador tentar aceder à pasta secreta
    {
        printf("%s%s[AVISO]> %sEstá a tentar aceder a uma pasta %sprivada%s, insira a %spassword%s\n",BOLD,YELLOW,WHITE,YELLOW,WHITE,YELLOW,NORMAL);
        fgets(password,256,stdin);
        strtok(password, "\n");
        if(strcmp(password,"admin")!=0) //password correta
        {
            goto requestFileError;
        }
    }
    P.cliente=cliente; //inserção do cliente no pedido
    P.tipo=1;   //inserção do tipo de pedido
    strcpy(P.nome_ficheiro,aux); //inserção do nome do ficheiro
    write (writefd, &P, sizeof(PEDIDO)); //pedido
    read (readfd, &R, sizeof(FICHEIRO)); //resposta
    if (stat(secret, &st) == -1) { //se a subpasta pedida nao existir 
    mkdir(secret, 0700); //crio a subpasta
    }
    if(R.sucesso==0) //Em caso de resposta negativa do servidor
    {
        requestFileError:
        printf("%s%sErro ao enviar o ficheiro%s\n",BOLD,RED,NORMAL);
    }
    else //em caso de resposta positiva do servidor
    {
        printf("%s%sFicheiro recebido com sucesso%s\n",BOLD,GREEN,NORMAL);
        fdOut = open(aux, O_WRONLY);
        if (fdOut<0) //se a file nao existir, crio
        {
            fdOut=creat(aux, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        }

        while(R.conteudo[i]!='\0') //ciclo para calcular o tamanho do conteudo do ficheiro
        {
            i++;   
        } 
        write(fdOut,R.conteudo,i); //escrita do conteudo no ficheiro na diretoria do cliente
    }
}

void enviarMensagem(int readfd, int writefd)
{
    char msg[100];
    RESPOSTA R;
    printf("%sEscreva a %smensagem%s a enviar%s\n ",BOLD,MAGENTA,WHITE,NORMAL);
    fgets(msg,256,stdin);
    strtok(msg, "\n");
    PEDIDO P;
    P.cliente=cliente; //inserção do cliente no pedido
    P.tipo=2; //inserção do tipo no pedido
    strcpy(P.mensagem,msg); //inserção da mensagem no pedido
    MENSAGEM areceber[1000];
    write (writefd, &P, sizeof(PEDIDO)); //pedido
    read (readfd, &R,sizeof(RESPOSTA)); //reposta
    nr_mensagens=R.nr_mensagens;
    if(R.sucesso==0)
    {
        printf("%s%sErro ao enviar mensagem%s\n",BOLD,RED,NORMAL);
    }
    else
    {
        printf("%s%sMensagem enviada%s\n",BOLD,GREEN,NORMAL);
    }
}

void consultarMensagens(int readfd, int writefd)
{
    atualizarMensagens(readfd,writefd);
    PEDIDO P;
    MENSAGEM M;
    P.cliente=cliente; //inserção do cliente no pedido
    P.tipo=3; //inserção do tipo no pedido
    MENSAGEM ArrayMensagens[nr_mensagens];
    int i;
    printf("%sExistem de momento %s%d%s mensagens%s\n",BOLD,YELLOW,nr_mensagens,WHITE,NORMAL);
    write (writefd, &P, sizeof(PEDIDO)); //pedido
    read (readfd, &ArrayMensagens, nr_mensagens*sizeof(MENSAGEM)); //resposta
    for(i=0;i<nr_mensagens;i++)
    {

            printf(" %s%s___%s%d%s_______________________________________________________%s\n",BOLD,BLUE,YELLOW,ArrayMensagens[i].id,BLUE,NORMAL);
            printf("%s%s|                                                           |%s\n",BOLD,BLUE,NORMAL);
            printf("%s  %sCliente:%s %s%d  \n  %s %s ",BOLD,MAGENTA,NORMAL,BOLD,ArrayMensagens[i].id_cliente,ArrayMensagens[i].conteudo,NORMAL);
            printf("\n%s%s|                                                           |%s\n",BOLD,BLUE,NORMAL);
            printf("%s%s ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾%s\n",BOLD,BLUE,NORMAL);
    }
}

void eliminarMensagens(int readfd, int writefd)
{
    PEDIDO P;
    RESPOSTA R;
    int del,delete;
    char del_str[2];
    del_str[1]='0';
    printf("%sInsira o %sid%s da mensagem que deseja %seliminar%s\n",BOLD,MAGENTA,WHITE,MAGENTA,NORMAL);
    fgets(del_str, 12, stdin);
    strtok(del_str, "\n");
    del=atoi(del_str);
    if(del<0 || del>nr_mensagens) //se for inserido um id óbviamente inválido
    {
        printf("%s%sId de mensagem inválido%s\n",BOLD,RED,NORMAL);
        goto eliminarMensagemErro;
    }
    delete=del;
    P.cliente=cliente; //inserção do cliente no pedido
    P.tipo=4; //inserção do tipo no pedido
    P.eliminar=delete;
    write (writefd, &P, sizeof(PEDIDO)); //pedido
    read (readfd, &R, sizeof(RESPOSTA)); //resposta
    nr_mensagens=R.nr_mensagens;
    if(R.sucesso==0)
    {
        eliminarMensagemErro:
        printf("%s%sErro ao eliminar a mensagem%s\n",BOLD,RED,NORMAL);
    }
    else
    {
        printf("%s%sMensagem eliminada com sucesso%s\n",BOLD,GREEN,NORMAL);
    }
}

void *client()
{
    umask(0);
    int menu_PedirFicheiros,exit_menuPedirFicheiros,menu_mensagens,exit_menuMensagem,menu_principal,menu_login_int=1;
    char fifostring[20],fifochar[4],menu_principal_str[1],menu_PedirFicheiros_str[1],menu_mensagens_str[1],menu_login_str[1],menu_login_user_str[256],menu_login_password_str[256],choice[1],username[256],password[256];
    int i=0,allfifos[256],int_choice=-1,id_utilizador,UserNameID;
    FILE *logins;
    //abertura dos fifos
    for(i=0;i<256;i++)
    {
        strcpy(fifostring,"/tmp/fifo.");
        snprintf(fifochar, 12, "%d", i); 
        strcat(fifostring,fifochar);
        if(i==0)
        {
            allfifos[i]=open(fifostring,1);
        }
        else
        {
            allfifos[i]=open(fifostring,0);
        }
        strcpy(fifostring,"");
    }
    //chamada à função conectado para avisar o servidor
    char userbuf[256],passbuf[256];
    int usado,cont;
    while(int_choice != 0)
    {
        usado=0;
        cont=0;
        menu_login();
        fgets(menu_login_str, 12, stdin);
        strtok(menu_login_str, "\n");
        menu_login_int=atoi(menu_login_str);
        switch (menu_login_int)
        {
            case 1:
                printf("REGISTAR:\n\n");
                printf("Insira o nome do utilizador:>  ");
                fgets(menu_login_user_str, 256, stdin);
                strtok(menu_login_user_str, "\n");
                printf("Insira a palavra passe:>  ");
                fgets(menu_login_password_str, 256, stdin);
                strtok(menu_login_password_str, "\n");
                logins = fopen ("logins.txt","r+");
                while(!feof(logins))
                {
                    fscanf(logins,"%s\n",userbuf);
                    fscanf(logins,"%s\n",passbuf);
                    if(strcmp(userbuf,menu_login_user_str)==0)
                    {
                        printf("%s%s[ERRO]> %s Utilizador já registado\n",BOLD,RED,WHITE);
                        usado=1;
                    }
                }
                fclose(logins);
                if(usado==0)
                {
                    logins = fopen("logins.txt","a");
                    fprintf(logins,"%s\n%s\n",menu_login_user_str,menu_login_password_str);
                    fclose(logins);
                }
            break;
            case 2:
                printf("LOGINS:\n\n");
                printf("Insira o nome do utilizador:>  ");
                fgets(menu_login_user_str, 256, stdin);
                strtok(menu_login_user_str, "\n");
                printf("Insira a palavra passe:>  ");
                fgets(menu_login_password_str, 256, stdin);
                strtok(menu_login_password_str, "\n");
                logins = fopen ("logins.txt","r+");
                cliente=0;
                while(!feof(logins))
                {
                    fscanf(logins,"%s\n",userbuf);
                    fscanf(logins,"%s\n",passbuf);
                    cliente++;
                    if(strcmp(userbuf,menu_login_user_str)==0 && strcmp(passbuf,menu_login_password_str)==0)
                    {
                        int_choice=0;
                        cont++;
                        goto aceite;
                    }
                }
                if(cont==0)
                {
                    printf("%s%s[ERRO]> %s Utilizador ou password inválidos\n",BOLD,RED,WHITE);
                }
                aceite:
                fclose(logins);
            break;
            case 0:
            desconectado(allfifos[cliente],allfifos[0]);
            exit(EXIT_SUCCESS);
            default:
            printf("%s%sOpção inválida%s\n",BOLD,RED,NORMAL);
            break;
    }
    }
    conectado(allfifos[cliente],allfifos[0]);
    //-----------------------------
    while(1)
    {
        printf("%sFoi-lhe atribuido o ID%s: %d%s!\n",BOLD,YELLOW,cliente,NORMAL);
        menu();
        fgets(menu_principal_str, 12, stdin);
        strtok(menu_principal_str, "\n");
        menu_principal=atoi(menu_principal_str);
        switch (menu_principal)
        {
            case 1:
                do
                {
                    menu_PedirFicheiro(allfifos[cliente],allfifos[0]);
                    fgets(menu_PedirFicheiros_str, 12, stdin);
                    strtok(menu_PedirFicheiros_str, "\n");
                    menu_PedirFicheiros=atoi(menu_PedirFicheiros_str);
                    switch (menu_PedirFicheiros)
                    {
                        case 1:
                            pedirFicheiro(allfifos[cliente],allfifos[0]);
                        break;
                        case 0:
                            exit_menuPedirFicheiros=1;
                        break;
                        default:
                            printf("%s%sOpção inválida%s\n",BOLD,RED,NORMAL);
                        break;
                    }
                } while (exit_menuPedirFicheiros!=1);
            break;

            case 2:
                do{
                consultarMensagens(allfifos[cliente],allfifos[0]);
                menu_Mensagem(allfifos[cliente],allfifos[0]);
                    fgets(menu_mensagens_str, 12, stdin);
                    strtok(menu_mensagens_str, "\n");
                    menu_mensagens=atoi(menu_mensagens_str);
                    switch (menu_mensagens)
                    {
                        case 1:
                            enviarMensagem(allfifos[cliente],allfifos[0]);
                        break;
                        case 2:
                            eliminarMensagens(allfifos[cliente],allfifos[0]);
                        break;
                        case 3:
                            printf("%srefreshing.%s..\n",BOLD,NORMAL);
                            sleep(1);
                            break;
                        break;
                        case 0:
                            exit_menuMensagem=1;
                        break;
                        default:
                            printf("%s%sOpção inválida%s\n",BOLD,RED,NORMAL);
                        break;
                    }
                } while (exit_menuMensagem!=1);
            break;
            case 0:
            desconectado(allfifos[cliente],allfifos[0]);
            exit(EXIT_SUCCESS);
            break;
            default:
                printf("%s%sOpção inválida%s\n",BOLD,RED,NORMAL);
                break;
        }   
    }
    desconectado(allfifos[cliente],allfifos[0]);
    exit(EXIT_SUCCESS);
}



int main()
{
    umask(0);
    pthread_t limite;
    printf("%s%s[AVISO]> %s A partir de agora tem 10 min para usar as funcionalidades do servidor, ao fim desse minutos, será DESCONECTADO%s\n\n",BOLD,YELLOW,WHITE,NORMAL);
    pthread_create(&limite,NULL,client,NULL); //criação de uma thread que trata de todo o processo do cliente
    sleep(600); //espera 60 segundos aka 1min
    exit(EXIT_SUCCESS); 
    return 0;
}