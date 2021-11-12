#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#define PORT 15755
#define PASS_LENGTH 20

int irctc(int sock);

int menu2(int sock, int type);
int do_admin_action(int sock, int action);
int do_action(int sock, int opt);
int view_booking(int sock);

int main(int argc, char * argv[]){
	char *ip = "127.0.0.1"; //loopback address
	if(argc==2){
		ip = argv[1];
	}
	
	//socket is created
	//AF_INET is for family protocol telling that it is for INTERNET
	//SOCK_STREAM is used for specifying the type of service here it is TCP
	//and 0 is for default protocol
	int cli_fd = socket(AF_INET, SOCK_STREAM, 0);
	
	//If Socket connection failed then it returns -1
	if(cli_fd == -1){
		printf("Socket Creation Failed.\n");
		exit(0);
	}
	
	//It stores the system info 
	struct sockaddr_in ca;
	ca.sin_family=AF_INET; // for family protocol
	ca.sin_port= htons(PORT); //to store the port number
	ca.sin_addr.s_addr = inet_addr(ip); //to store the IP address
	
	//cli_fd is the socket 
	//connect is a system call which connects the socket cli_fd to the address specified by ca
	
	if(connect(cli_fd, (struct sockaddr *)&ca, sizeof(ca))==-1){
		printf("Connect Failed.\n");
		exit(0);
	}
	printf("Connection Established.\n");
	
	while(irctc(cli_fd)!=3);// when user opts for exit in opening page so it will close the connection of client socket
	//else it will continue the execution
	close(cli_fd);

	return 0;
}

int irctc(int sock){
	int opt;
	system("clear");
	//first opening page
	printf("\n\n\t\t\tRAILWAY TICKET BOOKING SYSTEM\n\n");
	printf("\t\t\t1. Log In\n");
	printf("\t\t\t2. Sign Up\n");
	printf("\t\t\t3. Exit\n");
	printf("\t\t\tEnter Your Choice:: ");
	scanf("%d", &opt);
	write(sock, &opt, sizeof(opt)); //sending data into server in service_cli function
	
	//Login
	if(opt==1){
		int type, acc_no;
		char password[PASS_LENGTH];
		system("clear");
		printf("\n\t\t\tEnter the type of account You want to Log In:\n");
		printf("\t\t\t1. Customer\n\t\t\t2. Agent\n\t\t\t3. Administrator\n");
		printf("\t\t\tYour Response:: ");
		scanf("%d", &type);
		//Login system to enter their respective accounts
		printf("\t\t\tEnter Your Account Number:: ");
		scanf("%d", &acc_no);
		strcpy(password,getpass("\t\t\tEnter the password:: "));
		//sending the credentials in the server login() for verification
		write(sock, &type, sizeof(type));
		write(sock, &acc_no, sizeof(acc_no));
		write(sock, &password, strlen(password));
		
		int valid_login;
		read(sock, &valid_login, sizeof(valid_login));
		//login is valid therefore pass on to next menu
		if(valid_login == 1){
			while(menu2(sock, type)!=-1);// take the user to the second page
			system("clear");
			return 1;
		}
		//Login credentials were invalid therefore not allowing it to enter their accounts
		else{
			printf("\t\t\tLogin Failed \n");
			while(getchar()!='\n');
			getchar();
			return 1;
		}
	}
	else if(opt==2){
		int type, acc_no;
		char password[PASS_LENGTH], secret_pin[5], name[10];
		system("clear");
		printf("\n\t\t\tEnter The Type Of Account You Want to Sign UP:: \n");
		printf("\t\t\t1. Customer\n\t\t\t2. Agent\n\t\t\t3. Administration\n");
		printf("\t\t\tYour Response: ");
		scanf("%d", &type);
		printf("\t\t\tEnter Your Name: ");scanf("%s", name);
		strcpy(password,getpass("\t\t\tEnter Your Password: "));
		if(type == 3){
			int attempt = 3;
			while(1){
				strcpy(secret_pin, getpass("\t\t\tEnter secret code to create ADMIN account: "));
				attempt--;
				if(strcmp(secret_pin, "qwerty")!=0 && attempt>0) printf("\t\tThe code you entered is INVALID. You have %d more attempts\n",attempt);
				else break;
			}
			if(!strcmp(secret_pin, "qwerty"));
			else exit(0);
		}
		write(sock, &type, sizeof(type));
		write(sock, &name, sizeof(name));
		write(sock, &password, strlen(password));

		read(sock, &acc_no, sizeof(acc_no));
		printf("\t\tRemember Your Account Number For Further Logins:: %d\n", acc_no);
		while(getchar()!='\n');
		getchar();		
		return 2;
	}
	else
		return 3;
}

int menu2(int sock, int type){
	int opt = 0;
	if(type == 1 || type == 2){
		system("clear");
		printf("\t\tWELCOME YOU CAN CHOOSE ANYTHING FROM GIVEN OPTIONS\n");
		printf("\t\t\t1. Book Ticket\n");
		printf("\t\t\t2. View Bookings\n");
		printf("\t\t\t3. Update Booking\n");
		printf("\t\t\t4. Cancel booking\n");
		printf("\t\t\t5. Logout\n");
		printf("\t\tPlease Enter Your Choice: ");
		scanf("%d", &opt);
		return do_action(sock, opt);
		return -1;
	}
	else{
		system("clear");
		printf("\t\tWELCOME YOU CAN CHOOSE ANYTHING FROM GIVEN OPTIONS\n");
		printf("\t\t\t1. Add New Train\n");
		printf("\t\t\t2. Delete Train\n");
		printf("\t\t\t3. Modify Train\n");
		printf("\t\t\t4. Add Root User\n");
		printf("\t\t\t5. Delete User\n");
		printf("\t\t\t6. Logout\n");
		printf("\t\tPlease Enter Your Choice: ");
		scanf("%d", &opt);
		return do_admin_action(sock, opt);
	}
}
//For Administrators
int do_admin_action(int sock, int opt){
	switch(opt){
		case 1:{
			int tno;
			char tname[50];
			write(sock, &opt, sizeof(opt));
			printf("\t\t\tEnter Train Name:: ");
			scanf("%s", tname);
			fflush(stdin);
			printf("\t\t\tEnter Train Number:: ");
			scanf("%d", &tno);
			//fflush(stdin);
			//printf("%d",tno);
			write(sock, &tname, sizeof(tname));
			write(sock, &tno, sizeof(int));
			read(sock, &opt, sizeof(opt));
			if(opt == 1 ) printf("\t\t\tTrain Added Successfully.\n");
			while(getchar()!='\n');
			getchar();
			return opt;
			break;
		}
		case 2:{
			int no_of_trains;
			write(sock, &opt, sizeof(opt));
			read(sock, &no_of_trains, sizeof(int));
			//printf("no of train:%d",no_of_trains);
			if(no_of_trains<=0){
				printf("\n\t\t\tNo Trains available to delete\n");
				exit(0);
			}
			while(no_of_trains>0){
				int tid, tno;
				char tname[20];
				read(sock, &tid, sizeof(tid));
				read(sock, &tname, sizeof(tname));
				read(sock, &tno, sizeof(tno));
				if(strcmp(tname, "deleted")!=0)
					printf("\t\t\tTrain Id\tTrain Number\tTrain Name/n");
					printf("\t\t\t%d.\t%d\t%s\n", tid, tno, tname);
				no_of_trains--;
			}
			printf("\n\n\t\t\tEnter -2 To Cancel.\n\t\t\tEnter The Train ID To Delete:: "); 
			scanf("%d", &no_of_trains);
			//printf("no of train:%d",no_of_trains);
			write(sock, &no_of_trains, sizeof(int));
			read(sock, &opt, sizeof(opt));
			if(opt != -2) printf("\t\t\tTrain Deleted Successfully\n");
			else printf("\t\t\tOperation Cancelled!\n");
			while(getchar()!='\n');
			return opt;
			break;
		}
		case 3:{
			int no_of_trains;
			write(sock, &opt, sizeof(opt));
			read(sock, &no_of_trains, sizeof(int));
			if(no_of_trains<=0){
				printf("\n\t\t\tNo Trains available to modify\n");
				exit(0);
			}
			while(no_of_trains>0){
				int tid, tno;
				char tname[20];
				read(sock, &tid, sizeof(tid));
				read(sock, &tname, sizeof(tname));
				read(sock, &tno, sizeof(tno));
				if(!strcmp(tname, "deleted"));
				else{
					printf("\t\t\tTrain Id.\t\tTrain Number\t\tTrain Name\n");
					printf("\t\t\t%d.\t\t%d\t\t%s\n", tid+1, tno, tname);
				}
				
				no_of_trains--;
			}
			printf("\n\n\t\t\tEnter The Train ID To Modify: "); 
			scanf("%d", &no_of_trains);
			write(sock, &no_of_trains, sizeof(int));
			printf("\n\t\t\tWhat Parameter Do You Want To Modify?\n\t\t\t1. Train Name\n\t\t\t2. Train No.\n\t\t\t3. Available Seats\n");
			printf("\n\t\t\tYour Choice:: ");
			scanf("%d", &no_of_trains);
			write(sock, &no_of_trains, sizeof(int));
			if(no_of_trains == 2 || no_of_trains == 3){
				read(sock, &no_of_trains, sizeof(int));
				printf("\n\t\t\tCurrent Value:: %d\n", no_of_trains);				
				printf("\t\t\tEnter New Value:: ");scanf("%d", &no_of_trains);
				write(sock, &no_of_trains, sizeof(int));
			}
			else{
				char name[20];
				read(sock, &name, sizeof(name));
				printf("\n\t\t\tCurrent Value:: %s\n", name);
				printf("\t\t\tEnter New Value:: ");
				scanf("%s", name);
				write(sock, &name, sizeof(name));
			}
			read(sock, &opt, sizeof(opt));
			if(opt == 3) printf("\t\t\tTrain Data Modified Successfully\n");
			while(getchar()!='\n');
			return opt;
			break;
		}
		case 4:{
			write(sock, &opt, sizeof(opt));
			char pass[PASS_LENGTH],name[10];
			printf("\t\t\tEnter The Name: ");scanf("%s", name);
			strcpy(pass, getpass("\t\t\tEnter A Password For The ADMIN Account: "));
			write(sock, &name, sizeof(name));
			write(sock, &pass, sizeof(pass));
			read(sock, &opt, sizeof(opt));
			printf("\n\n\t\t\tThe Account Number For This ADMIN Is: %d\n", opt);
			read(sock, &opt, sizeof(opt));
			if(opt == 4)printf("\n\t\t\tSuccessfully Created ADMIN Account\n");
			while(getchar()!='\n');
			return opt;
			break;
		}
		case 5: {
			int choice, users, id;
			write(sock, &opt, sizeof(opt));
			printf("\n\t\t\tWhat Kind Of Account Do You Want To Delete?\n");
			printf("\t\t\t1. Customer\n\t\t\t2. Agent\n\t\t\t3. Admin\n");
			printf("\n\t\t\tYour Choice:: ");
			scanf("%d", &choice);
			write(sock, &choice, sizeof(choice));
			read(sock, &users, sizeof(users));
			if(users<=0){
				printf("\n\t\tNo Users to delete\n");
				exit(0);
			}
			while(users--){
				char name[10];
				read(sock, &id, sizeof(id));
				read(sock, &name, sizeof(name));
				if(strcmp(name, "deleted")!=0)
				printf("\t\t\tID\t Name\n");
				printf("\t\t\t%d\t\%s\n", id, name);
			}
			printf("\n\t\t\tEnter The ID To Delete: ");
			scanf("%d", &id);
			write(sock, &id, sizeof(id));
			read(sock, &opt, sizeof(opt));
			if(opt == 5) printf("\n\t\t\tSuccessfully Deleted User\n");
			while(getchar()!='\n');
			return opt;
		}
		case 6: {
			write(sock, &opt, sizeof(opt));
			read(sock, &opt, sizeof(opt));
			if(opt==6) printf("\n\t\t\tLogged Out Successfully.\n");
			while(getchar()!='\n');
			getchar();
			return -1;
			break;
		}
		default: return -1;
	}
}
//for Customers And Agents
int do_action(int sock, int opt){
	switch(opt){
		//Book a ticket
		case 1:{
			int trains, trainid, trainavseats, trainno, required_seats;
			char trainname[20];
			//sending the choice to server that user mentioned
			write(sock, &opt, sizeof(opt));
			//reading no of trains sent from server and storing it in trains
			read(sock, &trains, sizeof(trains));
			//If user tries to book a ticket and No train is available for booking
			if(trains<=0){
				perror("\n\t\tSorry,No trains available for booking.\n\t\tPlease Try Again Later!!");
				exit(0);
			}
			//If there are trains to book
			printf("\n\t\tTrain ID\tTrain No\tAvailable Seats\tTrain Name\n");
			while(trains--){
			//storing the details of train sent from server
				read(sock, &trainid, sizeof(trainid));
				read(sock, &trainno, sizeof(trainno));
				read(sock, &trainavseats, sizeof(trainavseats));
				read(sock, &trainname, sizeof(trainname));
				if(strcmp(trainname, "deleted")!=0)
			 	printf("\t\t%d\t\t%d\t\t%d\t\t%s\n",trainid,trainno,trainavseats,trainname);
			}
			//Taking the details from user
			printf("\n\t\tEnter The Train ID You Want To Book: ");
			scanf("%d", &trainid);
			write(sock, &trainid, sizeof(trainid));//sending the train id to server
			read(sock, &trainavseats, sizeof(trainavseats));//reading the available seats from server
			printf("\n\t\tEnter the number of seats you want to book: "); 
			scanf("%d", &required_seats);
			
			//if the required seats are less than or equal to the availble seats in that train then ticket will be booked
			if(trainavseats>=required_seats && required_seats>0)
				write(sock, &required_seats, sizeof(required_seats)); // sending the details of how much seat user has booked
			else{
				required_seats = -1;
				write(sock, &required_seats, sizeof(required_seats));
			}
			read(sock, &opt, sizeof(opt)); //reading the status of whether the tickets are booked or not
			
			if(opt == 1) printf("\n\n\tCongratulations!! Your Ticket is Booked Successfully!!\n");
			else printf("\n\nWe are sorry to inform you that your Tickets are not Booked. Please Try Again.\n");
			printf("\n\n\t\t\tPress Any Key To Continue...\n");
			while(getchar()!='\n');
			while(!getchar());
			return 1;
		}
		//View your bookings		
		case 2:{
			write(sock, &opt, sizeof(opt)); //sending the option value to server
			int ret=view_booking(sock);
			if(ret==-2) exit(0);
			read(sock, &opt, sizeof(opt));
			return 2;
		}
		//Update bookings
		case 3:{

			int val;
			write(sock, &opt, sizeof(opt));
			int ret_val=view_booking(sock);
			if(ret_val==-2) exit(0);
			if(ret_val!=-2)
			{
				printf("\n\t\t\tEnter The Booking ID To Be Updated:: "); scanf("%d", &val);
				write(sock, &val, sizeof(int));	//Sending Booking ID to server
				printf("\n\t\t\tWhat Information Do You Want To Update:\n\t\t\t1. Increase Number of Seats\n\t\t\t2. Decrease Number of Seats\n\t\t\tYour Choice:: ");
				scanf("%d", &val);
				write(sock, &val, sizeof(int));	//Sending the option user chosed ( Increase or Decrease ) to server
				//For increasing the seats
				if(val == 1){
					printf("\n\t\t\tHow Many Tickets Do You Want To Increase:: ");
					scanf("%d",&val);
					write(sock, &val, sizeof(int));	//Sedning the No of Seats to server
				}else if(val == 2){
					printf("\n\t\t\tHow Many Tickets Do You Want To Decrease:: ");scanf("%d",&val);
					write(sock, &val, sizeof(int));	//No of Seats		
				}
				read(sock, &opt, sizeof(opt));
				if(opt == -2)
					printf("\n\n\t\t\tOperation Failed. No More Available Seats.\n");
				else printf("\n\n\t\t\tUpdate Successfull.\n");
			}
			while(getchar()!='\n');
			return 3;
		}
		//Cancel booking
		case 4: {
			write(sock, &opt, sizeof(opt));
			int ret= view_booking(sock);
			if(ret==-2) exit(0);
			int val;
			printf("\n\t\t\tEnter The Booking ID To Be Deleted:: "); scanf("%d", &val);
			write(sock, &val, sizeof(int));	//Booking ID
			read(sock, &opt, sizeof(opt));
			if(opt == -2)
				printf("\n\n\t\t\tOperation Failed. No More Available Seats.\n");
			else printf("\n\n\t\t\tDeletion Successfull.\n");
			while(getchar()!='\n');
			return 3;
			}
		case 5: {
			write(sock, &opt, sizeof(opt));
			read(sock, &opt, sizeof(opt));
			if(opt == 5) printf("\n\n\t\t\tLogged Out Successfully.\n");
			while(getchar()!='\n');
			return -1;
			break;
		}
		default: return -1;
	}
}

int view_booking(int sock){
	int entries;
	read(sock, &entries, sizeof(int));
	//when there are no previous booking
	if(entries<= 0)
	{ 
		printf("\n\n\t\t\tNo Records Found.\n");
		return -2;
	}
	//when there are bookings
	else printf("\n\n\t\tShowing Your Recent %d Bookings:\n", entries);
	while(!getchar());
	//showing your enteries in client 
	while(entries--){
		int bid, bks_seat, bke_seat, cancelled;
		char trainname[20];
		
		read(sock,&bid, sizeof(bid)); //reading booking id from server
		read(sock,&trainname, sizeof(trainname));
		read(sock,&bks_seat, sizeof(int)); //reading total seats booked from server
		//read(sock,&bke_seat, sizeof(int));
		read(sock,&cancelled, sizeof(int)); 
		
		//checking if the booking is cancelled
		if(!cancelled) //if the booking is not cancelled then it will be shown to client 
		//printf("\t\tBookingID: %d\t1st Ticket: %d\tLast Ticket: %d\tTRAIN :%s\n", bid, bks_seat, bke_seat, trainname);
		printf("\t\tBookingID: %d\tTotal Tickets Booked: %d\tTRAIN :%s\n", bid, bks_seat, trainname);
	}
	printf("\n\n\t\t\tPress Any Key To Continue...\n");
	while(getchar()!='\n');

	return 0;
}
