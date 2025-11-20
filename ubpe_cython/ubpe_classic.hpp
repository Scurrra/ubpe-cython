#ifndef UBPE_CLASSIC_CPP
#define UBPE_CLASSIC_CPP

#include "ubpe_base.hpp"

namespace ubpe {

/// Classic implementation of Byte-Pair Encoding, but for general sequences.
class UbpeClassic : protected UbpeBase {
   private:
    std::vector<std::vector<uint32_t>> pairs;

   public:
    UbpeClassic(uint32_t, uint32_t);
    UbpeClassic(uint32_t, uint32_t, std::map<uint32_t, uint32_t>);
    UbpeClassic(uint32_t, uint32_t, std::map<uint32_t, uint32_t>,
                std::map<uint32_t, uint32_t>,
                std::map<std::vector<uint32_t>, uint32_t>,
                std::map<uint32_t, std::vector<uint32_t>>,
                std::map<uint32_t, float>);
    UbpeClassic(const UbpeClassic&) = default;
    UbpeClassic(UbpeClassic&&) = default;
    UbpeClassic& operator=(const UbpeClassic&) = default;
    UbpeClassic& operator=(UbpeClassic&&) = default;
    ~UbpeClassic() = default;

    void fit(std::vector<std::vector<uint32_t>>, uint32_t = 50,
             bool = true) override;

    std::vector<std::pair<std::vector<uint32_t>, float>> encode(
        std::vector<uint32_t>, uint8_t = 1) const override;

    std::vector<uint32_t> decode(std::vector<uint32_t>) const override;
};

}  // namespace ubpe

#endif  // UBPE_CLASSIC_CPP