#include <iostream>
#include <string>
#include "sqlite3.h"

using namespace std;

sqlite3* db;

// دالة مساعدة لتنفيذ أوامر SQL البسيطة
void runSQL(const string& sql) {
    char* errMsg = 0;
    if (sqlite3_exec(db, sql.c_str(), 0, 0, &errMsg) != SQLITE_OK) {
        cout << "SQL Error: " << errMsg << endl;
        sqlite3_free(errMsg);
    }
}

// دالة لطباعة نتائج الاستعلام (SELECT)
int printCallback(void* NotUsed, int argc, char** argv, char** azColName) {
    for (int i = 0; i < argc; i++) {
        cout << azColName[i] << ": " << (argv[i] ? argv[i] : "NULL") << " | ";
    }
    cout << endl;
    return 0;
}

// 1. إضافة حساب جديد
void addAccount() {
    string accNum, name, type, phone, address;
    double balance;
    cout << "Account Number: "; cin >> accNum;
    cin.ignore();
    cout << "Holder Name: "; getline(cin, name);
    cout << "Type (Savings/Current): "; cin >> type;
    cout << "Initial Balance: "; cin >> balance;
    cout << "Phone: "; cin >> phone;
    cin.ignore();
    cout << "Address: "; getline(cin, address);

    if (balance < 0) { cout << "Invalid balance!\n"; return; }

    // استخدام Prepared Statement للحماية من SQL Injection
    sqlite3_stmt* stmt;
    string sql = "INSERT INTO accounts (account_number, holder_name, account_type, balance, phone, address) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, accNum.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 4, balance);
    sqlite3_bind_text(stmt, 5, phone.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, address.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_DONE) cout << "Account Added!\n";
    else cout << "Error: Account number might already exist.\n";
    sqlite3_finalize(stmt);
}

// 2. عرض كل الحسابات
void displayAll() {
    cout << "\n--- All Accounts ---\n";
    sqlite3_exec(db, "SELECT * FROM accounts;", printCallback, 0, 0);
}

// 3. البحث عن حساب
void searchAccount() {
    string query;
    cin.ignore();
    cout << "Enter Account Number or Name: "; getline(cin, query);
    string sql = "SELECT * FROM accounts WHERE account_number = '" + query + "' OR holder_name LIKE '%" + query + "%';";
    sqlite3_exec(db, sql.c_str(), printCallback, 0, 0);
}

// 4. إيداع وسحب وتحديث جدول المعاملات
void depositOrWithdraw(bool isDeposit) {
    string accNum;
    double amount;
    cout << "Account Number: "; cin >> accNum;
    cout << "Amount: "; cin >> amount;
    if (amount <= 0) { cout << "Invalid amount!\n"; return; }

    // التأكد من الحساب والرصيد الحالي
    sqlite3_stmt* stmt;
    string checkSql = "SELECT account_id, balance FROM accounts WHERE account_number = ?;";
    sqlite3_prepare_v2(db, checkSql.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, accNum.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int accId = sqlite3_column_int(stmt, 0);
        double currentBalance = sqlite3_column_double(stmt, 1);
        sqlite3_finalize(stmt);

        if (!isDeposit && currentBalance < amount) {
            cout << "Insufficient funds! (No overdrafts allowed)\n";
            return;
        }

        double newBalance = isDeposit ? (currentBalance + amount) : (currentBalance - amount);
        string type = isDeposit ? "Deposit" : "Withdraw";

        // تحديث الرصيد + إضافة سجل في جدول transactions
        runSQL("UPDATE accounts SET balance = " + to_string(newBalance) + " WHERE account_id = " + to_string(accId) + ";");
        runSQL("INSERT INTO transactions (account_id, type, amount, balance_after) VALUES (" +
               to_string(accId) + ", '" + type + "', " + to_string(amount) + ", " + to_string(newBalance) + ");");
        
        cout << type << " successful! New Balance: " << newBalance << endl;
    } else {
        cout << "Account not found!\n";
        sqlite3_finalize(stmt);
    }
}

// 5. تعديل بيانات الحساب
void updateAccount() {
    string accNum, name, phone, address;
    cout << "Account Number to update: "; cin >> accNum;
    cin.ignore();
    cout << "New Name: "; getline(cin, name);
    cout << "New Phone: "; cin >> phone;
    cin.ignore();
    cout << "New Address: "; getline(cin, address);

    sqlite3_stmt* stmt;
    string sql = "UPDATE accounts SET holder_name = ?, phone = ?, address = ? WHERE account_number = ?;";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, phone.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, address.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, accNum.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_DONE) cout << "Updated successfully!\n";
    sqlite3_finalize(stmt);
}

// 6. حذف حساب
void deleteAccount() {
    string accNum;
    cout << "Account Number to delete: "; cin >> accNum;
    runSQL("DELETE FROM accounts WHERE account_number = '" + accNum + "';");
    cout << "Account deleted (if it existed).\n";
}

int main() {
    // فتح أو إنشاء قاعدة البيانات
    if (sqlite3_open("bank.db", &db)) {
        cout << "Can't open database!\n";
        return 1;
    }

    // إنشاء الجداول بنفس المواصفات المطلوبة في الصورة الثانية
    runSQL("CREATE TABLE IF NOT EXISTS accounts ("
           "account_id INTEGER PRIMARY KEY AUTOINCREMENT, "
           "account_number VARCHAR(20) UNIQUE, "
           "holder_name VARCHAR(100), "
           "account_type VARCHAR(20), "
           "balance DECIMAL(12,2), "
           "phone VARCHAR(20), "
           "address VARCHAR(200), "
           "created_at DATETIME DEFAULT CURRENT_TIMESTAMP);");

    runSQL("CREATE TABLE IF NOT EXISTS transactions ("
           "transaction_id INTEGER PRIMARY KEY AUTOINCREMENT, "
           "account_id INTEGER, "
           "type VARCHAR(10), "
           "amount DECIMAL(12,2), "
           "balance_after DECIMAL(12,2), "
           "transaction_date DATETIME DEFAULT CURRENT_TIMESTAMP, "
           "FOREIGN KEY(account_id) REFERENCES accounts(account_id));");

    int choice;
    while (true) {
        cout << "\n=== Bank Management System ===\n"
             << "1. Add Account\n2. Display All\n3. Search Account\n"
             << "4. Deposit\n5. Withdraw\n6. Update Info\n7. Delete Account\n8. Exit\nChoice: ";
        cin >> choice;

        if (choice == 1) addAccount();
        else if (choice == 2) displayAll();
        else if (choice == 3) searchAccount();
        else if (choice == 4) depositOrWithdraw(true);
        else if (choice == 5) depositOrWithdraw(false);
        else if (choice == 6) updateAccount();
        else if (choice == 7) deleteAccount();
        else if (choice == 8) break;
        else cout << "Invalid choice!\n";
    }

    // 8. إغلاق الاتصال بقاعدة البيانات بأمان
    sqlite3_close(db);
    return 0;
}
