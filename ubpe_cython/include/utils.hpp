#ifndef UBPE_UTILS_HPP
#define UBPE_UTILS_HPP

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace ubpe {

/// @brief Function for replacing pair of adjacent tokens in a list with a new
/// one.
/// @param vec Vector in which adjacent pairs will be replaced.
/// @param sub A substitution map, where keys are first tokens in the pairs, and
/// the values are pair of the second token and the new one wrapped in a list.
void _replace_token_pairs(
    std::vector<uint32_t>& vec,
    const std::map<uint32_t, std::pair<uint32_t, uint32_t>>& sub) {
    // two pointers starting with `-1` for a hack
    auto left = -1, right = -1;
    // while we can access element with index `right+1`
    while (right < vec.size() - 2) {
        // here is the hack: assigning anyways
        vec[++left] = vec[++right];
        // here `vec[left] == vec[right]`
        // so if `vec[right]` is not a potential start of a pair to be replaced
        // we are done for this index
        if (!sub.contains(vec[right])) continue;

        // else check `vec[right+1]`
        if (vec[right + 1] == sub.at(vec[right]).first) {
            // replace `vec[left]` with the new value
            // and move `right` forward
            vec[left] = sub.at(vec[right++]).second;
        }
    }
    // `left` is the length of a new sequence, so we just resize the old one
    vec.resize(left);
}
}  // namespace ubpe

#endif
