#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include "sqlite3.h"

using namespace std;

class Account {
public:
    virtual void displayAccount() const = 0;
};

class SavingsAccount : public Account {
private:
    int id;
    string name;
    string contact;
    int age;
    double balance;
    vector<string> transactionHistory;

public:
    SavingsAccount(int accId, const string& accName, const string& accContact, int accAge)
        : id(accId), name(accName), contact(accContact), age(accAge), balance(0) {}

    void deposit(double amount, sqlite3 *db) {
        balance += amount;
        transactionHistory.push_back("Deposited: " + to_string(amount));
        cout << "Deposited: " << amount << ", New Balance: " << balance << endl;
        logTransaction(db, "Deposit", amount);
    }

    void withdraw(double amount, sqlite3 *db) {
        if (amount > balance) {
            cout << "Insufficient funds!" << endl;
        } else {
            balance -= amount;
            transactionHistory.push_back("Withdrawn: " + to_string(amount));
            cout << "Withdrawn: " << amount << ", New Balance: " << balance << endl;
            logTransaction(db, "Withdraw", amount);
        }
    }

    void displayAccount() const override {
        cout << "Account ID: " << id << ", Name: " << name << ", Contact: " << contact 
             << ", Age: " << age << ", Balance: " << balance << endl;
    }

    void displayTransactions() const {
        cout << "Transaction History for " << name << " (Account ID: " << id << "):" << endl;
        for (const auto& transaction : transactionHistory) {
            cout << transaction << endl;
        }
    }

    int getId() const { return id; }

    void logTransaction(sqlite3 *db, const string& type, double amount) const {
        string sql = "INSERT INTO Transactions (account_id, amount, type) VALUES (" +
                     to_string(id) + ", " + to_string(amount) + ", '" + type + "');";
        char *errMsg;
        int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &errMsg);
        if (rc != SQLITE_OK) {
            cerr << "SQL error: " << errMsg << endl;
            sqlite3_free(errMsg);
        }
    }
};

int generateAccountId() {
    return rand() % 9000000 + 1000000;
}

SavingsAccount createAccount(vector<SavingsAccount>& accounts) {
    string name, contact;
    int age;

    cout << "Enter account holder's name: ";
    cin.ignore();
    getline(cin, name);
    cout << "Enter account holder's contact number: ";
    getline(cin, contact);
    cout << "Enter account holder's age: ";
    cin >> age;

    int accountId = generateAccountId();
    SavingsAccount newAccount(accountId, name, contact, age);
    accounts.push_back(newAccount);

    cout << "Account created successfully! Account ID: " << accountId << endl;
    return newAccount;
}

void createTableIfNotExists(sqlite3 *db) {
    const char *sql = "CREATE TABLE IF NOT EXISTS Transactions ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "account_id INTEGER, "
                      "amount REAL, "
                      "type TEXT);";
    char *errMsg;
    int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
    }
}

void displayAllAccounts(const vector<SavingsAccount>& accounts) {
    cout << "\nAll Accounts:\n";
    cout << "ID\t\tName\t\tContact\t\tAge\tBalance\n";
    for (const auto& account : accounts) {
        account.displayAccount();
    }
}

void displayTransactions(sqlite3 *db) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT * FROM Transactions;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    
    if (rc != SQLITE_OK) {
        cerr << "Failed to fetch transactions: " << sqlite3_errmsg(db) << endl;
        return;
    }

    cout << "\nTransactions:\n";
    cout << left << setw(10) << "ID"
         << setw(15) << "Account ID"
         << setw(10) << "Amount"
         << "Type\n";

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        int accountId = sqlite3_column_int(stmt, 1);
        double amount = sqlite3_column_double(stmt, 2);
        const unsigned char *type = sqlite3_column_text(stmt, 3);
        
        cout << left << setw(10) << id
             << setw(15) << accountId
             << setw(10) << amount
             << type << endl;
    }

    sqlite3_finalize(stmt);
}

void deleteAccount(sqlite3 *db, vector<SavingsAccount>& accounts) {
    int accountId;
    cout << "Enter Account ID to delete: ";
    cin >> accountId;

    auto it = remove_if(accounts.begin(), accounts.end(),
        [accountId](const SavingsAccount& account) { return account.getId() == accountId; });
    
    if (it != accounts.end()) {
        accounts.erase(it, accounts.end());
        cout << "Account deleted successfully!\n";
        
        string sql = "DELETE FROM Transactions WHERE account_id = " + to_string(accountId) + ";";
        char *errMsg;
        int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &errMsg);
        if (rc != SQLITE_OK) {
            cerr << "SQL error: " << errMsg << endl;
            sqlite3_free(errMsg);
        } else {
            cout << "Transactions for account ID " << accountId << " deleted.\n";
        }
    } else {
        cout << "Account ID not found.\n";
    }
}

int main() {
    srand(time(0));
    sqlite3 *db;
    int rc = sqlite3_open("banking.db", &db);
    if (rc) {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return 1;
    }

    createTableIfNotExists(db);
    vector<SavingsAccount> accounts;
    int choice;
    double amount;

    while (true) {
        cout << "\n1. Create Account\n2. Deposit\n3. Withdraw\n4. View Transactions\n5. Display All Accounts\n6. Delete Account\n7. Exit\nChoose an option: ";
        
        if (!(cin >> choice)) {
            cout << "Invalid input! Please enter a number." << endl;
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }

        switch (choice) {
            case 1:
                createAccount(accounts);
                break;
            case 2: {
                int depositId;
                cout << "Enter account ID to deposit: ";
                cin >> depositId;
                bool found = false;
                for (auto& account : accounts) {
                    if (account.getId() == depositId) {
                        found = true;
                        cout << "Enter amount to deposit: ";
                        cin >> amount;
                        if (amount < 0) {
                            cout << "Please enter a positive amount." << endl;
                        } else {
                            account.deposit(amount, db);
                        }
                        break;
                    }
                }
                if (!found) {
                    cout << "Account ID not found." << endl;
                }
                break;
            }
            case 3: {
                int withdrawId;
                cout << "Enter account ID to withdraw: ";
                cin >> withdrawId;
                bool found = false;
                for (auto& account : accounts) {
                    if (account.getId() == withdrawId) {
                        found = true;
                        cout << "Enter amount to withdraw: ";
                        cin >> amount;
                        if (amount < 0) {
                            cout << "Please enter a positive amount." << endl;
                        } else {
                            account.withdraw(amount, db);
                        }
                        break;
                    }
                }
                if (!found) {
                    cout << "Account ID not found." << endl;
                }
                break;
            }
            case 4:
                displayTransactions(db);
                break;

            case 5:
                displayAllAccounts(accounts);
                break;

            case 6:
                deleteAccount(db, accounts);
                break;

            case 7:
                cout << "Exiting the application." << endl;
                sqlite3_close(db);
                return 0;

            default:
                cout << "Invalid choice. Please try again." << endl;
        }
    }

    sqlite3_close(db);
    return 0;
}
