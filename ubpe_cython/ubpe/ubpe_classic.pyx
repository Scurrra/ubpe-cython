# distutils: language = c++
import json

from cython.operator cimport dereference as deref
from libc.stdint cimport int64_t, uint32_t, uint8_t
from libcpp.map cimport map
from libcpp.memory cimport unique_ptr, make_unique
from libcpp.optional cimport optional, nullopt
from libcpp.set cimport set as cpp_set
from libcpp.string cimport string
from libcpp.vector cimport vector
from libcpp cimport nullptr


from interface cimport UbpeClassic


cdef class UbpeClassicInt:
    cdef unique_ptr[UbpeClassic[vector[int64_t], int64_t]] inner

    def __init__(
        self,
        *,
        alphabet: dict[int, int] | None = None,
        n_tokens: int = 2**10,
        known_words: list[list[int]] | dict[tuple[int, ...], int] | None = None,
        break_tokens: set[int] | list[int] | None = None,
        stop_tokens: set[int] | list[int] | None = None,
    ):
        cdef uint32_t _n_tokens

        if alphabet is None:
            raise TypeError(
                "`alphabet` must be specified, or model should be load from json string"
            )

        if not isinstance(alphabet, dict):
            raise TypeError("If `alphabet` is provided, it must be a dict")
        if sorted(alphabet.values()) != list(range(len(alphabet))):
            raise Exception("`alphabet` must have sequential integer keys")

        if known_words is not None and not isinstance(known_words, (list, dict)):
            raise TypeError("If `known_words` is provided, it must be a list or dict")

        if break_tokens is not None and not isinstance(break_tokens, (list, set)):
            raise TypeError("If `break_tokens` is provided, it must be a list or set")

        if stop_tokens is not None and not isinstance(stop_tokens, (list, set)):
            raise TypeError("If `stop_tokens` is provided, it must be a list or set")


        cdef map[int64_t, uint32_t] _alphabet
        cdef optional[map[vector[int64_t], uint32_t]] _known_words
        cdef optional[cpp_set[int64_t]] _break_tokens
        cdef optional[cpp_set[int64_t]] _stop_tokens

        _n_tokens = n_tokens
        for key, value in alphabet.items():
            _alphabet.insert((key, value))

        if known_words is not None:
            if isinstance(known_words, list):
                known_words = {
                    word: i for i, word in enumerate(known_words, start=len(alphabet))
                }
            _known_words.emplace()
            for word, token in known_words.items():
                _known_words.value().insert((word, token))
        if break_tokens is not None:
            _break_tokens.emplace()
            for token in break_tokens:
                _break_tokens.value().insert(token)
        if stop_tokens is not None:
            _stop_tokens.emplace()
            for token in stop_tokens:
                _stop_tokens.value().insert(token)

        self.inner = make_unique[UbpeClassic[vector[int64_t], int64_t]](
            _n_tokens,
            _alphabet,
            _known_words,
            _break_tokens, _stop_tokens,
        )

    def dumps(self) -> str:
        """
        Dumps model to a string.
        """
        alphabet = deref(self.inner).getAlphabet()
        tokens_mapper = deref(self.inner).getBackwardMapper()
        tokens_weights = deref(self.inner).getTokensWeights()

        inst = {
            "n_tokens": len(alphabet) + len(tokens_mapper),
            "alphabet": alphabet,
        }

        break_tokens = deref(self.inner).getBreakTokens()
        stop_tokens = deref(self.inner).getStopTokens()
        inv_known_words = deref(self.inner).getInverseKnownWords()

        if inv_known_words.has_value():
            inst["known_words"] = inv_known_words.value()
            inst["n_tokens"] += len(inst["known_words"])
        if break_tokens.has_value():
            inst["break_tokens"] = break_tokens.value()
        if stop_tokens.has_value():
            inst["stop_tokens"] = stop_tokens.value()

        inst["mapper"] = tokens_mapper
        inst["weights"] = tokens_weights

        return json.dumps(
            {field: value for field, value in inst.items() if value is not None}
        )


    @classmethod
    def loads(cls, dump: str):
        """
        Load a tokenizer model from a json-serialized string.
        """
        cdef uint32_t n_tokens
        cdef map[int64_t, uint32_t] alphabet
        cdef map[uint32_t, int64_t] inverse_alphabet
        cdef optional[map[vector[int64_t], uint32_t]] known_words
        cdef optional[cpp_set[int64_t]] break_tokens
        cdef optional[cpp_set[int64_t]] stop_tokens
        cdef map[vector[uint32_t], uint32_t] tokens_forward_mapping
        cdef map[uint32_t, vector[uint32_t]] tokens_backward_mapping
        cdef map[uint32_t, double] tokens_weights

        model = json.loads(dump)

        n_tokens = model["n_tokens"]
        for key, value in model["alphabet"].items():
            inverse_alphabet.insert((value, int(key)))
            alphabet.insert((int(key), value))
        for token, seq in model["mapper"].items():
            tokens_backward_mapping.insert((int(token), seq))
            tokens_forward_mapping.insert((seq, int(token)))
        for token, weight in model["weights"].items():
            tokens_weights.insert((int(token), weight))

        if "known_words" in model:
            known_words.emplace()
            for token, seq in model["known_words"].items():
                known_words.value().insert((seq, int(token)))
        if "break_tokens" in model:
            break_tokens.emplace()
            for token in model["break_tokens"]:
                break_tokens.value().insert(token)
        if "stop_tokens" in model:
            stop_tokens.emplace()
            for token in model["stop_tokens"]:
                stop_tokens.value().insert(token)

        cdef UbpeClassicInt inst = cls()
        inst.inner = make_unique[UbpeClassic[vector[int64_t], int64_t]](
            n_tokens,
            alphabet, inverse_alphabet,
            tokens_forward_mapping, tokens_backward_mapping,
            tokens_weights,
            known_words, break_tokens, stop_tokens
        )
        return inst

    def fit(self, vector[vector[int64_t]] corpus, uint32_t n_candidates = 50, bint rearrange_tokens = True, uint8_t split_mode = 0b1111, bint quiet = False):
        deref(self.inner).fit(corpus, n_candidates, rearrange_tokens, split_mode, quiet)

    def encode(self, vector[int64_t] doc, uint8_t top_n = 1, uint8_t split_mode = 0b1111):
        return deref(self.inner).encode(doc, top_n, split_mode)

    def decode(self, vector[uint32_t] tokens):
        return deref(self.inner).decode(tokens)


cdef class UbpeClassicChar:
    cdef unique_ptr[UbpeClassic[vector[int64_t], int64_t]] inner

    cdef readonly dict[str, int64_t] alphabet
    cdef readonly dict[int64_t, str] inverse_alphabet
    cdef readonly dict[int64_t, str] inverse_known_words

    # this can be None
    cdef readonly SplitPipeline split_pipeline

    def __init__(
        self,
        *,
        alphabet: dict[str, int64_t],
        n_tokens: int = 2**10,
        known_words: list[str] | dict[str, int64_t] | None = None,
        break_tokens: set[str] | list[str] | None = None,
        regex_str: str | None = None,
        stop_tokens: set[str] | list[str] | None = None,
    ):
        # ensure that `alphabet` is a dict
        if alphabet is None:
            raise ValueError("`alphabet` must be provided")
        if not isinstance(alphabet, dict):
            raise TypeError("If `alphabet` is provided, it must be a dict")
        if sorted(alphabet.values()) != list(range(len(alphabet))):
            raise Exception("`alphabet` must have sequential integer keys")

        self.alphabet = alphabet
        self.inverse_alphabet = {
            value: key
            for key, value in alphabet.items()
        }
        self.inverse_known_words = dict()

        cdef uint32_t _n_tokens = n_tokens
        cdef map[int64_t, uint32_t] _alphabet

        if regex_str is not None:
            # Python's and C++'s strings differ a lot,
            # so if regex present in the split pipeline,
            # split is done in Python
            self.split_pipeline = SplitPipeline(alphabet,
                known_words=known_words,
                break_tokens=break_tokens,
                regex_str=regex_str,
                stop_tokens=stop_tokens,
            )

            # alphabet and known words merged into a single map
            # that acts as the alphabet on the backend
            for index, token in enumerate(alphabet.values()):
                _alphabet[token] = index
            if self.split_pipeline.known_words is not None:
                for index, token in enumerate(self.split_pipeline.known_words.values(), start=len(alphabet)):
                    _alphabet[token] = index
                for word, token in self.split_pipeline.known_words.items():
                    self.inverse_known_words[token] = word
                if len(self.inverse_known_words) == 0:
                    self.inverse_known_words = None

            self.inner = make_unique[UbpeClassic[vector[int64_t], int64_t]](
                _n_tokens,
                _alphabet,
            )
            return

        cdef optional[map[vector[int64_t], uint32_t]] _known_words
        cdef optional[cpp_set[int64_t]] _break_tokens
        cdef optional[cpp_set[int64_t]] _stop_tokens

        for index, token in enumerate(alphabet.values()):
            _alphabet[token] = index

        if known_words is not None:
            if isinstance(known_words, list):
                known_words = {
                    word: i for i, word in enumerate(known_words, start=len(alphabet))
                }
            _known_words.emplace()
            for word, token in known_words.items():
                _known_words.value().insert(([
                    alphabet[c] for c in word
                ], token))
                self.inverse_known_words[token] = word
            if len(self.inverse_known_words) == 0:
                self.inverse_known_words = None
        if break_tokens is not None:
            _break_tokens.emplace()
            for token in break_tokens:
                if token in alphabet:
                    _break_tokens.value().insert(alphabet[token])
        if stop_tokens is not None:
            _stop_tokens.emplace()
            for token in stop_tokens:
                if token in alphabet:
                    _stop_tokens.value().insert(alphabet[token])

        self.inner = make_unique[UbpeClassic[vector[int64_t], int64_t]](
            _n_tokens,
            _alphabet,
            _known_words,
            _break_tokens, _stop_tokens,
        )

    def dumps(self) -> str:
        """
        Dumps model to a string.
        """
        tokens_mapper = deref(self.inner).getBackwardMapper()
        tokens_weights = deref(self.inner).getTokensWeights()
        break_tokens = deref(self.inner).getBreakTokens()
        stop_tokens = deref(self.inner).getStopTokens()
        inv_known_words = deref(self.inner).getInverseKnownWords()

        inst = {
            "n_tokens": len(self.alphabet) + len(tokens_mapper) + (
                len(self.inverse_known_words) if self.inverse_known_words is not None else 0
            ),
            "alphabet": self.alphabet,
            "known_words": self.inverse_known_words
            if self.inverse_known_words is not None and len(self.inverse_known_words) > 0
            else None,
        }

        if self.split_pipeline is not None:
            inst["break_tokens"] = self.split_pipeline.break_tokens
            inst["regex_str"] = self.split_pipeline.regex_str
            inst["stop_tokens"] = self.split_pipeline.stop_tokens
        else:
            inst["break_tokens"] = [self.inverse_alphabet[token] for token in break_tokens.value()] if break_tokens.has_value() and break_tokens.value().size() > 0 else None
            inst["stop_tokens"] = [self.inverse_alphabet[token] for token in stop_tokens.value()] if stop_tokens.has_value() and stop_tokens.value().size() > 0 else None

        inst["mapper"] = tokens_mapper
        inst["weights"] = tokens_weights

        return json.dumps(
            {field: value for field, value in inst.items() if value is not None}
        )

    @classmethod
    def loads(cls, dump: str):
        """
        Load a tokenizer model from a json-serialized string.
        """
        cdef uint32_t n_tokens
        cdef map[int64_t, uint32_t] alphabet
        cdef map[uint32_t, int64_t] inverse_alphabet
        cdef map[vector[uint32_t], uint32_t] tokens_forward_mapping
        cdef map[uint32_t, vector[uint32_t]] tokens_backward_mapping
        cdef map[uint32_t, double] tokens_weights

        model = json.loads(dump)

        n_tokens = model["n_tokens"]
        for token, seq in model["mapper"].items():
            tokens_backward_mapping.insert((int(token), seq))
            tokens_forward_mapping.insert((seq, int(token)))
        for token, weight in model["weights"].items():
            tokens_weights.insert((int(token), weight))

        inverse_known_words = model.get("known_words", None)

        cdef UbpeClassicChar inst = cls(alphabet=model["alphabet"],
            known_words={
                seq: int(token) for token, seq in inverse_known_words.items()
            } if inverse_known_words is not None else None,
            break_tokens=model.get("break_tokens", None),
            regex_str=model.get("regex_str", None),
            stop_tokens=model.get("stop_tokens", None)
        )
        inst.inverse_known_words = inverse_known_words

        for index, token in enumerate(inst.alphabet.values()):
            alphabet[token] = index
            inverse_alphabet[index] = token

        if inst.split_pipeline is not None:
            if inst.split_pipeline.known_words is not None:
                for index, token in enumerate(inst.split_pipeline.known_words.values(), start=len(inst.alphabet)):
                    alphabet[token] = index
                    inverse_alphabet[index] = token

            inst.inner = make_unique[UbpeClassic[vector[int64_t], int64_t]](
                n_tokens,
                alphabet, inverse_alphabet,
                tokens_forward_mapping, tokens_backward_mapping,
                tokens_weights
            )
            return inst

        cdef optional[map[vector[int64_t], uint32_t]] known_words
        cdef optional[cpp_set[int64_t]] break_tokens
        cdef optional[cpp_set[int64_t]] stop_tokens

        if model.get("known_words", None) is not None:
            known_words.emplace()
            for token, word in model["known_words"].items():
                known_words.value().insert(([
                    inst.alphabet[c] for c in word
                ], int(token)))
        if model.get("break_tokens", None) is not None:
            break_tokens.emplace()
            for token in model["break_tokens"]:
                break_tokens.value().insert(inst.alphabet[token])
        if model.get("stop_tokens", None) is not None:
            stop_tokens.emplace()
            for token in model["stop_tokens"]:
                stop_tokens.value().insert(inst.alphabet[token])

        inst.inner = make_unique[UbpeClassic[vector[int64_t], int64_t]](
            n_tokens,
            alphabet, inverse_alphabet,
            tokens_forward_mapping, tokens_backward_mapping,
            tokens_weights,
            known_words,
            break_tokens, stop_tokens
        )
        return inst


    def fit(self, list[str] corpus, uint32_t n_candidates = 50, bint rearrange_tokens = True, uint8_t split_mode = 0b1111, bint quiet = False):
        if self.split_pipeline is not None:
            deref(self.inner).fit([
                self.split_pipeline(doc, leave_separators=False)
                for doc in corpus
            ], n_candidates, rearrange_tokens, quiet)
            return

        cdef vector[vector[int64_t]] _corpus
        try:
            _corpus = [
                [
                    self.alphabet[letter]
                    for letter in doc
                ]
                for doc in corpus
            ]
        except:
            raise Exception("Unknown letter")
        deref(self.inner).fit(_corpus, n_candidates, rearrange_tokens, split_mode, quiet)

    def encode(self, str doc, uint8_t top_n = 1, uint8_t split_mode = 0b1111):
        if self.split_pipeline is not None:
            return deref(self.inner).encode(self.split_pipeline(doc, mode=SplitMode(split_mode)), top_n)

        cdef vector[int64_t] _doc
        try:
            _doc = [self.alphabet[letter] for letter in doc]
        except:
            raise Exception("Unknown letter")
        return deref(self.inner).encode(_doc, top_n, split_mode)

    def decode(self, vector[uint32_t] tokens):
        cdef vector[int64_t] doc
        doc = deref(self.inner).decode(tokens)
        return "".join(
            self.inverse_alphabet[token]
            if token in self.inverse_alphabet else
            self.inverse_known_words[token]
            for token in doc
        )
