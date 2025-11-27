#ifndef UBPE_CPP
#define UBPE_CPP

#include <cstdint>
#include <vector>

#include "../include/ssstree.hpp"
#include "ubpe_base.hpp"

namespace ubpe {

/// Universal Byte-Pair Encoding, that provides many options of encodings for
/// the document.
template <DocumentT DocType, typename TokenType = DocType::value_type>
class Ubpe : protected UbpeBase<DocType, TokenType> {
   private:
    SSSTree<std::vector<uint32_t>, uint32_t> lookup;

   public:
    Ubpe(uint32_t, uint32_t);
    Ubpe(uint32_t, uint32_t, std::map<TokenType, uint32_t>);
    Ubpe(uint32_t, uint32_t, std::map<TokenType, uint32_t>,
         std::map<uint32_t, TokenType>,
         std::map<std::vector<uint32_t>, uint32_t>,
         std::map<uint32_t, std::vector<uint32_t>>, std::map<uint32_t, float>);
    Ubpe(const Ubpe&) = default;
    Ubpe(Ubpe&&) = default;
    Ubpe& operator=(const Ubpe&) = default;
    Ubpe& operator=(Ubpe&&) = default;
    ~Ubpe() = default;

    void fit(const std::vector<DocType>&, uint32_t = 50, bool = true) override;

    std::vector<std::pair<std::vector<uint32_t>, float>> encode(
        const DocType&, uint8_t = 1) const override;

    DocType decode(const std::vector<uint32_t>&) const override;

    std::map<uint32_t, std::vector<uint32_t>> getBackwardMapper() const;
    std::map<uint32_t, float> getTokensWeights() const;
    std::map<TokenType, uint32_t> getAlphabet() const;
};

}  // namespace ubpe

#endif  // UBPE_CPP