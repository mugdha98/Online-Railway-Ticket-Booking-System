#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <ifaddrs.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include<errno.h>

#define PORT 15755
#define PASS_LENGTH 20
#define TRAIN "./db/train"
#define BOOKING "./db/booking"


struct account{
	int id;
	char name[10];
	char pass[PASS_LENGTH];
};

struct train{
	int tid;
	char train_name[20];
	int train_no;
	int av_seats;
	int last_seatno_used;
};

struct bookings{
	int bid;
	int type;
	int acc_no;
	int tr_id;
	char trainname[20];
	int seat_start;
	int seat_end;
	int cancelled;
};

char *ACC[3] = {"./db/accounts/customer", "./db/accounts/agent", "./db/accounts/admin"};

void service_cli(int sock);
int login(int sock);
int signup(int sock);
int menu2(int sock, int id);//for customer and agent
int menu1(int sock, int id, int type);//for admin
void view_booking(int sock, int id, int type);// for customer and agent
//void view_booking2(int sock, int id, int type);//for cancelling ticket
void sigstop_sigkill_handler(int sig);

void sigstop_sigkill_handler(int sig) {
	char str[5];
	printf("\n\t\t\tDo you want to stop the server(y/n)?:: ");
	scanf("%s", str);
	if(strcmp("y", str) == 0) {
		exit(0);
	}
	return;
}
int main(){
	signal(SIGTSTP, sigstop_sigkill_handler); //SIGTSTP is used to stop the process
	//ctrl+Z
	signal(SIGINT, sigstop_sigkill_handler);
	/*The SIGINT signal is sent to a process by its controlling terminal when a user wishes to interrupt the process. This is typically initiated by pressing Ctrl+C*/
	signal(SIGQUIT, sigstop_sigkill_handler); 
	
	//creation of socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	//AF_INET is used to specify connection for internet
	//SOCK_STREAM is used to create connection oriented TCP socket
	//0 specifies default protocol
	if(sockfd==-1) { //socket programming returns -1 on failure else it will return +ve number
		printf("Socket Creation Failed.\n");
		exit(0);
	}
	
	
	int optval = 1;
	int optlen = sizeof(optval);
	//setsockopt is used to manipulate for the socket referred to by file descriptor sockfd
	//on sucess 0 is returned else -1
	//for socket options we have to set the socket level as SOL_SOCKET for all sockets
	//SO_REUSEPORT permits multiple AF_INET sockets to be bound to an identical socket address.
	//It must be set on each socket prior to calling bind on socket
	//SO_REUSEADDR should allow reuse of local addresses.
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, optlen)==-1){
		printf("Set Socket Options Failed!!!\n");
		exit(0);
	}
	
	//For Binding
	struct sockaddr_in sa;//it stores the system info in which we have to bind 
	//we use this data structure to bind socket
	sa.sin_family = AF_INET; //for internet //used to specify the protocol family
	sa.sin_addr.s_addr = htonl(INADDR_ANY);  //used to store IP 
	//htonl means we are converting host byte order to network byte order. ex: if we are transferring some data to comp 1 to comp2 then data in comp1 is saved in host byte order so in order to transfer it in network we have to convert it in network byte order
	//INADDR_ANY we have to bind all available IP Address. 
	sa.sin_port = htons(PORT); //port number of system will be stored in thsi data structure

	//bind the sockfd with the info present inside sa of size given as third argument
	//here we typecasted sa to sockaddr as bind takes value in this structure
	if(bind(sockfd, (struct sockaddr *)&sa, sizeof(sa))==-1){
		printf("Binding Port Failed!!!\n");
		exit(0);
	}
	//now the socket will be assigned with an IP and Port Number
	
	//For Listening
	//this 100 is also called as backlog
	//listen is a loopback call it will wait till any client joins the network
	//so it says that listen in this socket (sockfd)
	// if no of request exeeds 100 then server will not entertain them
	if(listen(sockfd, 100)==-1){ //#f connections he has to wait here it is 100
		perror("Listen Failed!!!\n");
		exit(0);
	}
	
	//as soon as a request arises
	while(1){ 
		int connectedfd;
		//here server doesnot want to keep the information of client so the last 2 arguments are set as NULL as struct sockaddr will contain the info about client IP and port and the last argument contains the size of sockaddr
		//so it says accept the socket
		//if error then -1 else positive number
		if((connectedfd = accept(sockfd, (struct sockaddr *)NULL, NULL))==-1){
			printf("Connection Error!!!\n");
			exit(0);
		}
		
		//after successfully accepting the request child process is created
		if(fork()==0) 
			service_cli(connectedfd); // it will be called when call is successfully accepted
	}

	close(sockfd);
	return 0;
}
//reading the sock value from first page
void service_cli(int sock){
	int func_id;
	read(sock, &func_id, sizeof(int)); // reading the value from sock given from client side and storing it in func_id
	printf("\n\t\t\tClient [%d] Connected\n", sock);
	while(1){		
		if(func_id==1) {login(sock);read(sock, &func_id, sizeof(int));}
		if(func_id==2) {signup(sock);read(sock, &func_id, sizeof(int));}
		if(func_id==3) break;
	}
	close(sock);
	printf("\n\t\t\tClient [%d] Disconnected\n", sock);
}
//Login
int login(int sock){
	int type, acc_no, fd, valid=1, invalid=0, login_success=0;
	char password[PASS_LENGTH];
	struct account temp;
	//reading the credentials from client for verification
	read(sock, &type, sizeof(type)); //account type
	read(sock, &acc_no, sizeof(acc_no));
	read(sock, &password, sizeof(password));

	if((fd = open(ACC[type-1], O_RDWR))==-1)
		printf("File Error\n");
	//file opened
	//applying record locking so that multiple users can login simultaneously
	struct flock lock;
	
	lock.l_start = (acc_no-1)*sizeof(struct account); //specifying the starting point of record to be locked
	lock.l_len = sizeof(struct account); //specifying the length to be locked
	lock.l_whence = SEEK_SET;
	lock.l_pid = getpid(); //saving the pid of the process who is performing lock

	if(type == 1){// Customer
		lock.l_type = F_WRLCK; //so that multiples users don't login into the same account at the same time
		fcntl(fd,F_SETLK, &lock); //setting the lock to SETLK i.e if multiple users want to login into different account they can
		lseek(fd, (acc_no - 1)*sizeof(struct account), SEEK_CUR);//taking the pointer to that user account record
		read(fd, &temp, sizeof(struct account));// reading the record and storing it in temp
		if(temp.id == acc_no){ //checking if account number entered was correct or not
			if(!strcmp(temp.pass, password)){ //checking if the password entered is correct or not
				write(sock, &valid, sizeof(valid)); //sending the value 1 to client as user credentials matched
				while(-1!=menu1(sock, temp.id, type));
				login_success = 1;
			}
		}
		lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, &lock);
		close(fd);
		if(login_success)
		return 3;
	}
	else if(type == 2){// Agent		
		lock.l_type = F_RDLCK;
		fcntl(fd,F_SETLK, &lock);
		lseek(fd, (acc_no - 1)*sizeof(struct account), SEEK_SET);
		read(fd, &temp, sizeof(struct account));
		if(temp.id == acc_no){
			if(!strcmp(temp.pass, password)){
				write(sock, &valid, sizeof(valid));
				while(-1!=menu1(sock, temp.id, type));
				login_success = 1;
			}
		}
		lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, &lock);
		close(fd);
		if(login_success) return 3;
	
	}
	else if(type == 3){
		// Admin
		lock.l_type = F_WRLCK;
		int ret = fcntl(fd,F_SETLK, &lock);
		if(ret != -1){
			
			lseek(fd, (acc_no - 1)*sizeof(struct account), SEEK_SET);
			read(fd, &temp, sizeof(struct account));
			if(temp.id == acc_no){
				if(!strcmp(temp.pass, password)){
					write(sock, &valid, sizeof(valid));
					while(-1!=menu2(sock, temp.id));
					login_success = 1;
				}
			}
			lock.l_type = F_UNLCK;
			fcntl(fd, F_SETLK, &lock);
		}
		close(fd);
		if(login_success)
		return 3;
	}
	write(sock, &invalid, sizeof(invalid)); //if credentails were wrong then sending value as 0
	return 3;
}
//Sign UP
int signup(int sock){
	int type, fd, acc_no=0;
	char password[PASS_LENGTH], name[10];
	struct account temp;

	read(sock, &type, sizeof(type));
	read(sock, &name, sizeof(name));
	read(sock, &password, sizeof(password));

	if((fd = open(ACC[type-1], O_RDWR))==-1)
		printf("File Error\n");
	
	struct flock lock;
	lock.l_type = F_WRLCK;
	lock.l_start = 0;
	lock.l_len = 0;
	lock.l_whence = SEEK_SET;
	lock.l_pid = getpid();

	fcntl(fd,F_SETLKW, &lock);

	int fp = lseek(fd, 0, SEEK_END);

	if(fp==0){
		temp.id = 1;
		strcpy(temp.name, name);
		strcpy(temp.pass, password);
		write(fd, &temp, sizeof(temp));
		write(sock, &temp.id, sizeof(temp.id)); //sending the account number to client for further login
	}
	else{
		fp = lseek(fd, -1 * sizeof(struct account), SEEK_END);
		read(fd, &temp, sizeof(temp));
		temp.id++;
		strcpy(temp.name, name);
		strcpy(temp.pass, password);
		write(fd, &temp, sizeof(temp));
		write(sock, &temp.id, sizeof(temp.id));
	}
	

	lock.l_type = F_UNLCK;
	fcntl(fd, F_SETLK, &lock);

	close(fd);
	return 3;
}

int menu2(int sock, int id){
	int op_id;
	read(sock, &op_id, sizeof(op_id));
	if(op_id == 1){
		int tid = 0;
		int tno;
		char tname[20];
		read(sock, &tname, sizeof(tname));
		printf("Train Name Read:: %s\n",tname);
		//printf("Train Number:: %d\n",tno);
		read(sock, &tno, sizeof(tno));
		printf("Train Number Read:: %d\n",tno);
		struct train temp, temp2;

		temp.tid = tid;
		temp.train_no = tno;
		strncpy(temp.train_name, tname, 20);
		temp.av_seats = 20;
		temp.last_seatno_used = 0;

		int fd = open(TRAIN, O_RDWR);
		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd, F_SETLKW, &lock);

		int fp = lseek(fd, 0, SEEK_END);
		if(fp == 0){
			write(fd, &temp, sizeof(temp));
			lock.l_type = F_UNLCK;
			fcntl(fd, F_SETLK, &lock);
			close(fd);
			write(sock, &op_id, sizeof(op_id));
		}
		else{
			lseek(fd, -1 * sizeof(struct train), SEEK_CUR);
			read(fd, &temp2, sizeof(temp2));
			temp.tid = temp2.tid + 1;
			write(fd, &temp, sizeof(temp));
			write(sock, &op_id, sizeof(op_id));	
			lock.l_type = F_UNLCK;
			fcntl(fd, F_SETLK, &lock);
			close(fd);
		}
		return op_id;
	}
	if(op_id == 2){
		int fd = open(TRAIN, O_RDWR);

		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd, F_SETLKW, &lock);

		int fp = lseek(fd, 0, SEEK_END);
		int no_of_trains = fp / sizeof(struct train);
		write(sock, &no_of_trains, sizeof(int));
		lseek(fd, 0, SEEK_SET);
		struct train temp;
		while(fp != lseek(fd, 0, SEEK_CUR)){
			read(fd, &temp, sizeof(struct train));
			write(sock, &temp.tid, sizeof(int));
			write(sock, &temp.train_name, sizeof(temp.train_name));
			write(sock, &temp.train_no, sizeof(int));			
		}
		read(sock, &no_of_trains, sizeof(int));
		if(no_of_trains != -2)
		{
			struct train temp;
			lseek(fd, (no_of_trains)*sizeof(struct train), SEEK_SET);
			read(fd, &temp, sizeof(struct train));			
			strcpy(temp.train_name,"deleted");
			lseek(fd, -1*sizeof(struct train), SEEK_CUR);
			write(fd, &temp, sizeof(struct train));
		}
		write(sock, &no_of_trains, sizeof(int));
		lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, &lock);
		close(fd);
	}
	if(op_id == 3){
		int fd = open(TRAIN, O_RDWR);

		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd, F_SETLKW, &lock);

		int fp = lseek(fd, 0, SEEK_END);
		int no_of_trains = fp / sizeof(struct train);
		write(sock, &no_of_trains, sizeof(int));
		lseek(fd, 0, SEEK_SET);
		while(fp != lseek(fd, 0, SEEK_CUR)){
			struct train temp;
			read(fd, &temp, sizeof(struct train));
			write(sock, &temp.tid, sizeof(int));
			write(sock, &temp.train_name, sizeof(temp.train_name));
			write(sock, &temp.train_no, sizeof(int));			
		}
		read(sock, &no_of_trains, sizeof(int));

		struct train temp;
		lseek(fd, 0, SEEK_SET);
		lseek(fd, (no_of_trains-1)*sizeof(struct train), SEEK_CUR);
		read(fd, &temp, sizeof(struct train));			

		read(sock, &no_of_trains,sizeof(int));
		if(no_of_trains == 1){
			char name[20];
			write(sock, &temp.train_name, sizeof(temp.train_name));
			read(sock, &name, sizeof(name));
			strcpy(temp.train_name, name);
		}
		else if(no_of_trains == 2){
			write(sock, &temp.train_no, sizeof(temp.train_no));
			read(sock, &temp.train_no, sizeof(temp.train_no));
		}
		else{
			write(sock, &temp.av_seats, sizeof(temp.av_seats));
			read(sock, &temp.av_seats, sizeof(temp.av_seats));
		}

		no_of_trains = 3;
		lseek(fd, -1*sizeof(struct train), SEEK_CUR);
		write(fd, &temp, sizeof(struct train));
		write(sock, &no_of_trains, sizeof(int));

		lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, &lock);
		close(fd);
		return op_id;
	}
	if(op_id == 4){
		int type=3, fd, acc_no=0;
		char password[PASS_LENGTH], name[10];
		struct account temp;
		read(sock, &name, sizeof(name));
		read(sock, &password, sizeof(password));

		if((fd = open(ACC[type-1], O_RDWR))==-1)printf("File Error\n");
		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd,F_SETLKW, &lock);
		int fp = lseek(fd, 0, SEEK_END);
		fp = lseek(fd, -1 * sizeof(struct account), SEEK_CUR);
		read(fd, &temp, sizeof(temp));
		temp.id++;
		strcpy(temp.name, name);
		strcpy(temp.pass, password);
		write(fd, &temp, sizeof(temp));
		write(sock, &temp.id, sizeof(temp.id));
		lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, &lock);

		close(fd);
		op_id=4;
		write(sock, &op_id, sizeof(op_id));
		return op_id;
	}
	if(op_id == 5){
		int type, id;
		struct account var;
		read(sock, &type, sizeof(type));

		int fd = open(ACC[type - 1], O_RDWR);
		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_whence = SEEK_SET;
		lock.l_len = 0;
		lock.l_pid = getpid();

		fcntl(fd, F_SETLKW, &lock);

		int fp = lseek(fd, 0 , SEEK_END);
		int users = fp/ sizeof(struct account);
		write(sock, &users, sizeof(int));

		lseek(fd, 0, SEEK_SET);
		while(fp != lseek(fd, 0, SEEK_CUR)){
			read(fd, &var, sizeof(struct account));
			write(sock, &var.id, sizeof(var.id));
			write(sock, &var.name, sizeof(var.name));
		}

		read(sock, &id, sizeof(id));
		if(id == 0){write(sock, &op_id, sizeof(op_id));}
		else{
			lseek(fd, 0, SEEK_SET);
			lseek(fd, (id-1)*sizeof(struct account), SEEK_CUR);
			read(fd, &var, sizeof(struct account));
			lseek(fd, -1*sizeof(struct account), SEEK_CUR);
			strcpy(var.name,"deleted");
			strcpy(var.pass, "");
			write(fd, &var, sizeof(struct account));
			write(sock, &op_id, sizeof(op_id));
		}

		lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, &lock);

		close(fd);

		return op_id;
	}
	if(op_id == 6) {
		write(sock,&op_id, sizeof(op_id));
		return -1;
	}
}
//For Customer And Agent after successful Login
int menu1(int sock, int id, int type){ //id to know for which user we have to perform operation and type for which user type
	int op_id;
	read(sock, &op_id, sizeof(op_id)); //read from the client the option user mentioned
	//book a ticket
	if(op_id == 1){ 
		int fd = open(TRAIN, O_RDWR);
		
		//setting mandatory locking
		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd, F_SETLKW, &lock);

		struct train temp;
		int fp = lseek(fd, 0, SEEK_END);
		int no_of_trains = fp / sizeof(struct train);
		write(sock, &no_of_trains, sizeof(int));// returning the details of the train to client
		lseek(fd, 0, SEEK_SET);
		while(fp != lseek(fd, 0, SEEK_CUR)){
		//sending the details of train
			read(fd, &temp, sizeof(struct train));
			write(sock, &temp.tid, sizeof(int));
			write(sock, &temp.train_no, sizeof(int));	
			write(sock, &temp.av_seats, sizeof(int));	
			write(sock, &temp.train_name, sizeof(temp.train_name));		
		}
		//filling the memory pointed by temp with constant 0 upto the size of struct train
		memset(&temp,0,sizeof(struct train));
		int trainid, seats;
		read(sock, &trainid, sizeof(trainid)); //reading train id from client
		lseek(fd, trainid*sizeof(struct train), SEEK_SET);//Moving the pointer to hat specific record of train in order to get details
		read(fd, &temp, sizeof(struct train));//storing that particular train details in temp
		write(sock, &temp.av_seats, sizeof(int));//sending the available seats in that train to client
		read(sock, &seats, sizeof(seats));//reading the no of seats from client that user have booked 
		if(seats>0){ //it is for checking that user has not made any request which cant be fulfilled
			temp.av_seats -= seats; //reducing the available seats by val in order to know how much seats are now available 
			int fd2 = open(BOOKING, O_RDWR);
			fcntl(fd2, F_SETLKW, &lock); //setting the lock in order to write in the BOOKING file
			struct bookings bk;
			int fp2 = lseek(fd2, 0, SEEK_END);
			
			//if its not the first booking
			if(fp2 > 0)
			{
				lseek(fd2, -1*sizeof(struct bookings), SEEK_CUR);
				read(fd2, &bk, sizeof(struct bookings)); //reading the last booking id
				bk.bid++; //increasing the booking id by 1 to make a new booking
			}
			//if it is the first booking
			else 
				bk.bid = 0;
			
			bk.type = type; //whether customer is booking or agent
			bk.acc_no = id; //which customer is booking 
			bk.tr_id = trainid; //which train is getting booked
			bk.cancelled = 0; //so that booking don't get cancelled 
			strcpy(bk.trainname, temp.train_name); 
			bk.seat_start = temp.last_seatno_used + 1;
			bk.seat_end = temp.last_seatno_used + seats;
			temp.last_seatno_used = bk.seat_end; // so that we can track which seat was last booked 
			write(fd2, &bk, sizeof(bk)); //writing the booking details in the booking file
			//unlocking
			lock.l_type = F_UNLCK;
			fcntl(fd2, F_SETLK, &lock);
		 	close(fd2);
			lseek(fd, -1*sizeof(struct train), SEEK_CUR);
			write(fd, &temp, sizeof(temp)); //updating temp
		}
		fcntl(fd, F_SETLK, &lock);
	 	close(fd);

		if(seats<=0) //invalid request
			op_id = -1;
		write(sock, &op_id, sizeof(op_id)); //sending the status if -1 then not booked if 1 then booked
		return 1;
	}
	
	//View Booking
	if(op_id == 2){
		view_booking(sock, id, type);
		write(sock, &op_id, sizeof(op_id));
		return 2;
	}
	
	//Update Booking
	if(op_id == 3){
		view_booking(sock, id, type); //showing first all the booking to user

		int fd1 = open(TRAIN, O_RDWR);
		int fd2 = open(BOOKING, O_RDWR);
		//applying mandatory locking
		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd1, F_SETLKW, &lock);
		fcntl(fd2, F_SETLKW, &lock);

		int val;
		struct train temp1;
		struct bookings temp2;
		//Reading the Booking ID to be updated
		read(sock, &val, sizeof(int));	
		// read the booking to be updated
		lseek(fd2, 0, SEEK_SET);
		lseek(fd2, val*sizeof(struct bookings), SEEK_CUR);
		read(fd2, &temp2, sizeof(temp2)); //in temp2 : booking
		lseek(fd2, -1*sizeof(struct bookings), SEEK_CUR);
		
		// read the train details of the booking
		lseek(fd1, 0, SEEK_SET);
		lseek(fd1, (temp2.tr_id-1)*sizeof(struct train), SEEK_CUR);
		read(fd1, &temp1, sizeof(temp1)); //in temp1: train
		lseek(fd1, -1*sizeof(struct train), SEEK_CUR);


		read(sock, &val, sizeof(int));	//Reading the val from client (Increase or Decrease)

		//Increasing No of Seats
		if(val==1){
			read(sock, &val, sizeof(int)); //reading the value from client the no of seats they want to increase
			
			if(temp1.av_seats>= val){
				temp2.cancelled = 1; // cancelling the previous booking
				//temp1.av_seats += val;
				write(fd2, &temp2, sizeof(temp2)); //updating the value of booking with the previous booking id

				int tot_seats = temp2.seat_end - temp2.seat_start + 1 + val;
				struct bookings bk;
				
				int fp2 = lseek(fd2, 0, SEEK_END);
				lseek(fd2, -1*sizeof(struct bookings), SEEK_CUR);
				read(fd2, &bk, sizeof(struct bookings)); //reading the previous booking in order to make a new booking
				
				//bk.bid++; 
				bk.type = temp2.type;
				bk.acc_no = temp2.acc_no;
				bk.tr_id = temp2.tr_id;
				bk.cancelled = 0;
				strcpy(bk.trainname, temp2.trainname);
				bk.seat_start = temp1.last_seatno_used + 1;
				bk.seat_end = temp1.last_seatno_used + tot_seats;

				//temp1.av_seats -= tot_seats;
				temp1.av_seats -=val;
				temp1.last_seatno_used = bk.seat_end;
				
				lseek(fd2, -1*sizeof(struct bookings), SEEK_CUR);
				write(fd2, &bk, sizeof(bk)); //writing the new entry into the booking file
				write(fd1, &temp1, sizeof(temp1)); //updating the train info in the train file
			}
			else{
				op_id = -2;
				write(sock, &op_id, sizeof(op_id));
			}
		}
		//Decreasing No of seats
		else{		
			read(sock, &val, sizeof(int)); //No of Seats
			
			if(temp2.seat_end - val < temp2.seat_start){
				temp2.cancelled = 1;
				int x= temp2.seat_end-temp2.seat_start+1;
				//temp1.av_seats += val;
				temp1.av_seats += x;
			}
			else{
				temp2.seat_end -= val;
				temp1.av_seats += val;
			}
			write(fd2, &temp2, sizeof(temp2));
			write(fd1, &temp1, sizeof(temp1));
		}
		lock.l_type = F_UNLCK;
		fcntl(fd1, F_SETLK, &lock);
		fcntl(fd2, F_SETLK, &lock);
		close(fd1);
		close(fd2);
		if(op_id>0)
		write(sock, &op_id, sizeof(op_id));
		return 3;

	}
	//cancel booking
	if(op_id == 4) {
		
		view_booking(sock, id, type);


		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		int fd1 = open(TRAIN, O_RDWR);
		int fd2 = open(BOOKING, O_RDWR);
		fcntl(fd1, F_SETLKW, &lock);
		int val;
		struct train temp1;
		struct bookings temp2;
		read(sock, &val, sizeof(int));
		lseek(fd2, val*sizeof(struct bookings), SEEK_SET);
		

		lock.l_start = val*sizeof(struct bookings);
		lock.l_len = sizeof(struct bookings);
		fcntl(fd2, F_SETLKW, &lock);

		
		read(fd2, &temp2, sizeof(temp2));
		lseek(fd2, -1*sizeof(struct bookings), SEEK_CUR);
		

		lseek(fd1, (temp2.tr_id)*sizeof(struct train), SEEK_SET);
		lock.l_start = (temp2.tr_id)*sizeof(struct train);
		lock.l_len = sizeof(struct train);
		fcntl(fd1, F_SETLKW, &lock);
		read(fd1, &temp1, sizeof(temp1));
		lseek(fd1, -1*sizeof(struct train), SEEK_CUR);
		temp1.av_seats += temp2.seat_end - temp2.seat_start + 1;
		temp2.cancelled=1;
		write(fd2, &temp2, sizeof(temp2));
		write(fd1, &temp1, sizeof(temp1));
		
		lock.l_type = F_UNLCK;
		fcntl(fd1, F_SETLK, &lock);
		fcntl(fd2, F_SETLK, &lock);
		close(fd1);
		close(fd2);
		write(sock, &op_id, sizeof(op_id));
		return 4;
	}
	if(op_id == 5) {
		write(sock, &op_id, sizeof(op_id));
		return -1;
	}
	
	return 0;
}
//Used for View,Update
void view_booking(int sock, int id, int type){
	int fd = open(BOOKING, O_RDONLY);
	//applying mandatory lock
	struct flock lock;
	lock.l_type = F_RDLCK;
	lock.l_start = 0;
	lock.l_len = 0;
	lock.l_whence = SEEK_SET;
	lock.l_pid = getpid();
	
	fcntl(fd, F_SETLKW, &lock);

	int fp = lseek(fd, 0, SEEK_END);
	int entries = 0;
	if(fp == 0)
		write(sock, &entries, sizeof(entries)); //sending entries as 0 as there were no previous booking
	//if there are previous bookings
	else{
		struct bookings bk[10];
		while(fp>0 && entries<10){
			struct bookings temp;
			fp = lseek(fd, -1*sizeof(struct bookings), SEEK_CUR);
			read(fd, &temp, sizeof(struct bookings)); //reading the booking details and storing in temp
			if(temp.acc_no == id && temp.type == type && temp.cancelled==0) //checking booking with type and their ids since booking is common for all
				bk[entries++] = temp;
			fp = lseek(fd, -1*sizeof(struct bookings), SEEK_CUR);
		}
		write(sock, &entries, sizeof(entries)); //sending the no of entries to client
		for(fp=0;fp<entries;fp++){
			int x=bk[fp].seat_end-bk[fp].seat_start+1; //calculating total seats booked
			write(sock, &bk[fp].bid, sizeof(bk[fp].bid)); //sending booking id to client
			write(sock, &bk[fp].trainname, sizeof(bk[fp].trainname)); //sending train name to client
			write(sock, &x, sizeof(int)); //sending total seats booked to client
			//write(sock, &bk[fp].seat_start, sizeof(int));
			//write(sock, &bk[fp].seat_end, sizeof(int));
			write(sock, &bk[fp].cancelled, sizeof(int));// sending the value of cancelled to client
		}
	}
	lock.l_type = F_UNLCK;
	fcntl(fd, F_SETLK, &lock);
	close(fd);
}
