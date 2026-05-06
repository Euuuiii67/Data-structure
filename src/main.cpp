/*
 * ============================================================
 *  智慧型待辦事項優先權排程器
 *  Smart Priority Task Scheduler
 *
 *  資料結構：
 *    - Max-Heap (Priority Queue)  : O(log n) insert / extract
 *    - Binary Search Tree (BST)   : O(log n) date-search
 *    - 時間衰減函數 (Time Decay)  : score × (1 + 6·e^{-0.2·days})
 *
 *  編譯：  g++ -std=c++17 -o scheduler scheduler.cpp
 *  執行：  ./scheduler
 * ============================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <memory>
#include <functional>
#include <limits>

// ─── ANSI 顏色碼 ────────────────────────────────────────────
namespace Color {
    const std::string RESET   = "\033[0m";
    const std::string BOLD    = "\033[1m";
    const std::string DIM     = "\033[2m";

    // 前景色
    const std::string RED     = "\033[91m";
    const std::string YELLOW  = "\033[93m";
    const std::string GREEN   = "\033[92m";
    const std::string BLUE    = "\033[94m";
    const std::string CYAN    = "\033[96m";
    const std::string MAGENTA = "\033[95m";
    const std::string WHITE   = "\033[97m";
    const std::string GRAY    = "\033[90m";

    // 背景色
    const std::string BG_DARK   = "\033[48;5;235m";
    const std::string BG_AMBER  = "\033[48;5;214m";
    const std::string BG_PANEL  = "\033[48;5;236m";

    // 前景 256色
    const std::string ORANGE  = "\033[38;5;214m";
    const std::string AMBER   = "\033[38;5;220m";
    const std::string PINK    = "\033[38;5;213m";
}

// ─── 工具函式 ────────────────────────────────────────────────
void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    std::cout << "\033[2J\033[H";
#endif
}

std::string repeat(const std::string& s, int n) {
    std::string r;
    for (int i = 0; i < n; ++i) r += s;
    return r;
}

// 盒形繪製字元（UTF-8）
namespace Box {
    const std::string TL = "┌", TR = "┐", BL = "└", BR = "┘";
    const std::string H  = "─", V  = "│";
    const std::string ML = "├", MR = "┤", MT = "┬", MB = "┴";
    const std::string CR = "┼";
    const std::string HL = "═", VL = "║";
    const std::string TL2= "╔", TR2= "╗", BL2= "╚", BR2= "╝";
    const std::string ML2= "╠", MR2= "╣";
}

void drawHLine(int width, const std::string& left, const std::string& right,
               const std::string& fill = Box::H, const std::string& color = Color::GRAY) {
    std::cout << color << left;
    for (int i = 0; i < width; ++i) std::cout << fill;
    std::cout << right << Color::RESET << "\n";
}

// 印出固定寬度字串（超過截斷，不足補空白）
std::string padRight(const std::string& s, int w) {
    if ((int)s.size() >= w) return s.substr(0, w);
    return s + std::string(w - s.size(), ' ');
}
std::string padLeft(const std::string& s, int w) {
    if ((int)s.size() >= w) return s.substr(0, w);
    return std::string(w - s.size(), ' ') + s;
}

// ─── 日期工具 ────────────────────────────────────────────────
struct Date {
    int year = 0, month = 0, day = 0;
    bool valid = false;

    static Date today() {
        time_t t = time(nullptr);
        tm* lt = localtime(&t);
        return {lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, true};
    }

    static Date parse(const std::string& s) {
        // 格式 YYYY-MM-DD
        if (s.size() != 10 || s[4] != '-' || s[7] != '-') return {};
        try {
            int y = std::stoi(s.substr(0, 4));
            int m = std::stoi(s.substr(5, 2));
            int d = std::stoi(s.substr(8, 2));
            if (m < 1 || m > 12 || d < 1 || d > 31) return {};
            return {y, m, d, true};
        } catch (...) { return {}; }
    }

    std::string toString() const {
        if (!valid) return "(無截止日)";
        std::ostringstream oss;
        oss << year << "-"
            << std::setw(2) << std::setfill('0') << month << "-"
            << std::setw(2) << std::setfill('0') << day;
        return oss.str();
    }

    std::string toKey() const {
        if (!valid) return "";
        std::ostringstream oss;
        oss << year
            << std::setw(2) << std::setfill('0') << month
            << std::setw(2) << std::setfill('0') << day;
        return oss.str();
    }

    // 距今幾天（負數＝已逾期）
    double daysFromNow() const {
        if (!valid) return 9999.0;
        Date td = today();
        // 簡易天數計算
        auto toJulian = [](int y, int m, int d) -> long {
            long a = (14 - m) / 12;
            long Y = y + 4800 - a;
            long M = m + 12 * a - 3;
            return d + (153*M+2)/5 + 365*Y + Y/4 - Y/100 + Y/400 - 32045;
        };
        long j1 = toJulian(year, month, day);
        long j2 = toJulian(td.year, td.month, td.day);
        return (double)(j1 - j2);
    }

    bool operator<(const Date& o) const { return toKey() < o.toKey(); }
    bool operator==(const Date& o) const { return toKey() == o.toKey(); }
    bool operator>(const Date& o) const { return toKey() > o.toKey(); }
};

// ─── Task 資料結構 ───────────────────────────────────────────
static int g_idCounter = 1;

struct Task {
    int    id;
    std::string title;
    int    urgency;       // 1–5
    int    importance;    // 1–5
    double estimatedTime; // 小時
    Date   deadline;
    double score;         // 動態優先權分數

    // 計算優先權分數（含時間衰減）
    static double calcScore(int u, int i, double t, const Date& dl) {
        double base = (u * 2.0 + i * 3.0) / std::log2(t + 2.0);
        if (!dl.valid) return base;
        double days = dl.daysFromNow();
        if (days <= 0) return base * 15.0;   // 逾期
        double d = 1.0 + 6.0 * std::exp(-0.2 * days);
        return base * d;
    }

    void recalc() {
        score = calcScore(urgency, importance, estimatedTime, deadline);
    }

    // 運算子多載：讓 Priority Queue 能比較大小
    bool operator<(const Task& o) const { return score < o.score; }
    bool operator>(const Task& o) const { return score > o.score; }
    bool operator==(const Task& o) const { return id == o.id; }

    std::string deadlineLabel() const {
        if (!deadline.valid) return "(無)";
        double d = deadline.daysFromNow();
        if (d < 0)  return "逾期 " + std::to_string((int)std::abs(std::ceil(d))) + "天";
        if (d < 1)  return "今天到期!";
        if (d < 2)  return "明天到期";
        return deadline.toString();
    }

    std::string urgLabel() const {
        std::string bars;
        for (int k = 1; k <= 5; ++k) bars += (k <= urgency ? "█" : "░");
        return bars;
    }
    std::string impLabel() const {
        std::string bars;
        for (int k = 1; k <= 5; ++k) bars += (k <= importance ? "█" : "░");
        return bars;
    }

    std::string scoreColor() const {
        if (score >= 15) return Color::RED;
        if (score >= 8)  return Color::ORANGE;
        if (score >= 4)  return Color::YELLOW;
        return Color::CYAN;
    }
};

// ─── Max-Heap (Priority Queue) ──────────────────────────────
class MaxHeap {
    std::vector<Task> heap;

    void siftUp(int i) {
        while (i > 0) {
            int p = (i - 1) / 2;
            if (heap[p] > heap[i] || heap[p] == heap[i]) break;
            std::swap(heap[p], heap[i]);
            i = p;
        }
    }

    void siftDown(int i) {
        int n = (int)heap.size();
        while (true) {
            int best = i, l = 2*i+1, r = 2*i+2;
            if (l < n && heap[l] > heap[best]) best = l;
            if (r < n && heap[r] > heap[best]) best = r;
            if (best == i) break;
            std::swap(heap[i], heap[best]);
            i = best;
        }
    }

public:
    // O(log n)
    void insert(const Task& t) {
        heap.push_back(t);
        siftUp((int)heap.size() - 1);
    }

    // O(1)
    const Task* peek() const {
        return heap.empty() ? nullptr : &heap[0];
    }

    // O(log n)
    Task extractMax() {
        Task top = heap[0];
        heap[0] = heap.back();
        heap.pop_back();
        if (!heap.empty()) siftDown(0);
        return top;
    }

    // 重新計算所有分數並重建堆積 O(n log n)
    void recalcAll() {
        for (auto& t : heap) t.recalc();
        std::vector<Task> tmp;
        tmp.swap(heap);
        for (auto& t : tmp) insert(t);
    }

    // 移除指定 id O(n)
    bool remove(int id) {
        auto it = std::find_if(heap.begin(), heap.end(),
                               [id](const Task& t){ return t.id == id; });
        if (it == heap.end()) return false;
        heap.erase(it);
        // 重建
        std::vector<Task> tmp;
        tmp.swap(heap);
        for (auto& t : tmp) insert(t);
        return true;
    }

    bool empty() const { return heap.empty(); }
    int  size()  const { return (int)heap.size(); }

    // 按分數排序的拷貝（供顯示用）
    std::vector<Task> sorted() const {
        std::vector<Task> v = heap;
        std::sort(v.begin(), v.end(), [](const Task& a, const Task& b){ return a > b; });
        return v;
    }

    double maxScore() const { return heap.empty() ? 0 : heap[0].score; }
    double avgScore() const {
        if (heap.empty()) return 0;
        double s = 0; for (auto& t : heap) s += t.score;
        return s / heap.size();
    }
    int overdueCount() const {
        int c = 0;
        for (auto& t : heap)
            if (t.deadline.valid && t.deadline.daysFromNow() < 0) ++c;
        return c;
    }
};

// ─── BST（以截止日期為鍵值）────────────────────────────────
struct BSTNode {
    std::string key;       // YYYYMMDD
    std::vector<int> ids;  // task ids
    std::unique_ptr<BSTNode> left, right;

    BSTNode(const std::string& k, int id)
        : key(k), ids{id} {}
};

class BST {
    std::unique_ptr<BSTNode> root;

    BSTNode* insertNode(std::unique_ptr<BSTNode>& node, const std::string& k, int id) {
        if (!node) { node = std::make_unique<BSTNode>(k, id); return node.get(); }
        if (k < node->key) return insertNode(node->left, k, id);
        if (k > node->key) return insertNode(node->right, k, id);
        node->ids.push_back(id);
        return node.get();
    }

    std::vector<int> searchNode(BSTNode* node, const std::string& k) const {
        if (!node) return {};
        if (k == node->key) return node->ids;
        if (k < node->key) return searchNode(node->left.get(), k);
        return searchNode(node->right.get(), k);
    }

    void inorder(BSTNode* node, std::vector<std::pair<std::string,int>>& out) const {
        if (!node) return;
        inorder(node->left.get(), out);
        for (int id : node->ids) out.push_back({node->key, id});
        inorder(node->right.get(), out);
    }

public:
    void insert(const std::string& key, int id) { insertNode(root, key, id); }

    std::vector<int> search(const std::string& key) const {
        return searchNode(root.get(), key);
    }

    std::vector<std::pair<std::string,int>> inorderAll() const {
        std::vector<std::pair<std::string,int>> out;
        inorder(root.get(), out);
        return out;
    }

    void rebuild(const std::vector<Task>& tasks) {
        root.reset();
        for (auto& t : tasks)
            if (t.deadline.valid) insert(t.deadline.toKey(), t.id);
    }
};

// ─── 主 UI 類別 ──────────────────────────────────────────────
class SchedulerUI {
    MaxHeap heap;
    BST     bst;
    std::vector<Task> allTasks; // 保留所有任務（含被取出的）
    std::string statusMsg;
    std::string statusColor = Color::GREEN;

    static const int W = 70; // 介面寬度

    // ── 標題列 ──
    void drawHeader() const {
        std::cout << "\n";
        std::cout << Color::BOLD << Color::AMBER;
        drawHLine(W, "╔", "╗", "═", Color::BOLD + Color::AMBER);
        std::cout << Color::BOLD + Color::AMBER << "║"
                  << Color::WHITE + Color::BOLD
                  << padRight("  智慧型待辦事項優先權排程器  "
                              "Smart Priority Scheduler", W)
                  << Color::AMBER << "║\n";
        std::cout << Color::AMBER;
        drawHLine(W, "╠", "╣", "═", Color::BOLD + Color::AMBER);
        std::cout << Color::RESET;

        // 演算法標籤列
        std::cout << Color::AMBER << "║" << Color::RESET;
        std::cout << Color::GRAY << "  MAX-HEAP · BST · O(log n)  "
                     "score = (U×2+I×3)/log₂(T+2) × decay";
        int gap = W - 62;
        std::cout << std::string(gap, ' ');
        std::cout << Color::AMBER << "║" << Color::RESET << "\n";
        drawHLine(W, "╚", "╝", "═", Color::BOLD + Color::AMBER);
        std::cout << "\n";
    }

    // ── 統計卡片 ──
    void drawStats() const {
        int n   = heap.size();
        double mx = heap.maxScore();
        double av = heap.avgScore();
        int od  = heap.overdueCount();

        auto card = [&](const std::string& label, const std::string& val,
                        const std::string& vc = Color::WHITE) {
            int cw = (W - 5) / 4;
            std::cout << Color::GRAY << "│" << Color::RESET
                      << " " << Color::GRAY << padRight(label, cw - 2) << Color::RESET
                      << "\n";
            std::cout << Color::GRAY << "│" << Color::RESET
                      << " " << vc << Color::BOLD << padRight(val, cw - 2)
                      << Color::RESET << "\n";
        };

        std::cout << Color::GRAY;
        drawHLine(W, "┌", "┐");

        // 四欄
        int cw = (W - 5) / 4;
        // 頂部標籤
        std::cout << Color::GRAY << "│" << Color::RESET;
        std::cout << " " << Color::GRAY << padRight("HEAP SIZE", cw-1) << Color::RESET;
        std::cout << Color::GRAY << "│" << Color::RESET;
        std::cout << " " << Color::GRAY << padRight("TOP SCORE", cw-1) << Color::RESET;
        std::cout << Color::GRAY << "│" << Color::RESET;
        std::cout << " " << Color::GRAY << padRight("AVG SCORE", cw-1) << Color::RESET;
        std::cout << Color::GRAY << "│" << Color::RESET;
        std::cout << " " << Color::GRAY << padRight("OVERDUE", cw) << Color::RESET;
        std::cout << Color::GRAY << "│\n" << Color::RESET;

        std::cout << Color::GRAY << "│" << Color::RESET;
        std::ostringstream oss1, oss2, oss3, oss4;
        oss1 << n;
        oss2 << std::fixed << std::setprecision(1) << mx;
        oss3 << std::fixed << std::setprecision(1) << av;
        oss4 << od;
        std::cout << " " << Color::BOLD + Color::CYAN
                  << padRight(n > 0 ? oss1.str() : "0", cw-1) << Color::RESET;
        std::cout << Color::GRAY << "│" << Color::RESET;
        std::cout << " " << Color::BOLD + Color::ORANGE
                  << padRight(n > 0 ? oss2.str() : "—", cw-1) << Color::RESET;
        std::cout << Color::GRAY << "│" << Color::RESET;
        std::cout << " " << Color::BOLD + Color::WHITE
                  << padRight(n > 0 ? oss3.str() : "—", cw-1) << Color::RESET;
        std::cout << Color::GRAY << "│" << Color::RESET;
        std::cout << " " << Color::BOLD + Color::RED
                  << padRight(oss4.str(), cw) << Color::RESET;
        std::cout << Color::GRAY << "│\n" << Color::RESET;

        drawHLine(W, "└", "┘");
        std::cout << "\n";
    }

    // ── 焦點卡（最高優先任務） ──
    void drawFocusCard() const {
        const Task* top = heap.peek();

        std::cout << Color::BOLD + Color::ORANGE;
        drawHLine(W, "╔", "╗", "═", Color::BOLD + Color::ORANGE);
        std::cout << Color::ORANGE << "║" << Color::RESET
                  << " " << Color::BOLD + Color::AMBER << "▶  NOW FOCUS ON"
                  << std::string(W - 17, ' ')
                  << Color::ORANGE << "║" << Color::RESET << "\n";
        drawHLine(W, "╠", "╣", "─", Color::ORANGE);

        if (!top) {
            std::cout << Color::ORANGE << "║" << Color::RESET
                      << "  " << Color::GRAY
                      << padRight("尚無任務 — 請選擇 [A] 新增一個！", W - 2)
                      << Color::ORANGE << "║" << Color::RESET << "\n";
        } else {
            // 任務標題
            std::cout << Color::ORANGE << "║" << Color::RESET
                      << "  " << Color::BOLD + Color::WHITE
                      << padRight(top->title, W - 4)
                      << Color::ORANGE << "  ║" << Color::RESET << "\n";

            // 分數與屬性
            std::ostringstream meta;
            meta << "Score: " << std::fixed << std::setprecision(2) << top->score
                 << "  │  "
                 << "U:" << top->urgency << " I:" << top->importance
                 << " T:" << top->estimatedTime << "h"
                 << "  │  " << top->deadlineLabel();
            std::cout << Color::ORANGE << "║" << Color::RESET
                      << "  " << top->scoreColor() << padRight(meta.str(), W - 3)
                      << Color::ORANGE << " ║" << Color::RESET << "\n";
        }

        drawHLine(W, "╚", "╝", "═", Color::BOLD + Color::ORANGE);
        std::cout << "\n";
    }

    // ── 任務佇列列表 ──
    void drawQueue() const {
        auto tasks = heap.sorted();
        std::cout << Color::GRAY;
        drawHLine(W, "┌", "┐");
        std::cout << "│" << Color::RESET
                  << " " << Color::BOLD << padRight("任務佇列 (MAX-HEAP 排序)", 40)
                  << Color::RESET
                  << Color::GRAY << padRight("", W - 42) << "│\n" << Color::RESET;
        drawHLine(W, "├", "┤");

        if (tasks.empty()) {
            std::cout << Color::GRAY << "│" << Color::RESET
                      << "  " << Color::GRAY << padRight("堆積為空", W - 3)
                      << Color::GRAY << "│" << Color::RESET << "\n";
        } else {
            double maxS = tasks[0].score;
            for (int i = 0; i < (int)tasks.size() && i < 8; ++i) {
                const Task& t = tasks[i];
                // 進度條（基於最高分）
                int barMax = W - 46;
                int barFill = (maxS > 0) ? (int)((t.score / maxS) * barMax) : 0;
                std::string bar = std::string(barFill, '█') +
                                  std::string(barMax - barFill, '░');
                std::string rankMark = (i==0) ? Color::AMBER + "▶ " :
                                       (i==1) ? Color::GRAY  + "② " :
                                       (i==2) ? Color::GRAY  + "③ "
                                              : Color::GRAY  + "   ";
                std::cout << Color::GRAY << "│" << Color::RESET
                          << " " << rankMark
                          << Color::RESET + Color::WHITE
                          << padRight(t.title, 24) << Color::RESET
                          << " " << t.scoreColor() << Color::BOLD
                          << padLeft(([&]{
                                std::ostringstream o;
                                o << std::fixed << std::setprecision(1) << t.score;
                                return o.str();
                             }()), 5) << Color::RESET
                          << " " << Color::GRAY + Color::DIM
                          << bar << Color::RESET
                          << " " << Color::GRAY << "│" << Color::RESET << "\n";
            }
            if ((int)tasks.size() > 8) {
                std::cout << Color::GRAY << "│" << Color::RESET
                          << "  " << Color::GRAY
                          << padRight("  … 另有 " +
                                     std::to_string(tasks.size()-8) + " 筆任務", W - 3)
                          << Color::GRAY << "│" << Color::RESET << "\n";
            }
        }
        drawHLine(W, "└", "┘");
        std::cout << "\n";
    }

    // ── 公式說明 ──
    void drawFormula() const {
        std::cout << Color::GRAY;
        drawHLine(W, "┌", "┬", "─", Color::GRAY);
        // 三欄說明
        std::cout << "│" << Color::RESET
                  << Color::GRAY << " 優先權公式        " << Color::RESET
                  << Color::GRAY << "│" << Color::RESET
                  << Color::GRAY << " 時間衰減效果      " << Color::RESET
                  << Color::GRAY << "│" << Color::RESET
                  << Color::GRAY << " 演算法複雜度    " << Color::RESET
                  << Color::GRAY << "│\n" << Color::RESET;
        drawHLine(W, "├", "┤", "─", Color::GRAY);

        auto line3 = [&](const std::string& a, const std::string& b, const std::string& c) {
            std::cout << Color::GRAY << "│" << Color::RESET
                      << " " << Color::DIM << padRight(a, 19) << Color::RESET
                      << Color::GRAY << "│" << Color::RESET
                      << " " << Color::DIM << padRight(b, 19) << Color::RESET
                      << Color::GRAY << "│" << Color::RESET
                      << " " << Color::DIM << padRight(c, 18) << Color::RESET
                      << Color::GRAY << "│\n" << Color::RESET;
        };

        line3("base = (U×2+I×3)", "逾期: ×15.0", "insert: O(log n)");
        line3("  ÷ log₂(T+2)",    "1天後: ×7.0 ", "peek:   O(1)    ");
        line3("d=1+6·e^{-0.2·t}", "3天後: ×3.9 ", "extract:O(log n)");
        line3("score = base × d", "7天後: ×2.5 ", "search: O(log n)");

        drawHLine(W, "└", "┘", "─", Color::GRAY);
        std::cout << "\n";
    }

    // ── 選單 ──
    void drawMenu() const {
        if (!statusMsg.empty()) {
            std::cout << statusColor << " " << statusMsg << Color::RESET << "\n\n";
        }
        std::cout << Color::BOLD + Color::WHITE
                  << " [A] 新增任務"
                  << "  [C] 完成最高優先任務"
                  << "  [S] 暫緩"
                  << "  [D] 刪除任務"
                  << "  [F] BST 日期搜尋"
                  << "\n"
                  << " [R] 重算分數"
                  << "  [L] 全部任務(BST依日期)"
                  << "  [Q] 離開"
                  << Color::RESET << "\n\n"
                  << Color::GRAY << " 請選擇：" << Color::RESET;
    }

    // ── 新增任務表單 ──
    void formAddTask() {
        clearScreen();
        drawHeader();
        std::cout << Color::BOLD << " ── 新增任務 ──\n\n" << Color::RESET;

        auto prompt = [&](const std::string& label, const std::string& hint = "") -> std::string {
            std::cout << Color::CYAN << " " << label << Color::RESET;
            if (!hint.empty()) std::cout << Color::GRAY << " (" << hint << ")" << Color::RESET;
            std::cout << ": ";
            std::string s;
            std::getline(std::cin, s);
            return s;
        };

        std::string title = prompt("任務名稱");
        if (title.empty()) { setStatus("已取消", Color::GRAY); return; }

        // 預計時間
        double estTime = 1.0;
        while (true) {
            std::string s = prompt("預計花費時間", "小時，如 1.5");
            if (s.empty()) break;
            try { estTime = std::stod(s); if (estTime > 0) break; } catch (...) {}
            std::cout << Color::RED << " 請輸入正數\n" << Color::RESET;
        }

        // 緊急程度
        int urg = 3;
        while (true) {
            std::string s = prompt("緊急程度", "1–5");
            if (s.empty()) break;
            try { urg = std::stoi(s); if (urg >= 1 && urg <= 5) break; } catch (...) {}
            std::cout << Color::RED << " 請輸入 1–5\n" << Color::RESET;
        }

        // 重要程度
        int imp = 3;
        while (true) {
            std::string s = prompt("重要程度", "1–5");
            if (s.empty()) break;
            try { imp = std::stoi(s); if (imp >= 1 && imp <= 5) break; } catch (...) {}
            std::cout << Color::RED << " 請輸入 1–5\n" << Color::RESET;
        }

        // 截止日期
        Date dl;
        while (true) {
            std::string s = prompt("截止日期", "YYYY-MM-DD，直接 Enter 略過");
            if (s.empty()) break;
            dl = Date::parse(s);
            if (dl.valid) break;
            std::cout << Color::RED << " 格式錯誤，請使用 YYYY-MM-DD\n" << Color::RESET;
        }

        double sc = Task::calcScore(urg, imp, estTime, dl);
        std::cout << "\n" << Color::BOLD << Color::AMBER
                  << "  預覽優先權分數: " << std::fixed << std::setprecision(2) << sc
                  << Color::RESET << "\n";

        std::cout << Color::GRAY << "  確認新增? (Y/n): " << Color::RESET;
        std::string conf; std::getline(std::cin, conf);
        if (!conf.empty() && (conf[0] == 'n' || conf[0] == 'N')) {
            setStatus("已取消", Color::GRAY); return;
        }

        Task t;
        t.id = g_idCounter++;
        t.title = title;
        t.urgency = urg;
        t.importance = imp;
        t.estimatedTime = estTime;
        t.deadline = dl;
        t.score = sc;

        heap.insert(t);
        allTasks.push_back(t);
        if (dl.valid) bst.insert(dl.toKey(), t.id);

        setStatus("✓ 任務已加入 Max-Heap（O log n）", Color::GREEN);
    }

    // ── BST 搜尋 ──
    void formBSTSearch() {
        clearScreen();
        drawHeader();
        std::cout << Color::BOLD << " ── BST 截止日期搜尋 (O log n) ──\n\n" << Color::RESET;
        std::cout << Color::GRAY << " 輸入截止日期 (YYYY-MM-DD): " << Color::RESET;
        std::string s; std::getline(std::cin, s);
        Date dl = Date::parse(s);
        if (!dl.valid) { setStatus("日期格式錯誤", Color::RED); return; }

        auto ids = bst.search(dl.toKey());
        auto tasks = heap.sorted();
        std::cout << "\n" << Color::BOLD + Color::CYAN
                  << " BST 搜尋結果：" << dl.toString()
                  << " (" << ids.size() << " 筆)\n" << Color::RESET;
        drawHLine(W - 2, "┌", "┐");
        for (int id : ids) {
            auto it = std::find_if(tasks.begin(), tasks.end(),
                                   [id](const Task& t){ return t.id == id; });
            if (it != tasks.end()) {
                std::cout << "│ " << Color::WHITE << padRight(it->title, 35) << Color::RESET
                          << " Score: " << it->scoreColor() + Color::BOLD
                          << std::fixed << std::setprecision(2) << it->score
                          << Color::RESET << "     │\n";
            }
        }
        if (ids.empty())
            std::cout << "│ " << Color::GRAY << padRight("無任務", W - 4) << "│\n" << Color::RESET;
        drawHLine(W - 2, "└", "┘");

        std::cout << "\n" << Color::GRAY << " 按 Enter 返回..." << Color::RESET;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    // ── 顯示全部任務（BST 中序 by 日期） ──
    void showAllByDate() {
        clearScreen();
        drawHeader();
        std::cout << Color::BOLD << " ── BST 中序走訪 (依截止日期排序) ──\n\n" << Color::RESET;
        auto entries = bst.inorderAll();
        auto heapTasks = heap.sorted();

        if (entries.empty()) {
            std::cout << Color::GRAY << " 無含截止日期的任務\n" << Color::RESET;
        } else {
            drawHLine(W - 2, "┌", "┐");
            for (auto& [key, id] : entries) {
                auto it = std::find_if(heapTasks.begin(), heapTasks.end(),
                                       [id](const Task& t){ return t.id == id; });
                if (it == heapTasks.end()) continue;
                std::cout << "│ " << Color::GRAY << it->deadline.toString() << "  "
                          << Color::RESET + Color::WHITE << padRight(it->title, 28)
                          << "  " << it->scoreColor() + Color::BOLD
                          << std::fixed << std::setprecision(2) << it->score
                          << Color::RESET << "     │\n";
            }
            drawHLine(W - 2, "└", "┘");
        }

        // 無截止日任務
        std::cout << "\n" << Color::BOLD << " 無截止日期任務：\n" << Color::RESET;
        for (auto& t : heapTasks) {
            if (!t.deadline.valid)
                std::cout << "  " << Color::GRAY << "─  " << Color::RESET
                          << Color::WHITE << t.title << Color::RESET
                          << "  " << t.scoreColor()
                          << std::fixed << std::setprecision(2) << t.score
                          << Color::RESET << "\n";
        }

        std::cout << "\n" << Color::GRAY << " 按 Enter 返回..." << Color::RESET;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    // ── 刪除任務 ──
    void formDelete() {
        clearScreen();
        drawHeader();
        drawQueue();
        std::cout << Color::GRAY << " 輸入要刪除的任務 ID（可從上方列表確認）: " << Color::RESET;
        std::string s; std::getline(std::cin, s);
        if (s.empty()) { setStatus("已取消", Color::GRAY); return; }
        int id;
        try { id = std::stoi(s); } catch (...) { setStatus("無效 ID", Color::RED); return; }
        if (heap.remove(id)) {
            bst.rebuild(heap.sorted());
            setStatus("✓ 任務 #" + std::to_string(id) + " 已刪除", Color::GREEN);
        } else {
            setStatus("找不到 ID #" + std::to_string(id), Color::RED);
        }
    }

    void setStatus(const std::string& msg, const std::string& col = Color::GREEN) {
        statusMsg = msg; statusColor = col;
    }

    void addSampleTasks() {
        Date today = Date::today();
        auto makeDate = [&](int addDays) {
            // 簡易加天數
            time_t t = time(nullptr) + (long long)addDays * 86400;
            tm* lt = localtime(&t);
            return Date{lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday, true};
        };

        struct Sample { std::string title; int u,i; double t; int days; };
        std::vector<Sample> samples = {
            {"準備期末報告簡報", 5, 5, 3.0, 1},
            {"修復登入頁面 bug", 4, 4, 1.0, 2},
            {"回覆導師 email",   3, 3, 0.5, -1},
            {"閱讀演算法教材",   2, 4, 2.0,  7},
        };
        for (auto& s : samples) {
            Task t;
            t.id = g_idCounter++;
            t.title = s.title;
            t.urgency = s.u; t.importance = s.i; t.estimatedTime = s.t;
            t.deadline = makeDate(s.days);
            t.score = Task::calcScore(s.u, s.i, s.t, t.deadline);
            heap.insert(t);
            allTasks.push_back(t);
            if (t.deadline.valid) bst.insert(t.deadline.toKey(), t.id);
        }
    }

public:
    SchedulerUI() { addSampleTasks(); }

    void run() {
        while (true) {
            clearScreen();
            drawHeader();
            drawStats();
            drawFocusCard();
            drawQueue();
            drawFormula();
            drawMenu();

            statusMsg.clear();

            std::string choice;
            std::getline(std::cin, choice);
            if (choice.empty()) continue;
            char c = std::toupper(choice[0]);

            if (c == 'Q') {
                clearScreen();
                std::cout << Color::BOLD + Color::AMBER
                          << "\n  感謝使用智慧排程器！\n\n" << Color::RESET;
                break;
            } else if (c == 'A') {
                formAddTask();
            } else if (c == 'C') {
                if (heap.empty()) { setStatus("堆積為空！", Color::RED); }
                else {
                    Task top = heap.extractMax();
                    bst.rebuild(heap.sorted());
                    setStatus("✓ 完成：「" + top.title + "」已從 Heap 移除", Color::GREEN);
                }
            } else if (c == 'S') {
                const Task* top = heap.peek();
                if (!top) { setStatus("堆積為空！", Color::RED); }
                else {
                    int id = top->id;
                    auto tasks = heap.sorted();
                    heap.remove(id);
                    for (auto& t : tasks) {
                        if (t.id == id) {
                            Task mod = t;
                            mod.score *= 0.3;
                            heap.insert(mod);
                            break;
                        }
                    }
                    setStatus("已暫緩，分數降低至 30%", Color::YELLOW);
                }
            } else if (c == 'D') {
                formDelete();
            } else if (c == 'R') {
                heap.recalcAll();
                bst.rebuild(heap.sorted());
                setStatus("✓ 所有任務分數已重算（含時間衰減）", Color::CYAN);
            } else if (c == 'F') {
                formBSTSearch();
            } else if (c == 'L') {
                showAllByDate();
            } else {
                setStatus("未知指令：" + choice, Color::GRAY);
            }
        }
    }
};

// ─── 主程式入口 ──────────────────────────────────────────────
int main() {
    // 設定 UTF-8 輸出（Windows 需要）
#ifdef _WIN32
    system("chcp 65001 >nul 2>&1");
#endif

    SchedulerUI ui;
    ui.run();
    return 0;
}
