from libc.stdint cimport uint32_t, uint8_t
from libcpp.map cimport map
from libcpp.vector cimport vector
from libcpp.pair cimport pair

# UBPE Classic
cdef extern from "ubpe_classic.hpp" namespace "ubpe":
    cdef cppclass UbpeClassic[DocType, TokenType]:
        UbpeClassic(uint32_t, uint32_t) except +
        UbpeClassic(uint32_t, uint32_t, map[TokenType, uint32_t]) except +
        UbpeClassic(uint32_t, uint32_t, map[TokenType, uint32_t], map[uint32_t, TokenType], map[vector[uint32_t], uint32_t], map[uint32_t, vector[uint32_t]], map[uint32_t, double]) except +

        void fit(const vector[DocType]& corpus, uint32_t n_candidates, bint rearrange_tokens) except +

        vector[pair[vector[uint32_t], double]] encode(const DocType& doc, uint8_t top_n) except +

        DocType decode(const vector[uint32_t]& tokens) except +

        map[vector[uint32_t], uint32_t] getForwardMapper()

        map[uint32_t, vector[uint32_t]] getBackwardMapper()

        map[uint32_t, double] getTokensWeights()

        map[TokenType, uint32_t] getAlphabet()

        map[uint32_t, TokenType] getInverseAlphabet()


# UBPE
cdef extern from "ubpe.hpp" namespace "ubpe":
    cdef cppclass Ubpe[DocType, TokenType]:
        Ubpe(uint32_t, uint32_t) except +
        Ubpe(uint32_t, uint32_t, map[TokenType, uint32_t]) except +
        Ubpe(uint32_t, uint32_t, map[TokenType, uint32_t], map[uint32_t, TokenType], map[vector[uint32_t], uint32_t], map[uint32_t, vector[uint32_t]], map[uint32_t, double]) except +

        void fit(const vector[DocType]& corpus, uint32_t n_candidates, bint rearrange_tokens) except +

        vector[pair[vector[uint32_t], double]] encode(const DocType& doc, uint8_t top_n) except +

        DocType decode(const vector[uint32_t]& tokens) except +

        map[vector[uint32_t], uint32_t] getForwardMapper()

        map[uint32_t, vector[uint32_t]] getBackwardMapper()

        map[uint32_t, double] getTokensWeights()

        map[TokenType, uint32_t] getAlphabet()

        map[uint32_t, TokenType] getInverseAlphabet()
