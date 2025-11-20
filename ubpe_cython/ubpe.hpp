#ifndef UBPE_CPP
#define UBPE_CPP

#include <cstdint>
#include <vector>

#include "ssstree.hpp"
#include "ubpe_base.hpp"

namespace ubpe {

/// Universal Byte-Pair Encoding, that provides many options of encodings for
/// the document.
class Ubpe : protected UbpeBase {
   private:
    SSSTree<std::vector<uint32_t>, uint32_t> lookup;

   public:
    Ubpe(uint32_t, uint32_t);
    Ubpe(uint32_t, uint32_t, std::map<uint32_t, uint32_t>);
    Ubpe(uint32_t, uint32_t, std::map<uint32_t, uint32_t>,
         std::map<uint32_t, uint32_t>,
         std::map<std::vector<uint32_t>, uint32_t>,
         std::map<uint32_t, std::vector<uint32_t>>, std::map<uint32_t, float>);
    Ubpe(const Ubpe&) = default;
    Ubpe(Ubpe&&) = default;
    Ubpe& operator=(const Ubpe&) = default;
    Ubpe& operator=(Ubpe&&) = default;
    ~Ubpe() = default;

    void fit(std::vector<std::vector<uint32_t>>, uint32_t = 50,
             bool = true) override;

    std::vector<std::pair<std::vector<uint32_t>, float>> encode(
        std::vector<uint32_t>, uint8_t = 1) const override;

    std::vector<uint32_t> decode(std::vector<uint32_t>) const override;
};

}  // namespace ubpe

#endif  // UBPE_CPP