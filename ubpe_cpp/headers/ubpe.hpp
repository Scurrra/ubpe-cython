#ifndef UBPE_CPP
#define UBPE_CPP

#include <cmath>
#include <cstddef>
#include <map>
#include <numeric>
#include <print>
#include <stack>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "counter.hpp"
#include "pair_counter.hpp"
#include "ssstree.hpp"
#include "ubpe_base.hpp"

namespace ubpe {

/// Universal Byte-Pair Encoding, that provides many options of encodings for
/// the document.
template <DocumentT DocType, typename TokenType = DocType::value_type>
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
         std::map<uint32_t, float> tokens_weights)
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

            // find a banch of new tokens
            // first candidate is always added
            std::vector<std::pair<std::pair<uint32_t, uint32_t>, size_t>>
                token_pairs = {mc[0]};
            // all substituted tokens must be distinct,
            // and `current_set` tracks these tokens
            std::set<uint32_t> current_set = {mc[0].first.first,
                                              mc[0].first.second};

            // check each of top candidates
            for (const auto& [pair2, freq2] : mc) {
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
                    std::log(static_cast<float>(1 + corpus.size()) /
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
                this->tokens_forward_mapper[tokens_map] = max_token;
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

        // cache lookup of tokens for encoding
        for (const auto& [key, value] : this->inverse_alphabet) {
            auto _ = this->lookup +
                     std::make_pair(std::vector<uint32_t>{key}, value);
        }
        for (const auto& element : this->tokens_forward_mapper) {
            auto _ = this->lookup + element;
        }
    }

    std::vector<std::pair<std::vector<uint32_t>, float>> encode(
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

        std::println("Initial stacks: {} : {}", stacks, stacks.size());

        // build nodes
        std::map<size_t,
                 std::map<std::vector<uint32_t>, std::pair<uint32_t, size_t>>>
            nodes;
        std::println("Nodes: {}", nodes.size());
        // check all stacks form the end of `doc`
        while (!stacks.empty()) {
            // extract the top stack
            auto [start, stack] = stacks.top();
            stacks.pop();
            
            std::println("Start: {} :: Stack: {}", start, stack);

            // map which points keys (sequences of basic tokens) to pairs of
            // values and starts of following tokens
            std::map<std::vector<uint32_t>, std::pair<uint32_t, size_t>> next;
            // process each candidate
            for (const auto& [key, value] : stack) {
                // compute the start of the following token
                auto next_key_start = start + key.size();
                // add candidate to the map
                next[key] = std::make_pair(value, next_key_start);
                std::println("Key: {} => {} : {}", key, value, next_key_start);
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

            std::println("Node from {} size = {}", start, next.size());
        }

        std::println("Nodes: {}", nodes.size());
        if (nodes.contains(_doc.size())) {
            std::println("`nodes` contains the `doc`'s end");
        }
        std::println("Doc size: {}", _doc.size());
        

        // map that points start position to up to `top_n` candidate tails
        std::map<size_t, std::vector<std::tuple<float, std::vector<uint32_t>,
                                                Counter<uint32_t>>>>
            tails;
        // initialize a tail that is after the end of `doc`, that has zero
        // weight, is an empty sequence, and counts of it's tokens are zeros
        tails[_doc.size()] = {
            std::make_tuple(0.0, std::vector<uint32_t>{}, Counter<uint32_t>())};
        std::println("Initial tails: {}", tails);
        // form the end of `doc`
        for (int start = _doc.size() - 1; start >= 0; start--) {
            // all candidates from `start`
            std::vector<
                std::tuple<float, std::vector<uint32_t>, Counter<uint32_t>>>
                buf;
            // for each subsequence from `start`
            for (const auto& [_, node] : nodes[start]) {
                const auto& [token, next_start] = node;
                std::println("Token: {}, Next start = {}", token, next_start);
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
                    float buf_weight = std::accumulate(
                        buf_counter.cbegin(), buf_counter.cend(), 0.0,
                        [this](auto total, const auto& element) {
                            return total +
                                   (this->tokens_weights.contains(element.first)
                                        ? (1 + std::log(element.second)) *
                                              this->tokens_weights.at(
                                                  element.first)
                                        : 0.0);
                        });
                    // add a new candidate
                    buf.emplace_back(
                        std::make_tuple(buf_weight, buf_element, buf_counter));
                    std::println("Last buf: {}", buf.back());
                }
            }
            // max number of tails to add
            // almost always is `top_n` except of rare cases at ends of `doc`
            auto buf_n = top_n < buf.size() ? top_n : buf.size();
            // sort all tails by weight
            std::sort(buf.begin(), buf.end(), [](const auto& a, const auto& b) {
                return std::get<0>(a) > std::get<0>(b);
            });
            // add top candidates to `tails`

            tails[start] = std::vector(buf.cbegin(), buf.cbegin() + buf_n);
            std::println("Tails after {}: {}", start, tails);
        }

        // prepare result container
        std::vector<std::pair<std::vector<uint32_t>, float>> candidates;
        candidates.reserve(tails[0].size());
        // remove counter
        std::transform(
            tails[0].cbegin(), tails[0].cend(),
            std::back_inserter(candidates),
            [](const auto& element) -> std::pair<std::vector<uint32_t>, float> {
                return {std::get<1>(element), std::get<0>(element)};
            });
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

    // std::map<uint32_t, std::vector<uint32_t>> getBackwardMapper() const;
    // std::map<uint32_t, float> getTokensWeights() const;
    // std::map<TokenType, uint32_t> getAlphabet() const;
};

}  // namespace ubpe

#endif  // UBPE_CPP