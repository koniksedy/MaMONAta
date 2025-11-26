#ifndef TIMER_HH_
#define TIMER_HH_

#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>


// Enable timing only if TIMING_ENABLED is defined
#ifdef TIMING_ENABLED
// Measure execution time of a code line.
// Distinguish different timing sessions by function name.
#define TIME(code_line) \
    Timer::start(__func__); \
    code_line; \
    Timer::stop(__func__);
#else
// No-op macro when timing is disabled
#define TIME(code_line) code_line;
#endif

/**
 * @brief A simple timer utility for measuring code execution time.
 *
 * This class provides static methods to start and stop timing sessions
 * identified by string labels. It records durations in microseconds.
 */
class Timer {
public:
    using microseconds = std::chrono::microseconds::rep;
private:
    Timer() = default;
    Timer(const Timer&) = delete; // prevent copy
    Timer& operator=(const Timer&) = delete;  // prevent assignment

    using clock_t = std::chrono::high_resolution_clock;

    std::unordered_map<std::string, clock_t::time_point> start_times{};
    std::unordered_map<std::string, microseconds> durations{};

    // Get the singleton instance
    static Timer& instance() {
        static Timer instance;
        return instance;
    }

public:
    ~Timer() = default;

    /**
     * Start timing for a given label.
     *
     * @param label Identifier for the timing session.
     */
    static void start(const std::string& label) {
        instance().durations.erase(label); // clear previous duration
        instance().start_times[label] = std::chrono::high_resolution_clock::now();
    }

    /**
     * Stop timing for a given label and record the duration.
     *
     * @param label Identifier for the timing session.
     * @return Duration in microseconds.
     */
    static microseconds stop(const std::string& label) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto start_it = instance().start_times.find(label);
        if (start_it == instance().start_times.end()) {
            throw std::runtime_error("Timer for label '" + label + "' was not started.");
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_it->second).count();
        instance().durations[label] = duration;
        instance().start_times.erase(start_it);
        return duration;
    }

    /**
     * Get the recorded duration for a given label.
     *
     * @param label Identifier for the timing session.
     * @return Duration in microseconds.
     */
    static microseconds get(const std::string& label) {
        auto it = instance().durations.find(label);
        if (it == instance().durations.end()) {
            throw std::runtime_error("No recorded duration for label '" + label + "'.");
        }
        return it->second;
    }
};

#endif // TIMER_HH_
