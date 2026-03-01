#ifndef DOCUMENT_SPLITTER_HPP
#define DOCUMENT_SPLITTER_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "ssstree.hpp"
#include "utils.hpp"

namespace ubpe {

/// @brief Struct to represent the different modes for splitting documents.
struct SplitMode {
   private:
    /// Basic split modes.
    enum class SplitModeEnum {
        NONE = 0,
        KNOWN_WORDS = 1 << 0,
        BREAK_TOKENS = 1 << 1,
        REGEX = 1 << 2,
        STOP_TOKENS = 1 << 3
    };

   public:
    using value_type = Flags<SplitModeEnum>;

    /// No split mode.
    static constexpr auto NONE =
        value_type::template value<SplitModeEnum::NONE>();
    /// Split by known words.
    static constexpr auto KNOWN_WORDS =
        value_type::template value<SplitModeEnum::KNOWN_WORDS>();
    /// Split by break tokens.
    static constexpr auto BREAK_TOKENS =
        value_type::template value<SplitModeEnum::BREAK_TOKENS>();
    /// Split by regex::findall.
    static constexpr auto REGEX =
        value_type::template value<SplitModeEnum::REGEX>();
    /// Split by stop tokens.
    static constexpr auto STOP_TOKENS =
        value_type::template value<SplitModeEnum::STOP_TOKENS>();

    /// Full split mode.
    static constexpr auto FULL = value_type::template combine<
        SplitModeEnum::KNOWN_WORDS, SplitModeEnum::BREAK_TOKENS,
        SplitModeEnum::REGEX, SplitModeEnum::STOP_TOKENS>();
};

/// @brief Type to represent optional regex patterns.
///
/// Note: The type is conditional: if `TokenType` is `char` or `wchar_t`, it is
/// an optional regex pattern, otherwise it is `std::monostate`.
template <typename TokenType>
using OptionalRegexType = std::conditional_t<
    std::is_same_v<TokenType, char> || std::is_same_v<TokenType, wchar_t>,
    std::optional<std::basic_regex<TokenType>>, std::monostate>;

/// @brief Type to represent optional regex patterns as strings.
///
/// Note: The type is conditional: if `TokenType` is `char` or `wchar_t`, it is
/// an optional regex pattern string, otherwise it is `std::monostate`.
template <typename TokenType>
using OptionalPatternType = std::conditional_t<
    std::is_same_v<TokenType, char> || std::is_same_v<TokenType, wchar_t>,
    std::optional<std::basic_string<TokenType>>, std::monostate>;

/// @brief `SplitPipeline` configuration structure.
///
/// Fields:
/// - `known_words`: A variant that can hold a monostate, a vector of `DocType`,
/// a set of `DocType`, or a map of `DocType` to `uint32_t`.
/// - `break_tokens`: A variant that can hold a monostate, a vector of
/// `TokenType`, a set of `TokenType`, or an unordered set of `TokenType`.
/// - `regex_pattern`: An optional string representing a regex pattern.
/// - `stop_tokens`: A variant that can hold a monostate, a vector of
/// `TokenType`, a set of `TokenType`, or an unordered set of `TokenType`.
template <DocumentT DocType, typename TokenType = typename DocType::value_type>
struct SplitPipelineConfig {
    std::variant<std::monostate, std::vector<DocType>, std::set<DocType>,
                 std::map<DocType, uint32_t>>
        known_words{};
    std::variant<std::monostate, std::vector<TokenType>, std::set<TokenType>,
                 std::unordered_set<TokenType>>
        break_tokens{};
    [[no_unique_address]] OptionalPatternType<TokenType> regex_pattern{};
    std::variant<std::monostate, std::vector<TokenType>, std::set<TokenType>,
                 std::unordered_set<TokenType>>
        stop_tokens{};
};

/// @brief A class representing a pipeline for splitting documents.
template <DocumentT DocType, typename TokenType = typename DocType::value_type>
class SplitPipeline {
   private:
    std::map<TokenType, uint32_t> alphabet;
    std::optional<std::map<DocType, uint32_t>> known_words{};
    std::optional<std::unordered_set<TokenType>> break_tokens{};
    [[no_unique_address]] OptionalPatternType<TokenType> regex_pattern{};
    std::optional<std::unordered_set<TokenType>> stop_tokens{};

    [[no_unique_address]] OptionalRegexType<TokenType> regex{};
    std::optional<SSSTree<DocType, uint32_t>> kw_ssstree{};

   public:
    /// @brief Constructor for the SplitPipeline class.
    /// @param alphabet A map representing the alphabet.
    /// @param config Configuration for the pipeline.
    SplitPipeline(const std::map<TokenType, uint32_t>& alphabet,
                  SplitPipelineConfig<DocType, TokenType> config = {})
        : alphabet(alphabet) {
        uint32_t max_token = static_cast<uint32_t>(this->alphabet.size());

        std::unordered_set<TokenType> tokens;
        std::transform(this->alphabet.cbegin(), this->alphabet.cend(),
                       std::inserter(tokens, tokens.end()),
                       [](const auto& pair) { return pair.first; });

        if (std::holds_alternative<std::vector<DocType>>(config.known_words)) {
            const auto& known_words =
                std::get<std::vector<DocType>>(config.known_words);
            if (known_words.size() != 0) {
                this->known_words.emplace();
                for (const auto& word : known_words) {
                    this->known_words->emplace(word, max_token++);
                }
            }
        } else if (std::holds_alternative<std::set<DocType>>(
                       config.known_words)) {
            const auto& known_words =
                std::get<std::set<DocType>>(config.known_words);
            if (known_words.size() != 0) {
                this->known_words.emplace();
                for (const auto& word : known_words) {
                    this->known_words->emplace(word, max_token++);
                }
            }
        } else if (std::holds_alternative<std::map<DocType, uint32_t>>(
                       config.known_words)) {
            uint32_t _max_token = max_token;
            const auto& known_words =
                std::get<std::map<DocType, uint32_t>>(config.known_words);
            if (known_words.size() != 0) {
                this->known_words.emplace();
                for (const auto& [word, token] : known_words) {
                    if (token < max_token)
                        throw std::logic_error(
                            "Tokens of `known_words` must be greater than "
                            "alphabet tokens.");
                    this->known_words->emplace(word, token);
                    _max_token = std::max(_max_token, token);
                }
            }
        }
        if (this->known_words.has_value() && this->known_words->empty())
            this->known_words = std::nullopt;

        if (this->known_words.has_value()) {
            this->kw_ssstree.emplace();
            for (const auto& word : this->known_words.value()) {
                auto _ = (*this->kw_ssstree) + word;
            }
        }

        if (std::holds_alternative<std::vector<TokenType>>(
                config.break_tokens)) {
            const auto& break_tokens =
                std::get<std::vector<TokenType>>(config.break_tokens);
            if (break_tokens.size() != 0) {
                this->break_tokens.emplace();
                for (const auto& token : break_tokens) {
                    if (tokens.contains(token)) {
                        this->break_tokens->emplace(token);
                    }
                }
            }
        } else if (std::holds_alternative<std::set<TokenType>>(
                       config.break_tokens)) {
            const auto& break_tokens =
                std::get<std::set<TokenType>>(config.break_tokens);
            if (break_tokens.size() != 0) {
                this->break_tokens.emplace();
                for (const auto& token : break_tokens) {
                    if (tokens.contains(token)) {
                        this->break_tokens->emplace(token);
                    }
                }
            }
        } else if (std::holds_alternative<std::unordered_set<TokenType>>(
                       config.break_tokens)) {
            const auto& break_tokens =
                std::get<std::unordered_set<TokenType>>(config.break_tokens);
            if (break_tokens.size() != 0) {
                this->break_tokens.emplace();
                for (const auto& token : break_tokens) {
                    if (tokens.contains(token)) {
                        this->break_tokens->emplace(token);
                    }
                }
            }
        }
        if (this->break_tokens.has_value() && this->break_tokens->empty())
            this->break_tokens = std::nullopt;

        if constexpr (!std::is_same_v<OptionalPatternType<TokenType>,
                                      std::monostate>) {
            if (config.regex_pattern.has_value()) {
                this->regex_pattern = config.regex_pattern.value();
                const auto& pattern = config.regex_pattern.value();
                if (!pattern.empty()) {
                    this->regex.emplace(pattern);
                } else {
                    this->regex = std::nullopt;
                }
            } else {
                this->regex_pattern = std::nullopt;
                this->regex = std::nullopt;
            }
        }

        if (std::holds_alternative<std::vector<TokenType>>(
                config.stop_tokens)) {
            const auto& stop_tokens =
                std::get<std::vector<TokenType>>(config.stop_tokens);
            if (stop_tokens.size() != 0) {
                this->stop_tokens.emplace();
                for (const auto& token : stop_tokens) {
                    if (tokens.contains(token)) {
                        this->stop_tokens->emplace(token);
                    }
                }
            }
        } else if (std::holds_alternative<std::set<TokenType>>(
                       config.stop_tokens)) {
            const auto& stop_tokens =
                std::get<std::set<TokenType>>(config.stop_tokens);
            if (stop_tokens.size() != 0) {
                this->stop_tokens.emplace();
                for (const auto& token : stop_tokens) {
                    if (tokens.contains(token)) {
                        this->stop_tokens->emplace(token);
                    }
                }
            }
        } else if (std::holds_alternative<std::unordered_set<TokenType>>(
                       config.stop_tokens)) {
            const auto& stop_tokens =
                std::get<std::unordered_set<TokenType>>(config.stop_tokens);
            if (stop_tokens.size() != 0) {
                this->stop_tokens.emplace();
                for (const auto& token : stop_tokens) {
                    if (tokens.contains(token)) {
                        this->stop_tokens->emplace(token);
                    }
                }
            }
        }
        if (this->stop_tokens.has_value() && this->stop_tokens->empty())
            this->stop_tokens = std::nullopt;
    }
    SplitPipeline(const SplitPipeline&) = default;
    SplitPipeline(SplitPipeline&&) = default;
    SplitPipeline& operator=(const SplitPipeline&) = default;
    SplitPipeline& operator=(SplitPipeline&&) = default;
    ~SplitPipeline() = default;

    /// @brief Executes the pipeline on a document.
    /// @param doc The document to split.
    /// @param mode The splitting mode.
    /// @param leave_separators Whether to leave separators in the output.
    /// @return A vector of vectors of token indices.
    std::vector<std::vector<uint32_t>> operator()(
        const DocType& doc, SplitMode::value_type mode = SplitMode::FULL,
        bool leave_separators = true) {
        // if the `doc` should and can be splitted into parts by known words it
        // will be splitted into parts by known words, each known word will be
        // represented as a vector of a single element --- token number of the
        // word, and each other part will be splitted by other modes on
        if (mode.has(SplitMode::KNOWN_WORDS) && this->kw_ssstree.has_value()) {
            const auto& kw_ssstree = this->kw_ssstree.value();
            std::vector<std::vector<uint32_t>> parts{};

            // search for a known word, split the part before the word and
            // append the token of the word itself
            auto part_begin = doc.cbegin();
            for (auto si = 0; si < doc.size(); si++) {
                // vector of pairs, where
                // .first -- length of the known word
                // .second -- the token assigned to this word
                std::vector<std::pair<size_t, uint32_t>> kw_candidates =
                    kw_ssstree(doc, si, true);
                if (kw_candidates.empty()) continue;

                if (doc.cbegin() + si != part_begin) {
                    auto split = DocType(part_begin, doc.cbegin() + si);
                    for (const DocType& part :
                         this->split_part(split, mode, leave_separators)) {
                        std::vector<uint32_t> _part{};
                        _part.reserve(part.size());
                        std::transform(part.cbegin(), part.cend(),
                                       std::back_inserter(_part),
                                       [this](const TokenType& token) {
                                           return this->alphabet.at(token);
                                       });
                        parts.emplace_back(_part);
                    }
                }

                if (leave_separators) {
                    parts.push_back({kw_candidates.back().second});
                }
                part_begin = doc.cbegin() + si + kw_candidates.back().first;
                si += kw_candidates.back().first;
            }

            // add the remaining part if it exists
            if (part_begin != doc.cend()) {
                auto split = DocType(part_begin, doc.cend());
                for (const DocType& part :
                     this->split_part(split, mode, leave_separators)) {
                    std::vector<uint32_t> _part{};
                    _part.reserve(part.size());
                    std::transform(part.cbegin(), part.cend(),
                                   std::back_inserter(_part),
                                   [this](const TokenType& token) {
                                       return this->alphabet.at(token);
                                   });
                    parts.emplace_back(_part);
                }
            }

            return parts;
        }

        // else, the `doc` should be splitted into parts by other modes being
        // the only part
        std::vector<std::vector<uint32_t>> parts{};
        for (const DocType& part :
             this->split_part(doc, mode, leave_separators)) {
            std::vector<uint32_t> _part{};
            _part.reserve(part.size());
            std::transform(part.cbegin(), part.cend(),
                           std::back_inserter(_part),
                           [this](const TokenType& token) {
                               return this->alphabet.at(token);
                           });
            parts.emplace_back(_part);
        }
        return parts;
    }

   private:
    /// @brief Split a part of a document by tokens.
    static std::vector<DocType> split_part_by_tokens(
        const DocType& part, const std::unordered_set<TokenType>& tokens,
        bool leave_separators = true) {
        std::vector<DocType> parts{};
        auto part_begin = part.cbegin();

        for (auto it = part.cbegin(); it != part.cend(); it++) {
            if (!tokens.contains(*it)) continue;

            if (it != part_begin) parts.emplace_back(DocType(part_begin, it));
            if (leave_separators) parts.emplace_back(DocType(it, it + 1));
            part_begin = it + 1;
        }

        if (part_begin != part.cend())
            parts.emplace_back(DocType(part_begin, part.cend()));
        return parts;
    }

    std::vector<DocType> split_part_by_break_tokens(
        const DocType& part, bool leave_separators = true) {
        if (this->break_tokens.has_value())
            return this->split_part_by_tokens(part, this->break_tokens.value(),
                                              leave_separators);
        return {part};
    }

    std::vector<DocType> split_part_by_stop_tokens(
        const DocType& part, bool leave_separators = true) {
        if (this->stop_tokens.has_value())
            return this->split_part_by_tokens(part, this->stop_tokens.value(),
                                              leave_separators);
        return {part};
    }

    std::vector<DocType> split_part_by_regex(const DocType& part) {
        if constexpr ((std::is_same_v<TokenType, char> ||
                       std::is_same_v<TokenType, wchar_t>) &&
                      std::is_same_v<DocType, std::basic_string<TokenType>>) {
            if (this->regex.has_value()) {
                std::vector<DocType> matches;
                std::transform(std::sregex_iterator(part.begin(), part.end(),
                                                    this->regex.value()),
                               std::sregex_iterator(),
                               std::back_inserter(matches),
                               [](const auto& match) { return match.str(); });
                return matches;
            }
        }
        return {part};
    }

    std::vector<DocType> split_part(
        const DocType& part, SplitMode::value_type mode = SplitMode::FULL,
        bool leave_separators = true) {
        std::vector<DocType> parts = {part};

        if (mode.has(SplitMode::BREAK_TOKENS)) {
            std::vector<DocType> splits{};
            for (const auto& part : parts) {
                auto _parts =
                    this->split_part_by_break_tokens(part, leave_separators);
                splits.insert(splits.cend(),
                              std::make_move_iterator(_parts.begin()),
                              std::make_move_iterator(_parts.end()));
            }
            parts = std::move(splits);
        }

        if (mode.has(SplitMode::REGEX)) {
            std::vector<DocType> splits{};
            for (const auto& part : parts) {
                auto _parts = this->split_part_by_regex(part);
                splits.insert(splits.cend(),
                              std::make_move_iterator(_parts.begin()),
                              std::make_move_iterator(_parts.end()));
            }
            parts = std::move(splits);
        }

        if (mode.has(SplitMode::STOP_TOKENS)) {
            std::vector<DocType> splits{};
            for (const auto& part : parts) {
                auto _parts =
                    this->split_part_by_stop_tokens(part, leave_separators);
                splits.insert(splits.cend(),
                              std::make_move_iterator(_parts.begin()),
                              std::make_move_iterator(_parts.end()));
            }
            parts = std::move(splits);
        }

        return parts;
    }
};
}  // namespace ubpe

#endif  // DOCUMENT_SPLITTER_HPP
