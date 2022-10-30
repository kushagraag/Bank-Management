void connection_handler(int connectionFD); // Handles the communication with the client


// commonly related to admin and customer
bool get_account_details(int connFD, struct Account *customerAccount);
bool get_customer_details(int connFD, int customerID);
bool get_transaction_details(int connFD, int accountNumber);
bool login_handler(bool isAdmin, int connFD, struct Customer *ptrToCustomer);


// admin use only 
int add_customer(int connFD, bool isPrimary, int newAccountNumber);


// customer use only
bool customer_operation_handler(int connFD);
bool deposit(int connFD);
bool withdraw(int connFD);
bool get_balance(int connFD);
bool change_password(int connFD);
bool lock_critical_section(struct sembuf *semOp);
bool unlock_critical_section(struct sembuf *sem_op);
void write_transaction_to_array(int *transactionArray, int ID);
int write_transaction_to_file(int accountNumber, long int oldBalance, long int newBalance, bool operation);
