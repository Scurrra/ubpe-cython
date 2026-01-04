#ifndef UBPE_CLASSIC_CPP
#define UBPE_CLASSIC_CPP

#include <cmath>
#include <cstddef>
#include <numeric>

#include "counter.hpp"
#include "pair_counter.hpp"
#include "ubpe_base.hpp"

namespace ubpe {

/// Classic implementation of Byte-Pair Encoding, but for general sequences.
template <DocumentT DocType, typename TokenType = typename DocType::value_type>
class UbpeClassic : public UbpeBase<DocType, TokenType> {
   private:
    std::vector<std::vector<uint32_t>> pairs;

   public:
    UbpeClassic(uint32_t n_tokens, uint32_t alphabet_size)
        : UbpeBase<DocType, TokenType>(n_tokens, alphabet_size) {}

    UbpeClassic(uint32_t n_tokens, uint32_t alphabet_size,
                std::map<TokenType, uint32_t> alphabet)
        : UbpeBase<DocType, TokenType>(n_tokens, alphabet_size, alphabet) {}

    UbpeClassic(
        uint32_t n_tokens, uint32_t alphabet_size,
        std::map<TokenType, uint32_t> alphabet,
        std::map<uint32_t, TokenType> inverse_alphabet,
        std::map<std::vector<uint32_t>, uint32_t> tokens_forward_mapper,
        std::map<uint32_t, std::vector<uint32_t>> tokens_backward_mapper,
        std::map<uint32_t, double> tokens_weights)
        : UbpeBase<DocType, TokenType>(n_tokens, alphabet_size, alphabet,
                                       inverse_alphabet, tokens_forward_mapper,
                                       tokens_backward_mapper, tokens_weights) {
        std::transform(tokens_backward_mapper.cbegin(),
                       tokens_backward_mapper.cend(),
                       std::back_inserter(this->pairs),
                       [](const auto& element) { return element.second; });
    }

    UbpeClassic(const UbpeClassic&) = default;
    UbpeClassic(UbpeClassic&&) = default;
    UbpeClassic& operator=(const UbpeClassic&) = default;
    UbpeClassic& operator=(UbpeClassic&&) = default;
    ~UbpeClassic() = default;

    void fit(const std::vector<DocType>& corpus, uint32_t n_candidates = 50,
             bool rearrange_tokens = true) override {
        assert((n_candidates > 0) && "`n_candidates` should not be 0");
        auto max_token = this->alphabet_size - 1;

        std::vector<std::vector<uint32_t>> _corpus;
        _corpus.reserve(corpus.size());
        std::transform(
            corpus.cbegin(), corpus.cend(), std::back_inserter(_corpus),
            [this](const auto& doc) { return this->_doc_to_vec(doc); });

        // recursively fit tokenizer with `corpus`
        while (max_token < this->n_tokens) {
            // find number of occurences of each pair of adjacent tokens
            auto pairs_counter = PairCounter<uint32_t>(_corpus);
            // find most frequent bytepairs, a.k.a. candidates
            auto mc = pairs_counter.most_common(n_candidates);
            if (mc.size() == 0) break;

            // find a banch of new tokens
            // first candidate is always added
            std::vector<std::pair<std::pair<uint32_t, uint32_t>, size_t>>
                token_pairs = {mc[0]};
            // all substituted tokens must be distinct,
            // and `current_set` tracks these tokens
            std::set<uint32_t> current_set = {mc[0].first.first,
                                              mc[0].first.second};

            // check each of top candidates from the second one
            for (size_t i = 1; i < mc.size(); i++) {
                const auto& [pair2, freq2] = mc[i];

                if (current_set.contains(pair2.first) ||
                    current_set.contains(pair2.second)) {
                    continue;
                }
                // check that border pairs are not better
                auto good_to_add = true;
                for (const auto& [pair1, _] : token_pairs) {
                    good_to_add =
                        pairs_counter({pair2.second, pair1.first}).second <
                            freq2 &&
                        pairs_counter({pair1.second, pair2.first}).second <
                            freq2;

                    if (!good_to_add) break;
                }
                // finally add candidate if it is good
                if (good_to_add) {
                    token_pairs.emplace_back(std::make_pair(pair2, freq2));
                    current_set.insert({pair2.first, pair2.second});
                }
            }

            // add new pair mapping
            std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> sub;
            for (const auto& [pair, _] : token_pairs) {
                max_token++;
                this->tokens_weights[max_token] =
                    std::log(static_cast<double>(1 + corpus.size()) /
                             (1 + pairs_counter(pair).first));
                this->tokens_backward_mapper[max_token] = {pair.first,
                                                           pair.second};
                sub[pair.first] = {pair.second, max_token};
            }

            // update `corpus` with new tokens
            std::for_each(_corpus.begin(), _corpus.end(),
                          [this, &sub](auto& doc) {
                              this->_replace_token_pairs(doc, sub);
                          });
        }

        // rearrange fitted tokens
        if (rearrange_tokens) {
            this->_rearrange_tokens_by_weight();
        }

        std::transform(this->tokens_backward_mapper.cbegin(),
                       this->tokens_backward_mapper.cend(),
                       std::inserter(this->tokens_forward_mapper,
                                     this->tokens_forward_mapper.end()),
                       [](const auto& mapper)
                           -> std::pair<std::vector<uint32_t>, uint32_t> {
                           return {mapper.second, mapper.first};
                       });

        // cache pairs of tokens for encoding
        std::transform(this->tokens_backward_mapper.cbegin(),
                       this->tokens_backward_mapper.cend(),
                       std::back_inserter(this->pairs),
                       [](const auto& element) { return element.second; });
    }

    std::vector<std::pair<std::vector<uint32_t>, double>> encode(
        const DocType& doc, uint8_t = 1) const override {
        assert((this->pairs.size() != 0) && "Tokenizer was not fitted");
        assert((this->tokens_forward_mapper.size() != 0 &&
                this->tokens_backward_mapper.size() != 0 &&
                this->tokens_weights.size() != 0) &&
               "Can not rearrange non-fitted tokenizer");

        // handle empty sequence
        if (doc.size() == 0) return {};

        std::vector<uint32_t> _doc = this->_doc_to_vec(doc);

        // recursively encode
        while (true) {
            // generate adjacent pairs in `doc`
            std::set<std::vector<uint32_t>> pairs;
            std::transform(_doc.cbegin(), _doc.cend() - 1, _doc.cbegin() + 1,
                           std::inserter(pairs, pairs.end()),
                           [](const auto& left,
                              const auto& right) -> std::vector<uint32_t> {
                               return {left, right};
                           });

            // find the first most valueable pair of tokens in the `doc`
            size_t i = 0;
            while (i < this->pairs.size() && !pairs.contains(this->pairs[i]))
                i++;

            // if `i` is out of bounds, encoding is completed
            if (i == this->pairs.size()) break;

            // pairs of old tokens to be substituted
            std::vector<std::vector<uint32_t>> tokens = {this->pairs[i]};
            // all substituted tokens must be distinct,
            // and `current_set` tracks these tokens
            std::set<uint32_t> current_set(this->pairs[i].cbegin(),
                                           this->pairs[i].cend());

            // find pairs for replacement
            for (size_t j = i + 1; j < this->pairs.size(); j++) {
                // subsequence of most valueable pairs
                if (current_set.contains(this->pairs[j][0]) ||
                    current_set.contains(this->pairs[j][1])) {
                    break;
                }
                // that can be interrapted by tokens that do not present in
                // `doc`
                if (pairs.contains(this->pairs[j])) {
                    tokens.emplace_back(this->pairs[j]);
                    current_set.insert(this->pairs[j].cbegin(),
                                       this->pairs[j].cend());
                }
            }

            std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> sub;
            std::transform(
                tokens.cbegin(), tokens.cend(), std::inserter(sub, sub.end()),
                [this](const auto& token)
                    -> std::pair<uint32_t, std::pair<uint32_t, uint32_t>> {
                    return {token[0],
                            {token[1], this->tokens_forward_mapper.at(token)}};
                });

            this->_replace_token_pairs(_doc, sub);
        }

        // compute weight of encoded `doc`
        auto counter = Counter<uint32_t>(_doc);
        double weight = std::accumulate(
            counter.cbegin(), counter.cend(), 0.0f,
            [this](auto total, auto& element) {
                return total + (this->tokens_weights.contains(element.first)
                                    ? (1 + std::log(element.second)) *
                                          this->tokens_weights.at(element.first)
                                    : 0.0);
            });

        return {{_doc, weight}};
    }

    DocType decode(const std::vector<uint32_t>& tokens) const override {
        assert((this->tokens_forward_mapper.size() != 0 &&
                this->tokens_backward_mapper.size() != 0 &&
                this->tokens_weights.size() != 0) &&
               "Can not rearrange non-fitted tokenizer");

        // handle empty sequence
        if (tokens.size() == 0) return {};

        // future decoded sequence
        std::vector<uint32_t> document;

        // for each token
        for (size_t ti = 0, di = 0; ti < tokens.size(); ti++) {
            // append a new token
            document.emplace_back(tokens[ti]);

            // recursively expand new token
            // at the end of the while loop `di` will point at the next inserted
            // token
            while (di < document.size()) {
                // if current token can be substituded with a pair of tokens
                if (this->tokens_backward_mapper.contains(document[di])) {
                    // according to algorithm, it's a vector of size 2
                    auto expander =
                        this->tokens_backward_mapper.at(document[di]);
                    // reassign token with the first in the substitution pair
                    document[di] = expander[0];
                    // and insert the second to the right
                    document.emplace(document.begin() + di + 1, expander[1]);
                    // `di` is not changed, so it point's to the first token in
                    // the substitution pair and it will be checked at the next
                    // iteration
                } else {
                    // `document[di]` is in alphabet as it can not be
                    // substituted so go to the next one
                    di++;
                }
            }
        }

        return this->_vec_to_doc(document);
    }
};

}  // namespace ubpe

#endif  // UBPE_CLASSIC_CPP