#ifndef UBPE_BASE_CPP
#define UBPE_BASE_CPP

#include <cstdint>
#include <map>
#include <vector>

namespace ubpe {

class UbpeBase {
   protected:
    uint32_t n_tokens;
    uint32_t alphabet_size;

    std::map<std::vector<uint32_t>, uint32_t> tokens_forward_mapper;
    std::map<uint32_t, std::vector<uint32_t>> tokens_backward_mapper;

    std::map<uint32_t, float> tokens_weights;

    /// @brief Function that rearranges found tokens according to their weights
    /// and trims dictionary of the tokenizer to be not greater than
    /// `this.n_tokens`.
    void _rearrange_tokens_by_weight();

   public:
    UbpeBase(uint32_t, uint32_t);
    UbpeBase(uint32_t, uint32_t, std::map<std::vector<uint32_t>, uint32_t>,
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
};

}  // namespace ubpe

#endif  // UBPE_BASE_CPP
