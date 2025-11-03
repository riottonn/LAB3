#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <functional>
class SafeStruct {
private:
    int fields[2];
    mutable std::mutex field_mutex[2];
    mutable std::mutex all_mutex;
public:
    SafeStruct() {
        fields[0] = fields[1] = 0;
    }
    int get(int i) const {
        if (i < 0 || i >= 2) return 0;
        std::lock_guard<std::mutex> lg(field_mutex[i]);
        return fields[i];
    }
    void set(int i, int val) {
        if (i < 0 || i >= 2) return;
        std::lock_guard<std::mutex> lg(field_mutex[i]);
        fields[i] = val;
    }
    operator std::string() const {
        std::lock_guard<std::mutex> guard_all(all_mutex);
        std::lock_guard<std::mutex> lg0(field_mutex[0]);
        std::lock_guard<std::mutex> lg1(field_mutex[1]);
        std::ostringstream ss;
        ss << "{" << fields[0] << ", " << fields[1] << "}";
        return ss.str();
    }
};
struct Instr {
    enum Type { READ, WRITE, STRING } type;
    int idx;
    int value;
};
bool load_instructions(const std::string &filename, std::vector<Instr> &out) {
    std::ifstream fin(filename);
    if (!fin.is_open()) return false;
    out.clear();
    std::string op;
    while (fin >> op) {
        if (op == "write") {
            int idx, val;
            if (!(fin >> idx >> val)) return false;
            out.push_back({Instr::WRITE, idx, val});
        } else if (op == "read") {
            int idx;
            if (!(fin >> idx)) return false;
            out.push_back({Instr::READ, idx, 0});
        } else if (op == "string") {
            out.push_back({Instr::STRING, 0, 0});
        } else {
            return false;
        }
    }
    return true;
}
void execute_instructions(const std::vector<Instr> &instrs, SafeStruct &data) {
    for (const auto &ins : instrs) {
        switch (ins.type) {
            case Instr::READ:
                data.get(ins.idx);
                break;
            case Instr::WRITE:
                data.set(ins.idx, ins.value);
                break;
            case Instr::STRING: ;
                (void)static_cast<std::string>(data);
                break;
            }
        }
    }
}

int main(int argc, char** argv) {
    std::string f1 = "actions1.txt";
    std::string f2 = "actions2.txt";
    std::string f3 = "actions3.txt";
    if (argc >= 4) {
        f1 = argv[1]; f2 = argv[2]; f3 = argv[3];
    } else {
    }

    std::vector<Instr> instr1, instr2, instr3;
    if (!load_instructions(f1, instr1)) {
        std::cerr << "Не вдалося відкрити або прочитати " << f1 << "\n";
        return 1;
    }
    if (!load_instructions(f2, instr2)) {
        std::cerr << "Не вдалося відкрити або прочитати " << f2 << "\n";
        return 1;
    }
    if (!load_instructions(f3, instr3)) {
        std::cerr << "Не вдалося відкрити або прочитати " << f3 << "\n";
        return 1;
    }
    for (int threads = 1; threads <= 3; ++threads) {
        SafeStruct data; 
        std::vector<std::vector<Instr>> per_thread_instr;
        per_thread_instr.emplace_back(instr1);
        if (threads >= 2) per_thread_instr.emplace_back(instr2);
        if (threads >= 3) per_thread_instr.emplace_back(instr3);
        auto t_start = std::chrono::high_resolution_clock::now();
        std::vector<std::thread> workers;
        for (int i = 0; i < threads; ++i) {
            workers.emplace_back(execute_instructions, std::cref(per_thread_instr[i]), std::ref(data));
        }
        for (auto &th : workers) th.join();
        auto t_end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
        std::cout << "Threads: " << threads << "  Time (ms): " << dur << '\n';
    }
    return 0;
}
