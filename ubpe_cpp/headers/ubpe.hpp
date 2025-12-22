#ifndef UBPE_CPP
#define UBPE_CPP

#include <cmath>
#include <numeric>
#include <stack>

#include "counter.hpp"
#include "pair_counter.hpp"
#include "ssstree.hpp"
#include "top_elements.hpp"
#include "ubpe_base.hpp"

namespace ubpe {

/// @brief Information about candidate to be used in selection with
/// `ubpe::TopElements`.
struct EncodingCandidate {
    double weight;
    std::vector<uint32_t> sequence;
    ubpe::Counter<uint32_t> counter;

    EncodingCandidate() = default;
    EncodingCandidate(double weight, std::vector<uint32_t> sequence,
                      ubpe::Counter<uint32_t> counter)
        : weight(weight), sequence(sequence), counter(counter) {}
    EncodingCandidate(const EncodingCandidate&) = default;
    EncodingCandidate(EncodingCandidate&&) = default;
    EncodingCandidate& operator=(const EncodingCandidate&) = default;
    EncodingCandidate& operator=(EncodingCandidate&&) = default;

    friend bool operator<(const EncodingCandidate& lhs,
                          const EncodingCandidate& rhs);
    bool operator<(const EncodingCandidate& rhs) {
        if (this->weight == rhs.weight) {
            return this->sequence.size() > rhs.sequence.size();
        }
        return this->weight < rhs.weight;
    }

    friend bool operator>(const EncodingCandidate& lhs,
                          const EncodingCandidate& rhs);
    bool operator>(const EncodingCandidate& rhs) {
        if (this->weight == rhs.weight) {
            return this->sequence.size() < rhs.sequence.size();
        }
        return this->weight > rhs.weight;
    }

    /// @brief Construct a sequence-weight pair from the candidate.
    std::pair<std::vector<uint32_t>, double> operator()() const {
        return {this->sequence, this->weight};
    }
};

bool operator<(const EncodingCandidate& lhs, const EncodingCandidate& rhs) {
    if (lhs.weight == rhs.weight) {
        return lhs.sequence.size() > rhs.sequence.size();
    }
    return lhs.weight < rhs.weight;
}

bool operator>(const EncodingCandidate& lhs, const EncodingCandidate& rhs) {
    if (lhs.weight == rhs.weight) {
        return lhs.sequence.size() < rhs.sequence.size();
    }
    return lhs.weight > rhs.weight;
}

/// Universal Byte-Pair Encoding, that provides many options of encodings for
/// the document.
template <DocumentT DocType, typename TokenType = typename DocType::value_type>
class Ubpe : public UbpeBase<DocType, TokenType> {
   private:
    SSSTree<std::vector<uint32_t>, uint32_t> lookup;

   public:
    Ubpe(uint32_t n_tokens, uint32_t alphabet_size)
        : UbpeBase<DocType, TokenType>(n_tokens, alphabet_size) {}

    Ubpe(uint32_t n_tokens, uint32_t alphabet_size,
         std::map<TokenType, uint32_t> alphabet)
        : UbpeBase<DocType, TokenType>(n_tokens, alphabet_size, alphabet) {}

    Ubpe(uint32_t n_tokens, uint32_t alphabet_size,
         std::map<TokenType, uint32_t> alphabet,
         std::map<uint32_t, TokenType> inverse_alphabet,
         std::map<std::vector<uint32_t>, uint32_t> tokens_forward_mapper,
         std::map<uint32_t, std::vector<uint32_t>> tokens_backward_mapper,
         std::map<uint32_t, double> tokens_weights)
        : UbpeBase<DocType, TokenType>(n_tokens, alphabet_size, alphabet,
                                       inverse_alphabet, tokens_forward_mapper,
                                       tokens_backward_mapper, tokens_weights) {
        // cache lookup of tokens for encoding
        for (const auto& [key, value] : inverse_alphabet) {
            auto _ = this->lookup +
                     std::make_pair(std::vector<uint32_t>{key}, value);
        }
        for (const auto& element : tokens_forward_mapper) {
            auto _ = this->lookup + element;
        }
    }

    Ubpe(const Ubpe&) = default;
    Ubpe(Ubpe&&) = default;
    Ubpe& operator=(const Ubpe&) = default;
    Ubpe& operator=(Ubpe&&) = default;
    ~Ubpe() = default;

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

            // merge subsequences for each pair of tokens and add it to the
            // mapings
            std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> sub;
            for (const auto& [pair, _] : token_pairs) {
                max_token++;
                this->tokens_weights[max_token] =
                    std::log(static_cast<double>(1 + corpus.size()) /
                             (1 + pairs_counter(pair).first));
                // merge subsequences
                std::vector<uint32_t> tokens_map;
                if (this->tokens_backward_mapper.contains(pair.first)) {
                    tokens_map.insert(
                        tokens_map.end(),
                        this->tokens_backward_mapper[pair.first].cbegin(),
                        this->tokens_backward_mapper[pair.first].cend());
                } else {
                    tokens_map.emplace_back(pair.first);
                }
                if (this->tokens_backward_mapper.contains(pair.second)) {
                    tokens_map.insert(
                        tokens_map.end(),
                        this->tokens_backward_mapper[pair.second].cbegin(),
                        this->tokens_backward_mapper[pair.second].cend());
                } else {
                    tokens_map.emplace_back(pair.second);
                }

                this->tokens_backward_mapper[max_token] = tokens_map;
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

        // cache lookup of tokens for encoding
        for (const auto& [key, value] : this->inverse_alphabet) {
            auto _ = this->lookup +
                     std::make_pair(std::vector<uint32_t>{key}, value);
        }
        for (const auto& element : this->tokens_forward_mapper) {
            auto _ = this->lookup + element;
        }
    }

    std::vector<std::pair<std::vector<uint32_t>, double>> encode(
        const DocType& doc, uint8_t top_n = 1) const override {
        assert((!this->lookup.empty()) && "Tokenizer was not fitted");
        assert((this->tokens_forward_mapper.size() != 0 &&
                this->tokens_backward_mapper.size() != 0 &&
                this->tokens_weights.size() != 0) &&
               "Can not rearrange non-fitted tokenizer");

        // handle empty sequence
        if (doc.size() == 0) return {};

        std::vector<uint32_t> _doc = this->_doc_to_vec(doc);

        // build initial stack
        std::stack<std::pair<
            size_t, std::vector<std::pair<std::vector<uint32_t>, uint32_t>>>>
            stacks;
        // from start
        size_t start = 0;
        // while the while `doc` is being searched in
        while (start < _doc.size()) {
            // try to find all key-value pairs in lookup tree where keys are
            // subsequences from `start` in `doc`, i.e. basic tokens' sequences
            // from `start` in `doc`
            auto stack = this->lookup(_doc, start);
            // place start-stack pair in `stacks`
            stacks.emplace(std::make_pair(start, stack));
            // next search will start after the longest key found
            start += stack.back().first.size();
        }

        // build nodes
        std::map<size_t,
                 std::map<std::vector<uint32_t>, std::pair<uint32_t, size_t>>>
            nodes;
        // check all stacks form the end of `doc`
        while (!stacks.empty()) {
            // extract the top stack
            auto [start, stack] = stacks.top();
            stacks.pop();

            // map which points keys (sequences of basic tokens) to pairs of
            // values and starts of following tokens
            std::map<std::vector<uint32_t>, std::pair<uint32_t, size_t>> next;
            // process each candidate
            for (const auto& [key, value] : stack) {
                // compute the start of the following token
                auto next_key_start = start + key.size();
                // add candidate to the map
                next[key] = std::make_pair(value, next_key_start);
                // check if the following token was already added
                if (next_key_start != _doc.size() &&
                    !nodes.contains(next_key_start)) {
                    // add new start-stack pairs to `stacks`
                    stacks.emplace(std::make_pair(
                        next_key_start, this->lookup(_doc, next_key_start)));
                }
            }
            // create new node
            nodes[start] = next;
        }

        // map that points start position to up to `top_n` candidate tails
        std::map<size_t, std::vector<EncodingCandidate>> tails;
        // initialize a tail that is after the end of `doc`, that has zero
        // weight, is an empty sequence, and counts of it's tokens are zeros
        tails[_doc.size()] = {
            {0.0, std::vector<uint32_t>{}, Counter<uint32_t>()}};
        // form the end of `doc`
        for (int start = _doc.size() - 1; start >= 0; start--) {
            // all candidates from `start`
            TopElements<EncodingCandidate> buf(top_n);
            // for each subsequence from `start`
            for (const auto& [_, node] : nodes[start]) {
                const auto& [token, next_start] = node;
                // for each tail that starts where the subsequence ends
                for (const auto& [_, tail, counter] : tails[next_start]) {
                    // new tail
                    std::vector<uint32_t> buf_element = {token};
                    buf_element.insert(buf_element.end(), tail.cbegin(),
                                       tail.cend());
                    // new counter
                    auto buf_counter = counter;
                    buf_counter[token]++;
                    // weight of the tail
                    double buf_weight = std::accumulate(
                        buf_counter.cbegin(), buf_counter.cend(), 0.0f,
                        [this](double total, const auto& element) {
                            return total +
                                   (this->tokens_weights.contains(element.first)
                                        ? (1 + std::log(element.second)) *
                                              this->tokens_weights.at(
                                                  element.first)
                                        : 0.0);
                        });
                    // add a new candidate
                    buf.push({buf_weight, buf_element, buf_counter});
                }
            }

            // add top candidates to `tails`
            tails[start] = buf.sorted();
        }

        // prepare result container
        std::vector<std::pair<std::vector<uint32_t>, double>> candidates;
        candidates.reserve(tails[0].size());
        // remove counter
        std::transform(tails[0].cbegin(), tails[0].cend(),
                       std::back_inserter(candidates),
                       [](const auto& element) { return element(); });
        return candidates;
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
        for (const auto& token : tokens) {
            // if `token` is artificial
            if (this->tokens_backward_mapper.contains(token)) {
                // append all basic tokens for this `token` to `document`
                document.insert(document.end(),
                                this->tokens_backward_mapper.at(token).cbegin(),
                                this->tokens_backward_mapper.at(token).cend());

            } else {
                // append `token` itself
                document.emplace_back(token);
            }
        }

        return this->_vec_to_doc(document);
    }
};

}  // namespace ubpe

#endif  // UBPE_CPP