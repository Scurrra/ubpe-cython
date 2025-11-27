#ifndef UBPE_CLASSIC_CPP
#define UBPE_CLASSIC_CPP

#include "ubpe_base.hpp"

namespace ubpe {

/// Classic implementation of Byte-Pair Encoding, but for general sequences.
template <DocumentT DocType, typename TokenType = DocType::value_type>
class UbpeClassic : protected UbpeBase<DocType, TokenType> {
   private:
    std::vector<std::vector<uint32_t>> pairs;

   public:
    UbpeClassic(uint32_t, uint32_t);
    UbpeClassic(uint32_t, uint32_t, std::map<TokenType, uint32_t>);
    UbpeClassic(uint32_t, uint32_t, std::map<TokenType, uint32_t>,
                std::map<uint32_t, TokenType>,
                std::map<std::vector<uint32_t>, uint32_t>,
                std::map<uint32_t, std::vector<uint32_t>>,
                std::map<uint32_t, float>);
    UbpeClassic(const UbpeClassic&) = default;
    UbpeClassic(UbpeClassic&&) = default;
    UbpeClassic& operator=(const UbpeClassic&) = default;
    UbpeClassic& operator=(UbpeClassic&&) = default;
    ~UbpeClassic() = default;

    void fit(const std::vector<DocType>&, uint32_t = 50, bool = true) override;

    std::vector<std::pair<std::vector<uint32_t>, float>> encode(
        const DocType&, uint8_t = 1) const override;

    DocType decode(const std::vector<uint32_t>&) const override;

    std::map<uint32_t, std::vector<uint32_t>> getBackwardMapper() const;
    std::map<uint32_t, float> getTokensWeights() const;
    std::map<TokenType, uint32_t> getAlphabet() const;
};

}  // namespace ubpe

#endif  // UBPE_CLASSIC_CPP