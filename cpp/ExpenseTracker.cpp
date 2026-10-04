#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <cstdio>
#include <emscripten/bind.h>

using namespace std;
using namespace emscripten;

// Helper function to format money to 2 decimal places (e.g., 10.50)
string formatAmount(double amount) {
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "%.2f", amount);
    return string(buffer);
}

// 1. EXCEPTION HANDLING
// We create our own custom error if the user enters a bad amount
class InvalidExpenseException : public runtime_error {
public:
    InvalidExpenseException(const string& message) : runtime_error(message) {}
};

// 2. ENCAPSULATION & ABSTRACTION (Base Class)
// This is the parent class for an Expense
class Expense {
protected: // Protected means children classes can see these variables
    int id;
    double amount;
    string category;
    string description;
    string date;

public:
    // Constructor: This runs when a new Expense is created
    Expense(int id, double amount, string category, string description, string date)
        : id(id), amount(amount), category(category), description(description), date(date) {
        
        // Throw an error if the amount is 0 or negative
        if (amount <= 0) {
            throw InvalidExpenseException("Amount must be greater than 0!");
        }
        if (category.empty() || description.empty()) {
            throw InvalidExpenseException("Fields cannot be empty!");
        }
    }
    
    // Virtual destructor is safe practice for inheritance
    virtual ~Expense() = default;

    // Getters (Encapsulation: we hide variables but allow reading them safely)
    int getId() const { return id; }
    double getAmount() const { return amount; }
    string getCategory() const { return category; }
    string getDescription() const { return description; }
    
    // 3. POLYMORPHISM
    // Virtual function. This allows child classes to change how the row is drawn.
    virtual string getHTMLRow() const {
        string html = "<tr>";
        html += "<td>" + date + "</td>";
        html += "<td>" + description + "</td>";
        html += "<td>" + category + "</td>";
        html += "<td>One-time</td>";
        html += "<td>₹" + formatAmount(amount) + "</td>";
        html += "<td><button class='btn-delete' onclick='deleteExpense(" + to_string(id) + ")'>Delete</button></td>";
        html += "</tr>";
        return html;
    }
    
    // Convert object data to a JSON string so JavaScript can save it in LocalStorage
    virtual string toJSON() const {
        return "{\"id\":" + to_string(id) + ",\"amount\":" + to_string(amount) + 
               ",\"category\":\"" + category + "\",\"description\":\"" + description + 
               "\",\"date\":\"" + date + "\",\"type\":\"One-time\"}";
    }
};

// 4. INHERITANCE
// RecurringExpense is a child of Expense. It copies everything from Expense automatically.
class RecurringExpense : public Expense {
public:
    // Call the parent's constructor to set the basic details
    RecurringExpense(int id, double amount, string category, string description, string date)
        : Expense(id, amount, category, description, date) {}

    // Overriding Polymorphic Methods (Changing parent's default behavior)
    string getHTMLRow() const override {
        string html = "<tr>";
        html += "<td>" + date + "</td>";
        html += "<td>" + description + "</td>";
        html += "<td>" + category + "</td>";
        html += "<td><b>Recurring</b></td>"; // Bold text so it looks different
        html += "<td>₹" + formatAmount(amount) + "</td>";
        html += "<td><button class='btn-delete' onclick='deleteExpense(" + to_string(id) + ")'>Delete</button></td>";
        html += "</tr>";
        return html;
    }
    
    string toJSON() const override {
        return "{\"id\":" + to_string(id) + ",\"amount\":" + to_string(amount) + 
               ",\"category\":\"" + category + "\",\"description\":\"" + description + 
               "\",\"date\":\"" + date + "\",\"type\":\"Recurring\"}";
    }
};

// 5. MANAGER CLASS (Uses STL Vectors and Maps)
// This class acts as the database managing the list of all expenses
class ExpenseTracker {
private:
    // STL Vector (A dynamic array) that holds pointers to Expense objects
    vector<Expense*> expensesList; 

    // Helper to convert a string to lowercase for searching
    string makeLowerCase(string text) const {
        transform(text.begin(), text.end(), text.begin(), ::tolower);
        return text;
    }

public:
    ExpenseTracker() {}

    // Destructor: Clean up memory when the program closes
    ~ExpenseTracker() {
        for (auto expense : expensesList) {
            delete expense;
        }
    }

    // Add a new expense. Returns "Success" or an error message.
    string addExpense(int id, double amount, string category, string description, string date, bool isRecurring) {
        try {
            if (isRecurring) {
                // Create a Recurring child object
                expensesList.push_back(new RecurringExpense(id, amount, category, description, date));
            } else {
                // Create a standard Parent object
                expensesList.push_back(new Expense(id, amount, category, description, date));
            }
            return "Success";
        } catch (const InvalidExpenseException& error) {
            // We caught our custom error (e.g. amount was negative)
            return string("Error: ") + error.what(); 
        } catch (const exception& error) {
            return "Unknown error occurred.";
        }
    }

    // Find the expense by ID and remove it from the vector
    void deleteExpense(int id) {
        for (auto it = expensesList.begin(); it != expensesList.end(); ++it) {
            if ((*it)->getId() == id) {
                delete *it; // Free memory
                expensesList.erase(it); // Remove from list
                break;
            }
        }
    }

    // Loop through all expenses and add up the amounts
    double getTotalExpenses() const {
        double total = 0.0;
        for (const auto expense : expensesList) {
            total += expense->getAmount();
        }
        return total;
    }

    // Find which category we spent the most money on
    string getHighestCategory() const {
        // STL Map (Like a dictionary) to store total spent per category
        map<string, double> categoryTotals; 
        
        for (const auto expense : expensesList) {
            categoryTotals[expense->getCategory()] += expense->getAmount();
        }

        string highestCategoryName = "None";
        double maxSpent = 0.0;
        
        // Loop through the dictionary to find the maximum
        for (const auto& pair : categoryTotals) {
            if (pair.second > maxSpent) {
                maxSpent = pair.second;
                highestCategoryName = pair.first;
            }
        }
        return highestCategoryName;
    }

    // How many items are in the list?
    int getTransactionCount() const {
        return expensesList.size();
    }

    // C++ generates the HTML table for the web browser directly
    string getTableHTML(string filterCategory, string searchWord) const {
        string finalHTML = "";
        string searchLower = makeLowerCase(searchWord);

        // Loop backwards so the newest expense is at the top of the table
        for (auto it = expensesList.rbegin(); it != expensesList.rend(); ++it) {
            Expense* currentExpense = *it;
            
            // If we are filtering by category and it doesn't match, skip it
            if (filterCategory != "All" && currentExpense->getCategory() != filterCategory) {
                continue;
            }
            
            // If the user typed something in the search bar, check if it matches the description
            if (!searchLower.empty()) {
                string descriptionLower = makeLowerCase(currentExpense->getDescription());
                // string::npos means "not found"
                if (descriptionLower.find(searchLower) == string::npos) {
                    continue; // Skip it because it doesn't match the search
                }
            }
            
            // Polymorphism in action: Calls the correct getHTMLRow() automatically!
            finalHTML += currentExpense->getHTMLRow();
        }
        return finalHTML;
    }

    // Convert all expenses to a giant JSON string for saving in the browser
    string getExpensesJSON() const {
        string json = "[";
        for (size_t i = 0; i < expensesList.size(); ++i) {
            json += expensesList[i]->toJSON();
            if (i < expensesList.size() - 1) {
                json += ","; // Add a comma between items
            }
        }
        json += "]";
        return json;
    }
};

// 6. EMSCRIPTEN (The Bridge)
// This tells WebAssembly to make our C++ class accessible to JavaScript
EMSCRIPTEN_BINDINGS(expense_module) {
    class_<ExpenseTracker>("ExpenseTracker")
        .constructor<>()
        .function("addExpense", &ExpenseTracker::addExpense)
        .function("deleteExpense", &ExpenseTracker::deleteExpense)
        .function("getTotalExpenses", &ExpenseTracker::getTotalExpenses)
        .function("getHighestCategory", &ExpenseTracker::getHighestCategory)
        .function("getTransactionCount", &ExpenseTracker::getTransactionCount)
        .function("getTableHTML", &ExpenseTracker::getTableHTML)
        .function("getExpensesJSON", &ExpenseTracker::getExpensesJSON);
}
