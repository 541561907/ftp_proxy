#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>

int proxy_cmd_socket    = 0;    //proxy listen控制连接
int accept_cmd_socket   = 0;    //proxy accept客户端请求的控制连接
int connect_cmd_socket  = 0;    //proxy connect服务器建立控制连接
int proxy_data_socket   = 0;    //proxy listen数据连接
int accept_data_socket  = 0;    //proxy accept得到请求的数据连接（主动模式时accept得到服务器数据连接的请求，被动模式时accept得到客户端数据连接的请求）
int connect_data_socket = 0;    //proxy connect建立数据连接 （主动模式时connect客户端建立数据连接，被动模式时connect服务器端建立数据连接）
int connect_data1_socket = 0;
int clientPort = 0; //port number
int clientPort1 = 0;
unsigned short bbb = 0;
int mode = 1;//default is active
int main(int argc, const char *argv[])
{
    int bindAndListenSocket1(); //bind and listen 21 port on server
    int bindAndListenSocket2(); //bind and listen a specific port on client
    int bindAndListenSocket3(); //bind and listen a specific port on server
    int acceptCmdSocket(); //build socket which connect the client and proxy for accept command
    int connectToServer(); //build socket which connect the socket and proxy for accept command
    int connectToClient(); //build socket which connect the client and proxy for accept command
    int acceptDataSocket(); //build socket which connect the client and proxy for accept data
    char fileName[200][250] = {0};
    int dataFlag = 0;
    fd_set master_set, working_set;  //文件描述符集合
    struct timeval timeout;          //select 参数中的超时结构体
    int selectResult = 0;     //select函数返回值
    int select_sd = 10;    //select 函数监听的最大文件描述符
    int d = 0; //all argument that used in "for" circle
    int ppp = 0;
    int o = 0;
    int p = 0;
    int g = 0;
    int k = 0;
    int fileNumber = 0;//file number in the cache
    int n = 0;
    int length = 0;
    int isTransfer = 0;
    int useCache = 0;
    char portNumber[20] = {0};
    char portNumber1[20] = {0};
    int port1 = 0, port2 = 0;
    int port3 = 0, port4 = 0;
    const char *delims = ",";
    char *result = NULL;
    char *result1 = NULL;
    int i = 0;
    char filename[2000] = {0};

    
    FD_ZERO(&master_set);   //清空master_set集合
    bzero(&timeout, sizeof(timeout));
    
    proxy_cmd_socket = bindAndListenSocket1();  //开启proxy_cmd_socket、bind（）、listen操作
    //开启proxy_data_socket、bind（）、listen操作
    proxy_data_socket = bindAndListenSocket2();
    FD_SET(proxy_cmd_socket, &master_set);  //将proxy_cmd_socket加入master_set集合
    FD_SET(proxy_data_socket, &master_set);//将proxy_data_socket加入master_set集合
    
    timeout.tv_sec = 600;    //Select的超时结束时间
    timeout.tv_usec = 0;    //ms
    
    while (1) {
        FD_ZERO(&working_set); //清空working_set文件描述符集合
        memcpy(&working_set, &master_set, sizeof(master_set)); //将master_set集合copy到working_set集合
        
        //select循环监听 这里只对读操作的变化进行监听（working_set为监视读操作描述符所建立的集合）,第三和第四个参数的NULL代表不对写操作、和误操作进行监听
        selectResult = select(select_sd, &working_set, NULL, NULL, &timeout);
        
        // fail
        if (selectResult < 0) {
            perror("select() failed\n");
            exit(1);
        }
        
        // timeout/
        if (selectResult == 0) {
            printf("select() timed out.\n");
            continue;
        }
        
        // selectResult > 0 时 开启循环判断有变化的文件描述符为哪个socket
        for (i = 0; i < select_sd; i++) {
            //判断变化的文件描述符是否存在于working_set集合
            if (FD_ISSET(i, &working_set)) {
                if (i == proxy_cmd_socket) {
                    accept_cmd_socket = acceptCmdSocket();  //执行accept操作,建立proxy和客户端之间的控制连接
                    connect_cmd_socket = connectToServer(); //执行connect操作,建立proxy和服务器端之间的控制连接
                    
                    //将新得到的socket加入到master_set结合中
                    FD_SET(accept_cmd_socket, &master_set);
                    FD_SET(connect_cmd_socket, &master_set);
                }
                
                else if (i == accept_cmd_socket) {
                    char buff[30000] = {0};
                    
                    if ((length = read(i, buff, 30000)) == 0) {
                        close(i); //如果接收不到内容,则关闭Socket
                        close(connect_cmd_socket);
                        
                        //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                        FD_CLR(i, &master_set);
                        FD_CLR(connect_cmd_socket, &master_set);
                        
                    } else {
                        //如果接收到内容,则对内容进行必要的处理，之后发送给服务器端（写入connect_cmd_socket）
                        //处理客户端发给proxy的request，部分命令需要进行处理，如PORT、RETR、STOR
                        //PORT
                            isTransfer = 0;//not transfer file
                        if(buff[0]=='P'&&buff[1]=='O'&&buff[2]=='R'&&buff[3]=='T'){//active mode
                            for (d = 18; d < strlen(buff)-2; d++)
                                portNumber[d-18] = buff[d];     //obtain the port number
                            
                            portNumber[length-20] = '\0';
                            result = strtok(portNumber, delims);
                            port1 = atoi(result);
                            result = strtok(NULL, delims);
                            port2 = atoi(result);
                            clientPort = 256*port1 + port2;
                            strcpy(buff, "PORT 192,168,56,101,97,169\r\n");//change the command
                            if (mode == 0){//if it is passive mode before, then change to active mode
                                close(proxy_data_socket);
                              //  FD_CLR(proxy_data_socket, &master_set);
                                proxy_data_socket = bindAndListenSocket2();
                              //  FD_SET(proxy_data_socket, &master_set);
                            }
                            mode = 1;
                            write(connect_cmd_socket, buff, strlen(buff));
                        }
                        else if (buff[0]=='R'&&buff[1]=='E'&&buff[2]=='T'&&buff[3]=='R'){              
        
                            isTransfer = 1;//transferring file
                            char toZero[2000] ={0};
                            
                            
                            dataFlag = -1;
                            strcpy(filename, toZero);
                            for (p = 5; p < length-2; p++)//obtain the file name
                                filename[p-5] = buff[p];

                                filename[length-7] = '\0';
                            
                            
                            for (k = 0; k < fileNumber; k++){//check if this file has been existed in cache
                                if ((strcmp(fileName[k],filename)) == 0){
                                    dataFlag = k;
                                }
                            }
                            
                            if (dataFlag == -1){// it is not in cache
                                useCache = 0; // not use cache
                                strcpy(fileName[fileNumber],filename);//put this file into cache
                                fileNumber++;
                                write(connect_cmd_socket, buff, strlen(buff));
                            }
                            else {//into cache
                                if (mode == 1) {//active mode
                                    useCache = 1;//use cache
                                    
                                    connect_data_socket = connectToClient();//first build a connection between proxy and client
                                                                            // to transfer file
                          
                                    char *zhongjie1 = "150 Opening BINARY mode data connection for '";//send to client to inform
                                                                                                      //it ready to receive data
                                    char zhongjie2[2000] = {0};
                                    for (ppp = 0; ppp < strlen(filename); ppp++)
                                         zhongjie2[ppp] = filename[ppp];
                                    zhongjie2[strlen(filename)] = '\0';
                                    char *zhongjie3 = "' (";
                                    char *zhongjie4 = "4373875";
                                    
                                    strcpy(buff, zhongjie1);
                                    strcat(buff, zhongjie2);
                                    strcat(buff, zhongjie3);
                                    strcat(buff, zhongjie4);
                                    strcat(buff, " bytes).\r\n");
                                    write(accept_cmd_socket, buff, strlen(buff));
                                    int fd1 = open(filename, O_RDWR);
                                    int length1 = read(fd1, buff, 30000);
                                    write(connect_data_socket, buff, length1);       
                                    for (;length1 > 0;){//write data 30000 bytes in one time
                                         length1 = read(fd1, buff, 30000);
                    
                                         write(connect_data_socket, buff, length1);
                                     }
                                    close(fd1);
               
                                    usleep(300000);
                                    close(connect_data_socket);
                                    char *zhongjie5 = "226 Transfer complete.\r\n";//tell client that I have finished transferred
                                    strcpy(buff, zhongjie5);
                                    write(accept_cmd_socket, buff, strlen(buff));
                                   
                                }
                                else if (mode == 0){//passive mode
                                   
                                    useCache = 1;//use cache
    
                                    if ((FD_ISSET(accept_data_socket, &master_set)) == 0){//check if the data connection has been build
                                            accept_data_socket = acceptDataSocket();
                                    }
                                    if (FD_ISSET(connect_data_socket, &master_set)){//check if the data connection has been build
                                        close(connect_data_socket);
                                        FD_CLR(connect_data_socket, &master_set);
                                    }

                                    
                                        char *zhongjie1 = "150 Opening BINARY mode data connection for '";
                                        char zhongjie2[2000] = {0};
                                        strcpy(zhongjie2, filename);
                                        char *zhongjie3 = "' (";
                                        char *zhongjie4 = "10816929";
                                    
                                        strcpy(buff, zhongjie1);
                                        strcat(buff, zhongjie2);
                                        strcat(buff, zhongjie3);
                                        strcat(buff, zhongjie4);
                                        strcat(buff, " bytes).\r\n");
                                    
                                        write(accept_cmd_socket, buff, strlen(buff));
                               
           
                                        for (ppp = 0; ppp >= 0; ppp++){//write data 30000 bytes in on time
                                            int fd1 = open(filename, O_RDWR);
                                            lseek(fd1, 30000*ppp, SEEK_SET);
                                            int length2 = read(fd1, buff, 30000);
                                            if (length2 != 30000){
                                                    close(fd1);
                                                    write(accept_data_socket, buff, length2);
                                                    break;
                                            }
                                        close(fd1);
                                        write(accept_data_socket, buff, length2);
                                    }     
                                        usleep(300000);                                 
                                        close(accept_data_socket);
                                    if (FD_ISSET(accept_data_socket, &master_set)){
                                        FD_CLR(accept_data_socket, &master_set);
                                    }
                                        char *zhongjie5 = "226 Transfer complete.\r\n";
                                        strcpy(buff, zhongjie5);
          
                                        write(accept_cmd_socket,buff,strlen(buff));
                   
         
                                }
                                
                            }
                        }
                        else{
                            write(connect_cmd_socket, buff, length);
                        }
                    }
                }
                
                else if (i == connect_cmd_socket) {
                    char buff[20000] = {0};
                    //处理服务器端发给proxy的reply，写入accept_cmd_socket
                    if ((length = read(i, buff, 20000)) == 0) {
                        close(i); //如果接收不到内容,则关闭Socket
                        close(accept_cmd_socket);
                        
                        //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                        FD_CLR(i, &master_set);
                        FD_CLR(accept_cmd_socket, &master_set);
                        
                    } else{
                        if (buff[0]=='2'&&buff[1]=='2'&&buff[2]=='7'){
                            for (o = 40; o < (length-3); o++)//obtain the port number
                                portNumber1[o-40] = buff[o];
                            portNumber1[length-42] = '\0'; 
                            result1 = strtok(portNumber1, delims);
                            port3 = atoi(result1);
                            result1 = strtok(NULL, delims);
                            port4 = atoi(result1);
                            clientPort1 = 256*port3 + port4;
                            
                            close(proxy_data_socket);
                          
                            proxy_data_socket = bindAndListenSocket3();
                                  
                            sprintf(buff, "227 Entering Passive Mode (192,168,56,101,105,%d)\r\n", 120+bbb);//change the command
                            mode = 0;
                            write(accept_cmd_socket, buff, strlen(buff));
                        }
                        
                        
                        else if (buff[0]=='2'&&buff[1]=='2'&&buff[2]=='6'){//226 means transfer complete.    
                            write(accept_cmd_socket, buff, length);
                        }
                
                        else
                            write(accept_cmd_socket, buff, length);
                    }
                }
                else if (i == proxy_data_socket) {
                    char buff[20000] = {0};
                    //建立data连接(accept_data_socket、connect_data_socket)
                 
                        accept_data_socket = acceptDataSocket();
                        connect_data_socket = connectToClient();
                        
                        FD_SET(accept_data_socket, &master_set);
                        FD_SET(connect_data_socket, &master_set);
                    
                }
                
                
                else if (i == accept_data_socket) {
                    char buff[20000] = {0};
                    //判断主被动和传输方式（上传、下载）决定如何传输数据
                    
                    if ((length = read(i, buff, 20000)) == 0) {
                        close(i); //如果接收不到内容,则关闭Socket
                        close(connect_data_socket);
                        
                        //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                        FD_CLR(i, &master_set);
                        FD_CLR(connect_data_socket, &master_set);
                        
                    } else {
                        
                        write(connect_data_socket, buff, length); 
                        if((mode == 1)&&(dataFlag == -1)&&(isTransfer == 1)){//write the data into this filename
                            int fd = open(filename, O_RDWR|O_CREAT);
                            lseek(fd, 0, SEEK_END);
                            write(fd, buff, length);
                        }
                    
                    }
                }
                
                else if (i == connect_data_socket) {
                    char buff[20000] = {0};
                    //判断主被动和传输方式（上传、下载）决定如何传输数据
                    if ((length = read(i, buff, 20000)) == 0) {
                        close(i); //如果接收不到内容,则关闭Socket
                        close(accept_data_socket);
       
                        //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                        FD_CLR(i, &master_set);
                        FD_CLR(accept_data_socket, &master_set);

                    } else{
                        write(accept_data_socket, buff, length);
                        if((mode == 0)&&(dataFlag == -1)&&(isTransfer == 1)){//write the data into this filename
                            int fd = open(filename, O_RDWR|O_CREAT);
                            lseek(fd, 0, SEEK_END);
                            write(fd, buff, length);
                         }
                        
                    }
                }
            }
        }
    }
    
    return 0;
}
int bindAndListenSocket1(){
    
    int sock;
    struct sockaddr_in echoServAddr;
    unsigned short echoServPort = 21;
    int queueLen = 5;
    
    if ((sock = socket(PF_INET, SOCK_STREAM,0)) < 0)
        printf("socket1() failed.\n");
    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(echoServPort);
    
    if ((bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr))) < 0)
        printf("bind1() failed.\n");
    
    if ((listen(sock, queueLen)) != 0)
        printf("listen error.\n");
    
    return sock;
    
}

int bindAndListenSocket2(){
    int sock;
    struct sockaddr_in echoServAddr;
    unsigned short echoServPort = 25001;
    int queueLen = 8;
    
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        printf("socker2() failed.\n");
    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(echoServPort);
    
    if ((bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr))) < 0)
        printf("bind2() failed.\n");
    
    if ((listen(sock, queueLen)) != 0)
        printf("listen error.\n");
    
    return sock;
}


int bindAndListenSocket3(){
    bbb++;
    int sock;
    struct sockaddr_in echoServAddr;
    unsigned short echoServPort = 27000+bbb;
    int queueLen = 7;
    
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        printf("socker3() failed.\n");
    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(echoServPort);
    
    if ((bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr))) < 0)
        printf("bind3() failed.\n");
    
    if ((listen(sock, queueLen)) != 0)
        printf("listen error.\n");
    
    return sock;
}

int acceptCmdSocket(){
    int connfd;
    if ((connfd = accept(proxy_cmd_socket, NULL, NULL)) == -1)
        printf("accept() failed.\n");
    
    return connfd;
}

int acceptDataSocket(){
    int connfd1;
    if ((connfd1 = accept(proxy_data_socket, NULL, NULL)) == -1)
        printf("accept111() failed.\n");
    
    return connfd1;
}

int connectToServer(){
    int sock;
    struct sockaddr_in echoServAddr;
    unsigned short echoServPort = 21;
    char *servIP = "192.168.56.1";
    
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        printf("socket() failed.\n");
    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);
    echoServAddr.sin_port = htons(echoServPort);
    
    if ((connect(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr))) != 0)
        printf("connect() failed.\n");
    
    return sock;
}

int connectToClient(){
    int sock1;
    struct sockaddr_in echoServAddr1;
    unsigned short echoServPort1 = clientPort;
    if (mode == 0)
        echoServPort1 = clientPort1;
    char *servIP1 = "192.168.56.1";
    if ((sock1 = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        printf("socke11111111t() failed.\n");
    memset(&echoServAddr1, 0, sizeof(echoServAddr1));
    echoServAddr1.sin_family = AF_INET;
    echoServAddr1.sin_addr.s_addr = inet_addr(servIP1);
    echoServAddr1.sin_port = htons(echoServPort1);
    
    if ((connect(sock1, (struct sockaddr *) &echoServAddr1, sizeof(echoServAddr1))) != 0)
        printf("connect111() failed.\n");
    
    return sock1;
}



