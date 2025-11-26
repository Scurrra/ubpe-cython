#include "../headers/ubpe.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <numeric>
#include <stack>
#include <tuple>
#include <utility>
#include <vector>

#include "../include/counter.hpp"
#include "../include/pair_counter.hpp"
#include "../include/utils.hpp"

namespace ubpe {

Ubpe::Ubpe(uint32_t n_tokens, uint32_t alphabet_size)
    : UbpeBase(n_tokens, alphabet_size) {}

Ubpe::Ubpe(uint32_t n_tokens, uint32_t alphabet_size,
           std::map<uint32_t, uint32_t> alphabet)
    : UbpeBase(n_tokens, alphabet_size, alphabet) {}

Ubpe::Ubpe(uint32_t n_tokens, uint32_t alphabet_size,
           std::map<uint32_t, uint32_t> alphabet,
           std::map<uint32_t, uint32_t> inverse_alphabet,
           std::map<std::vector<uint32_t>, uint32_t> tokens_forward_mapper,
           std::map<uint32_t, std::vector<uint32_t>> tokens_backward_mapper,
           std::map<uint32_t, float> tokens_weights)
    : UbpeBase(n_tokens, alphabet_size, alphabet, inverse_alphabet,
               tokens_forward_mapper, tokens_backward_mapper, tokens_weights) {
    // cache lookup of tokens for encoding
    for (const auto& [key, value] : inverse_alphabet) {
        auto _ =
            this->lookup + std::make_pair(std::vector<uint32_t>{key}, value);
    }
    for (const auto& element : tokens_forward_mapper) {
        auto _ = this->lookup + element;
    }
}

void Ubpe::fit(std::vector<std::vector<uint32_t>> corpus, uint32_t n_candidates,
               bool rearrange_tokens) {
    assert((n_candidates > 0) || "`n_candidates` should not be 0");
    auto max_token = this->alphabet_size - 1;

    // recursively fit tokenizer with `corpus`
    while (max_token < this->n_tokens) {
        // find number of occurences of each pair of adjacent tokens
        auto pairs_counter = PairCounter<uint32_t>(corpus);
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
                    pairs_counter({pair2.second, pair1.first}).second < freq2 &&
                    pairs_counter({pair1.second, pair2.first}).second < freq2;

                if (!good_to_add) break;
            }
            // finally add candidate if it is good
            if (good_to_add) {
                token_pairs.emplace_back(std::make_pair(pair2, freq2));
                current_set.insert({pair2.first, pair2.second});
            }
        }

        // merge subsequences for each pair of tokens and add it to the mapings
        std::map<uint32_t, std::pair<uint32_t, uint32_t>> sub;
        for (const auto& [pair, _] : token_pairs) {
            max_token++;
            this->tokens_weights[max_token] =
                std::log((1 + corpus.size()) / (1 + pairs_counter(pair).first));
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
        std::for_each(corpus.begin(), corpus.end(), [this, &sub](auto& doc) {
            _replace_token_pairs(doc, sub);
        });
    }

    // rearrange fitted tokens
    if (rearrange_tokens) {
        this->_rearrange_tokens_by_weight();
    }

    // cache lookup of tokens for encoding
    for (const auto& [key, value] : this->inverse_alphabet) {
        auto _ =
            this->lookup + std::make_pair(std::vector<uint32_t>{key}, value);
    }
    for (const auto& element : this->tokens_forward_mapper) {
        auto _ = this->lookup + element;
    }
}

std::vector<std::pair<std::vector<uint32_t>, float>> Ubpe::encode(
    std::vector<uint32_t> doc, uint8_t top_n) const {
    assert((this->lookup.empty()) && "Tokenizer was not fitted");
    assert((this->tokens_forward_mapper.size() == 0 ||
            this->tokens_backward_mapper.size() == 0 ||
            this->tokens_weights.size() == 0) &&
           "Can not rearrange non-fitted tokenizer");

    // handle empty sequence
    if (doc.size() == 0) return {};

    // build initial stack
    std::stack<std::pair<
        size_t, std::vector<std::pair<std::vector<uint32_t>, uint32_t>>>>
        stacks;
    // from start
    size_t start = 0;
    // while the while `doc` is being searched in
    while (start < doc.size()) {
        // try to find all key-value pairs in lookup tree where keys are
        // subsequences from `start` in `doc`, i.e. basic tokens' sequences from
        // `start` in `doc`
        auto stack = this->lookup(doc, start);
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
    while (stacks.size() != 0) {
        // extract the top stack
        const auto& [start, stack] = stacks.top();
        stacks.pop();
        // map which points keys (sequences of basic tokens) to pairs of values
        // and starts of following tokens
        std::map<std::vector<uint32_t>, std::pair<uint32_t, size_t>> next;
        // process each candidate
        for (const auto& [key, value] : stack) {
            // compute the start of the following token
            auto next_key_start = start + key.size();
            // add candidate to the map
            next[key] = std::make_pair(value, next_key_start);
            // check if the following token was already added
            if (next_key_start != doc.size() &&
                !nodes.contains(next_key_start)) {
                // add new start-stack pairs to `stacks`
                stacks.emplace(std::make_pair(
                    next_key_start, this->lookup(doc, next_key_start)));
            }
        }
        // create new node
        nodes[start] = std::move(next);
    }

    // map that points start position to up to `top_n` candidate tails
    std::map<size_t, std::vector<std::tuple<float, std::vector<uint32_t>,
                                            Counter<uint32_t>>>>
        tails;
    // initialize a tail that is after the end of `doc`, that has zero weight,
    // is an empty sequence, and counts of it's tokens are zeros
    tails[doc.size()] = {
        std::make_tuple(0.0, std::vector<uint32_t>{}, Counter<uint32_t>())};
    // form the end of `doc`
    for (auto start = doc.size(); start > 0; start--) {
        // all candidates from `start`
        std::vector<std::tuple<float, std::vector<uint32_t>, Counter<uint32_t>>>
            buf;
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
                auto buf_weight = std::accumulate(
                    buf_counter.cbegin(), buf_counter.cend(), 0.0,
                    [this](auto total, const auto& element) {
                        return total +
                               (this->tokens_weights.contains(element.first)
                                    ? (1 + std::log(element.second)) *
                                          this->tokens_weights.at(element.first)
                                    : 0.0);
                    });
                // add a new candidate
                buf.emplace_back(
                    std::make_tuple(buf_weight, buf_element, buf_counter));
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
    }

    // prepare result container
    std::vector<std::pair<std::vector<uint32_t>, float>> candidates;
    candidates.reserve(tails[0].size());
    // remove counter
    std::transform(
        tails[0].cbegin(), tails[0].cbegin(), std::back_inserter(candidates),
        [](const auto& element) -> std::pair<std::vector<uint32_t>, float> {
            return {std::get<1>(element), std::get<0>(element)};
        });
    return candidates;
}

std::vector<uint32_t> Ubpe::decode(const std::vector<uint32_t>& tokens) const {
    assert((this->tokens_forward_mapper.size() == 0 ||
            this->tokens_backward_mapper.size() == 0 ||
            this->tokens_weights.size() == 0) &&
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

    return document;
}

}  // namespace ubpe