#include <chrono>
#include <map>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace bencher {
    template<typename State>
    struct Iterator {
        Iterator(State& state) : state(state) {}
        ~Iterator() { state.stop(); }
        const State& operator*() const {
            return state;
        }
        Iterator& operator++() {
            state.next();
            return *this;
        }
        bool operator!=(const Iterator<State>& other) {
            return !state.ended();
        }
    private:
        State& state;
    };

    template<int Count = 1'000'000>
    struct ExecutorState {
        using chrono = std::chrono::high_resolution_clock;
        Iterator<ExecutorState> begin() {
            execution_count = 0;
            start_time = chrono::now();
            return *this;
        }
        Iterator<ExecutorState> end() { return *this; }
        void next() {
            ++execution_count;
        }
        bool ended() { return execution_count == Count; }
        void stop() { end_time = chrono::now(); }
        size_t execution_count = 0;
        decltype(chrono::now()) start_time;
        decltype(chrono::now()) end_time;
    };

    template<typename StatePolicy = ExecutorState<>>
    struct Bencher {
        struct Result {
            Result() = default; 
            Result(const StatePolicy& state)
                : duration(std::chrono::duration_cast<std::chrono::milliseconds>(state.end_time - state.start_time).count())
                , execution_count(state.execution_count)
                { }
            size_t duration = 0;
            size_t execution_count = 0;
        };
        struct TableIndex {
            std::string row;
            std::string col;
            bool operator<(const TableIndex& other) const {
                return std::tie(row, col) < std::tie(other.row, other.col);
            }
        };
        template<typename T>
        void bench(T function) {
            StatePolicy state;
            function(state);
            results[TableIndex { "", "" }] = Result(state);
        }
        template<typename T>
        void bench(const std::string& row, const std::string& col, T function) {
            StatePolicy state;
            function(state);
            results[TableIndex { row, col }] = Result(state);
        }
        void clear() { results.clear(); }
        static size_t digits(size_t i) { return !i? 1 : 1 + log10(i); }

        void display() {
            struct ResultPosition { size_t position = 0; size_t size = 0; };
            std::map<std::string, ResultPosition> cols;
            size_t row_header_size = 0;
            for (auto& result : results) {
                auto col = cols.insert(std::make_pair(result.first.col, ResultPosition { 0, result.first.col.size() } ));
                col.first->second.size = std::max(col.first->second.size, digits(result.second.duration) + 2);
                row_header_size = std::max(row_header_size, result.first.row.size());
            }
            size_t col_size = row_header_size + 2;
            for (auto& col : cols) {
                col.second.position = col_size;
                col_size += col.second.size + 3;
            }
            auto last = std::prev(cols.end());
            std::string base_row = std::string(last->second.position + last->second.size + 2, ' ');
            for (auto& col : cols)
                base_row[col.second.position] = '|';
            base_row.append(" |\n");
            auto header_row = base_row;
            for (auto& col : cols)
                header_row.replace(col.second.position + 2, col.first.size(), col.first);
            std::cout << header_row;
            auto current_row = base_row;
            std::string last_index = results.begin()->first.row;
            current_row.replace(0 , last_index.size(), last_index);
            for (const auto& result : results) {
                if (last_index != result.first.row) {
                    std::cout << current_row;
                    current_row = base_row;
                    last_index = result.first.row;
                    current_row.replace(0 , last_index.size(), last_index);
                }
                auto val = std::to_string(result.second.duration).append("ms");
                current_row.replace(cols[result.first.col].position + 2, val.size(), val);
            }
            std::cout << current_row;
        }
        std::map<TableIndex, Result> results;
    };
}