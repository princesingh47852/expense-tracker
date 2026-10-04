let tracker = null;
let currentBudget = 10000;

// Initialize Application once WebAssembly module is loaded
async function initApp() {
    try {
        if (typeof createExpenseModule === 'undefined') return alert("WASM Module missing! Compile it first.");
        
        const Module = await createExpenseModule();
        tracker = new Module.ExpenseTracker();
        
        loadData();
        setupEventListeners();
        updateUI();
    } catch (e) {
        console.error("Failed to load WASM module", e);
    }
}

function loadData() {
    currentBudget = parseFloat(localStorage.getItem('monthlyBudget')) || 10000;
    document.getElementById('budgetInput').value = currentBudget;

    // Load saved JSON and push into C++ memory
    const savedExpenses = JSON.parse(localStorage.getItem('expenses') || '[]');
    savedExpenses.forEach(e => {
        tracker.addExpense(e.id, e.amount, e.category, e.description, e.date, e.type === 'Recurring');
    });
}

function saveData() {
    localStorage.setItem('monthlyBudget', currentBudget);
    localStorage.setItem('expenses', tracker.getExpensesJSON()); // C++ generates the JSON
}

function setupEventListeners() {
    document.getElementById('expenseForm').addEventListener('submit', (e) => {
        e.preventDefault();
        
        // Pass data to C++ WebAssembly
        const status = tracker.addExpense(
            Date.now(),
            parseFloat(document.getElementById('amount').value),
            document.getElementById('category').value,
            document.getElementById('description').value,
            document.getElementById('date').value,
            document.getElementById('isRecurring').checked
        );

        // Exception Handling check from C++
        if (status !== "Success") {
            alert(status); // C++ throws error if amount <= 0
            return;
        }
        
        saveData();
        updateUI();
        e.target.reset();
    });

    document.getElementById('setBudgetBtn').addEventListener('click', () => {
        currentBudget = parseFloat(document.getElementById('budgetInput').value);
        saveData();
        updateUI();
    });

    document.getElementById('searchDesc').addEventListener('input', updateUI);
    document.getElementById('filterCategory').addEventListener('change', updateUI);
}

// Global function for inline HTML delete buttons
window.deleteExpense = function(id) {
    if(confirm("Are you sure you want to delete this expense?")) {
        tracker.deleteExpense(id);
        saveData();
        updateUI();
    }
}

// Drastically reduced JavaScript because C++ handles the logic and HTML generation!
function updateUI() {
    const search = document.getElementById('searchDesc').value;
    const cat = document.getElementById('filterCategory').value;

    // 1. C++ generates the entire table HTML directly!
    document.getElementById('expenseTableBody').innerHTML = tracker.getTableHTML(cat, search);

    // 2. Dashboard Totals calculated in C++
    const totalSpent = tracker.getTotalExpenses();
    
    document.getElementById('totalSpent').textContent = totalSpent.toFixed(2);
    document.getElementById('highestCategory').textContent = tracker.getHighestCategory();
    document.getElementById('transactionCount').textContent = tracker.getTransactionCount();
    
    // 3. Update Budget Progress
    document.getElementById('spentAmount').textContent = totalSpent.toFixed(2);
    const leftAmount = currentBudget - totalSpent;
    document.getElementById('leftAmount').textContent = leftAmount >= 0 ? leftAmount.toFixed(2) : '0 (Over!)';

    const percent = Math.min((totalSpent / currentBudget) * 100, 100);
    const progressFill = document.getElementById('progressFill');
    progressFill.style.width = percent + '%';
    progressFill.style.background = percent >= 90 ? '#ffb3b3' : '#b5c9a7';
}

document.addEventListener("DOMContentLoaded", initApp);
