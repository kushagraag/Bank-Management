#include "./includes.h"
#include "./prototypes.h"


void main()
{
    int socketFD, socketBindStatus, socketListenStatus, connectionFD;
    struct sockaddr_in serverAddress, clientAddress;

    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD == -1)
    {
        perror("Error while creating server socket!");
        _exit(0);
    }

    serverAddress.sin_family = AF_INET;                // IPv4
    // for host to network port (server to client)
    serverAddress.sin_port = htons(8088);              // Server will listen to port 8080
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); // Binds the socket to all interfaces

    // binding to client and listening
    socketBindStatus = bind(socketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (socketBindStatus == -1)
    {
        perror("Error while binding to server socket!");
        _exit(0);
    }

    // listening to client, (max 10)
    socketListenStatus = listen(socketFD, 10);
    if (socketListenStatus == -1)
    {
        perror("Error while listening for connections on the server socket!");
        close(socketFD);
        _exit(0);
    }

    int clientSize;
    while (1)
    {
        clientSize = (int)sizeof(clientAddress);
        // accepting client connection
        // connctionFileDescriptor for contacting client to server and vice versa
        connectionFD = accept(socketFD, (struct sockaddr *)&clientAddress, &clientSize);
        if (connectionFD == -1)
        {
            perror("Error while connecting to client!");
            close(socketFD);
        }
        else
        {
            // new connection created and will now work as new child
            if (!fork())
            {
                // Child will enter this branch
                connection_handler(connectionFD);
                close(connectionFD);
                _exit(0);
            }
        }
    }

    close(socketFD);
}


void connection_handler(int connectionFD)
{
    printf("Client has connected to the server!\n");

    char readBuffer[1000], writeBuffer[1000];
    ssize_t readBytes, writeBytes;
    int userChoice;

    writeBytes = write(connectionFD, INITIAL_PROMPT, strlen(INITIAL_PROMPT));
    if (writeBytes == -1)
        perror("Error while sending first prompt to the user!");
    else
    {
        // bzero for clearing buffer 
        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connectionFD, readBuffer, sizeof(readBuffer));
        if (readBytes == -1)
            perror("Error while reading from client");
        else if (readBytes == 0)
            printf("No data was sent by the client");
        else
        {
            userChoice = atoi(readBuffer);
            switch (userChoice)
            {
            case 1:
                // Admin
                if (login_handler(true, connectionFD, NULL))
                {
                    ssize_t writeBytes, readBytes;            // Number of bytes read from / written to the client
                    char readBuffer[1000], writeBuffer[1000]; // A buffer used for reading & writing to the client
                    bzero(writeBuffer, sizeof(writeBuffer));
                    strcpy(writeBuffer, ADMIN_LOGIN_SUCCESS);
                    while (1)
                    {
                        strcat(writeBuffer, "\n");
                        strcat(writeBuffer, ADMIN_MENU);
                        writeBytes = write(connectionFD, writeBuffer, strlen(writeBuffer));
                        if (writeBytes == -1)
                        {
                            perror("Error while writing ADMIN_MENU to client!");
                        }
                        bzero(writeBuffer, sizeof(writeBuffer));

                        readBytes = read(connectionFD, readBuffer, sizeof(readBuffer));
                        if (readBytes == -1)
                        {
                            perror("Error while reading client's choice for ADMIN_MENU");
                        }

                        int choice = atoi(readBuffer);
                        switch (choice)
                        {
                            case 1:
                                get_customer_details(connectionFD, -1);
                                break;
                            case 2:
                                get_account_details(connectionFD, NULL);
                                break;
                            case 3: 
                                get_transaction_details(connectionFD, -1);
                                break;


                            // Add account
                            case 4:
                            {
                                // Add account
                                ssize_t readBytes, writeBytes;
                                char readBuffer[1000], writeBuffer[1000];

                                struct Account newAccount, prevAccount;

                                int accountFDAddAcc = open(ACCOUNT_FILE, O_RDONLY);
                                if (accountFDAddAcc == -1 && errno == ENOENT)
                                {
                                    // Account file was never created
                                    newAccount.accountNumber = 0;
                                }
                                else if (accountFDAddAcc == -1)
                                {
                                    perror("Error while opening account file");
                                }
                                else
                                {
                                    int offset = lseek(accountFDAddAcc, -sizeof(struct Account), SEEK_END);
                                    if (offset == -1)
                                    {
                                        perror("Error seeking to last Account record!");
                                    }

                                    // setting read lock so that another admin cannot modify account 
                                    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
                                    // check locking status
                                    int lockingStatus = fcntl(accountFDAddAcc, F_SETLKW, &lock);
                                    if (lockingStatus == -1)
                                    {
                                        perror("Error obtaining read lock on Account record!");
                                    }

                                    readBytes = read(accountFDAddAcc, &prevAccount, sizeof(struct Account));
                                    if (readBytes == -1)
                                    {
                                        perror("Error while reading Account record from file!");
                                    }

                                    lock.l_type = F_UNLCK;
                                    fcntl(accountFDAddAcc, F_SETLK, &lock);

                                    close(accountFDAddAcc);

                                    newAccount.accountNumber = prevAccount.accountNumber + 1;
                                }
                                // set account type
                                writeBytes = write(connectionFD, ADMIN_ADD_ACCOUNT_TYPE, strlen(ADMIN_ADD_ACCOUNT_TYPE));
                                if (writeBytes == -1)
                                {
                                    perror("Error writing ADMIN_ADD_ACCOUNT_TYPE message to client!");
                                }

                                bzero(readBuffer, sizeof(readBuffer));
                                readBytes = read(connectionFD, &readBuffer, sizeof(readBuffer));
                                if (readBytes == -1)
                                {
                                    perror("Error reading account type response from client!");
                                }

                                newAccount.isRegularAccount = atoi(readBuffer) == 1 ? true : false;

                                newAccount.owners[0] = add_customer(connectionFD, true, newAccount.accountNumber);

                                if (newAccount.isRegularAccount)
                                    newAccount.owners[1] = -1;
                                else
                                    newAccount.owners[1] = add_customer(connectionFD, false, newAccount.accountNumber);

                                // settting initials for new account
                                newAccount.active = true;
                                newAccount.balance = 0;

                                memset(newAccount.transactions, -1, MAX_TRANSACTIONS * sizeof(int));

                                accountFDAddAcc = open(ACCOUNT_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);
                                if (accountFDAddAcc == -1)
                                {
                                    perror("Error while creating / opening account file!");
                                }

                                writeBytes = write(accountFDAddAcc, &newAccount, sizeof(struct Account));
                                if (writeBytes == -1)
                                {
                                    perror("Error while writing Account record to file!");
                                }

                                close(accountFDAddAcc);

                                bzero(writeBuffer, sizeof(writeBuffer));
                                sprintf(writeBuffer, "%s%d", ADMIN_ADD_ACCOUNT_NUMBER, newAccount.accountNumber);
                                strcat(writeBuffer, "\nRedirecting you to the main menu ...^");
                                writeBytes = write(connectionFD, writeBuffer, sizeof(writeBuffer));
                                readBytes = read(connectionFD, readBuffer, sizeof(read)); // Dummy read
                                break;
                            }


                            // Delete Account
                            case 5:
                            {
                                // Delete Account
                                struct Account account;
                                // get account number to delete
                                writeBytes = write(connectionFD, ADMIN_DEL_ACCOUNT_NO, strlen(ADMIN_DEL_ACCOUNT_NO));
                                if (writeBytes == -1)
                                {
                                    perror("Error writing ADMIN_DEL_ACCOUNT_NO to client!");
                                }

                                bzero(readBuffer, sizeof(readBuffer));
                                readBytes = read(connectionFD, readBuffer, sizeof(readBuffer));
                                if (readBytes == -1)
                                {
                                    perror("Error reading account number response from the client!");
                                }

                                int accountNumber = atoi(readBuffer);

                                int accountFDDeleteAcc = open(ACCOUNT_FILE, O_RDONLY);
                                if (accountFDDeleteAcc == -1)
                                {
                                    // Account record doesn't exist
                                    bzero(writeBuffer, sizeof(writeBuffer));
                                    strcpy(writeBuffer, ACCOUNT_ID_DOESNT_EXIT);
                                    strcat(writeBuffer, "^");
                                    writeBytes = write(connectionFD, writeBuffer, strlen(writeBuffer));
                                    if (writeBytes == -1)
                                    {
                                        perror("Error while writing ACCOUNT_ID_DOESNT_EXIT message to client!");
                                    }
                                    readBytes = read(connectionFD, readBuffer, sizeof(readBuffer)); // Dummy read
                                }


                                int offset = lseek(accountFDDeleteAcc, accountNumber * sizeof(struct Account), SEEK_SET);
                                if (errno == EINVAL)
                                {
                                    // Customer record doesn't exist
                                    bzero(writeBuffer, sizeof(writeBuffer));
                                    strcpy(writeBuffer, ACCOUNT_ID_DOESNT_EXIT);
                                    strcat(writeBuffer, "^");
                                    writeBytes = write(connectionFD, writeBuffer, strlen(writeBuffer));
                                    if (writeBytes == -1)
                                    {
                                        perror("Error while writing ACCOUNT_ID_DOESNT_EXIT message to client!");
                                    }
                                    readBytes = read(connectionFD, readBuffer, sizeof(readBuffer)); // Dummy read
                                }
                                else if (offset == -1)
                                {
                                    perror("Error while seeking to required account record!");
                                }
                                // obtain read lock
                                struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
                                int lockingStatus = fcntl(accountFDDeleteAcc, F_SETLKW, &lock);
                                if (lockingStatus == -1)
                                {
                                    perror("Error obtaining read lock on Account record!");
                                }

                                readBytes = read(accountFDDeleteAcc, &account, sizeof(struct Account));
                                if (readBytes == -1)
                                {
                                    perror("Error while reading Account record from file!");
                                }

                                lock.l_type = F_UNLCK;
                                fcntl(accountFDDeleteAcc, F_SETLK, &lock);

                                close(accountFDDeleteAcc);

                                bzero(writeBuffer, sizeof(writeBuffer));
                                if (account.balance == 0)
                                {
                                    // No money, hence can close account
                                    account.active = false;
                                    accountFDDeleteAcc = open(ACCOUNT_FILE, O_WRONLY);
                                    if (accountFDDeleteAcc == -1)
                                    {
                                        perror("Error opening Account file in write mode!");
                                    }

                                    offset = lseek(accountFDDeleteAcc, accountNumber * sizeof(struct Account), SEEK_SET);
                                    if (offset == -1)
                                    {
                                        perror("Error seeking to the Account!");
                                    }

                                    lock.l_type = F_WRLCK;
                                    lock.l_start = offset;

                                    int lockingStatus = fcntl(accountFDDeleteAcc, F_SETLKW, &lock);
                                    if (lockingStatus == -1)
                                    {
                                        perror("Error obtaining write lock on the Account file!");
                                    }

                                    writeBytes = write(accountFDDeleteAcc, &account, sizeof(struct Account));
                                    if (writeBytes == -1)
                                    {
                                        perror("Error deleting account record!");
                                    }

                                    lock.l_type = F_UNLCK;
                                    fcntl(accountFDDeleteAcc, F_SETLK, &lock);

                                    strcpy(writeBuffer, ADMIN_DEL_ACCOUNT_SUCCESS);
                                }
                                else
                                    // Account has some money ask customer to withdraw it
                                    strcpy(writeBuffer, ADMIN_DEL_ACCOUNT_FAILURE);
                                writeBytes = write(connectionFD, writeBuffer, strlen(writeBuffer));
                                if (writeBytes == -1)
                                {
                                    perror("Error while writing final DEL message to client!");
                                }
                                readBytes = read(connectionFD, readBuffer, sizeof(readBuffer)); // Dummy read
                                break;
                            }

                            
                            // Modify customer info
                            case 6:
                            {
                                struct Customer customer;

                                int customerID;

                                off_t offset;
                                int lockingStatus;

                                writeBytes = write(connectionFD, ADMIN_MOD_CUSTOMER_ID, strlen(ADMIN_MOD_CUSTOMER_ID));
                                if (writeBytes == -1)
                                {
                                    perror("Error while writing ADMIN_MOD_CUSTOMER_ID message to client!");
                                }
                                bzero(readBuffer, sizeof(readBuffer));
                                readBytes = read(connectionFD, readBuffer, sizeof(readBuffer));
                                if (readBytes == -1)
                                {
                                    perror("Error while reading customer ID from client!");
                                }

                                customerID = atoi(readBuffer);

                                int customerFileDescriptor = open(CUSTOMER_FILE, O_RDONLY);
                                if (customerFileDescriptor == -1)
                                {
                                    // Customer File doesn't exist
                                    bzero(writeBuffer, sizeof(writeBuffer));
                                    strcpy(writeBuffer, CUSTOMER_ID_DOESNT_EXIT);
                                    strcat(writeBuffer, "^");
                                    writeBytes = write(connectionFD, writeBuffer, strlen(writeBuffer));
                                    if (writeBytes == -1)
                                    {
                                        perror("Error while writing CUSTOMER_ID_DOESNT_EXIT message to client!");
                                    }
                                    readBytes = read(connectionFD, readBuffer, sizeof(readBuffer)); // Dummy read
                                }
                                
                                offset = lseek(customerFileDescriptor, customerID * sizeof(struct Customer), SEEK_SET);
                                if (errno == EINVAL)
                                {
                                    // Customer record doesn't exist
                                    bzero(writeBuffer, sizeof(writeBuffer));
                                    strcpy(writeBuffer, CUSTOMER_ID_DOESNT_EXIT);
                                    strcat(writeBuffer, "^");
                                    writeBytes = write(connectionFD, writeBuffer, strlen(writeBuffer));
                                    if (writeBytes == -1)
                                    {
                                        perror("Error while writing CUSTOMER_ID_DOESNT_EXIT message to client!");
                                    }
                                    readBytes = read(connectionFD, readBuffer, sizeof(readBuffer)); // Dummy read
                                }
                                else if (offset == -1)
                                {
                                    perror("Error while seeking to required customer record!");
                                }

                                struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};

                                // Lock the record to be read
                                lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
                                if (lockingStatus == -1)
                                {
                                    perror("Couldn't obtain lock on customer record!");
                                }

                                readBytes = read(customerFileDescriptor, &customer, sizeof(struct Customer));
                                if (readBytes == -1)
                                {
                                    perror("Error while reading customer record from the file!");
                                }

                                // Unlock the record
                                lock.l_type = F_UNLCK;
                                fcntl(customerFileDescriptor, F_SETLK, &lock);

                                close(customerFileDescriptor);

                                writeBytes = write(connectionFD, ADMIN_MOD_CUSTOMER_MENU, strlen(ADMIN_MOD_CUSTOMER_MENU));
                                if (writeBytes == -1)
                                {
                                    perror("Error while writing ADMIN_MOD_CUSTOMER_MENU message to client!");
                                }
                                readBytes = read(connectionFD, readBuffer, sizeof(readBuffer));
                                if (readBytes == -1)
                                {
                                    perror("Error while getting customer modification menu choice from client!");
                                }

                                int choice = atoi(readBuffer);
                                if (choice == 0)
                                { // A non-numeric string was passed to atoi
                                    bzero(writeBuffer, sizeof(writeBuffer));
                                    strcpy(writeBuffer, ERRON_INPUT_FOR_NUMBER);
                                    writeBytes = write(connectionFD, writeBuffer, strlen(writeBuffer));
                                    if (writeBytes == -1)
                                    {
                                        perror("Error while writing ERRON_INPUT_FOR_NUMBER message to client!");
                                    }
                                    readBytes = read(connectionFD, readBuffer, sizeof(readBuffer)); // Dummy read
                                }

                                bzero(readBuffer, sizeof(readBuffer));
                                switch (choice)
                                {
                                case 1:
                                    writeBytes = write(connectionFD, ADMIN_MOD_CUSTOMER_NEW_NAME, strlen(ADMIN_MOD_CUSTOMER_NEW_NAME));
                                    if (writeBytes == -1)
                                    {
                                        perror("Error while writing ADMIN_MOD_CUSTOMER_NEW_NAME message to client!");
                                    }
                                    readBytes = read(connectionFD, &readBuffer, sizeof(readBuffer));
                                    if (readBytes == -1)
                                    {
                                        perror("Error while getting response for customer's new name from client!");
                                    }
                                    strcpy(customer.name, readBuffer);
                                    break;
                                case 2:
                                    writeBytes = write(connectionFD, ADMIN_MOD_CUSTOMER_NEW_AGE, strlen(ADMIN_MOD_CUSTOMER_NEW_AGE));
                                    if (writeBytes == -1)
                                    {
                                        perror("Error while writing ADMIN_MOD_CUSTOMER_NEW_AGE message to client!");
                                    }
                                    readBytes = read(connectionFD, &readBuffer, sizeof(readBuffer));
                                    if (readBytes == -1)
                                    {
                                        perror("Error while getting response for customer's new age from client!");
                                    }
                                    int updatedAge = atoi(readBuffer);
                                    if (updatedAge == 0)
                                    {
                                        // Either client has sent age as 0 (which is invalid) or has entered a non-numeric string
                                        bzero(writeBuffer, sizeof(writeBuffer));
                                        strcpy(writeBuffer, ERRON_INPUT_FOR_NUMBER);
                                        writeBytes = write(connectionFD, writeBuffer, strlen(writeBuffer));
                                        if (writeBytes == -1)
                                        {
                                            perror("Error while writing ERRON_INPUT_FOR_NUMBER message to client!");
                                        }
                                        readBytes = read(connectionFD, readBuffer, sizeof(readBuffer)); // Dummy read
                                    }
                                    customer.age = updatedAge;
                                    break;
                                case 3:
                                    writeBytes = write(connectionFD, ADMIN_MOD_CUSTOMER_NEW_GENDER, strlen(ADMIN_MOD_CUSTOMER_NEW_GENDER));
                                    if (writeBytes == -1)
                                    {
                                        perror("Error while writing ADMIN_MOD_CUSTOMER_NEW_GENDER message to client!");
                                    }
                                    readBytes = read(connectionFD, &readBuffer, sizeof(readBuffer));
                                    if (readBytes == -1)
                                    {
                                        perror("Error while getting response for customer's new gender from client!");
                                    }
                                    customer.gender = readBuffer[0];
                                    break;
                                default:
                                    bzero(writeBuffer, sizeof(writeBuffer));
                                    strcpy(writeBuffer, INVALID_MENU_CHOICE);
                                    writeBytes = write(connectionFD, writeBuffer, strlen(writeBuffer));
                                    if (writeBytes == -1)
                                    {
                                        perror("Error while writing INVALID_MENU_CHOICE message to client!");
                                    }
                                    readBytes = read(connectionFD, readBuffer, sizeof(readBuffer)); // Dummy read
                                }

                                customerFileDescriptor = open(CUSTOMER_FILE, O_WRONLY);
                                if (customerFileDescriptor == -1)
                                {
                                    perror("Error while opening customer file");
                                }
                                offset = lseek(customerFileDescriptor, customerID * sizeof(struct Customer), SEEK_SET);
                                if (offset == -1)
                                {
                                    perror("Error while seeking to required customer record!");
                                }

                                lock.l_type = F_WRLCK;
                                lock.l_start = offset;
                                lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
                                if (lockingStatus == -1)
                                {
                                    perror("Error while obtaining write lock on customer record!");
                                }

                                writeBytes = write(customerFileDescriptor, &customer, sizeof(struct Customer));
                                if (writeBytes == -1)
                                {
                                    perror("Error while writing update customer info into file");
                                }

                                lock.l_type = F_UNLCK;
                                fcntl(customerFileDescriptor, F_SETLKW, &lock);

                                close(customerFileDescriptor);

                                writeBytes = write(connectionFD, ADMIN_MOD_CUSTOMER_SUCCESS, strlen(ADMIN_MOD_CUSTOMER_SUCCESS));
                                if (writeBytes == -1)
                                {
                                    perror("Error while writing ADMIN_MOD_CUSTOMER_SUCCESS message to client!");
                                }
                                readBytes = read(connectionFD, readBuffer, sizeof(readBuffer)); // Dummy read
                                break;
                            }


                            default:
                                writeBytes = write(connectionFD, ADMIN_LOGOUT, strlen(ADMIN_LOGOUT));
                                break;
                        }
                    }
                }
                else
                {
                    // ADMIN LOGIN FAILED
                    printf("Unable to login admin");
                }
                break;
            case 2:
                // Customer
                customer_operation_handler(connectionFD);
                break;
            default:
                // Exit
                printf("Wrong choice entered!!\nExiting...\n");
                break;
            }
        }
    }
    printf("Terminating connection to client!\n");
}

// to get account details of customer
bool get_account_details(int connFD, struct Account *customerAccount)
{
    ssize_t readBytes, writeBytes;            // Number of bytes read from / written to the socket
    char readBuffer[1000], writeBuffer[1000]; // A buffer for reading from / writing to the socket
    char tempBuffer[1000];

    int accountNumber;
    struct Account account;
    int accountFileDescriptor;

    if (customerAccount == NULL)
    {
        writeBytes = write(connFD, GET_ACCOUNT_NUMBER, strlen(GET_ACCOUNT_NUMBER));
        if (writeBytes == -1)
        {
            perror("Error writing GET_ACCOUNT_NUMBER message to client!");
            return false;
        }

        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        if (readBytes == -1)
        {
            perror("Error reading account number response from client!");
            return false;
        }

        accountNumber = atoi(readBuffer);
    }
    else
        accountNumber = customerAccount->accountNumber;

    accountFileDescriptor = open(ACCOUNT_FILE, O_RDONLY);
    if (accountFileDescriptor == -1)
    {
        // Account record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, ACCOUNT_ID_DOESNT_EXIT);
        strcat(writeBuffer, "^");
        perror("Error opening account file in get_account_details!");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing ACCOUNT_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }

    int offset = lseek(accountFileDescriptor, accountNumber * sizeof(struct Account), SEEK_SET);
    if (offset == -1 && errno == EINVAL)
    {
        // Account record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, ACCOUNT_ID_DOESNT_EXIT);
        strcat(writeBuffer, "^");
        perror("Error seeking to account record in get_account_details!");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing ACCOUNT_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    else if (offset == -1)
    {
        perror("Error while seeking to required account record!");
        return false;
    }
    // here coming means account exists so obtain lock
    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};

    int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1)
    {
        perror("Error obtaining read lock on account record!");
        return false;
    }

    // read details
    readBytes = read(accountFileDescriptor, &account, sizeof(struct Account));
    if (readBytes == -1)
    {
        perror("Error reading account record from file!");
        return false;
    }

    // unlock
    lock.l_type = F_UNLCK;
    fcntl(accountFileDescriptor, F_SETLK, &lock);

    if (customerAccount != NULL)
    {
        *customerAccount = account;
        return true;
    }

    bzero(writeBuffer, sizeof(writeBuffer));
    // print details
    sprintf(writeBuffer, "Account Details - \n\tAccount Number : %d\n\tAccount Type : %s\n\tAccount Status : %s", account.accountNumber, (account.isRegularAccount ? "Regular" : "Joint"), (account.active) ? "Active" : "Deactived");
    if (account.active)
    {
        sprintf(tempBuffer, "\n\tAccount Balance:₹ %ld", account.balance);
        strcat(writeBuffer, tempBuffer);
    }

    sprintf(tempBuffer, "\n\tPrimary Owner ID: %d", account.owners[0]);
    strcat(writeBuffer, tempBuffer);
    // if its a joint account
    if (account.owners[1] != -1)
    {
        sprintf(tempBuffer, "\n\tSecondary Owner ID: %d", account.owners[1]);
        strcat(writeBuffer, tempBuffer);
    }

    strcat(writeBuffer, "\n^");

    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

    return true;
}

// to get details of customer
bool get_customer_details(int connFD, int customerID)
{
    ssize_t readBytes, writeBytes;             // Number of bytes read from / written to the socket
    char readBuffer[1000], writeBuffer[10000]; // A buffer for reading from / writing to the socket
    char tempBuffer[1000];

    struct Customer customer;
    int customerFileDescriptor;
    struct flock lock = {F_RDLCK, SEEK_SET, 0, sizeof(struct Account), getpid()};

    if (customerID == -1)
    {
        writeBytes = write(connFD, GET_CUSTOMER_ID, strlen(GET_CUSTOMER_ID));
        if (writeBytes == -1)
        {
            perror("Error while writing GET_CUSTOMER_ID message to client!");
            return false;
        }

        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        if (readBytes == -1)
        {
            perror("Error getting customer ID from client!");
            ;
            return false;
        }

        customerID = atoi(readBuffer);
    }

    customerFileDescriptor = open(CUSTOMER_FILE, O_RDONLY);
    if (customerFileDescriptor == -1)
    {
        // Customer File doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, CUSTOMER_ID_DOESNT_EXIT);
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing CUSTOMER_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    int offset = lseek(customerFileDescriptor, customerID * sizeof(struct Customer), SEEK_SET);
    if (errno == EINVAL)
    {
        // Customer record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, CUSTOMER_ID_DOESNT_EXIT);
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing CUSTOMER_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    else if (offset == -1)
    {
        perror("Error while seeking to required customer record!");
        return false;
    }
    lock.l_start = offset;

    int lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1)
    {
        perror("Error while obtaining read lock on the Customer file!");
        return false;
    }

    readBytes = read(customerFileDescriptor, &customer, sizeof(struct Customer));
    if (readBytes == -1)
    {
        perror("Error reading customer record from file!");
        return false;
    }

    lock.l_type = F_UNLCK;
    fcntl(customerFileDescriptor, F_SETLK, &lock);

    bzero(writeBuffer, sizeof(writeBuffer));
    sprintf(writeBuffer, "Customer Details - \n\tID : %d\n\tName : %s\n\tGender : %c\n\tAge: %d\n\tAccount Number : %d\n\tLoginID : %s", customer.id, customer.name, customer.gender, customer.age, customer.account, customer.login);

    strcat(writeBuffer, "\n\nYou'll now be redirected to the main menu...^");

    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
    if (writeBytes == -1)
    {
        perror("Error writing customer info to client!");
        return false;
    }

    readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
    return true;
}

// to get details of all transactions related to customer
bool get_transaction_details(int connFD, int accountNumber)
{

    ssize_t readBytes, writeBytes;                               // Number of bytes read from / written to the socket
    char readBuffer[1000], writeBuffer[10000], tempBuffer[1000]; // A buffer for reading from / writing to the socket

    struct Account account;

    if (accountNumber == -1)
    {
        // Get the accountNumber
        writeBytes = write(connFD, GET_ACCOUNT_NUMBER, strlen(GET_ACCOUNT_NUMBER));
        if (writeBytes == -1)
        {
            perror("Error writing GET_ACCOUNT_NUMBER message to client!");
            return false;
        }

        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        if (readBytes == -1)
        {
            perror("Error reading account number response from client!");
            return false;
        }

        account.accountNumber = atoi(readBuffer);
    }
    else
        account.accountNumber = accountNumber;

    if (get_account_details(connFD, &account))
    {
        int iter;

        struct Transaction transaction;
        struct tm transactionTime;

        bzero(writeBuffer, sizeof(readBuffer));

        int transactionFileDescriptor = open(TRANSACTION_FILE, O_RDONLY);
        if (transactionFileDescriptor == -1)
        {
            perror("Error while opening transaction file!");
            write(connFD, TRANSACTIONS_NOT_FOUND, strlen(TRANSACTIONS_NOT_FOUND));
            read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
            return false;
        }

        for (iter = 0; iter < MAX_TRANSACTIONS && account.transactions[iter] != -1; iter++)
        {

            int offset = lseek(transactionFileDescriptor, account.transactions[iter] * sizeof(struct Transaction), SEEK_SET);
            if (offset == -1)
            {
                perror("Error while seeking to required transaction record!");
                return false;
            }

            struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Transaction), getpid()};

            int lockingStatus = fcntl(transactionFileDescriptor, F_SETLKW, &lock);
            if (lockingStatus == -1)
            {
                perror("Error obtaining read lock on transaction record!");
                return false;
            }

            readBytes = read(transactionFileDescriptor, &transaction, sizeof(struct Transaction));
            if (readBytes == -1)
            {
                perror("Error reading transaction record from file!");
                return false;
            }

            lock.l_type = F_UNLCK;
            fcntl(transactionFileDescriptor, F_SETLK, &lock);

            transactionTime = *localtime(&(transaction.transactionTime));

            bzero(tempBuffer, sizeof(tempBuffer));
            sprintf(tempBuffer, "Details of transaction %d - \n\t Date : %d:%d %d/%d/%d \n\t Operation : %s \n\t Balance - \n\t\t Before : %ld \n\t\t After : %ld \n\t\t Difference : %ld\n", (iter + 1), transactionTime.tm_hour, transactionTime.tm_min, transactionTime.tm_mday, (transactionTime.tm_mon + 1), (transactionTime.tm_year + 1900), (transaction.operation ? "Deposit" : "Withdraw"), transaction.oldBalance, transaction.newBalance, (transaction.newBalance - transaction.oldBalance));

            if (strlen(writeBuffer) == 0)
                strcpy(writeBuffer, tempBuffer);
            else
                strcat(writeBuffer, tempBuffer);
        }

        close(transactionFileDescriptor);

        if (strlen(writeBuffer) == 0)
        {
            write(connFD, TRANSACTIONS_NOT_FOUND, strlen(TRANSACTIONS_NOT_FOUND));
            read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
            return false;
        }
        else
        {
            strcat(writeBuffer, "^");
            writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
            read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        }
    }
}


bool login_handler(bool isAdmin, int connFD, struct Customer *ptrToCustomerID)
{
    ssize_t readBytes, writeBytes;            // Number of bytes written to / read from the socket
    char readBuffer[1000], writeBuffer[1000]; // Buffer for reading from / writing to the client
    char tempBuffer[1000];
    struct Customer customer;

    int ID;

    bzero(readBuffer, sizeof(readBuffer));
    bzero(writeBuffer, sizeof(writeBuffer));

    // Get login message for respective user type
    if (isAdmin)
        strcpy(writeBuffer, ADMIN_LOGIN_WELCOME);
    else
        strcpy(writeBuffer, CUSTOMER_LOGIN_WELCOME);

    // Append the request for LOGIN ID message
    strcat(writeBuffer, "\n");
    strcat(writeBuffer, LOGIN_ID);

    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
    if (writeBytes == -1)
    {
        perror("Error writing WELCOME & LOGIN_ID message to the client!");
        return false;
    }

    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1)
    {
        perror("Error reading login ID from client!");
        return false;
    }

    bool userFound = false;
    // if user is admin or customer
    if (isAdmin)
    {
        // check with admin login id (only 1 )
        if (strcmp(readBuffer, ADMIN_LOGIN_ID) == 0)
            userFound = true;
    }
    else
    {
        bzero(tempBuffer, sizeof(tempBuffer));
        strcpy(tempBuffer, readBuffer);
        strtok(tempBuffer, "-");
        ID = atoi(strtok(NULL, "-"));
    // open customer file and look for enteries
        int customerFileFD = open(CUSTOMER_FILE, O_RDONLY);
        if (customerFileFD == -1)
        {
            perror("Error opening customer file in read mode!");
            return false;
        }

        off_t offset = lseek(customerFileFD, ID * sizeof(struct Customer), SEEK_SET);
        if (offset >= 0)
        {
            // get read lock to prevent modification
            struct flock lock = {F_RDLCK, SEEK_SET, ID * sizeof(struct Customer), sizeof(struct Customer), getpid()};

            int lockingStatus = fcntl(customerFileFD, F_SETLKW, &lock);
            if (lockingStatus == -1)
            {
                perror("Error obtaining read lock on customer record!");
                return false;
            }

            readBytes = read(customerFileFD, &customer, sizeof(struct Customer));
            if (readBytes == -1)
            {
                ;
                perror("Error reading customer record from file!");
            }

            lock.l_type = F_UNLCK;
            fcntl(customerFileFD, F_SETLK, &lock);

            if (strcmp(customer.login, readBuffer) == 0)
                userFound = true;

            close(customerFileFD);
        }
        // if customer login id not found
        else
        {
            writeBytes = write(connFD, CUSTOMER_LOGIN_ID_DOESNT_EXIT, strlen(CUSTOMER_LOGIN_ID_DOESNT_EXIT));
        }
    }

    // if user found, ask for password
    if (userFound)
    {
        bzero(writeBuffer, sizeof(writeBuffer));
        writeBytes = write(connFD, PASSWORD, strlen(PASSWORD));
        if (writeBytes == -1)
        {
            perror("Error writing PASSWORD message to client!");
            return false;
        }

        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        if (readBytes == 1)
        {
            perror("Error reading password from the client!");
            return false;
        }

        char hashedPassword[1000];
        strcpy(hashedPassword, readBuffer);

// login for respective person
        if (isAdmin)
        {
            if (strcmp(hashedPassword, ADMIN_PASSWORD) == 0)
                return true;
        }
        else
        {
            if (strcmp(hashedPassword, customer.password) == 0)
            {
                *ptrToCustomerID = customer;
                return true;
            }
        }

        bzero(writeBuffer, sizeof(writeBuffer));
        writeBytes = write(connFD, INVALID_PASSWORD, strlen(INVALID_PASSWORD));
    }
    // in case password is not correct
    else
    {
        bzero(writeBuffer, sizeof(writeBuffer));
        writeBytes = write(connFD, INVALID_LOGIN, strlen(INVALID_LOGIN));
    }

    return false;
}


// adding new customer to list
int add_customer(int connFD, bool isPrimary, int newAccountNumber)
{
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];

    struct Customer newCustomer, previousCustomer;

    // open customer file
    int customerFileDescriptor = open(CUSTOMER_FILE, O_RDONLY);
    if (customerFileDescriptor == -1 && errno == ENOENT)
    {
        // Customer file was never created
        newCustomer.id = 0;
    }
    else if (customerFileDescriptor == -1)
    {
        perror("Error while opening customer file");
        return -1;
    }
    else
    {
        int offset = lseek(customerFileDescriptor, -sizeof(struct Customer), SEEK_END);
        if (offset == -1)
        {
            perror("Error seeking to last Customer record!");
            return false;
        }

        // obtaining read lock on customer file so other cannot modify it
        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};
        int lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
        if (lockingStatus == -1)
        {
            perror("Error obtaining read lock on Customer record!");
            return false;
        }

        readBytes = read(customerFileDescriptor, &previousCustomer, sizeof(struct Customer));
        if (readBytes == -1)
        {
            perror("Error while reading Customer record from file!");
            return false;
        }

        lock.l_type = F_UNLCK;
        fcntl(customerFileDescriptor, F_SETLK, &lock);

        close(customerFileDescriptor);

        newCustomer.id = previousCustomer.id + 1;
    }

    // for customer/s name
    if (isPrimary)
        sprintf(writeBuffer, "%s%s", ADMIN_ADD_CUSTOMER_PRIMARY, ADMIN_ADD_CUSTOMER_NAME);
    else
        sprintf(writeBuffer, "%s%s", ADMIN_ADD_CUSTOMER_SECONDARY, ADMIN_ADD_CUSTOMER_NAME);

    writeBytes = write(connFD, writeBuffer, sizeof(writeBuffer));
    if (writeBytes == -1)
    {
        perror("Error writing ADMIN_ADD_CUSTOMER_NAME message to client!");
        return false;
    }

    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1)
    {
        perror("Error reading customer name response from client!");
        ;
        return false;
    }

    strcpy(newCustomer.name, readBuffer);

    // for customer/s gender
    writeBytes = write(connFD, ADMIN_ADD_CUSTOMER_GENDER, strlen(ADMIN_ADD_CUSTOMER_GENDER));
    if (writeBytes == -1)
    {
        perror("Error writing ADMIN_ADD_CUSTOMER_GENDER message to client!");
        return false;
    }

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1)
    {
        perror("Error reading customer gender response from client!");
        return false;
    }

    if (readBuffer[0] == 'M' || readBuffer[0] == 'F' || readBuffer[0] == 'O')
        newCustomer.gender = readBuffer[0];
    else
    {
        writeBytes = write(connFD, ADMIN_ADD_CUSTOMER_WRONG_GENDER, strlen(ADMIN_ADD_CUSTOMER_WRONG_GENDER));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }

    // for customer/s age
    bzero(writeBuffer, sizeof(writeBuffer));
    strcpy(writeBuffer, ADMIN_ADD_CUSTOMER_AGE);
    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
    if (writeBytes == -1)
    {
        perror("Error writing ADMIN_ADD_CUSTOMER_AGE message to client!");
        return false;
    }

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1)
    {
        perror("Error reading customer age response from client!");
        return false;
    }

    int customerAge = atoi(readBuffer);
    if (customerAge == 0)
    {
        // Either client has sent age as 0 (which is invalid) or has entered a non-numeric string
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, ERRON_INPUT_FOR_NUMBER);
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing ERRON_INPUT_FOR_NUMBER message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    newCustomer.age = customerAge;

    newCustomer.account = newAccountNumber;

    strcpy(newCustomer.login, newCustomer.name);
    strcat(newCustomer.login, "-");
    sprintf(writeBuffer, "%d", newCustomer.id);
    strcat(newCustomer.login, writeBuffer);

    char hashedPassword[1000];
    strcpy(hashedPassword, AUTOGEN_PASSWORD);
    strcpy(newCustomer.password, hashedPassword);

    customerFileDescriptor = open(CUSTOMER_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);
    if (customerFileDescriptor == -1)
    {
        perror("Error while creating / opening customer file!");
        return false;
    }
    writeBytes = write(customerFileDescriptor, &newCustomer, sizeof(newCustomer));
    if (writeBytes == -1)
    {
        perror("Error while writing Customer record to file!");
        return false;
    }

    close(customerFileDescriptor);

    bzero(writeBuffer, sizeof(writeBuffer));
    sprintf(writeBuffer, "%s%s-%d\n%s%s", ADMIN_ADD_CUSTOMER_AUTOGEN_LOGIN, newCustomer.name, newCustomer.id, ADMIN_ADD_CUSTOMER_AUTOGEN_PASSWORD, AUTOGEN_PASSWORD);
    strcat(writeBuffer, "^");
    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
    if (writeBytes == -1)
    {
        perror("Error sending customer loginID and password to the client!");
        return false;
    }

    readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

    return newCustomer.id;
}


bool customer_operation_handler(int connFD)
{
    if (login_handler(false, connFD, &loggedInCustomer))
    {
        ssize_t writeBytes, readBytes;            // Number of bytes read from / written to the client
        char readBuffer[1000], writeBuffer[1000]; // A buffer used for reading & writing to the client

        // Get a semaphore for the user
        key_t semKey = ftok(CUSTOMER_FILE, loggedInCustomer.account); // Generate a key based on the account number hence, different customers will have different semaphores

        union semun
        {
            int val; // Value of the semaphore
        } semSet;

        int semctlStatus;
        semIdentifier = semget(semKey, 1, 0); // Get the semaphore if it exists
        if (semIdentifier == -1)
        {
            semIdentifier = semget(semKey, 1, IPC_CREAT | 0700); // Create a new semaphore
            if (semIdentifier == -1)
            {
                perror("Error while creating semaphore!");
                _exit(1);
            }

            semSet.val = 1; // Set a binary semaphore
            semctlStatus = semctl(semIdentifier, 0, SETVAL, semSet);
            if (semctlStatus == -1)
            {
                perror("Error while initializing a binary sempahore!");
                _exit(1);
            }
        }

        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, CUSTOMER_LOGIN_SUCCESS);
        while (1)
        {
            strcat(writeBuffer, "\n");
            strcat(writeBuffer, CUSTOMER_MENU);
            writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
            if (writeBytes == -1)
            {
                perror("Error while writing CUSTOMER_MENU to client!");
                return false;
            }
            bzero(writeBuffer, sizeof(writeBuffer));

            bzero(readBuffer, sizeof(readBuffer));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer));
            if (readBytes == -1)
            {
                perror("Error while reading client's choice for CUSTOMER_MENU");
                return false;
            }
            
            int choice = atoi(readBuffer);
            switch (choice)
            {
            case 1:
                get_customer_details(connFD, loggedInCustomer.id);
                break;
            case 2:
                deposit(connFD);
                break;
            case 3:
                withdraw(connFD);
                break;
            case 4:
                get_balance(connFD);
                break;
            case 5:
                get_transaction_details(connFD, loggedInCustomer.account);
                break;
            case 6:
                change_password(connFD);
                break;
            default:
                writeBytes = write(connFD, CUSTOMER_LOGOUT, strlen(CUSTOMER_LOGOUT));
                return false;
            }
        }
    }
    else
    {
        // CUSTOMER LOGIN FAILED
        return false;
    }
    return true;
}


bool deposit(int connFD)
{
    char readBuffer[1000], writeBuffer[1000];
    ssize_t readBytes, writeBytes;

    struct Account account;
    account.accountNumber = loggedInCustomer.account;

    long int depositAmount = 0;

    // Lock the critical section
    struct sembuf semOp;
    lock_critical_section(&semOp);

    if (get_account_details(connFD, &account))
    {
        
        if (account.active)
        {

            writeBytes = write(connFD, DEPOSIT_AMOUNT, strlen(DEPOSIT_AMOUNT));
            if (writeBytes == -1)
            {
                perror("Error writing DEPOSIT_AMOUNT to client!");
                unlock_critical_section(&semOp);
                return false;
            }

            bzero(readBuffer, sizeof(readBuffer));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer));
            if (readBytes == -1)
            {
                perror("Error reading deposit money from client!");
                unlock_critical_section(&semOp);
                return false;
            }

            depositAmount = atol(readBuffer);
            if (depositAmount != 0)
            {

                int newTransactionID = write_transaction_to_file(account.accountNumber, account.balance, account.balance + depositAmount, 1);
                write_transaction_to_array(account.transactions, newTransactionID);

                account.balance += depositAmount;

                int accountFileDescriptor = open(ACCOUNT_FILE, O_WRONLY);
                off_t offset = lseek(accountFileDescriptor, account.accountNumber * sizeof(struct Account), SEEK_SET);

                struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
                int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);
                if (lockingStatus == -1)
                {
                    perror("Error obtaining write lock on account file!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                writeBytes = write(accountFileDescriptor, &account, sizeof(struct Account));
                if (writeBytes == -1)
                {
                    perror("Error storing updated deposit money in account record!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                lock.l_type = F_UNLCK;
                fcntl(accountFileDescriptor, F_SETLK, &lock);

                write(connFD, DEPOSIT_AMOUNT_SUCCESS, strlen(DEPOSIT_AMOUNT_SUCCESS));
                read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

                get_balance(connFD);

                unlock_critical_section(&semOp);

                return true;
            }
            else
                writeBytes = write(connFD, DEPOSIT_AMOUNT_INVALID, strlen(DEPOSIT_AMOUNT_INVALID));
        }
        else
            write(connFD, ACCOUNT_DEACTIVATED, strlen(ACCOUNT_DEACTIVATED));
        read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

        unlock_critical_section(&semOp);
    }
    else
    {
        // FAIL
        unlock_critical_section(&semOp);
        return false;
    }
}


bool withdraw(int connFD)
{
    char readBuffer[1000], writeBuffer[1000];
    ssize_t readBytes, writeBytes;

    struct Account account;
    account.accountNumber = loggedInCustomer.account;

    long int withdrawAmount = 0;

    // Lock the critical section
    struct sembuf semOp;
    lock_critical_section(&semOp);

    if (get_account_details(connFD, &account))
    {
        if (account.active)
        {

            writeBytes = write(connFD, WITHDRAW_AMOUNT, strlen(WITHDRAW_AMOUNT));
            if (writeBytes == -1)
            {
                perror("Error writing WITHDRAW_AMOUNT message to client!");
                unlock_critical_section(&semOp);
                return false;
            }

            bzero(readBuffer, sizeof(readBuffer));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer));
            if (readBytes == -1)
            {
                perror("Error reading withdraw amount from client!");
                unlock_critical_section(&semOp);
                return false;
            }

            withdrawAmount = atol(readBuffer);

            if (withdrawAmount != 0 && account.balance - withdrawAmount >= 0)
            {

                int newTransactionID = write_transaction_to_file(account.accountNumber, account.balance, account.balance - withdrawAmount, 0);
                write_transaction_to_array(account.transactions, newTransactionID);

                account.balance -= withdrawAmount;

                int accountFileDescriptor = open(ACCOUNT_FILE, O_WRONLY);
                off_t offset = lseek(accountFileDescriptor, account.accountNumber * sizeof(struct Account), SEEK_SET);

                struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
                int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);
                if (lockingStatus == -1)
                {
                    perror("Error obtaining write lock on account record!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                writeBytes = write(accountFileDescriptor, &account, sizeof(struct Account));
                if (writeBytes == -1)
                {
                    perror("Error writing updated balance into account file!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                lock.l_type = F_UNLCK;
                fcntl(accountFileDescriptor, F_SETLK, &lock);

                write(connFD, WITHDRAW_AMOUNT_SUCCESS, strlen(WITHDRAW_AMOUNT_SUCCESS));
                read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

                get_balance(connFD);

                unlock_critical_section(&semOp);

                return true;
            }
            else
                writeBytes = write(connFD, WITHDRAW_AMOUNT_INVALID, strlen(WITHDRAW_AMOUNT_INVALID));
        }
        else
            write(connFD, ACCOUNT_DEACTIVATED, strlen(ACCOUNT_DEACTIVATED));
        read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

        unlock_critical_section(&semOp);
    }
    else
    {
        // FAILURE while getting account information
        unlock_critical_section(&semOp);
        return false;
    }
}


bool get_balance(int connFD)
{
    char buffer[1000];
    struct Account account;
    account.accountNumber = loggedInCustomer.account;
    if (get_account_details(connFD, &account))
    {
        bzero(buffer, sizeof(buffer));
        if (account.active)
        {
            sprintf(buffer, "You have ₹ %ld imaginary money in our bank!^", account.balance);
            write(connFD, buffer, strlen(buffer));
        }
        else
            write(connFD, ACCOUNT_DEACTIVATED, strlen(ACCOUNT_DEACTIVATED));
        read(connFD, buffer, sizeof(buffer)); // Dummy read
    }
    else
    {
        // ERROR while getting balance
        return false;
    }
}


bool change_password(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000], hashedPassword[1000];

    char newPassword[1000];

    // Lock the critical section
    struct sembuf semOp = {0, -1, SEM_UNDO};
    int semopStatus = semop(semIdentifier, &semOp, 1);
    if (semopStatus == -1)
    {
        perror("Error while locking critical section");
        return false;
    }

    writeBytes = write(connFD, PASSWORD_CHANGE_OLD_PASS, strlen(PASSWORD_CHANGE_OLD_PASS));
    if (writeBytes == -1)
    {
        perror("Error writing PASSWORD_CHANGE_OLD_PASS message to client!");
        unlock_critical_section(&semOp);
        return false;
    }

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1)
    {
        perror("Error reading old password response from client");
        unlock_critical_section(&semOp);
        return false;
    }

    if (strcmp(readBuffer, loggedInCustomer.password) == 0)
    {
        // Password matches with old password
        writeBytes = write(connFD, PASSWORD_CHANGE_NEW_PASS, strlen(PASSWORD_CHANGE_NEW_PASS));
        if (writeBytes == -1)
        {
            perror("Error writing PASSWORD_CHANGE_NEW_PASS message to client!");
            unlock_critical_section(&semOp);
            return false;
        }
        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        if (readBytes == -1)
        {
            perror("Error reading new password response from client");
            unlock_critical_section(&semOp);
            return false;
        }

        strcpy(newPassword, readBuffer);

        writeBytes = write(connFD, PASSWORD_CHANGE_NEW_PASS_RE, strlen(PASSWORD_CHANGE_NEW_PASS_RE));
        if (writeBytes == -1)
        {
            perror("Error writing PASSWORD_CHANGE_NEW_PASS_RE message to client!");
            unlock_critical_section(&semOp);
            return false;
        }
        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        if (readBytes == -1)
        {
            perror("Error reading new password reenter response from client");
            unlock_critical_section(&semOp);
            return false;
        }

        if (strcmp(readBuffer, newPassword) == 0)
        {
            // New & reentered passwords match

            strcpy(loggedInCustomer.password, newPassword);

            int customerFileDescriptor = open(CUSTOMER_FILE, O_WRONLY);
            if (customerFileDescriptor == -1)
            {
                perror("Error opening customer file!");
                unlock_critical_section(&semOp);
                return false;
            }

            off_t offset = lseek(customerFileDescriptor, loggedInCustomer.id * sizeof(struct Customer), SEEK_SET);
            if (offset == -1)
            {
                perror("Error seeking to the customer record!");
                unlock_critical_section(&semOp);
                return false;
            }

            struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};
            int lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
            if (lockingStatus == -1)
            {
                perror("Error obtaining write lock on customer record!");
                unlock_critical_section(&semOp);
                return false;
            }

            writeBytes = write(customerFileDescriptor, &loggedInCustomer, sizeof(struct Customer));
            if (writeBytes == -1)
            {
                perror("Error storing updated customer password into customer record!");
                unlock_critical_section(&semOp);
                return false;
            }

            lock.l_type = F_UNLCK;
            lockingStatus = fcntl(customerFileDescriptor, F_SETLK, &lock);

            close(customerFileDescriptor);

            writeBytes = write(connFD, PASSWORD_CHANGE_SUCCESS, strlen(PASSWORD_CHANGE_SUCCESS));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

            unlock_critical_section(&semOp);

            return true;
        }
        else
        {
            // New & reentered passwords don't match
            writeBytes = write(connFD, PASSWORD_CHANGE_NEW_PASS_INVALID, strlen(PASSWORD_CHANGE_NEW_PASS_INVALID));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        }
    }
    else
    {
        // Password doesn't match with old password
        writeBytes = write(connFD, PASSWORD_CHANGE_OLD_PASS_INVALID, strlen(PASSWORD_CHANGE_OLD_PASS_INVALID));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
    }

    unlock_critical_section(&semOp);

    return false;
}


bool lock_critical_section(struct sembuf *semOp)
{
    semOp->sem_flg = SEM_UNDO;
    semOp->sem_op = -1;
    semOp->sem_num = 0;
    int semopStatus = semop(semIdentifier, semOp, 1);
    if (semopStatus == -1)
    {
        perror("Error while locking critical section");
        return false;
    }
    return true;
}


bool unlock_critical_section(struct sembuf *semOp)
{
    semOp->sem_op = 1;
    int semopStatus = semop(semIdentifier, semOp, 1);
    if (semopStatus == -1)
    {
        perror("Error while operating on semaphore!");
        _exit(1);
    }
    return true;
}


void write_transaction_to_array(int *transactionArray, int ID)
{
    // Check if there's any free space in the array to write the new transaction ID
    int iter = 0;
    while (transactionArray[iter] != -1)
        iter++;

    if (iter >= MAX_TRANSACTIONS)
    {
        // No space
        for (iter = 1; iter < MAX_TRANSACTIONS; iter++)
            // Shift elements one step back discarding the oldest transaction
            transactionArray[iter - 1] = transactionArray[iter];
        transactionArray[iter - 1] = ID;
    }
    else
    {
        // Space available
        transactionArray[iter] = ID;
    }
}


int write_transaction_to_file(int accountNumber, long int oldBalance, long int newBalance, bool operation)
{
    struct Transaction newTransaction;
    newTransaction.accountNumber = accountNumber;
    newTransaction.oldBalance = oldBalance;
    newTransaction.newBalance = newBalance;
    newTransaction.operation = operation;
    newTransaction.transactionTime = time(NULL);

    ssize_t readBytes, writeBytes;

    int transactionFileDescriptor = open(TRANSACTION_FILE, O_CREAT | O_APPEND | O_RDWR, S_IRWXU);

    // Get most recent transaction number
    off_t offset = lseek(transactionFileDescriptor, -sizeof(struct Transaction), SEEK_END);
    if (offset >= 0)
    {
        // There exists at least one transaction record
        struct Transaction prevTransaction;
        readBytes = read(transactionFileDescriptor, &prevTransaction, sizeof(struct Transaction));

        newTransaction.transactionID = prevTransaction.transactionID + 1;
    }
    else
        // No transaction records exist
        newTransaction.transactionID = 0;

    writeBytes = write(transactionFileDescriptor, &newTransaction, sizeof(struct Transaction));

    return newTransaction.transactionID;
}
