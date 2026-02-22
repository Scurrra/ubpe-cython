#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ubpe {
class Logger;

/// @brief Configuration for progress bar.
struct ProgressConfig {
    std::string unit = "item";
    uint8_t precision = 3;
};

/// @brief Progress bar for tracking progress of a task.
class Progress {
   private:
    Logger* logger = nullptr;
    std::string unit;
    uint8_t precision;

    bool is_running = false;
    bool is_active = false;
    std::optional<uint64_t> total = std::nullopt;
    std::optional<uint64_t> initial = std::nullopt;
    std::optional<uint64_t> current = std::nullopt;
    std::optional<std::chrono::time_point<std::chrono::steady_clock>>
        initial_time = std::nullopt;
    std::optional<std::chrono::time_point<std::chrono::steady_clock>>
        current_time = std::nullopt;
    std::optional<float> rate = std::nullopt;
    std::optional<size_t> old_length = std::nullopt;

    void reset() {
        if (this->is_running) {
            throw std::logic_error("Progress is running");
        }

        this->total = std::nullopt;
        this->initial = std::nullopt;
        this->current = std::nullopt;
        this->initial_time = std::nullopt;
        this->current_time = std::nullopt;
        this->rate = std::nullopt;
        this->old_length = std::nullopt;
        this->is_active = false;
    }

   public:
    friend class Logger;

    /// @brief Initialize the progress meter instance.
    ///
    /// @param logger (Logger): Logger instance to log progress.
    /// @param pc (ProgressConfig): Configuration for progress bar.
    Progress(Logger& logger, ProgressConfig pc = ProgressConfig())
        : logger(&logger) {
        this->unit = pc.unit;
        this->precision = pc.precision;
    }

    /// @brief Initialize the progress meter instance.
    ///
    /// @param pc (ProgressConfig): Configuration for progress bar.
    Progress(ProgressConfig pc = ProgressConfig()) {
        this->unit = pc.unit;
        this->precision = pc.precision;
    }

    Progress(const Progress&) = default;
    Progress(Progress&&) = default;
    Progress& operator=(const Progress&) = default;
    Progress& operator=(Progress&&) = default;
    ~Progress() = default;

    /// @brief Get the current progress.
    ///
    /// @returns uint64_t: Current progress.
    uint64_t get_current() {
        if (!this->is_active) {
            throw std::logic_error("Progress is not active");
        }
        return this->current.value();
    }

    /// @brief Initialize the progress meter.
    ///
    /// @param total (uint64_t): Total progress value.
    /// @param initial (uint64_t): Initial progress value.
    Progress& operator()(uint64_t total, uint64_t initial) {
        if (this->is_active || this->is_running) {
            this->stop();
        }

        this->initial = initial;
        this->current = initial;
        this->total = total;
        this->initial_time = std::chrono::steady_clock::now();
        this->current_time = this->initial_time;
        this->rate = 0.0;
        this->old_length = std::nullopt;
        this->is_active = true;
        this->is_running = false;

        return *this;
    }

    /// @brief Initialize the progress meter.
    ///
    /// @param total (uint64_t): Total progress value.
    Progress& operator()(uint64_t total) {
        if (this->is_active || this->is_running) {
            this->stop();
        }

        this->initial = 0;
        this->current = 0;
        this->total = total;
        this->initial_time = std::chrono::steady_clock::now();
        this->current_time = this->initial_time;
        this->rate = 0.0;
        this->old_length = std::nullopt;
        this->is_active = true;
        this->is_running = false;

        return *this;
    }

    /// @brief Start the progress meter.
    void run();

    /// @brief Manually update the progress meter.
    ///
    /// @param inc (uint64_t): Number of items processed.
    void update(uint64_t inc);

    /// @brief Stop the progress meter.
    void stop();

    /// @brief Iterator over progress
    class iterator {
       private:
        std::reference_wrapper<Progress> progress;
        uint64_t value;

       public:
        friend class Progress;
        friend class const_iterator;

        using value_type = uint64_t;
        using difference_type = std::ptrdiff_t;
        using pointer = uint64_t*;
        using reference = uint64_t&;
        using iterator_category = std::forward_iterator_tag;
        using element_type = uint64_t;

        constexpr iterator(Progress& progress, uint64_t value) noexcept
            : progress(std::ref(progress)) {
            this->value = value;
        }

        constexpr reference operator*() const noexcept {
            return this->progress.get().current.value();
        }

        iterator& operator++();
        iterator operator++(int);

        constexpr bool operator==(const iterator& other) const noexcept {
            return this->value == other.value;
        }

        constexpr bool operator!=(const iterator& other) const noexcept {
            return !(*this == other);
        }
    };

    constexpr iterator begin() {
        this->run();
        return iterator(*this, this->initial.value());
    }

    constexpr iterator end() {
        if (!this->is_active) {
            throw std::logic_error("Progress is not active");
        }
        return iterator(*this, this->total.value());
    }
};

/// @brief Configuration for the logger.
struct LoggerConfig {
    bool quiet = false;
    std::string scope = "";
};

/// @brief Logger class for logging messages and progress updates.
class Logger {
   private:
    bool quiet;
    std::string scope;
    std::string prefix;

   public:
    /// @brief Reusable progress meter that is logged by the logger.
    Progress progress;

    /// @brief Constructor for the logger.
    ///
    /// @param lc (LoggerConfig): Configuration for the logger.
    /// @param pc (ProgressConfig): Configuration for the progress meter.
    Logger(LoggerConfig lc, ProgressConfig pc = ProgressConfig())
        : quiet(lc.quiet), scope(lc.scope), progress(*this, pc) {
        if (lc.scope.size() == 0) {
            this->prefix = "";
        } else {
            this->prefix = lc.scope + "::";
        }
    }

    /// @brief Constructor for the logger.
    ///
    /// @param pc (ProgressConfig): Configuration for the progress meter.
    Logger(ProgressConfig pc = ProgressConfig()) : progress(*this, pc) {
        LoggerConfig lc = LoggerConfig();
        this->quiet = lc.quiet;
        this->scope = lc.scope;
        if (lc.scope.size() == 0) {
            this->prefix = "";
        } else {
            this->prefix = lc.scope + "::";
        }
    }

    Logger(const Logger&) = default;
    Logger(Logger&&) = default;
    Logger& operator=(const Logger&) = default;
    Logger& operator=(Logger&&) = default;
    ~Logger() = default;

    /// @brief Logs an informational message.
    ///
    /// @param msg (std::string): The message to log.
    void info(std::string msg) {
        if (this->quiet) return;
        std::cerr << "[" << this->prefix << "INFO]: " << msg << "\n";
    }

    /// @brief Logs a debug message.
    ///
    /// @param msg (std::string): The message to log.
    void debug(std::string msg) {
        if (this->quiet) return;
        std::cerr << "[" << this->prefix << "DEBUG]: " << msg << "\n";
    }

    /// @brief Logs a warning message.
    ///
    /// @param msg (std::string): The message to log.
    void warn(std::string msg) {
        if (this->quiet) return;
        std::cerr << "[" << this->prefix << "WARN]: " << msg << "\n";
    }

    /// @brief Logs an error message.
    ///
    /// @param msg (std::string): The message to log.
    void error(std::string msg) {
        if (this->quiet) return;
        std::cerr << "[" << this->prefix << "ERROR]: " << msg << "\n";
    }

    /// @brief Logs a progress message.
    ///
    /// Note: The method is called automatically when the progress is updated.
    std::optional<size_t> log_progress() {
        if (this->quiet) return std::nullopt;

        auto elapsed = static_cast<double>(
                           std::chrono::duration_cast<std::chrono::nanoseconds>(
                               this->progress.current_time.value() -
                               this->progress.initial_time.value())
                               .count()) /
                       1e9f;
        auto left =
            this->progress.total.value() > this->progress.current.value()
                ? this->progress.rate.value() > 0.0
                      ? (this->progress.total.value() -
                         this->progress.current.value()) /
                            this->progress.rate.value()
                      : 0.0
                : 0.0;
        auto estimated = elapsed + left;

        std::ostringstream times;
        times << std::fixed << std::setprecision(0)
              << std::floor(elapsed / 60.0) << ":" << std::right
              << std::setfill('0') << std::setw(2) << std::fmod(elapsed, 60.0)
              << "<" << std::fixed << std::setprecision(0)
              << std::floor(estimated / 60.0) << ":" << std::right
              << std::setfill('0') << std::setw(2)
              << std::fmod(estimated, 60.0);

        std::ostringstream msgs;
        msgs << "\r[" << this->prefix
             << "PROGRESS]: " << this->progress.current.value() << " / "
             << this->progress.total.value();
        msgs << " [" << times.str() << ", ";
        msgs << std::fixed << std::setprecision(this->progress.precision);
        if (this->progress.rate >= 1.0 || this->progress.rate == 0.0) {
            msgs << this->progress.rate.value() << " " << this->progress.unit
                 << "s/sec]";
        } else {
            msgs << this->progress.rate.value() << " sec/"
                 << this->progress.unit << "]";
        }
        std::string msg = msgs.str();
        if (this->progress.old_length.has_value()) {
            auto width =
                std::max(this->progress.old_length.value(), msg.size());
            std::cerr << std::left << std::setw(width) << std::setfill(' ')
                      << msg;
        } else {
            std::cerr << msg;
        }
        if (this->progress.current.value() >= this->progress.total.value()) {
            std::cerr << "\n";
        }
        return msg.size();
    }
};

void Progress::run() {
    if (!this->is_active) {
        throw std::logic_error("Progress is not active");
    }
    if (this->is_running) {
        throw std::logic_error("Progress is already running");
    }

    this->is_running = true;

    if (this->logger) {
        this->old_length = this->logger->log_progress();
    } else {
        std::cout << this->current.value() << " / " << this->total.value()
                  << std::fixed << std::setprecision(this->precision) << " ["
                  << this->rate.value() << " " << this->unit << "s/sec]";
    }
}

void Progress::update(uint64_t inc) {
    if (!this->is_running) {
        throw std::logic_error("Progress is not running");
    }

    this->current.value() += inc;
    this->current_time.value() = std::chrono::steady_clock::now();
    auto elapsed =
        static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                this->current_time.value() - this->initial_time.value())
                .count()) /
        1e9f;
    this->rate.value() =
        (this->current.value() - this->initial.value()) / elapsed;

    if (this->logger) {
        this->old_length = this->logger->log_progress();
    } else {
        auto left = this->total.value() > this->current.value()
                        ? this->rate.value() > 0.0
                              ? (this->total.value() - this->current.value()) /
                                    this->rate.value()
                              : 0.0
                        : 0.0;
        auto estimated = elapsed + left;

        std::ostringstream times;
        times << std::fixed << std::setprecision(0)
              << std::floor(elapsed / 60.0) << ":" << std::right
              << std::setfill('0') << std::setw(2) << std::fmod(elapsed, 60.0)
              << "<" << std::fixed << std::setprecision(0)
              << std::floor(estimated / 60.0) << ":" << std::right
              << std::setfill('0') << std::setw(2)
              << std::fmod(estimated, 60.0);

        if (this->rate.value() >= 1.0 || this->rate.value() == 0.0) {
            std::cout << this->current.value() << " / " << this->total.value()
                      << std::fixed << std::setprecision(this->precision)
                      << " [" << times.str() << ", " << this->rate.value()
                      << " " << this->unit << "s/sec]";
        } else {
            std::cout << this->current.value() << " / " << this->total.value()
                      << std::fixed << std::setprecision(this->precision)
                      << " [" << times.str() << ", " << this->rate.value()
                      << " sec/" << this->unit << "]";
        }
    }
}

void Progress::stop() {
    this->is_running = false;
    this->reset();
}

Progress::iterator& Progress::iterator::operator++() {
    if (!this->progress.get().is_running) {
        throw std::logic_error("Progress is not running");
    }
    this->progress.get().current.value() = ++this->value;
    this->progress.get().current_time.value() =
        std::chrono::steady_clock::now();

    auto elapsed = static_cast<double>(
                       std::chrono::duration_cast<std::chrono::nanoseconds>(
                           this->progress.get().current_time.value() -
                           this->progress.get().initial_time.value())
                           .count()) /
                   1e9f;
    this->progress.get().rate.value() = (this->progress.get().current.value() -
                                         this->progress.get().initial.value()) /
                                        elapsed;

    if (this->progress.get().logger) {
        this->progress.get().old_length =
            this->progress.get().logger->log_progress();
    } else {
        auto left = this->progress.get().total.value() >
                            this->progress.get().current.value()
                        ? this->progress.get().rate.value() > 0.0
                              ? (this->progress.get().total.value() -
                                 this->progress.get().current.value()) /
                                    this->progress.get().rate.value()
                              : 0.0
                        : 0.0;
        auto estimated = elapsed + left;

        std::ostringstream times;
        times << std::fixed << std::setprecision(0)
              << std::floor(elapsed / 60.0) << ":" << std::right
              << std::setfill('0') << std::setw(2) << std::fmod(elapsed, 60.0)
              << "<" << std::fixed << std::setprecision(0)
              << std::floor(estimated / 60.0) << ":" << std::right
              << std::setfill('0') << std::setw(2)
              << std::fmod(estimated, 60.0);

        if (this->progress.get().rate.value() >= 1.0 ||
            this->progress.get().rate.value() == 0.0) {
            std::cout << this->progress.get().current.value() << " / "
                      << this->progress.get().total.value() << std::fixed
                      << std::setprecision(this->progress.get().precision)
                      << " [" << times.str() << ", "
                      << this->progress.get().rate.value() << " "
                      << this->progress.get().unit << "s/sec]";
        } else {
            std::cout << this->progress.get().current.value() << " / "
                      << this->progress.get().total.value() << std::fixed
                      << std::setprecision(this->progress.get().precision)
                      << " [" << times.str() << ", "
                      << this->progress.get().rate.value() << " sec/"
                      << this->progress.get().unit << "]";
        }
    }

    return *this;
}

Progress::iterator Progress::iterator::operator++(int) {
    auto tmp = *this;
    ++(*this);
    return tmp;
}
}  // namespace ubpe

#endif  // LOGGER_HPP
