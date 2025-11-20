#include "ubpe_base.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <set>
#include <utility>
#include <vector>

namespace ubpe {

UbpeBase::UbpeBase(uint32_t n_tokens, uint32_t alphabet_size)
    : n_tokens(n_tokens), alphabet_size(alphabet_size) {
    std::generate_n(
        std::inserter(this->alphabet, this->alphabet.end()), alphabet_size,
        [i = 0]() mutable -> std::pair<uint32_t, uint32_t> { return {i, i}; });
    std::generate_n(
        std::inserter(this->inverse_alphabet, this->inverse_alphabet.end()),
        alphabet_size,
        [i = 0]() mutable -> std::pair<uint32_t, uint32_t> { return {i, i}; });
}

UbpeBase::UbpeBase(uint32_t n_tokens, uint32_t alphabet_size,
                   std::map<uint32_t, uint32_t> alphabet)
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

UbpeBase::UbpeBase(
    uint32_t n_tokens, uint32_t alphabet_size,
    std::map<uint32_t, uint32_t> alphabet,
    std::map<uint32_t, uint32_t> inverse_alphabet,
    std::map<std::vector<uint32_t>, uint32_t> tokens_forward_mapper,
    std::map<uint32_t, std::vector<uint32_t>> tokens_backward_mapper,
    std::map<uint32_t, float> tokens_weights)
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

void UbpeBase::_rearrange_tokens_by_weight() {
    assert((this->tokens_forward_mapper.size() == 0 ||
            this->tokens_backward_mapper.size() == 0 ||
            this->tokens_weights.size() == 0) &&
           "Can not rearrange non-fitted tokenizer");

    // buffer to sort by weight and eliminate some of them
    std::vector<std::pair<uint32_t, std::vector<uint32_t>>> buf(
        this->tokens_backward_mapper.cbegin(),
        this->tokens_backward_mapper.cend());

    // sort buffer
    std::sort(buf.begin(), buf.end(), [this](const auto& a, const auto& b) {
        return tokens_weights[a.first] < tokens_weights[b.first];
    });

    auto to_delete_quantity =
        this->tokens_weights.size() - this->n_tokens + this->alphabet_size;
    std::set<uint32_t> to_delete;

    for (uint32_t i = 0; i < buf.size(); i++) {
        // skip if `i` is already pended for deletion
        if (to_delete.contains(i)) continue;

        // if all values for deletion are already found
        if (to_delete.size() >= to_delete_quantity) break;

        to_delete.insert(i);

        // check some rare condition
        for (uint32_t j = i + 1; j < buf.size(); j++) {
            if (auto it = std::find(buf[j].second.begin(), buf[j].second.end(),
                                    buf[i].first);
                it != buf[j].second.end()) {
                to_delete.insert(j);
            }
        }
    }

    // make `to_delete` contain actual tokens
    std::set<uint32_t> to_delete_buf;
    std::transform(to_delete.cbegin(), to_delete.cend(),
                   std::inserter(to_delete_buf, to_delete_buf.end()),
                   [&buf](const auto& element) { return buf[element].first; });
    to_delete = std::move(to_delete_buf);

    // reverse `buf`
    std::reverse(buf.begin(), buf.end());

    // create mapping between old tokens and new tokens
    std::map<uint32_t, uint32_t> transformer;
    // for (uint32_t i = 0; i < this->alphabet_size; i++) {
    //     transformer[i] = i;
    // }
    std::generate_n(std::inserter(transformer, transformer.end()),
                    this->alphabet_size,
                    [i = -1]() mutable -> std::pair<uint32_t, uint32_t> {
                        i++;
                        return {i, i};
                    });
    // for (uint32_t i = 0; i < buf.size(); i++) {
    //     transformer[buf[i].first] = this->alphabet_size + i;
    // }
    std::generate_n(
        std::inserter(transformer, transformer.end()), buf.size(),
        [&buf, this, i = -1]() mutable -> std::pair<uint32_t, uint32_t> {
            i++;
            return {buf[i].first, alphabet_size + i};
        });

    // drop weights for deleted tokens
    std::erase_if(this->tokens_weights, [&to_delete](const auto& element) {
        return to_delete.contains(element.first);
    });

    // update backward mapper
    std::map<uint32_t, std::vector<uint32_t>> tokens_backward_mapper;
    std::transform(
        this->tokens_backward_mapper.cbegin(),
        this->tokens_backward_mapper.cend(),
        std::inserter(tokens_backward_mapper, tokens_backward_mapper.end()),
        [&](const auto& element) -> std::pair<uint32_t, std::vector<uint32_t>> {
            std::vector<uint32_t> new_sequence;
            new_sequence.reserve(element.second.size());
            std::transform(element.second.cbegin(), element.second.cend(),
                           std::back_inserter(new_sequence),
                           [&](const auto& el) { return transformer.at(el); });
            return {transformer.at(element.first), new_sequence};
        });
    this->tokens_backward_mapper = std::move(tokens_backward_mapper);

    // update forward mapper
    std::map<std::vector<uint32_t>, uint32_t> tokens_forward_mapper;
    std::transform(
        this->tokens_backward_mapper.cbegin(),
        this->tokens_backward_mapper.cend(),
        std::inserter(tokens_forward_mapper, tokens_forward_mapper.end()),
        [](const auto& element) -> std::pair<std::vector<uint32_t>, uint32_t> {
            return {element.second, element.first};
        });
    this->tokens_forward_mapper = std::move(tokens_forward_mapper);
}

std::map<std::vector<uint32_t>, uint32_t> UbpeBase::getForwardMapper() const {
    return this->tokens_forward_mapper;
}

std::map<uint32_t, std::vector<uint32_t>> UbpeBase::getBackwardMapper() const {
    return this->tokens_backward_mapper;
}

std::map<uint32_t, float> UbpeBase::getTokensWeights() const {
    return this->tokens_weights;
}

std::map<uint32_t, uint32_t> UbpeBase::getAlphabet() const {
    return this->alphabet;
}

std::map<uint32_t, uint32_t> UbpeBase::getInverseAlphabet() const {
    return this->inverse_alphabet;
}
}  // namespace ubpe
