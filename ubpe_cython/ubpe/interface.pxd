from libc.stdint cimport uint32_t, uint8_t
from libcpp.map cimport map
from libcpp.optional cimport optional
from libcpp.vector cimport vector
from libcpp.pair cimport pair
from libcpp.set cimport set as cpp_set

# UBPE Classic
cdef extern from "ubpe_classic.hpp" namespace "ubpe":
    cdef cppclass UbpeClassic[DocType, TokenType]:
        UbpeClassic(uint32_t n_tokens,
            map[TokenType, uint32_t] alphabet) except +
        UbpeClassic(uint32_t n_tokens,
            map[TokenType, uint32_t] alphabet,
            optional[map[DocType, uint32_t]] known_words,
            optional[cpp_set[TokenType]] break_tokens,
            optional[cpp_set[TokenType]] stop_tokens) except +
        UbpeClassic(uint32_t n_tokens,
            map[TokenType, uint32_t] alphabet,
            map[uint32_t, TokenType] inverse_alphabet,
            map[vector[uint32_t], uint32_t] tokens_forward_mapper,
            map[uint32_t, vector[uint32_t]] tokens_backward_mapper,
            map[uint32_t, double] tokens_weights) except +
        UbpeClassic(uint32_t n_tokens,
            map[TokenType, uint32_t] alphabet,
            map[uint32_t, TokenType] inverse_alphabet,
            map[vector[uint32_t], uint32_t] tokens_forward_mapper,
            map[uint32_t, vector[uint32_t]] tokens_backward_mapper,
            map[uint32_t, double] tokens_weights,
            optional[map[DocType, uint32_t]] known_words,
            optional[cpp_set[TokenType]] break_tokens,
            optional[cpp_set[TokenType]] stop_tokens) except +

        void fit(const vector[DocType]& corpus,
            uint32_t n_candidates,
            bint rearrange_tokens,
            uint8_t split_mode,
            bint quiet) except +
        void fit(const vector[vector[vector[uint32_t]]]& corpus,
            uint32_t n_candidates,
            bint rearrange_tokens,
            bint quiet) except +

        void rearrange_tokens(optional[uint32_t] n_tokens, bint quiet) except +

        vector[pair[vector[uint32_t], double]] encode(
            const DocType& doc,
            uint8_t top_n,
            uint8_t split_mode) except +
        vector[pair[vector[uint32_t], double]] encode(
            const vector[vector[uint32_t]]& doc,
            uint8_t top_n) except +

        DocType decode(const vector[uint32_t]& tokens) except +

        map[vector[uint32_t], uint32_t] getForwardMapper()

        map[uint32_t, vector[uint32_t]] getBackwardMapper()

        map[uint32_t, double] getTokensWeights()

        map[TokenType, uint32_t] getAlphabet()

        map[uint32_t, TokenType] getInverseAlphabet()

        optional[map[DocType, uint32_t]] getKnownWords()

        optional[map[uint32_t, DocType]] getInverseKnownWords()

        optional[cpp_set[TokenType]] getBreakTokens()

        optional[cpp_set[TokenType]] getStopTokens()


# UBPE
cdef extern from "ubpe.hpp" namespace "ubpe":
    cdef cppclass Ubpe[DocType, TokenType]:
        Ubpe(uint32_t n_tokens,
            map[TokenType, uint32_t] alphabet) except +
        Ubpe(uint32_t n_tokens,
            map[TokenType, uint32_t] alphabet,
            optional[map[DocType, uint32_t]] known_words,
            optional[cpp_set[TokenType]] break_tokens,
            optional[cpp_set[TokenType]] stop_tokens) except +
        Ubpe(uint32_t n_tokens,
            map[TokenType, uint32_t] alphabet,
            map[uint32_t, TokenType] inverse_alphabet,
            map[vector[uint32_t], uint32_t] tokens_forward_mapper,
            map[uint32_t, vector[uint32_t]] tokens_backward_mapper,
            map[uint32_t, double] tokens_weights) except +
        Ubpe(uint32_t n_tokens,
            map[TokenType, uint32_t] alphabet,
            map[uint32_t, TokenType] inverse_alphabet,
            map[vector[uint32_t], uint32_t] tokens_forward_mapper,
            map[uint32_t, vector[uint32_t]] tokens_backward_mapper,
            map[uint32_t, double] tokens_weights,
            optional[map[DocType, uint32_t]] known_words,
            optional[cpp_set[TokenType]] break_tokens,
            optional[cpp_set[TokenType]] stop_tokens) except +

        void fit(const vector[DocType]& corpus,
            uint32_t n_candidates,
            bint rearrange_tokens,
            uint8_t split_mode,
            bint quiet) except +
        void fit(const vector[vector[vector[uint32_t]]]& corpus,
            uint32_t n_candidates,
            bint rearrange_tokens,
            bint quiet) except +

        void rearrange_tokens(optional[uint32_t] n_tokens, bint quiet) except +

        vector[pair[vector[uint32_t], double]] encode(
            const DocType& doc,
            uint8_t top_n,
            uint8_t split_mode) except +
        vector[pair[vector[uint32_t], double]] encode(
            const vector[vector[uint32_t]]& doc,
            uint8_t top_n) except +

        DocType decode(const vector[uint32_t]& tokens) except +

        map[vector[uint32_t], uint32_t] getForwardMapper()

        map[uint32_t, vector[uint32_t]] getBackwardMapper()

        map[uint32_t, double] getTokensWeights()

        map[TokenType, uint32_t] getAlphabet()

        map[uint32_t, TokenType] getInverseAlphabet()

        optional[map[DocType, uint32_t]] getKnownWords()

        optional[map[uint32_t, DocType]] getInverseKnownWords()

        optional[cpp_set[TokenType]] getBreakTokens()

        optional[cpp_set[TokenType]] getStopTokens()
