#include <chrono>
#include <map>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <thread>

namespace bencher {
    // BENCH-TIMER
    struct Timer {
        using chrono = std::chrono::system_clock;
        chrono::time_point start = chrono::now();
        std::thread timer_thread { &Timer::execute_thread, this };
        bool end_request = false;
        bool destruction_request = false;
        size_t frequency;

        Timer(size_t frequency) : frequency(frequency) {}
        ~Timer() {
            destruction_request = true;
            timer_thread.join();
        }

        void execute_thread() {
            while (!destruction_request) {
                auto end = chrono::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                if (frequency - duration < 5 || duration > frequency) {
                    end_request = true;
                    std::this_thread::sleep_for(std::chrono::milliseconds(frequency));
                }
                else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(std::min(frequency, frequency - duration)));
                }
            }
        }
        void set_time() { start = chrono::now(); end_request = false; }
    };
    // BENCH-ITERATOR
    template<typename State>
    struct Iterator {
        Iterator(State& state) : state(state) {}
        ~Iterator() { state.stop(); }
        const State& operator*() const { return state; }
        Iterator& operator++() { state.next(); return *this; }
        bool operator!=(const Iterator<State>& other) { return !state.ended(); }
    private:
        State& state;
    };

    struct Result {
        size_t duration = 0;
        size_t execution_count = 0;
        bool extrapolated = false;
    };
    // BENCH-FIXED_COUNT_STATE
    template<int Count = 1'000'000>
    struct ExecutorState {
        using chrono = std::chrono::high_resolution_clock;
        struct Additionals { };
        ExecutorState(Additionals&) = default;
        Iterator<ExecutorState> begin() {
            result.execution_count = 0;
            start_time = chrono::now();
            return *this;
        }
        Iterator<ExecutorState> end() { return *this; }
        void next() { ++result.execution_count; }
        bool ended() { return result.execution_count == Count; }
        void stop() {
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(chrono::now() - start_time).count();
        }
        chrono::time_point start_time;
        Result result;
    };
    // BENCH-FIXED_COUNT_TIMED_STATE
    template<int Count = 1'000'000, int Time = 1'000>
    struct TimedExecutorState {
        using chrono = std::chrono::high_resolution_clock;
        struct Additionals { Timer timer { Time }; };
        TimedExecutorState(Additionals& additionals) : additionals(additionals) {
            additionals.timer.set_time();
        }
        Iterator<TimedExecutorState> begin() {
            result = Result();
            start_time = chrono::now();
            return *this;
        }
        Iterator<TimedExecutorState> end() { return *this; }
        void next() { ++result.execution_count; }
        bool ended() { return additionals.timer.end_request || result.execution_count == Count; }
        void stop() {
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(chrono::now() - start_time).count();
            if (result.execution_count != Count && result.execution_count != 0) {
                result.duration = (result.duration * Count) / result.execution_count;
                result.extrapolated = true;
            }
        }
        Additionals& additionals;
        chrono::time_point start_time;
        Result result;
    };

    // BENCHER
    template<typename StatePolicy = ExecutorState<>>
    struct Bencher {
    private:
        struct TableIndex {
            std::string row;
            std::string col;
            bool operator<(const TableIndex& other) const { return std::tie(row, col) < std::tie(other.row, other.col); }
        };
        std::map<TableIndex, Result> results;
        typename StatePolicy::Additionals additionals;
    public:
        template<typename T>
        void bench(T function) {
            StatePolicy state { additionals };
            function(state);
            results[TableIndex { "", "" }] = state.result;
        }
        template<typename T>
        void bench(const std::string& row, const std::string& col, T function) {
            StatePolicy state { additionals };
            function(state);
            results[TableIndex { row, col }] = state.result;
        }
        void clear() { results.clear(); }

        // BENCHER-DISPLAY
        void display(bool display_markdown_separator = true) {
            struct ResultPosition { size_t position = 0; size_t size = 0; };
            std::map<std::string, ResultPosition> cols;
            size_t row_header_size = 3;
            for (auto& result : results) {
                auto col = cols.insert(std::make_pair(result.first.col, ResultPosition { 0, result.first.col.size() } ));
                col.first->second.size = std::max(col.first->second.size, (result.second.duration ? 3 + (size_t)log10(result.second.duration) : 3));
                row_header_size = std::max(row_header_size, result.first.row.size());
            }
            size_t col_size = row_header_size + 3;
            for (auto& col : cols) {
                col.second.position = col_size;
                col_size += col.second.size + 3;
            }
            auto last = std::prev(cols.end());
            std::string base_row = std::string(last->second.position + last->second.size + 2, ' ');
            { // Fill the base row with pipe separators
                for (auto& col : cols)
                    base_row[col.second.position] = '|';
                base_row[0] = '|';
                base_row.append(" |\n");
            }
            { // Display base table
                auto header_row = base_row;
                for (auto& col : cols)
                    header_row.replace(col.second.position + 2, col.first.size(), col.first);
                std::cout << header_row;
                if (display_markdown_separator) {
                    header_row.replace(2, row_header_size, row_header_size , '-');
                    for (auto& col : cols)
                        header_row.replace(col.second.position + 2, col.second.size, col.second.size , '-');
                    std::cout << header_row;
                }
            }

            auto current_row = base_row;
            std::string last_index = results.begin()->first.row;
            current_row.replace(2 , last_index.size(), last_index);
            for (const auto& result : results) {
                if (last_index != result.first.row) {
                    std::cout << current_row;
                    current_row = base_row;
                    last_index = result.first.row;
                    current_row.replace(2 , last_index.size(), last_index);
                }
                auto val = std::to_string(result.second.duration).append("ms");
                const auto& offset = cols[result.first.col];
                current_row.replace(offset.position + offset.size + 2 - val.size(), val.size(), val);
                if (result.second.extrapolated)
                    current_row[offset.position + offset.size + 2] = '~';
            }
            std::cout << current_row;
        }
    };
}