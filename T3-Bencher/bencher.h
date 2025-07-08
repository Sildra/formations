#include <chrono>
#include <map>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <string>

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
        ExecutorState(Additionals&) { };
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
    // RESULT_FORMATER
    struct ResultNode {
        std::string row;
        std::string col;
        Result result;
    };
    struct Formatter {
        struct Options {
            bool sort_cols { false };
            bool sort_rows { false };
            bool display_markdown_separator { true };
        };
    private:
        static std::pair<std::vector<ResultNode>, std::map<std::string, size_t>> rearrange(const std::vector<ResultNode>& results, const Options& options) {
            std::vector<ResultNode> rearranged;
            std::map<std::string, size_t> col_indexer;
            std::map<std::string, size_t> row_indexer;

            for (const auto& result : results) {
                col_indexer.emplace(result.col, col_indexer.size());
                row_indexer.emplace(result.row, row_indexer.size());
            }
            if (options.sort_cols) {
                size_t i = 0;
                for (auto& col_index : col_indexer)
                    col_index.second = i++;
            }
            if (options.sort_rows) {
                size_t i = 0;
                for (auto& row_index : row_indexer)
                    row_index.second = i++;
            }

            rearranged.resize(col_indexer.size() * row_indexer.size());
            for (const auto& result : results) {
                rearranged[row_indexer[result.row] * col_indexer.size() + col_indexer[result.col]] = result;
            }
            return std::make_pair(std::move(rearranged), std::move(col_indexer));
        }
    public:
        static void display(const std::vector<ResultNode>& results, const Options& options) {
            if (results.empty())
                return;
            auto [sorted_results, cols_index]  = rearrange(results, options);
            struct ResultPosition { size_t position = 0; size_t size = 0; };
            std::vector<ResultPosition> cols(cols_index.size());
            for (const auto& col : cols_index) {
                cols[col.second].size = col.first.size();
            }
            size_t row_header_size = 3;
            for (auto& result : sorted_results) {
                if (result.result.execution_count == 0) continue;

                auto& col = cols[cols_index[result.col]];
                col.size = std::max(col.size, (result.result.duration ? 3 + (size_t)log10(result.result.duration) : 3));
                row_header_size = std::max(row_header_size, result.row.size());
            }
            size_t col_size = row_header_size + 3;
            for (auto& col : cols) {
                col.position = col_size;
                col_size += col.size + 3;
            }
            auto last = std::prev(cols.end());
            std::string base_row = std::string(last->position + last->size + 2, ' ');
            { // Fill the base row with pipe separators
                for (auto& col : cols)
                    base_row[col.position] = '|';
                base_row[0] = '|';
                base_row.append(" |\n");
            }
            { // Display base table
                auto header_row = base_row;
                for (auto& col : cols_index)
                    header_row.replace(cols[col.second].position + 2, col.first.size(), col.first);
                std::cout << header_row;
                if (options.display_markdown_separator) {
                    header_row.replace(2, row_header_size, row_header_size , '-');
                    for (auto& col : cols)
                        header_row.replace(col.position + 2, col.size, col.size , '-');
                    std::cout << header_row;
                }
            }

            auto current_row = base_row;
            std::string last_index = sorted_results.begin()->row;
            current_row.replace(2 , last_index.size(), last_index);
            for (const auto& result : sorted_results) {
                if (result.result.execution_count == 0) continue;
                if (last_index != result.row) {
                    std::cout << current_row;
                    current_row = base_row;
                    last_index = result.row;
                    current_row.replace(2 , last_index.size(), last_index);
                }
                auto val = std::to_string(result.result.duration).append("ms");
                const auto& offset = cols[cols_index[result.col]];
                current_row.replace(offset.position + offset.size + 2 - val.size(), val.size(), val);
                if (result.result.extrapolated)
                    current_row[offset.position + offset.size + 2] = '~';
            }
            std::cout << current_row;
        }
        static void display(const std::vector<ResultNode>& results)
        { display(results, {}); }

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
        std::vector<ResultNode> results;
        typename StatePolicy::Additionals additionals;
    public:
        template<typename T>
        void bench(T function) {
            StatePolicy state { additionals };
            function(state);
            results.push_back(ResultNode { "", "", state.result });
        }
        template<typename T>
        void bench(const std::string& row, const std::string& col, T function) {
            StatePolicy state { additionals };
            function(state);
            results.push_back(ResultNode { row, col, state.result });
        }
        const std::vector<ResultNode>& get_results() { return results; }
        void clear() { results.clear(); }
    };
}