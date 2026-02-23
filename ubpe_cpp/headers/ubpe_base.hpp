#ifndef UBPE_BASE_CPP
#define UBPE_BASE_CPP

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ubpe {

/// @brief Concept of type that can be a document or a key in a `ubpe::SSSTree`.
template <typename T>
concept DocumentT = std::ranges::range<T> ||
                    std::is_same_v<std::remove_cvref_t<T>,
                                   std::basic_string<typename T::value_type>>;

template <DocumentT DocType, typename TokenType = typename DocType::value_type>
class UbpeBase {
   protected:
    uint32_t n_tokens;
    uint32_t alphabet_size;

    std::map<TokenType, uint32_t> alphabet;
    std::map<uint32_t, TokenType> inverse_alphabet;

    std::map<std::vector<uint32_t>, uint32_t> tokens_forward_mapper;
    std::map<uint32_t, std::vector<uint32_t>> tokens_backward_mapper;

    std::map<uint32_t, double> tokens_weights;

    /// @brief Function that rearranges found tokens according to their weights
    /// and trims dictionary of the tokenizer to be not greater than
    /// `this.n_tokens`.
    void _rearrange_tokens_by_weight() {
        assert((this->tokens_backward_mapper.size() != 0 &&
                this->tokens_weights.size() != 0) &&
               "Can not rearrange non-fitted tokenizer");

        // buffer to sort by weight and eliminate some of them
        std::vector<std::pair<uint32_t, std::vector<uint32_t>>> buf(
            this->tokens_backward_mapper.cbegin(),
            this->tokens_backward_mapper.cend());

        // sort buffer
        std::stable_sort(buf.begin(), buf.end(),
                         [this](const auto& a, const auto& b) {
                             return this->tokens_weights[a.first] <
                                    this->tokens_weights[b.first];
                         });

        // min number of tokens to delete
        auto to_delete_quantity =
            this->tokens_weights.size() - this->n_tokens + this->alphabet_size;

        // find tokens to delete
        std::set<uint32_t> to_delete;
        // check tokens with smalest weights first
        for (uint32_t i = 0; i < buf.size(); i++) {
            // skip if `i` is already pended for deletion
            if (to_delete.contains(i)) continue;
            // if all values for deletion are already found
            if (to_delete.size() >= to_delete_quantity) break;
            // add token for deletion to the set
            to_delete.insert(i);

            // check some rare condition when found token is present in more
            // valueable subsequence of tokens for substitution
            for (uint32_t j = i + 1; j < buf.size(); j++) {
                if (auto it = std::find(buf[j].second.cbegin(),
                                        buf[j].second.cend(), buf[i].first);
                    it != buf[j].second.end()) {
                    to_delete.insert(j);
                }
            }
        }

        // make `to_delete` contain actual tokens
        std::set<uint32_t> to_delete_buf;
        std::transform(
            to_delete.cbegin(), to_delete.cend(),
            std::inserter(to_delete_buf, to_delete_buf.end()),
            [&buf](const auto& element) { return buf[element].first; });
        to_delete = std::move(to_delete_buf);

        // reverse `buf`
        std::reverse(buf.begin(), buf.end());

        // create mapping between old tokens and new tokens
        std::map<uint32_t, uint32_t> transformer;
        std::generate_n(std::inserter(transformer, transformer.end()),
                        this->alphabet_size,
                        [i = -1]() mutable -> std::pair<uint32_t, uint32_t> {
                            i++;
                            return {i, i};
                        });
        std::generate_n(
            std::inserter(transformer, transformer.end()),
            buf.size() - to_delete.size(),
            [&buf, &to_delete, this, i = -1,
             offset = 0]() mutable -> std::pair<uint32_t, uint32_t> {
                i++;
                while (to_delete.contains(buf[i + offset].first)) offset++;
                return {buf[i + offset].first, this->alphabet_size + i};
            });

        // drop weights for deleted tokens
        std::map<uint32_t, double> tokens_weights;
        std::transform(
            std::next(transformer.cbegin(), this->alphabet_size),
            transformer.cend(),
            std::inserter(tokens_weights, tokens_weights.end()),
            [this](const auto& mapper) -> std::pair<uint32_t, double> {
                return {mapper.second, this->tokens_weights[mapper.first]};
            });
        this->tokens_weights = std::move(tokens_weights);

        // update backward mapper
        std::map<uint32_t, std::vector<uint32_t>> tokens_backward_mapper;
        std::transform(
            std::next(transformer.cbegin(), this->alphabet_size),
            transformer.cend(),
            std::inserter(tokens_backward_mapper, tokens_backward_mapper.end()),
            [&, this](const auto& mapper)
                -> std::pair<uint32_t, std::vector<uint32_t>> {
                const auto& old_sequence =
                    this->tokens_backward_mapper[mapper.first];
                std::vector<uint32_t> new_sequence;
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
        std::vector<uint32_t>& vec,
        const std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>>&
            sub) {
        // two pointers
        size_t left = 0, right = 0;
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

    /// @brief Convert document of `DocType` to vector of base tokens.
    /// @param doc Document, i.e. data of type `DocType`.
    /// @return Vector of base tokens.
    std::vector<uint32_t> _doc_to_vec(const DocType& doc) const {
        std::vector<uint32_t> tokens;
        tokens.reserve(doc.size());
        std::transform(
            doc.cbegin(), doc.cend(), std::back_inserter(tokens),
            [this](const auto& element) { return this->alphabet.at(element); });
        return tokens;
    }

    /// @brief Convert vector of base tokens to document of `DocType`.
    /// @param tokens Vector of base tokens.
    /// @return Document, i.e. data of type `DocType`.
    DocType _vec_to_doc(const std::vector<uint32_t>& tokens) const {
        DocType doc;
        doc.reserve(tokens.size());
        std::transform(tokens.cbegin(), tokens.cend(), std::back_inserter(doc),
                       [this](const auto& token) {
                           return this->inverse_alphabet.at(token);
                       });
        return doc;
    }

   public:
    UbpeBase(uint32_t n_tokens, uint32_t alphabet_size)
        requires std::convertible_to<uint32_t, TokenType>
        : n_tokens(n_tokens), alphabet_size(alphabet_size) {
        std::generate_n(std::inserter(this->alphabet, this->alphabet.end()),
                        alphabet_size,
                        [i = -1]() mutable -> std::pair<TokenType, uint32_t> {
                            i++;
                            return {i, i};
                        });
        std::generate_n(
            std::inserter(this->inverse_alphabet, this->inverse_alphabet.end()),
            alphabet_size,
            [i = -1]() mutable -> std::pair<uint32_t, TokenType> {
                i++;
                return {i, i};
            });
    }

    UbpeBase(uint32_t n_tokens, uint32_t alphabet_size,
             std::map<TokenType, uint32_t> alphabet)
        : n_tokens(n_tokens), alphabet_size(alphabet_size) {
        assert((alphabet_size == alphabet.size()) &&
               "Provided `alphabet` should be of size `alphabet_size`.");
        this->alphabet = alphabet;
        std::transform(
            alphabet.cbegin(), alphabet.cend(),
            std::inserter(this->inverse_alphabet, this->inverse_alphabet.end()),
            [](const auto& element) -> std::pair<uint32_t, uint32_t> {
                return {element.second, element.first};
            });
    }

    UbpeBase(uint32_t n_tokens, uint32_t alphabet_size,
             std::map<TokenType, uint32_t> alphabet,
             std::map<uint32_t, TokenType> inverse_alphabet,
             std::map<std::vector<uint32_t>, uint32_t> tokens_forward_mapper,
             std::map<uint32_t, std::vector<uint32_t>> tokens_backward_mapper,
             std::map<uint32_t, double> tokens_weights)
        : n_tokens(n_tokens),
          alphabet_size(alphabet_size),
          alphabet(alphabet),
          inverse_alphabet(inverse_alphabet),
          tokens_forward_mapper(tokens_forward_mapper),
          tokens_backward_mapper(tokens_backward_mapper),
          tokens_weights(tokens_weights) {
        assert((alphabet_size == alphabet.size()) &&
               "Provided `alphabet` should be of size `alphabet_size`.");
        assert((alphabet.size() == inverse_alphabet.size()) &&
               "`alphabet` and `inverse_alphabet` should be of the same size.");
    }

    UbpeBase(const UbpeBase&) = default;
    UbpeBase(UbpeBase&&) = default;
    UbpeBase& operator=(const UbpeBase&) = default;
    UbpeBase& operator=(UbpeBase&&) = default;
    virtual ~UbpeBase() = default;

    /// @brief Get forward mapper for dumping.
    /// @return `this.tokens_forward_mapper`
    std::map<std::vector<uint32_t>, uint32_t> getForwardMapper() const {
        return this->tokens_forward_mapper;
    }

    /// @brief Get backward mapper for dumping.
    /// @return `this.tokens_backward_mapper`
    std::map<uint32_t, std::vector<uint32_t>> getBackwardMapper() const {
        return this->tokens_backward_mapper;
    }

    /// @brief Get token weighs.
    /// @return `this.tokens_weights`
    std::map<uint32_t, double> getTokensWeights() const {
        return this->tokens_weights;
    }

    /// @brief Get alphabet mapping.
    /// @return Base alphabet mapping.
    std::map<TokenType, uint32_t> getAlphabet() const { return this->alphabet; }

    /// @brief Get inverse alphabet mapping.
    /// @return Inverse abse alphabet mapping.
    std::map<uint32_t, TokenType> getInverseAlphabet() const {
        return this->inverse_alphabet;
    }

    /// @brief Fit tokenizer with `corpus`.
    /// @param docs Data to fit tokenizer with.
    /// @param n_candidates Number of most popular pairs of adjacent tokens to
    /// be substituted with new ones; ignored in `UbpeClassic`.
    /// @param rearrange_tokens If tokens should be rearranged to make tokens
    /// with smaller numbers be more valueable.
    /// @param quiet Whether to suppress logging.
    virtual void fit(const std::vector<DocType>&, uint32_t = 50,
                     bool = true, bool = false) = 0;

    /// @brief Encode `document` with fitted tokenizer.
    /// @param document Sequence of basic tokens to encode.
    /// @param top_n How many candidate ecoding to return; ignored in
    /// `UbpeClassic`.
    /// @return List of encoded documents with weights.
    virtual std::vector<std::pair<std::vector<uint32_t>, double>> encode(
        const DocType&, uint8_t = 1) const = 0;

    /// @brief Decode a vector of `tokens` with the fitted tokenizer.
    /// @param tokens An encoded sequence of tokens to decode.
    /// @return Decoded document.
    virtual DocType decode(const std::vector<uint32_t>&) const = 0;
};

}  // namespace ubpe

#endif  // UBPE_BASE_CPP
