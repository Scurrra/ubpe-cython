#ifndef UBPE_CLASSIC_CPP
#define UBPE_CLASSIC_CPP

#include "ubpe_base.hpp"

namespace ubpe {

/// Classic implementation of Byte-Pair Encoding, but for general sequences.
template <DocumentT T>
class UbpeClassic : protected UbpeBase<T> {
   private:
    std::vector<std::vector<uint32_t>> pairs;

   public:
    using TokenType = typename UbpeClassic<T>::TokenType;
    using DocType = typename UbpeClassic<T>::DocType;

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

    void fit(const std::vector<DocType>&, uint32_t = 50,
             bool = true) override;

    std::vector<std::pair<std::vector<uint32_t>, float>> encode(
        const DocType&, uint8_t = 1) const override;

    DocType decode(const std::vector<uint32_t>&) const override;
};

}  // namespace ubpe

#endif  // UBPE_CLASSIC_CPP