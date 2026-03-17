# distutils: language = c++

include "ssstree.pyx"

import re
from enum import Flag
from libcpp.vector cimport vector
from libc.stdint cimport uint32_t
from libc.stddef cimport size_t

class SplitMode(Flag):
    """SplitMode enum

    Options:
        NONE: No splitting.
        KNOWN_WORDS: Split on known words.
        BREAK_TOKENS: Split on break tokens.
        REGEX: Split on regex.
        STOP_TOKENS: Split on stop tokens.
        FULL: Split on all options.
    """

    NONE = 0b0000
    KNOWN_WORDS = 0b0001
    BREAK_TOKENS = 0b0010
    REGEX = 0b0100
    STOP_TOKENS = 0b1000
    FULL = KNOWN_WORDS | BREAK_TOKENS | REGEX | STOP_TOKENS

cdef list _split_part_by_tokens(str part, set tokens, bint leave_separators):
    """
    Split part by tokens.
    """
    cdef list parts = []
    cdef Py_ssize_t part_start = 0
    cdef Py_ssize_t ti
    cdef str token

    for ti, token in enumerate(part):
        if token not in tokens:
            continue
        if ti != part_start:
            parts.append(part[part_start:ti])
        if leave_separators:
            parts.append(part[ti:ti+1])
        part_start = ti + 1

    if part_start < len(part):
        parts.append(part[part_start:])
    return parts

cdef class SplitPipeline:
    """SplitPipeline class"""
    cdef dict alphabet
    cdef dict known_words
    cdef set break_tokens
    cdef set stop_tokens
    cdef str regex_str
    cdef object _regex
    cdef SSSTree kw_ssstree

    def __init__(
        self,
        alphabet,
        *,
        known_words=None,
        break_tokens=None,
        regex_str=None,
        stop_tokens=None,
    ):
        """Initialize SplitPipeline class

        Args:
            alphabet (list | set | str | dict): The alphabet to use for splitting.
            known_words (list | dict, optional): Known words to use for splitting. Defaults to None.
            break_tokens (set | list, optional): Break tokens to use for splitting. Defaults to None.
            stop_tokens (set | list, optional): Stop tokens to use for splitting. Defaults to None.
            regex_str (str, optional): Regex string to use for splitting. Defaults to None.
        """

        if alphabet is None:
            raise Exception("`alphabet` must be provided")
        if (
            isinstance(alphabet, list)
            or isinstance(alphabet, set)
            or isinstance(alphabet, str)
        ):
            self.alphabet = {token: i for i, token in enumerate(alphabet)}
        elif isinstance(alphabet, dict):
            if sorted(alphabet.values()) != list(range(len(alphabet))):
                raise Exception("`alphabet` must have sequential integer keys")
            self.alphabet = alphabet
        else:
            raise Exception("`alphabet` must be either `list` | `set` | `str` | `dict`")

        self.kw_ssstree = None
        self.known_words = None

        if known_words is not None:
            if isinstance(known_words, list) and len(known_words) != 0:
                if isinstance(known_words[0], str):
                    self.known_words = {
                        word: i
                        for i, word in enumerate(known_words, start=len(self.alphabet))
                    }
                else:
                    raise TypeError(
                        "If `known_words` is provided, it must be a list of strings or a dict "
                    )
                self.kw_ssstree = SSSTree()
                for kw, val in self.known_words.items():
                    self.kw_ssstree.add(kw, val)
            elif isinstance(known_words, dict):
                key_types = set(type(key) for key in known_words)
                if len(key_types) > 1:
                    raise TypeError(
                        "If `known_words` is a `dict` its keys must must have the same type"
                    )
                key_type = key_types.pop()
                if key_type not in (str,):
                    raise TypeError(
                        "If `known_words` is a `dict` its keys must be strings"
                    )

                kw_tokens = sorted(known_words.values())
                if kw_tokens != list(
                    range(len(alphabet), len(alphabet) + len(kw_tokens))
                ):
                    raise Exception(
                        "`known_words` dict must have sequential integer keys"
                    )
                self.known_words = known_words
                self.kw_ssstree = SSSTree()
                for kw, val in self.known_words.items():
                    self.kw_ssstree.add(kw, val)

        self.break_tokens = None
        if break_tokens is not None and (
            isinstance(break_tokens, set)
            or isinstance(break_tokens, list)
            or isinstance(break_tokens, str)
        ):
            self.break_tokens = set(
                token for token in break_tokens if token in self.alphabet
            )
            if len(self.break_tokens) == 0:
                self.break_tokens = None

        # Regex is always present
        self.regex_str = regex_str
        self._regex = re.compile(regex_str)

        self.stop_tokens = None
        if stop_tokens is not None and (
            isinstance(stop_tokens, set)
            or isinstance(stop_tokens, list)
            or isinstance(stop_tokens, str)
        ):
            self.stop_tokens = set(
                token for token in stop_tokens if token in self.alphabet
            )
            if len(self.stop_tokens) == 0:
                self.stop_tokens = None

    def __call__(
        self,
        str doc,
        *,
        mode=SplitMode.FULL,
        bint leave_separators=True,
    ):
        """Split a document into parts based on known words and stop tokens.

        Args:
            doc: The document to split.
            mode: The splitting mode.
            leave_separators: Whether to leave separators in the parts.

        Returns:
            A list of parts.
        """
        cdef vector[vector[uint32_t]] parts
        cdef Py_ssize_t si = 0
        cdef Py_ssize_t part_start = 0
        cdef Py_ssize_t len_doc = len(doc)
        cdef list kw_candidates
        cdef vector[uint32_t] current_ids
        cdef str part
        cdef str token
        cdef list str_parts
        cdef Py_ssize_t i

        if SplitMode.KNOWN_WORDS in mode and self.kw_ssstree is not None:
            while si < len_doc:
                kw_candidates = self.kw_ssstree(doc, start=si, fast=True)
                if len(kw_candidates) == 0:
                    si += 1
                    continue
                if si != part_start:
                    str_parts = self._split_part(doc[part_start:si], mode, leave_separators)
                    for part in str_parts:
                        current_ids = vector[uint32_t]()
                        for token in part:
                            current_ids.push_back(self.alphabet[token])
                        parts.push_back(current_ids)
                if leave_separators:
                    current_ids = vector[uint32_t]()
                    current_ids.push_back(kw_candidates[-1][1])
                    parts.push_back(current_ids)
                part_start = si + kw_candidates[-1][0]
                si = part_start
            if part_start < len_doc:
                str_parts = self._split_part(doc[part_start:], mode, leave_separators)
                for part in str_parts:
                    current_ids = vector[uint32_t]()
                    for token in part:
                        current_ids.push_back(self.alphabet[token])
                    parts.push_back(current_ids)
        else:
            str_parts = self._split_part(doc, mode, leave_separators)
            for part in str_parts:
                current_ids = vector[uint32_t]()
                for token in part:
                    current_ids.push_back(self.alphabet[token])
                parts.push_back(current_ids)
        return parts

    cpdef list _split_part(
        self,
        str part,
        mode,
        bint leave_separators,
    ):
        cdef list parts = [part]
        cdef list splits
        cdef str p

        if SplitMode.BREAK_TOKENS in mode:
            splits = []
            for p in parts:
                splits.extend(self._split_part_by_break_tokens(p, leave_separators))
            parts = splits
        if SplitMode.REGEX in mode:
            splits = []
            for p in parts:
                splits.extend(self._split_part_by_regex(p))
            parts = splits
        if SplitMode.STOP_TOKENS in mode:
            splits = []
            for p in parts:
                splits.extend(self._split_part_by_stop_tokens(p, leave_separators))
            parts = splits
        return parts

    cpdef list _split_part_by_break_tokens(
        self, str part, bint leave_separators
    ):
        return (
            _split_part_by_tokens(part, self.break_tokens, leave_separators)
            if self.break_tokens is not None
            else [part]
        )

    cpdef list _split_part_by_regex(self, str part):
        if self._regex is None:
            return [part]
        return self._regex.findall(part)

    cpdef list _split_part_by_stop_tokens(
        self, str part, bint leave_separators
    ):
        return (
            _split_part_by_tokens(part, self.stop_tokens, leave_separators)
            if self.stop_tokens is not None
            else [part]
        )
