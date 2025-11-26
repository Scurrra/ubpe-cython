#ifndef UBPE_BASE_CPP
#define UBPE_BASE_CPP

#include <cstdint>
#include <map>
#include <type_traits>
#include <vector>

#include "../include/utils.hpp"

namespace ubpe {

template <DocumentT T>
class UbpeBase {
   protected:
    using DocType = typename std::remove_cvref_t<T>;
    using TokenType = typename DocType::value_type;

    uint32_t n_tokens;
    uint32_t alphabet_size;

    std::map<TokenType, uint32_t> alphabet;
    std::map<uint32_t, TokenType> inverse_alphabet;

    std::map<std::vector<uint32_t>, uint32_t> tokens_forward_mapper;
    std::map<uint32_t, std::vector<uint32_t>> tokens_backward_mapper;

    std::map<uint32_t, float> tokens_weights;

    /// @brief Function that rearranges found tokens according to their weights
    /// and trims dictionary of the tokenizer to be not greater than
    /// `this.n_tokens`.
    void _rearrange_tokens_by_weight();

    /// @brief Convert document of `Ubpe<T>::DocType` to vector of base tokens.
    /// @param doc Document, i.e. data of type `Ubpe<T>::DocType`.
    /// @return Vector of base tokens.
    std::vector<uint32_t> _doc_to_vec(const DocType& doc);

    /// @brief Convert vector of base tokens to document of `Ubpe<T>::DocType`.
    /// @param tokens Vector of base tokens.
    /// @return Document, i.e. data of type `Ubpe<T>::DocType`.
    DocType _vec_to_doc(const std::vector<uint32_t>& tokens);

   public:
    UbpeBase(uint32_t, uint32_t)
        requires std::convertible_to<uint32_t, TokenType>;
    UbpeBase(uint32_t, uint32_t, std::map<TokenType, uint32_t>);
    UbpeBase(uint32_t, uint32_t, std::map<TokenType, uint32_t>,
             std::map<uint32_t, TokenType>,
             std::map<std::vector<uint32_t>, uint32_t>,
             std::map<uint32_t, std::vector<uint32_t>>,
             std::map<uint32_t, float>);
    UbpeBase(const UbpeBase&) = default;
    UbpeBase(UbpeBase&&) = default;
    UbpeBase& operator=(const UbpeBase&) = default;
    UbpeBase& operator=(UbpeBase&&) = default;
    ~UbpeBase() = default;

    /// @brief Get forward mapper for dumping.
    /// @return `this.tokens_forward_mapper`
    std::map<std::vector<uint32_t>, uint32_t> getForwardMapper() const;

    /// @brief Get backward mapper for dumping.
    /// @return `this.tokens_backward_mapper`
    std::map<uint32_t, std::vector<uint32_t>> getBackwardMapper() const;

    /// @brief Get token weighs.
    /// @return `this.tokens_weights`
    std::map<uint32_t, float> getTokensWeights() const;

    /// @brief Get alphabet mapping.
    /// @return Base alphabet mapping.
    std::map<TokenType, uint32_t> getAlphabet() const;

    /// @brief Get inverse alphabet mapping.
    /// @return Inverse abse alphabet mapping.
    std::map<uint32_t, TokenType> getInverseAlphabet() const;

    /// @brief Fit tokenizer with `corpus`.
    /// @param docs Data to fit tokenizer with.
    /// @param n_candidates Number of most popular pairs of adjacent tokens to
    /// be substituted with new ones; ignored in `UbpeClassic`.
    /// @param rearrange_tokens If tokens should be rearranged to make tokens
    /// with smaller numbers be more valueable.
    virtual void fit(const std::vector<DocType>&, uint32_t = 50,
                     bool = true) = 0;

    /// @brief Encode `document` with fitted tokenizer.
    /// @param document Sequence of basic tokens to encode.
    /// @param top_n How many candidate ecoding to return; ignored in
    /// `UbpeClassic`.
    /// @return List of encoded documents with weights.
    virtual std::vector<std::pair<std::vector<uint32_t>, float>> encode(
        const DocType&, uint8_t = 1) const = 0;

    /// @brief Decode a vector of `tokens` with the fitted tokenizer.
    /// @param tokens An encoded sequence of tokens to decode.
    /// @return Decoded document.
    virtual DocType decode(const std::vector<uint32_t>&) const = 0;
};

}  // namespace ubpe

#endif  // UBPE_BASE_CPP
