#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <iomanip>

using namespace std;

// 任務結構
struct Task {
    int id;
    string title;
    int urgency;      // 1-5
    int importance;   // 1-5
    double estimatedTime;
    string deadline;  // "YYYY-MM-DD" 或空字串
    double score;
};

// 工具函式：計算給定日期距離今天的天數
double getDaysFromNow(const string& dateStr) {
    if (dateStr.empty()) return 0.0;
    
    int y, m, d;
    if (sscanf(dateStr.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return 0.0;
    
    tm time_in = {0, 0, 0, d, m - 1, y - 1900};
    time_t targetTime = mktime(&time_in);
    time_t now = time(nullptr);
    
    double diffSeconds = difftime(targetTime, now);
    return diffSeconds / (60.0 * 60.0 * 24.0);
}

// 優先權分數計算
double calcScore(int u, int i, double t, const string& deadline) {
    double base = (u * 2 + i * 3) / log2(t + 2.0);
    if (deadline.empty()) return base;

    double days = getDaysFromNow(deadline);
    if (days <= 0) return base * 15.0; // 逾期
    
    double d_factor = 1.0 + 6.0 * exp(-0.2 * days);
    return base * d_factor;
}

// ==========================================
// BST 二元搜尋樹實作 (依截止日期索引)
// ==========================================
struct BSTNode {
    string key;
    vector<Task> tasks;
    BSTNode* left;
    BSTNode* right;
    
    BSTNode(string k, Task t) : key(k), left(nullptr), right(nullptr) {
        tasks.push_back(t);
    }
};

class TaskBST {
private:
    BSTNode* root;

    BSTNode* insert(BSTNode* node, const string& k, Task task) {
        if (!node) return new BSTNode(k, task);
        if (k < node->key) node->left = insert(node->left, k, task);
        else if (k > node->key) node->right = insert(node->right, k, task);
        else node->tasks.push_back(task);
        return node;
    }

    void inorder(BSTNode* node, vector<Task>& result) {
        if (!node) return;
        inorder(node->left, result);
        for (const auto& t : node->tasks) result.push_back(t);
        inorder(node->right, result);
    }

    BSTNode* search(BSTNode* node, const string& k) {
        if (!node) return nullptr;
        if (k == node->key) return node;
        return k < node->key ? search(node->left, k) : search(node->right, k);
    }

    void destroy(BSTNode* node) {
        if (node) {
            destroy(node->left);
            destroy(node->right);
            delete node;
        }
    }

public:
    TaskBST() : root(nullptr) {}
    ~TaskBST() { destroy(root); }

    void insertTask(Task task) {
        if (!task.deadline.empty()) {
            root = insert(root, task.deadline, task);
        }
    }

    vector<Task> searchTasks(const string& dateKey) {
        BSTNode* res = search(root, dateKey);
        return res ? res->tasks : vector<Task>();
    }

    vector<Task> getAllInorder() {
        vector<Task> result;
        inorder(root, result);
        return result;
    }

    void rebuild(const vector<Task>& tasks) {
        destroy(root);
        root = nullptr;
        for (const auto& t : tasks) {
            insertTask(t);
        }
    }
};

// ==========================================
// Max-Heap 最大堆積實作
// ==========================================
class TaskHeap {
private:
    vector<Task> heap;

    void heapSwap(int a, int b) {
        Task temp = heap[a];
        heap[a] = heap[b];
        heap[b] = temp;
    }

    void heapUp(int i) {
        while (i > 0) {
            int p = (i - 1) / 2;
            if (heap[p].score >= heap[i].score) break;
            heapSwap(p, i);
            i = p;
        }
    }

    void heapDown(int i) {
        int n = heap.size();
        while (true) {
            int max_idx = i;
            int l = 2 * i + 1;
            int r = 2 * i + 2;

            if (l < n && heap[l].score > heap[max_idx].score) max_idx = l;
            if (r < n && heap[r].score > heap[max_idx].score) max_idx = r;

            if (max_idx == i) break;
            heapSwap(i, max_idx);
            i = max_idx;
        }
    }

public:
    void insert(Task task) {
        heap.push_back(task);
        heapUp(heap.size() - 1);
    }

    Task* extractMax() {
        if (heap.empty()) return nullptr;
        
        Task* top = new Task(heap[0]);
        Task last = heap.back();
        heap.pop_back();
        
        if (!heap.empty()) {
            heap[0] = last;
            heapDown(0);
        }
        return top;
    }

    Task* peek() {
        return heap.empty() ? nullptr : &heap[0];
    }

    void snoozeTop() {
        if (heap.empty()) return;
        heap[0].score *= 0.3; // 暫緩：分數降為 30%
        heapDown(0);
    }

    vector<Task> getElements() const {
        return heap;
    }

    void rebuild() {
        vector<Task> items = heap;
        heap.clear();
        for (const auto& t : items) {
            insert(t);
        }
    }

    void recalculateAll() {
        for (auto& t : heap) {
            t.score = calcScore(t.urgency, t.importance, t.estimatedTime, t.deadline);
        }
        rebuild();
    }
};

// ==========================================
// 主程式與 UI
// ==========================================
int main() {
    TaskHeap taskHeap;
    TaskBST taskBst;
    int taskIdCounter = 1;

    // 載入預設資料
    vector<Task> samples = {
        {0, "準備期末報告簡報", 5, 5, 3.0, "2026-05-08", 0},
        {0, "修復登入頁面 bug", 4, 4, 1.0, "2026-05-09", 0},
        {0, "回覆導師 email", 3, 3, 0.25, "", 0},
        {0, "閱讀演算法教材第五章", 2, 4, 2.0, "2026-05-13", 0}
    };
    
    for (auto& st : samples) {
        st.id = taskIdCounter++;
        st.score = calcScore(st.urgency, st.importance, st.estimatedTime, st.deadline);
        taskHeap.insert(st);
        taskBst.insertTask(st);
    }

    while (true) {
        cout << "\n============================================\n";
        cout << " 智慧型待辦事項優先權排程器 (C++ Console) \n";
        cout << "============================================\n";
        
        Task* top = taskHeap.peek();
        cout << "[目前專注] ";
        if (top) {
            cout << top->title << " (分數: " << fixed << setprecision(2) << top->score << ")\n";
        } else {
            cout << "尚無任務\n";
        }
        
        cout << "--------------------------------------------\n";
        cout << "1. 新增任務\n";
        cout << "2. 完成最高優先權任務\n";
        cout << "3. 暫緩最高優先權任務 (Snooze)\n";
        cout << "4. 檢視所有任務 (依分數遞減)\n";
        cout << "5. BST 日期搜尋\n";
        cout << "6. 重算所有分數 (模擬時間推進)\n";
        cout << "0. 退出\n";
        cout << "請選擇操作: ";
        
        int choice;
        if (!(cin >> choice)) break;

        if (choice == 0) break;

        switch (choice) {
            case 1: {
                Task t;
                t.id = taskIdCounter++;
                cout << "任務名稱: ";
                cin.ignore();
                getline(cin, t.title);
                cout << "緊急程度 (1-5): "; cin >> t.urgency;
                cout << "重要程度 (1-5): "; cin >> t.importance;
                cout << "預計時間 (小時): "; cin >> t.estimatedTime;
                cout << "截止日期 (YYYY-MM-DD，無則留空): ";
                cin.ignore();
                getline(cin, t.deadline);
                
                t.score = calcScore(t.urgency, t.importance, t.estimatedTime, t.deadline);
                taskHeap.insert(t);
                taskBst.insertTask(t);
                cout << ">> 任務已加入堆積！分數為 " << t.score << "\n";
                break;
            }
            case 2: {
                Task* completed = taskHeap.extractMax();
                if (completed) {
                    cout << ">> 已完成任務: " << completed->title << "\n";
                    delete completed;
                    taskBst.rebuild(taskHeap.getElements());
                } else {
                    cout << ">> 佇列為空！\n";
                }
                break;
            }
            case 3: {
                taskHeap.snoozeTop();
                taskBst.rebuild(taskHeap.getElements());
                cout << ">> 已暫緩當前任務\n";
                break;
            }
            case 4: {
                vector<Task> elements = taskHeap.getElements();
                sort(elements.begin(), elements.end(), [](const Task& a, const Task& b){
                    return a.score > b.score;
                });
                cout << "\n--- 任務佇列 ---\n";
                for (const auto& t : elements) {
                    cout << "- [" << fixed << setprecision(2) << t.score << "] " 
                         << t.title << " (U:" << t.urgency << " I:" << t.importance << ")";
                    if (!t.deadline.empty()) cout << " 截止:" << t.deadline;
                    cout << "\n";
                }
                break;
            }
            case 5: {
                string dateKey;
                cout << "請輸入搜尋日期 (YYYY-MM-DD): ";
                cin >> dateKey;
                vector<Task> results = taskBst.searchTasks(dateKey);
                cout << "\n>> BST 搜尋結果 (" << results.size() << " 筆):\n";
                for (const auto& t : results) {
                    cout << "- " << t.title << " (分數: " << t.score << ")\n";
                }
                break;
            }
            case 6: {
                taskHeap.recalculateAll();
                taskBst.rebuild(taskHeap.getElements());
                cout << ">> 已依據目前系統時間重算所有分數\n";
                break;
            }
            default:
                cout << "無效的選擇。\n";
        }
    }

    return 0;
}
