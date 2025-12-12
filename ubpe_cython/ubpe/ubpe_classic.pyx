# distutils: language = c++
import json

from cython.operator cimport dereference as deref
from libc.stdint cimport uint32_t, uint8_t
from libcpp cimport bool
from libcpp.map cimport map
from libcpp.memory cimport unique_ptr, make_unique
from libcpp.string cimport string
from libcpp.vector cimport vector
from libcpp cimport nullptr


from interface cimport UbpeClassic


cdef class UbpeClassicInt:
    cdef unique_ptr[UbpeClassic[vector[int], int]] inner

    def __init__(
        self,
        alphabet_size: int | None = None,
        alphabet: dict[int, int] | None = None,
        n_tokens: int = 2**10,
    ):
        cdef uint32_t _alphabet_size
        cdef map[int, uint32_t] _alphabet
        cdef uint32_t _n_tokens

        assert not (alphabet is None and alphabet_size is None), "Either `alphabet_size` or `alphabet` must be specified, or model should be load from json string"

        # constructor without an alphabet
        if alphabet is None and alphabet_size is not None:
            _n_tokens = n_tokens
            _alphabet_size = alphabet_size
            self.inner = make_unique[UbpeClassic[vector[int], int]](_n_tokens, _alphabet_size)
            return

        # ensure that `alphabet` is a dict
        assert isinstance(
            alphabet, dict
        ), "If `alphabet` is provided, it must be a dict"
        
        if alphabet_size is None:
            alphabet_size = len(alphabet)
            
        _n_tokens = n_tokens
        _alphabet_size = alphabet_size
        for key, value in alphabet.items():
            _alphabet.insert((key, value))    
        self.inner = make_unique[UbpeClassic[vector[int], int]](_n_tokens, _alphabet_size, _alphabet)

    def dumps(self) -> str:
        """
        Dumps model to a string.
        """
        alphabet = deref(self.inner).getAlphabet()
        tokens_mapper = deref(self.inner).getBackwardMapper()
        tokens_weights = deref(self.inner).getTokensWeights()
        return json.dumps(
            {
                "n_tokens": len(alphabet) + len(tokens_mapper),
                "alphabet": alphabet,
                "mapper": tokens_mapper,
                "weights": tokens_weights,
            }
        )

    def loads(self, dump: str):
        """
        Load a tokenizer model from a json-serialized string.
        """
        cdef uint32_t n_tokens
        cdef uint32_t alphabet_size
        cdef map[int, uint32_t] alphabet
        cdef map[uint32_t, int] inverse_alphabet
        cdef map[vector[uint32_t], uint32_t] tokens_forward_mapping
        cdef map[uint32_t, vector[uint32_t]] tokens_backward_mapping
        cdef map[uint32_t, float] tokens_weights

        model = json.loads(dump)

        n_tokens = model["n_tokens"]
        alphabet = model["alphabet"]
        alphabet_size = len(model["alphabet"])
        for key, value in model["alphabet"].items():
            inverse_alphabet.insert((value, key))
        for token, seq in model["mapper"].items():
            tokens_forward_mapping.insert((token, seq))
        for token, seq in model["mapper"].items():
            tokens_backward_mapping.insert((seq, token))
        for token, weight in model["weights"].items():
            tokens_weights.insert((token, weight))
        
        self.inner = make_unique[UbpeClassic[vector[int], int]](
            n_tokens, alphabet_size,
            alphabet, inverse_alphabet, 
            tokens_forward_mapping, tokens_backward_mapping,
            tokens_weights
        )
        return self

    def fit(self, vector[vector[int]] corpus, uint32_t n_candidates, bool rearrange_tokens):
        deref(self.inner).fit(corpus, n_candidates, rearrange_tokens)

    def encode(self, vector[int] doc, uint8_t top_n = 1):
        return deref(self.inner).encode(doc, top_n)

    def decode(self, vector[uint32_t] tokens):
        return deref(self.inner).decode(tokens)


cdef class UbpeClassicChar:
    cdef unique_ptr[UbpeClassic[string, char]] inner

    def __init__(
        self,
        alphabet_size: int | None = None,
        alphabet: dict[int, int] | None = None,
        n_tokens: int = 2**10,
    ):
        cdef uint32_t _alphabet_size
        cdef map[char, uint32_t] _alphabet
        cdef uint32_t _n_tokens

        assert not (alphabet is None and alphabet_size is None), "Either `alphabet_size` or `alphabet` must be specified, or model should be load from json string"

        # constructor without an alphabet
        if alphabet is None and alphabet_size is not None:
            _n_tokens = n_tokens
            _alphabet_size = alphabet_size
            self.inner = make_unique[UbpeClassic[string, char]](_n_tokens, _alphabet_size)
            return

        # ensure that `alphabet` is a dict
        assert isinstance(
            alphabet, dict
        ), "If `alphabet` is provided, it must be a dict"
        
        if alphabet_size is None:
            alphabet_size = len(alphabet)
            
        _n_tokens = n_tokens
        _alphabet_size = alphabet_size
        for key, value in alphabet.items():
            _alphabet.insert((key, value))    
        self.inner = make_unique[UbpeClassic[string, char]](_n_tokens, _alphabet_size, _alphabet)

    def dumps(self) -> str:
        """
        Dumps model to a string.
        """
        alphabet = deref(self.inner).getAlphabet()
        tokens_mapper = deref(self.inner).getBackwardMapper()
        tokens_weights = deref(self.inner).getTokensWeights()
        return json.dumps(
            {
                "n_tokens": len(alphabet) + len(tokens_mapper),
                "alphabet": alphabet,
                "mapper": tokens_mapper,
                "weights": tokens_weights,
            }
        )

    def loads(self, dump: str):
        """
        Load a tokenizer model from a json-serialized string.
        """
        cdef uint32_t n_tokens
        cdef uint32_t alphabet_size
        cdef map[char, uint32_t] alphabet
        cdef map[uint32_t, char] inverse_alphabet
        cdef map[vector[uint32_t], uint32_t] tokens_forward_mapping
        cdef map[uint32_t, vector[uint32_t]] tokens_backward_mapping
        cdef map[uint32_t, float] tokens_weights

        model = json.loads(dump)

        n_tokens = model["n_tokens"]
        alphabet = model["alphabet"]
        alphabet_size = len(model["alphabet"])
        for key, value in model["alphabet"].items():
            inverse_alphabet.insert((value, key))
        for token, seq in model["mapper"].items():
            tokens_forward_mapping.insert((token, seq))
        for token, seq in model["mapper"].items():
            tokens_backward_mapping.insert((seq, token))
        for token, weight in model["weights"].items():
            tokens_weights.insert((token, weight))
        
        self.inner = make_unique[UbpeClassic[string, char]](
            n_tokens, alphabet_size,
            alphabet, inverse_alphabet, 
            tokens_forward_mapping, tokens_backward_mapping,
            tokens_weights
        )
        return self

    def fit(self, vector[string] corpus, uint32_t n_candidates, bool rearrange_tokens):
        deref(self.inner).fit(corpus, n_candidates, rearrange_tokens)

    def encode(self, string doc, top_n=1):
        return deref(self.inner).encode(doc, top_n)

    def decode(self, vector[uint32_t] tokens):
        return deref(self.inner).decode(tokens)
