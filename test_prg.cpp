/**
 *  Programmer : Akshay Gore
 *  Program Name : Goldchase
 *  Subject : CSCI 611
 */

#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <errno.h>
#include "goldchase.h"
#include "Map.h"
#include <fstream>
#include <string>
#include <signal.h>
#include <sys/types.h>
#include <mqueue.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h> //for read/write
#include <string.h> //for memset
#include <stdio.h> //for fprintf, stderr, etc.
#include<stdlib.h> //for exit
#include "fancyRW.h"


using namespace std;

/** GameBoard Structure for the Goldchase */

struct GameBoard {
	int rows; //4 bytes
	int cols; //4 bytes
	pid_t player_pid[5];
	unsigned char player;
	unsigned char map[0];
	int daemonID = 0;

};

/** Global Variables **/
Map *mapptr;
mqd_t readqueue_fd; //message queue file descriptor
struct mq_attr mq_attributes;
string mqueue_name[5];
bool sig=false;
int debugFD;
GameBoard* goldmap;
GameBoard* gm;
GameBoard* client_goldmap;
unsigned char* localClient;
int fd;
unsigned char* local_mapcopy;
sem_t* GameBoard_Sem;
int pipefd[2];
int NumberOfRows = -1;
int NumberOfColumns = -1;
unsigned char SockPlayer;
unsigned char byte,listen_var,listen_var1;
int sockfd,new_sockfd;
/** Function Declaration */
int insertGold(unsigned char*,int ,int ,int);
int insertPlayer(unsigned char*,unsigned char ,int ,int);
void handle_interrupt(int);
void read_message(int);
void clean_up(int);
void readQueue(string mqueue_name);
void writeQueue(string mqueue_name,string strMessage);
void refreshScreen(GameBoard* goldmap);
void create_server_daemon();
void sigusr1_handeler(int z);
void sigusr2_handeler(int z);
void sighup_handeler(int z);
void client(string);

int main(int argc, char* argv[])
{
	//string client_ip = "localhost" ;
		if(argc == 2)
		{
		//	string client_ip = argv[1] ;
		//	client(client_ip);
		client ("192.168.98.211");
		}


	/** Variable Declaration */
	ifstream inputFile;
	string Line,CompleteMapString= "";
	int No_Of_Gold,mapSize;
	bool firstline = true;
	bool collen_flag = true;

	char* theMine;
	const char* ptr;
	unsigned char* mp;
	unsigned char* client_mp;
	int player_position;
	unsigned char current_player;
	pid_t current_pid;
	string msgString;


mqueue_name[0] = "/agore_player0_mq";
mqueue_name[1] = "/agore_player1_mq";
mqueue_name[2] = "/agore_player2_mq";
mqueue_name[3] = "/agore_player3_mq";
mqueue_name[4] = "/agore_player4_mq";

	/** Signal */
	struct sigaction sigactionObject;
	sigactionObject.sa_handler=handle_interrupt;
	sigemptyset(&sigactionObject.sa_mask);
	sigactionObject.sa_flags=0;
	sigactionObject.sa_restorer=NULL;

	sigaction(SIGUSR1, &sigactionObject, NULL);
	struct sigaction exit_handler;
	exit_handler.sa_handler=clean_up;
	sigemptyset(&exit_handler.sa_mask);
	exit_handler.sa_flags=0;
	sigaction(SIGINT, &exit_handler, NULL);
	//sigaction(SIGHUP, &exit_handler, NULL);
	sigaction(SIGTERM, &exit_handler, NULL);

	//make sure we can handle the SIGUSR2
	//message when the message queue
	//notification sends the signal
	struct sigaction action_to_take;
	action_to_take.sa_handler= read_message;	//handle with this function interrupt
	sigemptyset(&action_to_take.sa_mask);//zero out the mask (allow any signal to)
	action_to_take.sa_flags=0;//tell how to handle SIGINT
	sigaction(SIGUSR2, &action_to_take, NULL);// struct mq_attr mq_attributes;
	mq_attributes.mq_flags=0;
	mq_attributes.mq_maxmsg=10;
	mq_attributes.mq_msgsize=120;

if(argc != 2)
{
	/** Reading map file */
	inputFile.open("mymap.txt");
	while(inputFile != '\0')
	{
		if(firstline == true)
		{
			getline(inputFile,Line);
			No_Of_Gold = stoi(Line);
			firstline = false;
		}
		else
		{
			getline(inputFile,Line);
			CompleteMapString += Line;
			NumberOfRows++;
			//MAP_ROW++;
			if(collen_flag == true)
			{
				NumberOfColumns = Line.length();
				collen_flag =  false;
			}
		}
	}
	inputFile.close();

	mapSize = NumberOfRows * NumberOfColumns;
}
/*
	argv checking
*/


	/** Creating semaphore */


	GameBoard_Sem = sem_open("/GameBoard_Sem",O_RDWR|O_EXCL,S_IROTH|S_IWOTH|S_IRGRP|S_IWGRP|S_IRUSR|S_IWUSR,1);

	if( GameBoard_Sem == SEM_FAILED) /** Code for First Player */
	{
		GameBoard_Sem = sem_open("/GameBoard_Sem",O_CREAT|O_EXCL|O_RDWR ,S_IROTH|S_IWOTH|S_IRGRP|S_IWGRP|S_IRUSR|S_IWUSR,1);///create a semaphore

		/** Code for Shared memory creation*/

		sem_wait(GameBoard_Sem);
		fd = shm_open("/GameBoard_Mem", O_CREAT|O_EXCL|O_RDWR , S_IRUSR | S_IWUSR);
		goldmap = (GameBoard*)mmap(NULL, NumberOfRows*NumberOfColumns+sizeof(GameBoard), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

		if(ftruncate(fd,mapSize + sizeof(GameBoard)) != 0 )
		{
			perror("Truncate Memory");
		}

		if(goldmap ==  MAP_FAILED)
		{
			perror("MMaP Error :");
		}

		goldmap->rows = NumberOfRows;
		goldmap->cols = NumberOfColumns;

		current_player = G_PLR0;


		//Convert the ASCII bytes into bit fields drawn from goldchase.h
		theMine = &CompleteMapString[0];
		ptr = theMine;
		mp = goldmap->map;
		while(*ptr!='\0')
		{
			if(*ptr==' ')      *mp=0;
			else if(*ptr=='*') *mp=G_WALL; //A wall
			++ptr;
			++mp;
		}

		/** Insert GOLD into the shared memory */
		insertGold(goldmap->map,No_Of_Gold,NumberOfRows,NumberOfColumns);

		/** Insert First player into the shared memory */
		player_position = insertPlayer(goldmap->map,current_player,NumberOfRows,NumberOfColumns);

		for(int i = 0; i < 5; i++)
		{
			goldmap->player_pid[i] = 0;
		}


		goldmap->player_pid[0] = getpid();

		readQueue(mqueue_name[0]);   // read mqueue of first player
		sem_post(GameBoard_Sem);

	}
	else  /** Code for Subsequent Players*/
	{
		GameBoard_Sem = sem_open("/GameBoard_Sem",O_RDWR|O_EXCL,S_IROTH|S_IWOTH|S_IRGRP|S_IWGRP|S_IRUSR|S_IWUSR,1);

		int fdiscriptor = shm_open("/GameBoard_Mem", O_RDWR|O_EXCL , S_IRUSR | S_IWUSR);
		if(fdiscriptor == -1)
		{
			cerr<<"second FD "<<endl;
		}

		// read shared map
		int map_rows, map_columns ;
		READ(fdiscriptor, &map_rows, sizeof(int));
		READ(fdiscriptor, &map_columns, sizeof(int));

		goldmap = (GameBoard*)mmap(NULL, map_rows*map_columns+sizeof(GameBoard), PROT_READ|PROT_WRITE, MAP_SHARED, fdiscriptor, 0);
		if(goldmap == MAP_FAILED )
		{
			cerr<<"MAP Reading for Else Part "<<endl;
		}

		NumberOfColumns = goldmap->cols;
		NumberOfRows = goldmap->rows;

		if(goldmap->player_pid[0] == 0)
		{
			current_player = G_PLR0;
			goldmap->player_pid[0] = getpid();
		//		goldmap->daemonID = 0;
			readQueue(mqueue_name[0]);
		}
		else if (goldmap->player_pid[1] == 0)
		{
			current_player = G_PLR1;
			goldmap->player_pid[1] = getpid();
			readQueue(mqueue_name[1]);
		}
		else if (goldmap->player_pid[2] == 0)
		{
			current_player = G_PLR2;

			goldmap->player_pid[2] =getpid();
		readQueue(mqueue_name[2]);
		}
		else if (goldmap->player_pid[3] == 0)
		{
			current_player = G_PLR3;
			goldmap->player_pid[3] = getpid();
			readQueue(mqueue_name[3]);

		}
		else if (goldmap->player_pid[4] == 0)
		{
			current_player = G_PLR4;
			goldmap->player_pid[4] = getpid();
			readQueue(mqueue_name[4]);
		}
		else
		{
			cout << "No more players allowed" <<endl;
			return 0;
		}

		player_position = insertPlayer(goldmap->map,current_player,NumberOfRows,NumberOfColumns);

	}

	sem_wait(GameBoard_Sem);
	goldmap->player |= current_player;
	sem_post(GameBoard_Sem);


	if(goldmap->daemonID == 0)
	{
			cout << "IS It??" << endl;
			create_server_daemon();
	}
	else if(goldmap->daemonID != 0 )
	{
		kill(goldmap->daemonID,SIGHUP);
	}

	Map goldMine(goldmap->map,goldmap->rows,goldmap->cols);


	mapptr = &goldMine;

	refreshScreen(goldmap);
//	goldmap->daemonID = 0;
//	cout<<"deamon ig before :"<< goldmap->daemonID << endl;



	/** Code of Movement*/


	int currentColumn,currentRow;
	int a=0;
	bool running_flag =  true,realGoldFound=false, sendSignal = false;

	while(running_flag && sig==false)
	{
		a = goldMine.getKey();
		currentRow = player_position / NumberOfColumns;
		currentColumn = player_position % NumberOfColumns;

		if(currentColumn == 0)
		{
			currentColumn = NumberOfColumns;
		}

		if( a == 104 ) //  'h' Key for Left Movement
		{

			if(realGoldFound == true && currentColumn-1 == 0)
			{
				sem_wait(GameBoard_Sem);
				goldmap->map[player_position] &= ~current_player;
				sem_post(GameBoard_Sem);
				break;
			}
			if(goldmap->map[player_position-1] != G_WALL )
			{
				goldmap->map[player_position] &= ~current_player;
				player_position = player_position -1;
				goldmap->map[player_position]  |= current_player;
				sendSignal = true;
			}
		}
		else if( a == 108 ) /// 'l' Key for Right Movement
		{
			if(realGoldFound == true && currentColumn == goldmap->cols -1)
			{
				sem_wait(GameBoard_Sem);
				goldmap->map[player_position] &= ~current_player;
				sem_post(GameBoard_Sem);
				break;
			}
			if(goldmap->map[player_position+1] != G_WALL )//| currentColumn != 0 )
			{
				goldmap->map[player_position] &= ~current_player;
				player_position = player_position+1;
				goldmap->map[player_position]  |= current_player;
				sendSignal = true;
			}
		}
		else if( a == 106 ) // 'j' Key for Down Movement
		{
			if(realGoldFound == true && currentRow >= NumberOfRows-1 )
			{
				sem_wait(GameBoard_Sem);
				goldmap->map[player_position] &= ~current_player;
				sem_post(GameBoard_Sem);
				break;
			}
			if(goldmap->map[player_position+NumberOfColumns] != G_WALL  && currentRow < NumberOfRows-1 )
			{
				goldmap->map[player_position] &= ~current_player;
				player_position = player_position+NumberOfColumns;
				goldmap->map[player_position]  |= current_player;
				sendSignal = true;
			}
		}
		else if( a == 107 ) // 'k' Key for Upward Movement
		{
			if(realGoldFound == true && player_position-NumberOfColumns <= 0)
			{
				sem_wait(GameBoard_Sem);
				goldmap->map[player_position] &= ~current_player;
				sem_post(GameBoard_Sem);
				break;
			}
			if(player_position-NumberOfColumns > 0)
			{
				if(goldmap->map[player_position-NumberOfColumns] != G_WALL)
				{

					goldmap->map[player_position] &= ~current_player;
					player_position = player_position-NumberOfColumns;
					goldmap->map[player_position]  |= current_player;
					sendSignal = true;
				}
			}
		}
		else if(a == 81)   /// Q for quit
		{
			running_flag = false;

			sem_wait(GameBoard_Sem);
			goldmap->player--;
			goldmap->map[player_position] &= ~current_player;
			sem_post(GameBoard_Sem);


		}
		else if(a == 77 || a == 109) // key 'M' or 'm' for Message sending
		{
			string rank;
			unsigned int mask=0;
			for(int i = 0; i < 5;i++)
			{
				if(goldmap->player_pid[i] != 0)
				{
					switch (i) {
						case 0:
							mask |= G_PLR0;
							break;
						case 1:
							mask |= G_PLR1;
							break;
						case 2:
							mask |= G_PLR2;
							break;
						case 3:
							mask |= G_PLR3;
							break;
						case 4:
							mask |= G_PLR4;
							break;
						}
				}
			}

			unsigned int destination_player;
			destination_player = mapptr->getPlayer(mask);
			int q_index=0;

			if (destination_player == G_PLR0)
			{
				q_index = 0;
			}
			else if (destination_player == G_PLR1)
			{
				q_index = 1;
			}
			else if(destination_player == G_PLR2)
			{
				q_index = 2;
			}
			else if(destination_player == G_PLR3)
			{
				q_index = 3;
			}
			else if(destination_player == G_PLR4)
			{
				q_index = 4;
			}
			if(current_player == G_PLR0)
			{
				rank="Player 1 says: ";
			}
			else if(current_player == G_PLR1)
			{
				rank="Player 2 says: ";
			}
			else if(current_player == G_PLR2)
			{
				rank="Player 3 says: ";
			}
			else if(current_player == G_PLR3)
			{
				rank="Player 4 says: ";
			}
			else if(current_player == G_PLR4)
			{
				rank="Player 4 says: ";
			}
			msgString = rank+mapptr->getMessage();
			if(current_player != destination_player)
			{
				writeQueue(mqueue_name[q_index],msgString);
			}
		}
		else if(a == 66 || a == 98) // "B" or 'b' key --- broadcast
		{
			string rank;
			if(current_player == G_PLR0)
			{
				rank="Player 1 says: ";
			}
			else if(current_player == G_PLR1)
			{
				rank="Player 2 says: ";
			}
			else if(current_player == G_PLR2)
			{
				rank="Player 3 says: ";
			}
			else if(current_player == G_PLR3)
			{
				rank="Player 4 says: ";
			}
			else if(current_player == G_PLR4)
			{
				rank="Player 4 says: ";
			}
			msgString = rank+mapptr->getMessage();
			for (int i = 0; i < 5;i++)
			{
				if(goldmap->player_pid[i] != 0 && goldmap->player_pid[i]!=getpid())
				{
					writeQueue(mqueue_name[i],msgString);
				}
			}
		}

		if(goldmap->map[player_position] & G_FOOL)
		{
			goldmap->map[player_position] &= ~G_FOOL;
			goldMine.postNotice(" Fools Gold..!!");
		}
		else if(goldmap->map[player_position] & G_GOLD)
		{
			realGoldFound = true;
			goldmap->map[player_position] &= ~G_GOLD;
			goldMine.postNotice(" Real Gold Found..To QUIT go to any edge of the Map!!");
		}

		if(sendSignal == true)
		{
				sendSignal = false;
			  refreshScreen(goldmap);
		}
	}

sem_wait(GameBoard_Sem);
	if(current_player == G_PLR0)
	{
		if(realGoldFound==true)
		{
			msgString = "Player 1 has won...!!";
			for (int i = 0; i < 5;i++)
			{
				if(goldmap->player_pid[i] != 0 && goldmap->player_pid[i]!=getpid())
				{
					writeQueue(mqueue_name[i],msgString);
				}
			}
		}
		mq_close(readqueue_fd);
		mq_unlink(mqueue_name[0].c_str());
		goldmap->player_pid[0] = 0;
	}
	else if(current_player == G_PLR1)
	{
			if(realGoldFound==true)
			{
				msgString = "Player 2 has won...!!";
				for (int i = 0; i < 5;i++)
				{
					if(goldmap->player_pid[i] != 0 && goldmap->player_pid[i]!=getpid())
					{
						writeQueue(mqueue_name[i],msgString);
					}
				}
			}
		mq_close(readqueue_fd);
		mq_unlink(mqueue_name[1].c_str());
		goldmap->player_pid[1] = 0;
	}
	else if(current_player == G_PLR2)
	{
		if(realGoldFound==true)
		{
			msgString = "Player 3 has won...!!";
			for (int i = 0; i < 5;i++)
			{
				if(goldmap->player_pid[i] != 0 && goldmap->player_pid[i]!=getpid())
				{
						writeQueue(mqueue_name[i],msgString);
				}
			}
		}
		mq_close(readqueue_fd);
		mq_unlink(mqueue_name[2].c_str());
		goldmap->player_pid[2] = 0;
	}
	else if(current_player == G_PLR3)
	{
		if(realGoldFound==true)
		{
			msgString = "Player 4 has won...!!";
			for (int i = 0; i < 5;i++)
			{
				if(goldmap->player_pid[i] != 0 && goldmap->player_pid[i]!=getpid())
				{
						writeQueue(mqueue_name[i],msgString);
				}
			}
		}
		mq_close(readqueue_fd);
		mq_unlink(mqueue_name[3].c_str());
		goldmap->player_pid[3] = 0;
	}
	else if(current_player == G_PLR4)
	{
		if(realGoldFound==true)
		{
			msgString = "Player 5 has won...!!";
			for (int i = 0; i < 5;i++)
			{
				if(goldmap->player_pid[i] != 0 && goldmap->player_pid[i]!=getpid())
				{
					writeQueue(mqueue_name[i],msgString);
				}

			}
		}
		mq_close(readqueue_fd);
		mq_unlink(mqueue_name[4].c_str());
		goldmap->player_pid[4] = 0;
	}

	goldmap->map[player_position] &= ~current_player;

refreshScreen(goldmap);
sem_post(GameBoard_Sem);

	if(goldmap->player_pid[0]== 0 && goldmap->player_pid[1]== 0 && goldmap->player_pid[2]== 0 && goldmap->player_pid[3]== 0 && goldmap->player_pid[4]== 0)
	{
		// sem_close(GameBoard_Sem);
		// sem_unlink("/GameBoard_Sem");
		// shm_unlink("/GameBoard_Mem");
		kill(goldmap->daemonID, SIGHUP);
	}
	return 0;
}

void create_server_daemon()
{

	if(fork()>0)
	  {
	    return;
	  }
	  if(fork()>0)
		{
    	exit(0);
		}
	  if(setsid()==-1)
		{
			exit(1);
		}

	  for(int i = 0; i< sysconf(_SC_OPEN_MAX); ++i)
		{
			close(i);
		}
	  open("/dev/null", O_RDWR); //fd 0
	  open("/dev/null", O_RDWR); //fd 1
	  open("/dev/null", O_RDWR); //fd 2
	  umask(0);
	  chdir("/");

		//sleep(3);
		debugFD = open("/home/akshay/private_git_611/611/mypipe",O_WRONLY);

		WRITE(debugFD, "daemon created\n", sizeof("daemon created\n"));

		int shm_fd = shm_open("/GameBoard_Mem",O_EXCL|O_RDWR , S_IRUSR | S_IWUSR);

		if(shm_fd == -1)
		{
		//	cerr<<"deamon shm_Fd -- "<<endl;
			WRITE(debugFD, "ERROR : deamon shm_Fd \n", sizeof("ERROR : deamon shm_Fd \n"));
		}


		//goldmap->daemonID = getpid();
		int rows =0,cols = 0;
		//READ(shm_fd, &rows, sizeof(int));
		//READ(shm_fd, &cols, sizeof(int));

		rows = goldmap->rows;
		cols = goldmap->cols;

		gm = (GameBoard*)mmap(NULL, rows*cols+sizeof(GameBoard), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);

		if(gm== MAP_FAILED )
		{
			WRITE(debugFD, "MAP Reading for Daemon\n", sizeof("MAP Reading for Daemon\n"));
			// cerr<<"MAP Reading for Daemon "<<endl;
		}

		if(goldmap->rows == 26)
		{
				WRITE(debugFD, "row count is 26\n", sizeof("row count is 26\n"));
		}

	//	WRITE(debugFD, &goldmap->rows,sizeof(int));


		gm->daemonID = getpid();

		local_mapcopy = new unsigned char[(rows*cols)];

		//unsigned char local_mapcopy[rows*cols];

		for(int x = 0; x < rows*cols ; x++)
		{
			local_mapcopy[x] = gm->map[x];
		}

/*   ---- print local memory spit out

		for(int i =0;i < rows*cols ; i++)
		{

			if(local_mapcopy[i] == 0)
			{
				WRITE(debugFD," ",sizeof(" "));
			}
			else if(local_mapcopy[i] == G_WALL)
			{
					WRITE(debugFD,"*",sizeof("*"));
			}
		}


*/
//
// 		for(int i = 0; i < (r * c); ++i)
// {
//   WRITE(fd, &localCopy[i],sizeof(localCopy[i]));
// }


//////////////////////////-----SIGHUP Trapping ----/////////////////////////////

/*  Sighup*/
		struct sigaction sighup_action;
		//handle with this function
		sighup_action.sa_handler=sighup_handeler;
		//zero out the mask (allow any signal to interrupt)
		sigemptyset(&sighup_action.sa_mask);
		sighup_action.sa_flags=0;
		//tell how to handle SIGHUP
		sigaction(SIGHUP, &sighup_action, NULL);

/*  sigusr1*/


		struct sigaction sigusr1_action;
		//handle with this function
		sigusr1_action.sa_handler=sigusr1_handeler;
		//zero out the mask (allow any signal to interrupt)
		sigemptyset(&sigusr1_action.sa_mask);
		sigusr1_action.sa_flags=0;
		//tell how to handle SIGURS1
		sigaction(SIGUSR1, &sigusr1_action, NULL);


/*  sigsr2*/


		struct sigaction sigusr2_action;
		//handle with this function
		sigusr2_action.sa_handler=sigusr2_handeler;
		//zero out the mask (allow any signal to interrupt)
		sigemptyset(&sigusr2_action.sa_mask);
		sigusr2_action.sa_flags=0;
		//tell how to handle SIGURS2
		sigaction(SIGUSR2, &sigusr2_action, NULL);




//----------------------------------------------------------------------------//
	// while(1)
	// {
	// 	WRITE(debugFD, "Daemon is running!\n", sizeof("Daemon is running!\n"));
	// 	sleep(2);
	// }


//	int sockfd; //file descriptor for the socket
	int status; //for error checking

	//change this # between 2000-65k before using
	const char* portno="42424";
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints)); //zero out everything in structure
	hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
	hints.ai_socktype=SOCK_STREAM; // TCP stream sockets
	hints.ai_flags=AI_PASSIVE; //file in the IP of the server for me

	struct addrinfo *servinfo;
	if((status=getaddrinfo(NULL, portno, &hints, &servinfo))==-1)
	{
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		exit(1);
	}
	sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

	/*avoid "Address already in use" error*/
	int yes=1;
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1)
	{
		perror("setsockopt");
		exit(1);
	}

	//We need to "bind" the socket to the port number so that the kernel
	//can match an incoming packet on a port to the proper process
	if((status=bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
	{
		perror("bind");
		exit(1);
	}
	//when done, release dynamically allocated memory
	freeaddrinfo(servinfo);

	if(listen(sockfd,1)==-1)
	{
		perror("listen");
		exit(1);
	}

	printf("Blocking, waiting for client to connect\n");

	struct sockaddr_in client_addr;
	socklen_t clientSize=sizeof(client_addr);
	//int new_sockfd;
	do{
		new_sockfd=accept(sockfd, (struct sockaddr*) &client_addr, &clientSize);
	}while(new_sockfd==-1 && errno==EINTR);


	WRITE(debugFD,"accepted\n",sizeof("accepted\n"));
	//read & write to the socket
	char buffer[100];
	memset(buffer,0,100);
	int n;
	/*
	* listen to client
	**/
	// if((n=READ(new_sockfd, buffer, 99))==-1)
	// {
	// 	perror("read is failing");
	// 	printf("n=%d\n", n);
	// }

	 //WRITE(debugFD, buffer, sizeof(buffer));

	 if(goldmap->rows == 26)
	 {
		 	WRITE(debugFD, "12344\n", sizeof("12344\n"));
	 }
	 if(goldmap->cols == 80)
	 {
		 	WRITE(debugFD, "fdxcghjk\n", sizeof("fdxcghjk\n"));
	 }

	// rows = 26;

	  WRITE(new_sockfd,&goldmap->rows,sizeof(goldmap->rows));
	  WRITE(new_sockfd,&goldmap->cols,sizeof(goldmap->cols));

	unsigned char* mptr = local_mapcopy;

	for(int i = 0 ; i < goldmap->rows*goldmap->cols;i++)
	{
		WRITE(new_sockfd,&mptr[i],sizeof(mptr[i]));
	}

	//unsigned char byte = G_SOCKPLR;

	//WRITE(socket_fd, &byte, 1);
	WRITE(new_sockfd,&byte,sizeof(byte));


		while(1)
		{

			READ(sockfd,&listen_var,sizeof(unsigned char));

			if(listen_var & G_SOCKPLR)
			{
				  unsigned char player_bit[5]={G_PLR0, G_PLR1, G_PLR2,G_PLR3, G_PLR4};
				  for(int i=0; i<5; ++i) //loop through the player bits
				  {
				    // If player bit is on and shared memory ID is zero,
				    // a player (from other computer) has joined:
				    if(listen_var & player_bit[i] && gm->player_pid[i]==0)
				    {
						gm->player_pid[i] = getpid();//goldmap->daemonID;
						}
						else if(!(listen_var & player_bit[i]) && gm->player_pid[i]!=0)
				    {
								gm->player_pid[i] = 0;
						}
				  }
				  if(listen_var == G_SOCKPLR)
				    {
							//no players are left in the game.  Close and unlink the shared memory.
					    //Close and unlink the semaphore.  Then exit the program.
							kill(gm->daemonID, SIGHUP);
							//sighup call kr
					}
				}

			else if (listen_var == 0)
			{

				short  pvec_first_read,pvec_size;
				unsigned char pvec_second_read;
				//sig user 1 bheja
				READ(sockfd,&pvec_size,sizeof(pvec_size));
				for(short i = 0; i < pvec_size;i++)
				{
					READ(sockfd,&pvec_first_read,sizeof(short));
					READ(sockfd,&pvec_second_read,sizeof(unsigned char));
					gm->map[pvec_first_read] = pvec_second_read;
					local_mapcopy[pvec_first_read] = pvec_second_read;
				}



			}
			// else if(msg wala condition)
			// {
			// // need to write
			// }

			close(sockfd);

		}


	// const char* message="These are the times that try men's souls.";
	// WRITE(new_sockfd, message, strlen(message));
	close(new_sockfd);

////////////////////////////////SOCKET END//////////////////////////////////////
}


void client(string client_ip)
{
pipe(pipefd);
////////////////////////////////CLIENT DAEMON START////////////////////////////////////
	if(fork()>0)
  {
		close(pipefd[1]); //close write, parent only needs read
    int val;
    READ(pipefd[0], &val, sizeof(val));
    if(val==1)
      WRITE(1, "Success!\n", sizeof("Success!\n"));
    else
      WRITE(1, "Failure!\n", sizeof("Failure!\n"));
    return ;
  }
  if(fork()>0)
    exit(0);
  if(setsid()==-1)
    exit(1);
  for(int i = 0; i< sysconf(_SC_OPEN_MAX); ++i)
	{
		if(i!=pipefd[1])//close everything, except write
      close(i);
	}


  open("/dev/null", O_RDWR); //fd 0
  open("/dev/null", O_RDWR); //fd 1
  open("/dev/null", O_RDWR); //fd 2
  umask(0);
  chdir("/");

	debugFD = open("/home/akshay/private_git_611/611/mypipe",O_WRONLY);
	WRITE(debugFD,"client deamon running\n", sizeof("client deamon running\n"));
////////////////////////////////DAEMON END////////////////////////////////////

////////////////////////////////CLIENT SOCKET///////////////////////////////////

//int sockfd; //file descriptor for the socket
int status; //for error checking

//change this # between 2000-65k before using
const char* portno="42424";

struct addrinfo hints;
memset(&hints, 0, sizeof(hints)); //zero out everything in structure
hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
hints.ai_socktype=SOCK_STREAM; // TCP stream sockets

struct addrinfo *servinfo;
//instead of "localhost", it could by any domain name
if((status=getaddrinfo("192.168.98.211", portno, &hints, &servinfo))==-1)
{
	fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
	exit(1);
}
sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

if((status=connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
{
	perror("connect");
	exit(1);
}
//release the information allocated by getaddrinfo()
freeaddrinfo(servinfo);

const char* message="One small step for (a) man, one large  leap for Mankind";
//  const char* message = "Todd Gibson yudi jagani";
int n ;


 int rows1=0,cols1=0;
//
  READ(sockfd,&rows1,sizeof(int));
  READ(sockfd,&cols1,sizeof(int));
//
 //WRITE(debugFD,&rows,sizeof(rows));

  if (rows1 == 26)
  {
  	WRITE(debugFD,"Rows are printing correctly\n",sizeof("Rows are printing correctly\n"));

  }

	if (cols1 == 80)
  {
  	WRITE(debugFD,"Cols are printing correctly\n",sizeof("Cols are printing correctly\n"));

  }


	localClient = new unsigned char[(rows1 * cols1)];
 for(int i = 0; i < rows1*cols1;i++)
 {
	 unsigned char read_from_server;
 	 READ(sockfd, &read_from_server,sizeof(read_from_server));
 	 localClient[i] = read_from_server;
 }


	int client_mapSize = rows1 * cols1;

//SEM OPEN

GameBoard_Sem = sem_open("/GameBoard_Sem",O_RDWR|O_CREAT,S_IROTH|S_IWOTH|S_IRGRP|S_IWGRP|S_IRUSR|S_IWUSR,1);
 int client_shared_mem=shm_open("/GameBoard_Mem",O_RDWR|O_CREAT,S_IWUSR|S_IRUSR);
 ftruncate(client_shared_mem ,client_mapSize + sizeof(GameBoard));
 client_goldmap = (GameBoard*) mmap(0,client_mapSize + sizeof(GameBoard),PROT_WRITE,MAP_SHARED,client_shared_mem,0);

//
 client_goldmap->rows = rows1;
 client_goldmap->cols = cols1;
 client_goldmap->daemonID = getpid();
//unsigned char* mptr = clie


for(int i = 0; i < rows1*cols1;i++)
{
	client_goldmap->map[i] = localClient[i];
}

	int val=1;
  WRITE(pipefd[1], &val, sizeof(val));

	unsigned char client_byte;
	READ(sockfd, &client_byte,sizeof(client_byte));

  unsigned char player_bit[5]={G_PLR0, G_PLR1, G_PLR2, G_PLR3, G_PLR4};
	for(int i = 0; i < 5 ; i ++)
	{

		if(client_byte & player_bit[i] )
		{
			client_goldmap->player_pid[i] = client_goldmap->daemonID; //need to check
		}
	}


	while(1)
	{

		READ(sockfd,&listen_var1,sizeof(unsigned char));

		if(listen_var1 & G_SOCKPLR)
		{
			  unsigned char player_bit[5]={G_PLR0, G_PLR1, G_PLR2,G_PLR3, G_PLR4};
			  for(int i=0; i<5; ++i) //loop through the player bits
			  {
			    // If player bit is on and shared memory ID is zero,
			    // a player (from other computer) has joined:
			    if(listen_var1 & player_bit[i] && client_goldmap->player_pid[i]==0)
			    {
						client_goldmap->player_pid[i] = getpid();//goldmap->daemonID;
					}
					else if(!(listen_var1 & player_bit[i]) && client_goldmap->player_pid[i]!=0)
			    {
						client_goldmap->player_pid[i] = 0;
					}
			  }
			  if(listen_var1 == G_SOCKPLR)
			    {
						//no players are left in the game.  Close and unlink the shared memory.
				    //Close and unlink the semaphore.  Then exit the program.
						kill(client_goldmap->daemonID, SIGHUP);
						//sighup call kr
				}
			}

		else if (listen_var1 == 0)
		{
			 short pvec_first_read;
			unsigned char pvec_second_read;
			//sig user 1 bheja
			READ(sockfd,&pvec_first_read,sizeof(short));
			READ(sockfd,&pvec_second_read,sizeof(unsigned char));

			client_goldmap->map[pvec_first_read] == pvec_second_read;

		}
		// else if(msg wala condition)
		// {
		// // need to write
		// }

		close(sockfd);

	}



////////////////////////////CLIENT SOCKET END///////////////////////////////////

}

void sighup_handeler(int z)
{

		SockPlayer = G_SOCKPLR;

		for(int i=0; i<5; ++i)
		  if(gm->player_pid[i]!=0)
		    switch(i)
		    {
		      case 0:
		        SockPlayer|=G_PLR0;
		        break;
		      case 1:
		        SockPlayer|=G_PLR1;
		        break;
					case 2:
			      SockPlayer|=G_PLR2;
			      break;
					case 3:
			      SockPlayer|=G_PLR3;
			      break;
					case 4:
			      SockPlayer|=G_PLR4;
			      break;
		    }

				byte = SockPlayer;

				WRITE(new_sockfd, &SockPlayer,sizeof(SockPlayer));


	if(SockPlayer == G_SOCKPLR)
	{
		sem_close(GameBoard_Sem);
		sem_unlink("/GameBoard_Sem");
		shm_unlink("/GameBoard_Mem");
	}


}


void sigusr2_handeler(int z)
{

}
void sigusr1_handeler(int z)
{
	vector< pair<short,unsigned char> > pvec;
  unsigned char* shared_memory_map = goldmap->map;
	for(short i=0; i<goldmap->rows*goldmap->cols; ++i)
  {
    if(shared_memory_map[i]!=local_mapcopy[i])
    {
      pair<short,unsigned char> aPair;
      aPair.first=i;
      aPair.second=shared_memory_map[i];
      pvec.push_back(aPair);
      local_mapcopy[i]=shared_memory_map[i];
    }
  }

	int zero = 0;
	short pvec_size = pvec.size();
  if (pvec.size() > 0 )
	{
		WRITE(new_sockfd,&zero, sizeof(int));
		WRITE(new_sockfd,&pvec_size,sizeof(pvec_size));
		for(short i = 0; i < pvec.size(); ++i)
		{
			WRITE(new_sockfd,&pvec[i].first,sizeof(short));
			WRITE(new_sockfd,&pvec[i].second,sizeof(unsigned char));
	 	}
			/// socket write : Socket map
  }

}


void handle_interrupt(int)
{
	mapptr->drawMap();
}

/** Function to insert Fool's and Real Gold into the Map*/

int insertGold(unsigned char* map,int No_Of_Gold,int NumberOfRows,int NumberOfColumn)
{
	unsigned char* tempMap = map;
	srand(time(NULL));
	int position;
	bool insert_flag = true;
	int counter = No_Of_Gold-1;

	while(insert_flag)
	{
		position = rand() % (NumberOfRows*NumberOfColumn);

		if(tempMap[position] == 0)
		{
			if(counter != 0)
			{
				tempMap[position] = G_FOOL;
				counter--;
			}
			else
			{
				tempMap[position] = G_GOLD;
				insert_flag = false;
			}
		}
	}
	return 0;
}

/** Function to insert players into the Map*/
int insertPlayer(unsigned char* map,unsigned char player, int NumberOfRows, int NumberOfColumn)
{
	unsigned char* tempMap2 = map;
	srand(time(NULL));
	int position=0;
	while(1)
	{
		position = rand() % (NumberOfRows*NumberOfColumn);
		if(tempMap2[position] == 0)
		{
			tempMap2[position] = player;
			return position;
		}
	}
}

void read_message(int)  //// msg que read
{
  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);

  //read a message
  int err;
  char msg[121];
  memset(msg, 0, 121);//set all characters to '\0'
  while((err=mq_receive(readqueue_fd, msg, 120, NULL))!=-1)
  {
		mapptr->postNotice(msg);
		memset(msg, 0, 121);//set all characters to '\0'
  }
  //we exit while-loop when mq_receive returns -1
  //if errno==EAGAIN that is normal: there is no message waiting
  if(errno!=EAGAIN)
  {
    perror("mq_receive");
    exit(1);
  }
}

void clean_up(int)  /// msg queue cleanup
{
		mq_close(readqueue_fd);
		sig=true;
}


void readQueue(string mqueue_name)
{
	//I have added this signal-handling
	//code so that if you type ctrl-c to
	//abort the long, slow loop at the
	//end of main, then your message queue
	//will be properly cleaned up.
	if((readqueue_fd=mq_open(mqueue_name.c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
					S_IRUSR|S_IWUSR, &mq_attributes))==-1)
	{
		perror("mq_open 123");
		exit(1);
	}
	//set up message queue to receive signal whenever message comes in
	struct sigevent mq_notification_event;
	mq_notification_event.sigev_notify=SIGEV_SIGNAL;
	mq_notification_event.sigev_signo=SIGUSR2;
	mq_notify(readqueue_fd, &mq_notification_event);

}

void writeQueue(string mqueue_name,string msgString)
{
	mqd_t writequeue_fd; //message queue file descriptor
if((writequeue_fd = mq_open(mqueue_name.c_str(), O_WRONLY|O_NONBLOCK))==-1)
{
  perror("mq_open :");
  exit(1);
}
char message_text[121];
memset(message_text, 0, 121);
strncpy(message_text, msgString.c_str(), 120);

if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
{
  perror("mq_send");
  exit(1);
}
mq_close(writequeue_fd);
}

void refreshScreen(GameBoard* goldmap)
{
	for(int i =0 ; i < 5; i++)
	{
		if(goldmap->player_pid[i] != 0 )
		{
				kill(goldmap->player_pid[i], SIGUSR1);
		}
	}
}
