#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>

//HW4 - Operating Systems
//Arik skigin 312360449
//Or Eliyahu 307884890

//LOCK ACCOUNT FUNCTION///
#define ACN_LOCK(a) &(a->data).lock
#define MIN_ACN_LOCK(a,b) (((a->data.id)<(b->data.id))? ACN_LOCK(a) : ACN_LOCK(b))
#define MAX_ACN_LOCK(a,b) (((a->data.id)>(b->data.id))? ACN_LOCK(a) : ACN_LOCK(b))
//////////LOG/PRINT STRING//////////
#define LOG_ACCOUNT_EXISTS "Error %d: Your transaction failed - account with the same id exists\n"
#define LOG_ACCOUNT_NOTEXISTS "Error %d: Your transaction failed - account id %d does not exists\n"
#define LOG_ACCOUNT_WRONG_PASSWORD "Error %d: Your transaction failed - password for account id %d is incorrect\n"
#define LOG_ACCOUNT_OPENED_ACCOUNT "%d: New account id is %d with password %s and initial balance %d\n"
#define LOG_ACCOUNT_DESPOSITED_ACCOUNT "%d: Account %d new balance is %d after amount %d $ was deposited\n"
#define LOG_ACCOUNT_DRAWN_ACCOUNT "%d: Account %d new balance is %d after amount %d $ was withdrew\n"
#define LOG_ACCOUNT_BALANCED_ACCOUNT "%d: Account %d balance is %d\n"
#define LOG_ACCOUNT_CLOSED_ACCOUNT "%d: Account %d is now closed. Balance was %d\n"
#define LOG_ACCOUNT_TRANSFERRED_ACCOUNT "%d: Transfer %d from account %d to account %d new account balance is %d new target account balance is %d\n"
#define LOG_ACCOUNT_LOW_BALANCE "Error %d: Your transaction failed - account id %d balance is lower than %d\n"
#define BANK_PRINT_TITLE "Current Bank status\n"
#define BANK_PRINT_ACCOUNT "Account %d balance - %d $ , Account Password - %s\n"
#define BANK_AMOUNT_BALANCES "The Bank has %d $\n\n"
//General Settings///
#define BANK_LOGFILE "log.txt"
#define ACCOUNT_PASSWORD_LENGTH 4
#define FILE_WRITE_BUFFER 256


enum BOOL {TRUE = 1, FALSE = 0};
enum CRITICAL_ERRORS {ERR_NIL=-1, ERR_ALLOCATION=-2, ERR_NINPUT=-3, ERR_FILEOPEN=-4, ERR_FILECLOSE=-5, ERR_CREATETHREAD=-6, ERR_LINEOVERFLOW=-7, ERR_ATM_INVALIDCMD=-8, ERR_FILEWRITE=-9 };

enum ATM_ACTION { ACCOUNT_OPEN = 'O', ACCOUNT_DEPOSIT = 'D', ACCOUNT_WITHDRAW = 'W', ACCOUNT_BALANCE = 'B', ACCOUNT_CLOSE = 'Q', ACCOUNT_TRANSFER = 'T'};
enum ATM_ERRORS {ATM_SUCCESS=0, ATM_INVALIDCMD, ATM_ACCOUNT_NOTEXISTS, ATM_ACCOUNT_EXISTS, ATM_ACCOUNT_WRONG_PASSWORD, ATN_ACCOUNT_NOBALANCE};

//WRITER-READER LOCK STRUCTURE
typedef struct WR_LOCK{
	int readc;
	sem_t read_lock, write_lock;
}WR_LOCK;

//Account details(data) with lock
typedef struct Account_Details{
	int id;
	char password[ACCOUNT_PASSWORD_LENGTH+1];
	int balance;
	WR_LOCK lock;
}Account_Details;

//Account node in bank accounts
typedef struct Account_Node{
	Account_Details data;
	struct Account_Node *next,*perv;
}Account_Node;

//Bank accounts with lock
typedef struct Bank {
	Account_Node *acs;
	pthread_t tBank;
	WR_LOCK lock;
}Bank;

//Local ATM
typedef struct Local_ATM{
	int id;
	pthread_t tATM;
	char *file;
	int hr_file;
}Local_ATM;

//ATMs
typedef struct ATMs{
	Local_ATM *atm;
	int size;
}ATMs;

//LOG FILE
typedef struct LOG_FILE{
	int hw_file;
	sem_t write_lock;
}LOG_FILE;

//Threads of ATMs and Bank
void* ATM(void* latm);
void* BANK();

//Initialization
void BANK_init();
void ATMs_init(Local_ATM *latms, char **files, int size);
enum CRITICAL_ERRORS LOG_FILE_init();

//Log
enum CRITICAL_ERRORS LOG_FILE_close();
enum CRITICAL_ERRORS LOG_FILE_write(char *buffer);

//WR initialization&locks
void WR_Lock_init(WR_LOCK* lock);
void WR_startRead(WR_LOCK* lock);
void WR_endRead(WR_LOCK* lock);
void WR_startWrite(WR_LOCK* lock);
void WR_endWrite(WR_LOCK* lock);

//Troubleshooting
void CRITICAL_ERROR_EXIT(int THRD_HAND_ID);
enum BOOL CRITICAL_ERROR(int THRD_HAND_ID, enum CRITICAL_ERRORS err, char* fname);
void BANK_deleteAccounts();

//ATM/BANK auxiliary functions
void ATM_writeToLOG(enum ATM_ACTION action, enum ATM_ERRORS err, int t1, int t2, int t3, int t4, int t5, int t6, char*t7);
enum ATM_ACTION actionProcessing(char* cmd, int *actID, char pass[ACCOUNT_PASSWORD_LENGTH+1], int *t1, int *t2);
enum ATM_ERRORS ATM_Execution(int atmID, char* cmd);
Account_Node* BANK_getAccount(int actID);
Account_Node* ACCOUNT_create(int actID, char *password, int init_amount);
enum BOOL BANK_existsACN(Account_Node **acn, int atmID, int actID, void(*endLock)(WR_LOCK*), enum BOOL existsError);
enum ATM_ERRORS BANK_existsAndPass(Account_Node **acn, int atmID, int actID, char*password, void(*endLock)(WR_LOCK*));

//ACTIONS FUNCTIONS
enum ATM_ERRORS BANK_addAccount(int atmID, int actID, char *password, int init_amount);
enum ATM_ERRORS BANK_depositAccount(int atmID, int actID, char*password, int amount);
enum ATM_ERRORS BANK_withdrawAccount(int atmID, int actID, char*password, int amount);
enum ATM_ERRORS BANK_checkBalanceAccount(int atmID, int actID, char*password);
enum ATM_ERRORS BANK_deleteAccount(int atmID, int actID, char*password);
enum ATM_ERRORS BANK_transferFTAccount(int atmID, int actID, char*password, int t_actID, int amount);

//GLOBAL
ATMs atms;
LOG_FILE _log;
Bank bnk;

int main(int argc, char* argv[]){
	int i, ans[argc];
	Local_ATM latms[argc-1]; //create ATMs

	if(argc == 1) CRITICAL_ERROR(0, ERR_NINPUT, "main");

	CRITICAL_ERROR(0, LOG_FILE_init(), "main"); //try to open log file
	BANK_init();
	ATMs_init(latms, argv+1 , argc-1);

	ans[0]=pthread_create(&bnk.tBank, NULL, BANK, NULL);
	for(i=0; i<atms.size; i++)
		ans[i+1]=pthread_create(&atms.atm[i].tATM, NULL, ATM, &atms.atm[i]);

	for(i=0; i<argc; i++)
		if(ans[i]) CRITICAL_ERROR(0, ERR_CREATETHREAD, "main");;

	for(i=0; i<atms.size; i++) pthread_join(atms.atm[i].tATM, NULL);

	BANK_deleteAccounts();
	CRITICAL_ERROR(0, LOG_FILE_close(), "main");
	return EXIT_SUCCESS;
}

//ATM - ATM thread
//get (void *) - local_atm strcut(contains details about the activity)
//get (void*) - default of thread(not important)
void* ATM(void* latm){
	Local_ATM *atm = (Local_ATM*)latm; //cast to local_atm
	char buffer[1],line[FILE_WRITE_BUFFER+1];//buffers to read
	int retin, indexfile = 0;	
	if((atm->hr_file = open(atm->file, O_RDONLY)) == -1) CRITICAL_ERROR(atm->id, ERR_FILEOPEN, "ATM THREAD");
	do{
		if(indexfile == sizeof(line)) CRITICAL_ERROR(atm->id, ERR_LINEOVERFLOW, "ATM THREAD"); //read line overflow
		retin = read(atm->hr_file, &buffer, sizeof(buffer));//read char
		if ((indexfile) && (((retin) && (buffer[0] == '\n')) || (!retin))){
			line[indexfile]='\0'; //end of string
			indexfile=0;
			//if its not only empty new line and run atm action
			//if have problem of invalid command critical error take care
			if(strcmp(line,"\n")) if(ATM_Execution(atm->id, line)) CRITICAL_ERROR(atm->id,ERR_ATM_INVALIDCMD,"ATM THREAD");
		}
		else
			line[indexfile++] = buffer[0];
		usleep(100*1000);	//1 microsecond = 0.001 milliseconds
	}while(retin);	
	if(close(atm->hr_file) == -1) CRITICAL_ERROR(atm->id,ERR_FILECLOSE,"ATM THREAD");
}

//BANK - BANK thread
//get (void*) - default of thread(not important)
void* BANK(){
	Account_Node *acn;
	int sum;
	while(TRUE){
		sleep(3);
		//need to consistent print => need read lock of bank!!!
		WR_startRead(&bnk.lock);
		acn=bnk.acs;
		sum=0;
		printf(BANK_PRINT_TITLE);
		while(acn){
			WR_startRead(ACN_LOCK(acn));
			printf(BANK_PRINT_ACCOUNT,acn->data.id,acn->data.balance,acn->data.password);
			sum+=acn->data.balance;
			WR_endRead(ACN_LOCK(acn));
			acn=acn->next;
		}
		printf(BANK_AMOUNT_BALANCES,sum);
		WR_endRead(&bnk.lock);
	}
}

//BANK_init - initialization global bank
void BANK_init(){
	bnk.acs=NULL;
	bnk.tBank=0;
	WR_Lock_init(&bnk.lock);
}

//ATMs_init - initialization global ATM
//get (Local_ATM *) - address to local atms array
//get (char**) - address to files string
//get (int) - number of local ATMs
void ATMs_init(Local_ATM *latms, char **files, int size){
	int i;
	atms.atm=latms;
	atms.size=size;
	for(i = 0; i<atms.size; i++){
		atms.atm[i].id=i+1;
		atms.atm[i].file=files[i]; //save only address!! (not need strcpy)
		atms.atm[i].hr_file=-1;
		atms.atm[i].tATM=0;
	}
}

//ATMs_init - initialization global ATM
//get (Local_ATM *) - address to local atms array
//get (char**) - address to files string
//get (int) - number of local ATMs
void CRITICAL_ERROR_EXIT(int THRD_HAND_ID){
	int i;
	if(bnk.tBank) pthread_cancel(bnk.tBank);
	for(i=0; i<atms.size; i++)
		if(atms.atm[i].id!=THRD_HAND_ID)
			if(atms.atm[i].tATM)
				pthread_cancel(atms.atm[i].tATM);
	BANK_deleteAccounts();
	close(_log.hw_file);
	for(i=0; i<atms.size; i++)
		close(atms.atm[i].hr_file);
	exit(EXIT_FAILURE);
}

//CRITICAL_ERROR - handling critical issues for running the system
//get (int) - who called the error handler(0 is main)
//get (enum CRITICAL_ERRORS) - the error sent
//get (char*) - function called
//return (enum BOOL) - false if thats not critical error
enum BOOL CRITICAL_ERROR(int THRD_HAND_ID, enum CRITICAL_ERRORS err, char* fname){
	if(err == ERR_NIL) return FALSE;		
	printf("CRITICAL ERROR! (THREAD NUMBER %d)\n",THRD_HAND_ID);
	printf("Function \"%s\" ERROR:\n", fname);
	switch(err){
		case ERR_NINPUT:
			printf("--need to get files of ATMs\n");
			break;
		case ERR_FILEOPEN:
			printf("--cannot open file\n");
			break;
		case ERR_FILECLOSE:
			printf("--cannot close file\n");
			break;
		case ERR_CREATETHREAD:
			printf("--cannot create thread\n");
			break;
		case ERR_ALLOCATION:
			printf("-- dynamic allocation failed\n");
			break;
		case ERR_ATM_INVALIDCMD:
			printf("-- Invalid ATM command file\n");
			break;
		case ERR_FILEWRITE:
			printf("--cannot write file\n");
			break;
		default:
			return FALSE;
			break;
	}
	CRITICAL_ERROR_EXIT(THRD_HAND_ID);
	return TRUE;
}

//WR_Lock_init - initialization WRITER-READER LOCK STRUCTURE
//get (WR_LOCK*) - address to WE LOCK
void WR_Lock_init(WR_LOCK* lock){
	lock->readc=0;
	sem_init(&(lock->read_lock),0,1);
	sem_init(&(lock->write_lock),0,1);
}

//WR_startRead - start read locking
//get (WR_LOCK*) - address to WR LOCK
void WR_startRead(WR_LOCK* lock){
	sem_wait(&(lock->read_lock));
	lock->readc++;
	if(lock->readc == 1)
		sem_wait(&(lock->write_lock));
	sem_post(&(lock->read_lock));
}

//WR_endRead - end read open locking
//get (WR_LOCK*) - address to WR LOCK
void WR_endRead(WR_LOCK* lock){
	sem_wait(&(lock->read_lock));
	lock->readc--;
	if(lock->readc == 0)
		sem_post(&(lock->write_lock));
	sem_post(&(lock->read_lock));
}

//WR_startWrite - start write locking
//get (WR_LOCK*) - address to WR LOCK
void WR_startWrite(WR_LOCK* lock){
	sem_wait(&(lock->write_lock));
}

//WR_endWrite - end write open locking
//get (WR_LOCK*) - address to WR LOCK
void WR_endWrite(WR_LOCK* lock){
	sem_post(&(lock->write_lock));
}

//LOG_FILE_init - initialization global log file
//get (enum CRITICAL_ERRORS) - error at initialization
enum CRITICAL_ERRORS LOG_FILE_init(){
	if((_log.hw_file = open(BANK_LOGFILE,O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) return ERR_FILEOPEN;
	sem_init(&(_log.write_lock),0,1);
	return ERR_NIL;
}

//LOG_FILE_close - close log file
//get (enum CRITICAL_ERRORS) - error at closing
enum CRITICAL_ERRORS LOG_FILE_close(){
	if(close(_log.hw_file) == -1) return ERR_FILECLOSE;
	_log.hw_file = -1;
	return ERR_NIL;
}

//LOG_FILE_write - write to log
//get (char*) - buffer to write
//get (enum CRITICAL_ERRORS) - error at writing
enum CRITICAL_ERRORS LOG_FILE_write(char *buffer){
	sem_wait(&(_log.write_lock));
	//save the lock of log file!!! no one can write now & if have error its ok to save the lock
	if(write(_log.hw_file, buffer, (ssize_t)strlen(buffer)) != strlen(buffer)) return ERR_FILEWRITE;
	sem_post(&(_log.write_lock));
	return ERR_NIL;
}

//ACCOUNT_create - create new account
//get (int) - account id
//get (char*) - buffer to write
//get (int) - initial balance
//get (Account_Node*) - Account node
Account_Node* ACCOUNT_create(int actID, char *password, int init_amount){
	Account_Node* acn = (Account_Node*)malloc(sizeof(Account_Node));
	if(acn){
		acn->data.id=actID;
		strcpy(acn->data.password,password);
		acn->data.balance=init_amount;
		WR_Lock_init(ACN_LOCK(acn));
		acn->next=acn->perv=NULL;
	}
	return acn;
}

//BANK_deleteAccounts - delete all accounts(free)
void BANK_deleteAccounts(){
	Account_Node *acn; 	
	acn = bnk.acs;
	while(bnk.acs){
		acn = bnk.acs->next;
		free(bnk.acs); //bye bye
		bnk.acs = acn;
	}
	bnk.acs=NULL;
}

//ATM_writeToLOG - atm preparing buffer to write to log & call to write
//get (enum ATM_ACTION) - atm action
//get (enum ATM_ERRORS) - atm error
//get (int...int) - parameter1-6
//get (char*) - parameter1 string
void ATM_writeToLOG(enum ATM_ACTION action, enum ATM_ERRORS err, int t1, int t2, int t3, int t4, int t5, int t6, char*t7){
	char buffer[FILE_WRITE_BUFFER+1];
	switch(err){
		case ATM_ACCOUNT_NOTEXISTS:
			snprintf(buffer,sizeof(buffer),LOG_ACCOUNT_NOTEXISTS,t1,t2);
			break;
		case ATM_ACCOUNT_EXISTS:
			snprintf(buffer,sizeof(buffer),LOG_ACCOUNT_EXISTS,t1);
			break;
		case ATM_ACCOUNT_WRONG_PASSWORD:
			snprintf(buffer,sizeof(buffer),LOG_ACCOUNT_WRONG_PASSWORD,t1,t2);
			break;
		case ATN_ACCOUNT_NOBALANCE:
			snprintf(buffer,sizeof(buffer),LOG_ACCOUNT_LOW_BALANCE,t1,t2,t3);
			break;
		case ATM_SUCCESS:
			switch(action){
				case ACCOUNT_OPEN:
					snprintf(buffer,sizeof(buffer),LOG_ACCOUNT_OPENED_ACCOUNT,t1,t2,t7,t3);
					break;
				case ACCOUNT_DEPOSIT:
					snprintf(buffer,sizeof(buffer),LOG_ACCOUNT_DESPOSITED_ACCOUNT,t1,t2,t3,t4);
					break;
				case ACCOUNT_WITHDRAW:
					snprintf(buffer,sizeof(buffer), LOG_ACCOUNT_DRAWN_ACCOUNT,t1,t2,t3,t4);
					break;
				case ACCOUNT_BALANCE:
					snprintf(buffer,sizeof(buffer),LOG_ACCOUNT_BALANCED_ACCOUNT,t1,t2,t3);
					break;
				case ACCOUNT_CLOSE:
					snprintf(buffer,sizeof(buffer),LOG_ACCOUNT_CLOSED_ACCOUNT,t1,t2,t3);
					break;
				case ACCOUNT_TRANSFER:
					snprintf(buffer,sizeof(buffer),LOG_ACCOUNT_TRANSFERRED_ACCOUNT,t1,t2,t3,t4,t5,t6);
					break;
				default:
					return;
					break;
			}
			break;
		default:
			return;
			break;
	}
	CRITICAL_ERROR(t1,LOG_FILE_write(buffer),"ATM_writeToLOG"); //call to write & 
}

//actionProcessing - get line and analyze to parameters
//get (char*) - line to analyze
//get (int*) - address to write account id
//get (char*) - address to write account password
//get (int*) - address to write another parameter
//get (int*) - address to write another parameter
//get (enum ATM_ACTION) - atm action
enum ATM_ACTION actionProcessing(char* cmd, int *actID, char pass[ACCOUNT_PASSWORD_LENGTH+1], int *t1, int *t2){
	char action;
	switch(sscanf(cmd,"%c %d %s %d %d",&action, actID, pass, t1, t2)-1){
		case 2:
			if((action == ACCOUNT_BALANCE) || (action == ACCOUNT_CLOSE)) return action;
			break;
		case 3:
			if((action == ACCOUNT_OPEN) || (action == ACCOUNT_DEPOSIT) || (action == ACCOUNT_WITHDRAW)) return action;
			break;
		case 4:
			if(action ==  ACCOUNT_TRANSFER) return action;
			break;
		default:
		break;
	}
	return ATM_INVALIDCMD;
}

//ATM_Execution - analyze command and execution
//get (int) - atm id(requesting action atm)
//get (char*) - command
//get (enum ATM_ERRORS) - atm action replay
enum ATM_ERRORS ATM_Execution(int atmID, char* cmd){
	int actID, t1=0, t2=0;
	char password[ACCOUNT_PASSWORD_LENGTH+1];
	switch(actionProcessing(cmd,&actID,password,&t1,&t2)){
		case ACCOUNT_OPEN:
			BANK_addAccount(atmID,actID,password,t1);
			break;
		case ACCOUNT_DEPOSIT:
			BANK_depositAccount(atmID,actID,password,t1);
			break;
		case ACCOUNT_WITHDRAW:
			BANK_withdrawAccount(atmID,actID,password,t1);
			break;
		case ACCOUNT_BALANCE:
			BANK_checkBalanceAccount(atmID,actID,password);
			break;
		case ACCOUNT_CLOSE:
			BANK_deleteAccount(atmID,actID,password);
			break;
		case ACCOUNT_TRANSFER:
			BANK_transferFTAccount(atmID,actID,password,t1,t2);
			break;
		default:
			return ATM_INVALIDCMD;
		break;
	}
	return ATM_SUCCESS;
}

////////////////////////////////WARNING!!!!//////////////////////////////
/////////////////BANK ACCOUNTS CRITICAL SECTION!!!//////////////////////
//////////////////USE ONLY WHEN DB IS LOCKED!!!!!!!/////////////////////
//BANK_getAccount - get account node from bank
//get (int) - account id
//get (Account_Node*) - account
Account_Node* BANK_getAccount(int actID){
	Account_Node* acn=NULL;
	acn=bnk.acs;
	while(acn){
		if(acn->data.id==actID)
			return acn;
		acn=acn->next;
	}
	return NULL;
}
//BANK_existsACN - check if account is exists
//get (Account_Node **) - address to write the address of account
//get (int) - atm id(requesting action atm)
//get (int) - account id
//get (void(*endLock)(WR_LOCK*)) - WR FUNC
//get (enum BOOL) - error at exists/notexists
//get (enum BOOL) - true if account is exist
enum BOOL BANK_existsACN(Account_Node **acn, int atmID, int actID, void(*endLock)(WR_LOCK*), enum BOOL existsError){
	*acn=BANK_getAccount(actID);
	if((existsError) && (*acn)){
		ATM_writeToLOG(FALSE,ATM_ACCOUNT_EXISTS,atmID,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE); //write to log that account is exist
		endLock(&bnk.lock);
		return TRUE;
	}
	if((!existsError) && (*acn == NULL)){
		ATM_writeToLOG(FALSE,ATM_ACCOUNT_NOTEXISTS,atmID,actID,FALSE,FALSE,FALSE,FALSE,FALSE); //write to log that account is exist
		endLock(&bnk.lock);
		return FALSE;
	}
	return (*acn) ? TRUE : FALSE;
}
//BANK_existsAndPass - check account exists and password
//get (Account_Node **) - address to write the address of account
//get (int) - atm id(requesting action atm)
//get (int) - account id
//get (char*) - account password
//get (void(*endLock)(WR_LOCK*)) - WR FUNC
//get (enum ATM_ERRORS) - error if not exists account or wrong password
enum ATM_ERRORS BANK_existsAndPass(Account_Node **acn, int atmID, int actID, char*password, void(*endLock)(WR_LOCK*)){	
	if(BANK_existsACN(acn,atmID,actID,endLock,FALSE) == FALSE) return ATM_ACCOUNT_NOTEXISTS;
	//CHECK PASSWORD IS NOT CRITICAL SECTION FOR ACCOUNT DATA
	//Assuming there is no password change
	//CHECK PASSWORD IS CRITICAL SECTION FOR BANK ACCOUNTS!!!
	if(strcmp((*acn)->data.password,password)){
		ATM_writeToLOG(FALSE,ATM_ACCOUNT_WRONG_PASSWORD,atmID,actID,FALSE,FALSE,FALSE,FALSE,FALSE); //write to log that the password is wrong
		endLock(&bnk.lock);
		return ATM_ACCOUNT_WRONG_PASSWORD;
	}
	return ATM_SUCCESS;
}
////////////////////////////////END WARNING!!!!/////////////////////////////////////


////////////////////////////////ATM ACTIONS////////////////////////////////////////


//BANK_addAccount - add new account to bank
//get (int) - atm id(requesting action atm)
//get (int) - account id
//get (char*) - account password
//get (int) - initial balance
//get (enum ATM_ERRORS) - atm action replay
enum ATM_ERRORS BANK_addAccount(int atmID, int actID, char *password, int init_amount){
	Account_Node *acn_cur, *acn;
	WR_startWrite(&bnk.lock); //BANK DB WRITER LOCK
	if(BANK_existsACN(&acn,atmID,actID,WR_endWrite,TRUE)) return ATM_ACCOUNT_EXISTS;

	//save the lock of bank!!! no one can change
	if((acn=ACCOUNT_create(actID,password,init_amount)) == NULL) CRITICAL_ERROR(atmID,ERR_ALLOCATION,"BANK_addAccount");

	if(bnk.acs == NULL){
		bnk.acs=acn;
	}else{
		if(bnk.acs->data.id>acn->data.id){
			acn->next=bnk.acs;
			bnk.acs->perv=acn;
			bnk.acs=acn;
		}else{
			acn_cur=bnk.acs;
			while((acn_cur->next) && (acn_cur->next->data.id<acn->data.id))
				acn_cur=acn_cur->next;

			acn->next=acn_cur->next;
			if(acn_cur->next)
				acn_cur->next->perv=acn;
			acn_cur->next=acn;
			acn->perv=acn_cur;
		}
	}
	sleep(1);
	ATM_writeToLOG(ACCOUNT_OPEN,ATM_SUCCESS,atmID,actID,init_amount,FALSE,FALSE,FALSE,password); //write to log
	WR_endWrite(&bnk.lock); //BANK DB RELEASE WRITER LOCK
	return ATM_SUCCESS;
}

//BANK_depositAccount - deposit to account
//get (int) - atm id(requesting action atm)
//get (int) - account id
//get (char*) - account password
//get (int) - amount to deposit
//get (enum ATM_ERRORS) - atm action replay
enum ATM_ERRORS BANK_depositAccount(int atmID, int actID, char*password, int amount){
	Account_Node *acn;
	enum ATM_ERRORS err;

	WR_startRead(&bnk.lock); //BANK DB READER LOCK
	if(err = BANK_existsAndPass(&acn,atmID,actID,password,WR_endRead)) return err;
	WR_startWrite(ACN_LOCK(acn)); //ACCOUNT DATA WRITE LOCK

	acn->data.balance+=amount;
	sleep(1);
	
	ATM_writeToLOG(ACCOUNT_DEPOSIT,ATM_SUCCESS,atmID,actID,acn->data.balance,amount,FALSE,FALSE,FALSE); //write to log
	WR_endWrite(ACN_LOCK(acn));//ACCOUNT DATA RELEASE WRITE LOCK
	WR_endRead(&bnk.lock); //BANK DB RELEASE READER LOCK
	return ATM_SUCCESS;
}

//BANK_withdrawAccount - withdraw from account
//get (int) - atm id(requesting action atm)
//get (int) - account id
//get (char*) - account password
//get (int) - amount to withdraw
//get (enum ATM_ERRORS) - atm action replay
enum ATM_ERRORS BANK_withdrawAccount(int atmID, int actID, char*password, int amount){
	Account_Node *acn;
	enum ATM_ERRORS err;

	WR_startRead(&bnk.lock); //BANK DB READER LOCK
	if(err = BANK_existsAndPass(&acn,atmID,actID,password,WR_endRead)) return err;
	WR_startWrite(ACN_LOCK(acn)); //ACCOUNT DATA WRITE LOCK

	if(acn->data.balance<amount){
		ATM_writeToLOG(ACCOUNT_WITHDRAW,ATN_ACCOUNT_NOBALANCE,atmID,actID,amount,FALSE,FALSE,FALSE,FALSE); //write to log(error balance)
		WR_endWrite(ACN_LOCK(acn)); //ACCOUNT DATA RELEASE WRITE LOCK
		WR_endRead(&bnk.lock); //BANK DB RELEASE READER LOCK
		return ATN_ACCOUNT_NOBALANCE;
	}
	acn->data.balance-=amount;
	sleep(1);

	ATM_writeToLOG(ACCOUNT_WITHDRAW,ATM_SUCCESS,atmID,actID,acn->data.balance,amount,FALSE,FALSE,FALSE); //write to log
	WR_endWrite(ACN_LOCK(acn)); //ACCOUNT DATA RELEASE WRITE LOCK
	WR_endRead(&bnk.lock); //BANK DB RELEASE READER LOCK
	return ATM_SUCCESS;
}

//BANK_checkBalanceAccount - account check balance
//get (int) - atm id(requesting action atm)
//get (int) - account id
//get (char*) - account password
//get (enum ATM_ERRORS) - atm action replay
enum ATM_ERRORS BANK_checkBalanceAccount(int atmID, int actID, char*password){
	Account_Node *acn;
	enum ATM_ERRORS err;

	WR_startRead(&bnk.lock); //BANK DB READER LOCK
	if(err = BANK_existsAndPass(&acn,atmID,actID,password,WR_endRead)) return err;
	WR_startRead(ACN_LOCK(acn)); //ACCOUNT DATA READER LOCK

	sleep(1);

	ATM_writeToLOG(ACCOUNT_BALANCE,ATM_SUCCESS,atmID,actID,acn->data.balance,FALSE,FALSE,FALSE,FALSE); //write to log
	WR_endRead(ACN_LOCK(acn)); //ACCOUNT DATA RELEASE READER LOCK
	WR_endRead(&bnk.lock); //BANK DB RELEASE READER LOCK
	return ATM_SUCCESS;
}

//BANK_deleteAccount - delete account from bank
//get (int) - atm id(requesting action atm)
//get (int) - account id
//get (char*) - account password
//get (enum ATM_ERRORS) - atm action replay
enum ATM_ERRORS BANK_deleteAccount(int atmID, int actID, char*password){
	Account_Node *acn;
	int tmpBalance;
	enum ATM_ERRORS err;

	WR_startWrite(&bnk.lock); //BANK DB WRITER LOCK
	if(err = BANK_existsAndPass(&acn,atmID,actID,password,WR_endWrite)) return err;
	WR_startWrite(ACN_LOCK(acn));//ACCOUNT DATA WRITE LOCK

	if(bnk.acs == acn){
		bnk.acs=acn->next;
		bnk.acs->perv=NULL;
	}
	else{
		acn->perv->next=acn->next;
		if(acn->next)
			acn->next->perv=acn->perv;
	}
	tmpBalance=acn->data.balance;
	free(acn);
	sleep(1);

	ATM_writeToLOG(ACCOUNT_CLOSE,ATM_SUCCESS,atmID,actID,tmpBalance,FALSE,FALSE,FALSE,FALSE); //write to log
	//Not releasing the lock of account
	WR_endWrite(&bnk.lock); //BANK DB RELEASE WRITE LOCK
	return ATM_SUCCESS;
}

//BANK_transferFTAccount - transfer between accounts
//get (int) - atm id(requesting action atm)
//get (int) - account id
//get (char*) - account password
//get (int) - target account id
//get (int) - amount to transfer
//get (enum ATM_ERRORS) - atm action replay
enum ATM_ERRORS BANK_transferFTAccount(int atmID, int actID, char*password, int t_actID, int amount){
	Account_Node *acn, *t_acn;
	enum ATM_ERRORS err;

	WR_startRead(&bnk.lock); //BANK DB READER LOCK
	if(err = BANK_existsAndPass(&acn,atmID,actID,password,WR_endRead)) return err;
	if(BANK_existsACN(&t_acn,atmID,t_actID,WR_endRead,FALSE) == FALSE) return ATM_ACCOUNT_NOTEXISTS;
	WR_startWrite(MIN_ACN_LOCK(acn,t_acn)); //ACCOUNT DATA WRITER LOCK
	WR_startWrite(MAX_ACN_LOCK(acn,t_acn)); //ACCOUNT DATA WRITER LOCK

	if(acn->data.balance<amount){
		ATM_writeToLOG(ACCOUNT_TRANSFER,ATN_ACCOUNT_NOBALANCE,atmID,actID,amount,FALSE,FALSE,FALSE,FALSE); //write to log(error balance)
		WR_endWrite(MIN_ACN_LOCK(acn,t_acn));//ACCOUNT DATA RELEASE WRITER LOCK
		WR_endWrite(MAX_ACN_LOCK(acn,t_acn));//ACCOUNT DATA RELEASE WRITER LOCK
		WR_endRead(&bnk.lock);//BANK DB RELEASE READER LOCK
		return ATN_ACCOUNT_NOBALANCE;
	}
	acn->data.balance-=amount;
	t_acn->data.balance+=amount;
	sleep(1);

	ATM_writeToLOG(ACCOUNT_TRANSFER,ATM_SUCCESS,atmID,amount,actID,t_actID,acn->data.balance,t_acn->data.balance,FALSE); //write to log
	WR_endWrite(MIN_ACN_LOCK(acn,t_acn)); //ACCOUNT DATA RELEASE WRITER LOCK
	WR_endWrite(MAX_ACN_LOCK(acn,t_acn)); //ACCOUNT DATA RELEASE WRITER LOCK
	WR_endRead(&bnk.lock); //BANK DB RELEASE READER LOCK
	return ATM_SUCCESS;
}


