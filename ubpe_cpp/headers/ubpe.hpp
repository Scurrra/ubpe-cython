#ifndef UBPE_CPP
#define UBPE_CPP

#include <cstdint>
#include <vector>

#include "../include/ssstree.hpp"
#include "ubpe_base.hpp"

namespace ubpe {

/// Universal Byte-Pair Encoding, that provides many options of encodings for
/// the document.
template <DocumentT T>
class Ubpe : protected UbpeBase<T> {
   private:
    SSSTree<std::vector<uint32_t>, uint32_t> lookup;

   public:
    using TokenType = typename Ubpe<T>::TokenType;
    using DocType = typename Ubpe<T>::DocType;

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
};

}  // namespace ubpe

#endif  // UBPE_CPP