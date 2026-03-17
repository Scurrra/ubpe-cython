#ifndef UBPE_BASE_CPP
#define UBPE_BASE_CPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "splitter.hpp"
#include "utils.hpp"

namespace ubpe {

template <DocumentT DocType, typename TokenType = typename DocType::value_type>
class UbpeBase {
   protected:
    std::uint32_t n_tokens;

    std::map<TokenType, std::uint32_t> alphabet;
    std::map<std::uint32_t, TokenType> inverse_alphabet;

    std::map<std::vector<std::uint32_t>, std::uint32_t> tokens_forward_mapper;
    std::map<std::uint32_t, std::vector<std::uint32_t>> tokens_backward_mapper;

    std::map<std::uint32_t, double> tokens_weights;

    std::optional<std::map<DocType, std::uint32_t>> known_words{};
    std::optional<std::map<std::uint32_t, DocType>> inverse_known_words{};
    std::optional<std::set<TokenType>> break_tokens{};
    [[no_unique_address]] OptionalPatternType<TokenType> regex_pattern{};
    std::optional<std::set<TokenType>> stop_tokens{};
    SplitPipeline<DocType, TokenType> split_pipeline{};

    /// @brief Function that rearranges found tokens according to their weights
    /// and trims dictionary of the tokenizer to be not greater than
    /// `this.n_tokens`.
    void _rearrange_tokens_by_weight(bool is_classic) {
        if (this->tokens_backward_mapper.size() == 0 ||
            this->tokens_weights.size() == 0)
            throw std::logic_error("Can not rearrange non-fitted tokenizer");

        // buffer to sort by weight and eliminate some of them
        std::vector<std::pair<std::uint32_t, std::vector<std::uint32_t>>> buf(
            this->tokens_backward_mapper.cbegin(),
            this->tokens_backward_mapper.cend());

        // sort buffer
        std::stable_sort(buf.begin(), buf.end(),
                         [this](const auto& a, const auto& b) {
                             return this->tokens_weights[a.first] <
                                    this->tokens_weights[b.first];
                         });

        // min number of tokens to delete
        auto min_token = static_cast<std::uint32_t>(
            this->alphabet.size() +
            (this->known_words.has_value() ? this->known_words->size() : 0));
        auto to_delete_quantity =
            this->tokens_weights.size() - this->n_tokens + min_token;

        // find tokens to delete
        std::set<std::uint32_t> to_delete;
        if (is_classic) {
            // check tokens with smalest weights first
            for (std::uint32_t i = 0; i < buf.size(); i++) {
                // skip if `i` is already pended for deletion
                if (to_delete.contains(i)) continue;
                // if all values for deletion are already found
                if (to_delete.size() >= to_delete_quantity) break;
                std::vector<std::uint32_t> queue_to_delete = {i};
                while (!queue_to_delete.empty()) {
                    auto current = queue_to_delete.back();
                    queue_to_delete.pop_back();
                    to_delete.insert(current);
                    for (std::uint32_t j = 0; j < buf.size(); j++) {
                        // as this is classic mode, we check if token is one of
                        // the elements in the pair, but pair in this case is a
                        // vector of two elements
                        if (buf[j].second[0] == buf[current].first ||
                            buf[j].second[1] == buf[current].first) {
                            queue_to_delete.push_back(j);
                        }
                    }
                }
            }
        } else {
            // check tokens with smalest weights first
            for (std::uint32_t i = 0; i < buf.size(); i++) {
                // if all values for deletion are already found
                if (to_delete.size() >= to_delete_quantity) break;
                // add token for deletion to the set
                to_delete.insert(i);
            }
        }

        // make `to_delete` contain actual tokens
        std::set<std::uint32_t> to_delete_buf;
        std::transform(
            to_delete.cbegin(), to_delete.cend(),
            std::inserter(to_delete_buf, to_delete_buf.end()),
            [&buf](const auto& element) { return buf[element].first; });
        to_delete = std::move(to_delete_buf);

        // reverse `buf`
        std::reverse(buf.begin(), buf.end());

        // create mapping between old tokens and new tokens
        std::map<std::uint32_t, std::uint32_t> transformer;
        std::generate_n(
            std::inserter(transformer, transformer.end()), min_token,
            [i = static_cast<std::uint32_t>(
                 -1)]() mutable -> std::pair<std::uint32_t, std::uint32_t> {
                i++;
                return {i, i};
            });
        std::generate_n(
            std::inserter(transformer, transformer.end()),
            buf.size() - to_delete.size(),
            [&buf, &to_delete, i = static_cast<std::size_t>(-1),
             offset = static_cast<std::size_t>(0),
             &min_token]() mutable -> std::pair<std::uint32_t, std::uint32_t> {
                i++;
                while (to_delete.contains(buf[i + offset].first)) offset++;
                return {buf[i + offset].first, min_token + i};
            });

        // drop weights for deleted tokens
        std::map<std::uint32_t, double> tokens_weights;
        std::transform(
            std::next(transformer.cbegin(), min_token), transformer.cend(),
            std::inserter(tokens_weights, tokens_weights.end()),
            [this](const auto& mapper) -> std::pair<std::uint32_t, double> {
                return {mapper.second, this->tokens_weights[mapper.first]};
            });
        this->tokens_weights = std::move(tokens_weights);

        // update backward mapper
        std::map<std::uint32_t, std::vector<std::uint32_t>>
            tokens_backward_mapper;
        std::transform(
            std::next(transformer.cbegin(), min_token), transformer.cend(),
            std::inserter(tokens_backward_mapper, tokens_backward_mapper.end()),
            [&, this](const auto& mapper)
                -> std::pair<std::uint32_t, std::vector<std::uint32_t>> {
                const auto& old_sequence =
                    this->tokens_backward_mapper[mapper.first];
                std::vector<std::uint32_t> new_sequence;
                new_sequence.reserve(old_sequence.size());
                std::transform(old_sequence.cbegin(), old_sequence.cend(),
                               std::back_inserter(new_sequence),
                               [&](const auto& el) { return transformer[el]; });
                return {mapper.second, new_sequence};
            });
        this->tokens_backward_mapper = std::move(tokens_backward_mapper);
    }

    /// @brief Function for replacing pair of adjacent tokens in a list with a
    /// new one.
    /// @param vec Vector in which adjacent pairs will be replaced.
    /// @param sub A substitution map, where keys are first tokens in the pairs,
    /// and the values are pair of the second token and the new one wrapped in a
    /// list.
    static void _replace_token_pairs(
        std::vector<std::uint32_t>& vec,
        const std::unordered_map<
            std::uint32_t, std::pair<std::uint32_t, std::uint32_t>>& sub) {
        // two pointers
        std::size_t left = 0, right = 0;
        vec[left] = vec[right];

        // while we can access element with index `right+1`
        while (right < vec.size() - 1) {
            // check `vec[right+1]`
            if (sub.contains(vec[right]) &&
                vec[right + 1] == sub.at(vec[right]).first) {
                // replace `vec[left]` with the new value
                // and move `right` forward
                vec[left++] = sub.at(vec[right]).second;
                right += 2;
            } else {
                vec[left++] = vec[right++];
            }
        }
        // optionally add the last token
        if (right < vec.size()) vec[left++] = vec[right++];

        // `left` is the length of a new sequence, so we just resize the old one
        vec.resize(left);
    }

    /// @brief Function for replacing pair of adjacent tokens in a list with a
    /// new one.
    /// @param vecs Vectors in which adjacent pairs will be replaced.
    /// @param sub A substitution map, where keys are first tokens in the pairs,
    /// and the values are pair of the second token and the new one wrapped in a
    /// list.
    static void _replace_token_pairs(
        std::vector<std::vector<std::uint32_t>>& vecs,
        const std::unordered_map<
            std::uint32_t, std::pair<std::uint32_t, std::uint32_t>>& sub) {
        std::for_each(vecs.begin(), vecs.end(),
                      [&sub](auto& doc) { _replace_token_pairs(doc, sub); });
    }

    /// @brief Convert document of `DocType` to vector of base tokens.
    /// @param doc Document, i.e. data of type `DocType`.
    /// @return Vector of base tokens.
    std::vector<std::uint32_t> _doc_to_vec(const DocType& doc) const {
        std::vector<std::uint32_t> tokens;
        tokens.reserve(doc.size());
        std::transform(
            doc.cbegin(), doc.cend(), std::back_inserter(tokens),
            [this](const auto& element) { return this->alphabet.at(element); });
        return tokens;
    }

    /// @brief Convert vector of base tokens to document of `DocType`.
    /// @param tokens Vector of base tokens.
    /// @return Document, i.e. data of type `DocType`.
    DocType _vec_to_doc(const std::vector<std::uint32_t>& tokens) const {
        if (this->inverse_known_words.has_value()) {
            DocType doc;
            for (const auto& token : tokens) {
                if (this->inverse_known_words->contains(token)) {
                    const auto& word = this->inverse_known_words->at(token);
                    doc.insert(doc.end(), word.cbegin(), word.cend());
                } else {
                    doc.emplace_back(this->inverse_alphabet.at(token));
                }
            }
            return doc;
        }
        DocType doc;
        doc.reserve(tokens.size());
        std::transform(tokens.cbegin(), tokens.cend(), std::back_inserter(doc),
                       [this](const auto& token) {
                           return this->inverse_alphabet.at(token);
                       });
        return doc;
    }

    virtual std::vector<std::pair<std::vector<std::uint32_t>, double>>
    encode_word(std::vector<std::uint32_t> word,
                std::uint8_t top_n = 1) const = 0;

   public:
    UbpeBase(std::uint32_t n_tokens,
             std::map<TokenType, std::uint32_t> alphabet,
             std::optional<std::map<DocType, std::uint32_t>> known_words =
                 std::nullopt,
             std::optional<std::set<TokenType>> break_tokens = std::nullopt,
             std::optional<std::variant<std::string, std::wstring>>
                 regex_pattern = std::nullopt,
             std::optional<std::set<TokenType>> stop_tokens = std::nullopt)
        : n_tokens(n_tokens),
          alphabet(alphabet),
          known_words(known_words),
          break_tokens(break_tokens),
          stop_tokens(stop_tokens) {
        std::transform(
            alphabet.cbegin(), alphabet.cend(),
            std::inserter(this->inverse_alphabet, this->inverse_alphabet.end()),
            [](const auto& element) -> std::pair<std::uint32_t, std::uint32_t> {
                return {element.second, element.first};
            });

        if (known_words.has_value()) {
            this->inverse_known_words.emplace();
            std::transform(
                known_words->cbegin(), known_words->cend(),
                std::inserter(*this->inverse_known_words,
                              this->inverse_known_words->end()),
                [](const auto& element) -> std::pair<std::uint32_t, DocType> {
                    return {element.second, element.first};
                });
        }

        SplitPipelineConfig<DocType, TokenType> split_pipeline_config{};
        if (known_words.has_value()) {
            split_pipeline_config.known_words = known_words.value();
        }
        if (break_tokens.has_value()) {
            split_pipeline_config.break_tokens = break_tokens.value();
        }
        if constexpr (!std::is_same_v<OptionalPatternType<TokenType>,
                                      std::monostate>) {
            split_pipeline_config.regex_pattern = regex_pattern;
            this->regex_pattern = regex_pattern;
        }
        if (stop_tokens.has_value()) {
            split_pipeline_config.stop_tokens = stop_tokens.value();
        }

        this->split_pipeline =
            SplitPipeline<DocType, TokenType>(alphabet, split_pipeline_config);
    }
    UbpeBase(std::uint32_t n_tokens,
             std::map<TokenType, std::uint32_t> alphabet,
             std::optional<std::map<DocType, std::uint32_t>> known_words,
             std::optional<std::set<TokenType>> break_tokens,
             std::optional<std::set<TokenType>> stop_tokens)
        : UbpeBase(n_tokens, alphabet, known_words, break_tokens, std::nullopt,
                   stop_tokens) {}

    UbpeBase(std::uint32_t n_tokens,
             std::map<TokenType, std::uint32_t> alphabet,
             SplitPipelineConfig<DocType, TokenType> split_pipeline_config)
        : n_tokens(n_tokens), alphabet(alphabet) {
        std::transform(
            alphabet.cbegin(), alphabet.cend(),
            std::inserter(this->inverse_alphabet, this->inverse_alphabet.end()),
            [](const auto& element) -> std::pair<std::uint32_t, std::uint32_t> {
                return {element.second, element.first};
            });

        this->split_pipeline =
            SplitPipeline<DocType, TokenType>(alphabet, split_pipeline_config);

        this->break_tokens = this->split_pipeline.get_break_tokens();
        if constexpr (!std::is_same_v<OptionalPatternType<TokenType>,
                                      std::monostate>) {
            this->regex_pattern = split_pipeline_config.regex_pattern;
        }
        this->stop_tokens = this->split_pipeline.get_stop_tokens();

        this->known_words = this->split_pipeline.get_known_words();
        if (this->known_words.has_value()) {
            this->inverse_known_words.emplace();
            std::transform(
                this->known_words->cbegin(), this->known_words->cend(),
                std::inserter(*this->inverse_known_words,
                              this->inverse_known_words->end()),
                [](const auto& element) -> std::pair<DocType, std::uint32_t> {
                    return {element.second, element.first};
                });
        }
    }

    UbpeBase(std::uint32_t n_tokens,
             std::map<TokenType, std::uint32_t> alphabet,
             std::map<std::uint32_t, TokenType> inverse_alphabet,
             std::map<std::vector<std::uint32_t>, std::uint32_t>
                 tokens_forward_mapper,
             std::map<std::uint32_t, std::vector<std::uint32_t>>
                 tokens_backward_mapper,
             std::map<std::uint32_t, double> tokens_weights,
             std::optional<std::map<DocType, std::uint32_t>> known_words =
                 std::nullopt,
             std::optional<std::set<TokenType>> break_tokens = std::nullopt,
             std::optional<std::variant<std::string, std::wstring>>
                 regex_pattern = std::nullopt,
             std::optional<std::set<TokenType>> stop_tokens = std::nullopt)
        : n_tokens(n_tokens),
          alphabet(alphabet),
          inverse_alphabet(inverse_alphabet),
          tokens_forward_mapper(tokens_forward_mapper),
          tokens_backward_mapper(tokens_backward_mapper),
          tokens_weights(tokens_weights),
          known_words(known_words),
          break_tokens(break_tokens),
          stop_tokens(stop_tokens) {
        if (alphabet.size() != inverse_alphabet.size())
            throw std::invalid_argument(
                "`alphabet` and `inverse_alphabet` should be of the same "
                "size.");

        if (known_words.has_value()) {
            this->inverse_known_words.emplace();
            std::transform(
                known_words->cbegin(), known_words->cend(),
                std::inserter(*this->inverse_known_words,
                              this->inverse_known_words->end()),
                [](const auto& element) -> std::pair<std::uint32_t, DocType> {
                    return {element.second, element.first};
                });
        }

        SplitPipelineConfig<DocType, TokenType> split_pipeline_config{};
        if (known_words.has_value()) {
            split_pipeline_config.known_words = known_words.value();
        }
        if (break_tokens.has_value()) {
            split_pipeline_config.break_tokens = break_tokens.value();
        }
        if constexpr (!std::is_same_v<OptionalPatternType<TokenType>,
                                      std::monostate>) {
            split_pipeline_config.regex_pattern = regex_pattern;
            this->regex_pattern = regex_pattern;
        }
        if (stop_tokens.has_value()) {
            split_pipeline_config.stop_tokens = stop_tokens.value();
        }

        this->split_pipeline =
            SplitPipeline<DocType, TokenType>(alphabet, split_pipeline_config);
    }
    UbpeBase(std::uint32_t n_tokens,
             std::map<TokenType, std::uint32_t> alphabet,
             std::map<std::uint32_t, TokenType> inverse_alphabet,
             std::map<std::vector<std::uint32_t>, std::uint32_t>
                 tokens_forward_mapper,
             std::map<std::uint32_t, std::vector<std::uint32_t>>
                 tokens_backward_mapper,
             std::map<std::uint32_t, double> tokens_weights,
             std::optional<std::map<DocType, std::uint32_t>> known_words,
             std::optional<std::set<TokenType>> break_tokens,
             std::optional<std::set<TokenType>> stop_tokens)
        : UbpeBase(n_tokens, alphabet, inverse_alphabet, tokens_forward_mapper,
                   tokens_backward_mapper, tokens_weights, known_words,
                   break_tokens, std::nullopt, stop_tokens) {}

    UbpeBase(const UbpeBase&) = default;
    UbpeBase(UbpeBase&&) = default;
    UbpeBase& operator=(const UbpeBase&) = default;
    UbpeBase& operator=(UbpeBase&&) = default;
    virtual ~UbpeBase() = default;

    /// @brief Get forward mapper for dumping.
    /// @return `this.tokens_forward_mapper`
    std::map<std::vector<std::uint32_t>, std::uint32_t> getForwardMapper()
        const {
        return this->tokens_forward_mapper;
    }

    /// @brief Get backward mapper for dumping.
    /// @return `this.tokens_backward_mapper`
    std::map<std::uint32_t, std::vector<std::uint32_t>> getBackwardMapper()
        const {
        return this->tokens_backward_mapper;
    }

    /// @brief Get token weighs.
    /// @return `this.tokens_weights`
    std::map<std::uint32_t, double> getTokensWeights() const {
        return this->tokens_weights;
    }

    /// @brief Get alphabet mapping.
    /// @return Base alphabet mapping.
    std::map<TokenType, std::uint32_t> getAlphabet() const {
        return this->alphabet;
    }

    /// @brief Get inverse alphabet mapping.
    /// @return Inverse abse alphabet mapping.
    std::map<std::uint32_t, TokenType> getInverseAlphabet() const {
        return this->inverse_alphabet;
    }

    /// @brief Get known words mapping.
    /// @return Known words mapping.
    std::optional<std::map<DocType, std::uint32_t>> getKnownWords() const {
        return this->known_words;
    }

    /// @brief Get inverse known words mapping.
    /// @return Inverse known words mapping.
    std::optional<std::map<std::uint32_t, DocType>> getInverseKnownWords()
        const {
        return this->inverse_known_words;
    }

    /// @brief Get break tokens.
    /// @return Break tokens.
    std::optional<std::set<TokenType>> getBreakTokens() const {
        return this->break_tokens;
    }

    /// @brief Get regex pattern.
    /// @return Regex pattern.
    ///
    /// Note: If the token type does not support regex patterns, this will
    /// return `std::nullopt`.
    OptionalPatternType<TokenType> getRegexPattern() const {
        if constexpr (!std::is_same_v<OptionalPatternType<TokenType>,
                                      std::monostate>) {
            return this->regex_pattern;
        }
        return std::nullopt;
    }

    /// @brief Get stop tokens.
    /// @return Stop tokens.
    std::optional<std::set<TokenType>> getStopTokens() const {
        return this->stop_tokens;
    }

    /// @brief Get split pipeline.
    /// @return Split pipeline.
    SplitPipeline<DocType, TokenType> getSplitPipeline() const {
        return this->split_pipeline;
    }

    /// @brief Fit tokenizer with `corpus`.
    /// @param corpus Data to fit tokenizer with.
    /// @param n_candidates Number of most popular pairs of adjacent tokens to
    /// be substituted with new ones; ignored in `UbpeClassic`.
    /// @param rearrange_tokens If tokens should be rearranged to make tokens
    /// with smaller numbers be more valueable.
    /// @param split_mode Split mode to use for corpus splitting.
    /// @param quiet Whether to suppress logging.
    virtual void fit(const std::vector<DocType>& corpus,
                     std::uint32_t n_candidates = 50,
                     bool rearrange_tokens = true,
                     SplitMode::value_type split_mode = SplitMode::FULL,
                     bool quiet = false) = 0;
    void fit(const std::vector<DocType>& corpus,
             std::uint32_t n_candidates = 50, bool rearrange_tokens = true,
             std::uint8_t split_mode = 0b1111, bool quiet = false) {
        fit(corpus, n_candidates, rearrange_tokens,
            SplitMode::value_type(split_mode), quiet);
    }

    /// @brief Fit tokenizer with `corpus`.
    /// @param corpus Data to fit tokenizer with.
    /// @param n_candidates Number of most popular pairs of adjacent tokens to
    /// be substituted with new ones; ignored in `UbpeClassic`.
    /// @param rearrange_tokens If tokens should be rearranged to make tokens
    /// with smaller numbers be more valueable.
    /// @param quiet Whether to suppress logging.
    ///
    /// Note: Each document in `corpus` should be a vector of vectors of token
    /// indices, i.e. already splitted and tokenized.
    virtual void fit(
        std::vector<std::vector<std::vector<std::uint32_t>>> corpus,
        std::uint32_t n_candidates = 50, bool rearrange_tokens = true,
        bool quiet = false) = 0;

    /// @brief Encode `document` with fitted tokenizer.
    /// @param doc Sequence of basic tokens to encode.
    /// @param top_n How many candidate ecoding to return; ignored in
    /// `UbpeClassic`.
    /// @param split_mode How to split the document into words.
    /// @return List of encoded documents with weights.
    virtual std::vector<std::pair<std::vector<std::uint32_t>, double>> encode(
        const DocType& doc, std::uint8_t top_n,
        SplitMode::value_type split_mode) const = 0;
    std::vector<std::pair<std::vector<std::uint32_t>, double>> encode(
        const DocType& doc, std::uint8_t top_n, std::uint8_t split_mode) const {
        return encode(doc, top_n, SplitMode::value_type(split_mode));
    }

    /// @brief Encode `document` with fitted tokenizer.
    /// @param parts Vector of vectors of basic tokens to encode.
    /// @param top_n How many candidate ecoding to return; ignored in
    /// `UbpeClassic`.
    /// @return List of encoded documents with weights.
    ///
    /// Note: `doc` should be a vector of vectors of token indices, i.e. already
    /// splitted and tokenized.
    virtual std::vector<std::pair<std::vector<std::uint32_t>, double>> encode(
        const std::vector<std::vector<std::uint32_t>>& parts,
        std::uint8_t top_n = 1) const = 0;

    /// @brief Encode `document` with fitted tokenizer.
    /// @param doc Sequence of basic tokens to encode.
    /// @param top_n How many candidate ecoding to return; ignored in
    /// `UbpeClassic`.
    /// @return List of encoded documents with weights.
    std::vector<std::pair<std::vector<std::uint32_t>, double>> encode(
        const DocType& doc, std::uint8_t top_n) const {
        return encode(doc, top_n, SplitMode::FULL);
    }

    /// @brief Encode `document` with fitted tokenizer.
    /// @param doc Sequence of basic tokens to encode.
    /// @param split_mode How to split the document into words.
    /// @return List of encoded documents with weights.
    std::vector<std::pair<std::vector<std::uint32_t>, double>> encode(
        const DocType& doc, SplitMode::value_type split_mode) const {
        return encode(doc, 1, split_mode);
    }

    /// @brief Encode `document` with fitted tokenizer.
    /// @param doc Sequence of basic tokens to encode.
    /// @return List of encoded documents with weights.
    std::vector<std::pair<std::vector<std::uint32_t>, double>> encode(
        const DocType& doc) const {
        return encode(doc, 1, SplitMode::FULL);
    }

    /// @brief Decode a vector of `tokens` with the fitted tokenizer.
    /// @param tokens An encoded sequence of tokens to decode.
    /// @return Decoded document.
    virtual DocType decode(const std::vector<std::uint32_t>& tokens) const = 0;
};

}  // namespace ubpe

#endif  // UBPE_BASE_CPP
