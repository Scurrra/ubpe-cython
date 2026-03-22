# distutils: language = c++

from libcpp.optional cimport optional
from libc.stdint cimport uint32_t

cdef class SSSTreeNode:
    """
    Node of a radix tree.
    """
    cdef str key
    cdef optional[uint32_t] value
    cdef list children

    def __init__(self, str key, uint32_t value):
        self.key = key
        self.value.emplace(value)
        self.children = []

    def add(self, str key, uint32_t value):
        """
        Add new entry to the tree that starts with the current node.
        """
        cdef Py_ssize_t i = 0
        cdef Py_ssize_t len_self = len(self.key)
        cdef Py_ssize_t len_key = len(key)
        cdef Py_ssize_t max_len = len_self if len_self < len_key else len_key
        while i < max_len and self.key[i] == key[i]:
            i += 1

        cdef SSSTreeNode split = SSSTreeNode(self.key[i:], 0)
        cdef bint is_new = True
        cdef SSSTreeNode child

        # key to insert is in the tree
        if i == len_key:
            # equal keys
            if i == len_self:
                if not self.value.has_value():
                    self.value.emplace(value)
                return self.value.value() == value

            # split vertex in two
            split.value = self.value
            split.children = self.children
            self.children = [split]
            self.key = key  # same as self.key[:i]
            self.value.emplace(value)

        # part of a key is in the tree
        else:
            key = key[i:]

            # the new key starts with the old one
            if i == len_self:
                for child in self.children:
                    if child.key[0] == key[0]:
                        child.add(key, value)
                        is_new = False
                        break
                if is_new:
                    self.children.append(SSSTreeNode(key, value))

            # the new and the old keys have common first i elements
            else:
                split.value = self.value
                split.children = self.children
                self.children = [split, SSSTreeNode(key, value)]
                self.key = self.key[:i]
                self.value = optional[uint32_t]()

    def __add__(self, tuple element):
        """
        Add new entry to the tree that starts with the current node.
        """
        cdef str key = element[0]
        cdef uint32_t value = element[1]
        return self.add(key, value)

    def __getitem__(self, str key):
        """
        Get the value from the tree for the provided key. If not found, `None` is returned.
        """
        if key == self.key:
            if self.value.has_value():
                return self.value.value()
            else:
                return None

        cdef Py_ssize_t len_self = len(self.key)
        cdef SSSTreeNode child
        if len(key) >= len_self and key[:len_self] == self.key:
            key = key[len_self:]
            for child in self.children:
                if child.key[0] == key[0]:
                    return child[key]
        return None

    def __call__(self, str key, list stack, Py_ssize_t start=0):
        """
        Trace `key` by the tree. Finds all entries `(k, v)`, where `key` starts with `k` and `v` is not `None`.
        """
        cdef Py_ssize_t len_self = len(self.key)
        if start + len_self > len(key):
            if len(stack) > 0:
                return stack[-1]
            else:
                return (key, None)

        cdef object py_value = self.value.value() if self.value.has_value() else None
        cdef SSSTreeNode child
        if key[start:(start + len_self)] == self.key:
            stack.append((self.key, py_value))
            start += len_self
            if start == len(key):
                return stack[-1]
            for child in self.children:
                if child.key[0] == key[start]:
                    child(key, stack, start)

        if len(stack) > 0:
            return stack[-1]
        else:
            return (key, None)


cdef class SSSTree:
    """
    SubSequence Search Tree.
    Well, it's a version of an optimized trie but with an efficient search operator `()`
    which return not the full match for the `key`, but all non-null entries
    which keys are prefixes  in the `key`.
    """

    cdef list children

    def __init__(self):
        self.children = []

    def add(self, str key, uint32_t value):
        """
        Add new entry to the tree.

        Function searches for the elder child subtree (of type `SSSTreeNode`) and adds the entry to this subtree.
        If subtree is not found, the new one is created.
        """
        cdef Py_ssize_t i = 0
        cdef SSSTreeNode node
        while i < len(self.children):
            node = self.children[i]
            if node.key[0] == key[0]:
                node.add(key, value)
                return True
            i += 1

        self.children.append(SSSTreeNode(key, value))
        return True

    def __add__(self, tuple element):
        """
        Add new entry to the tree.

        Function searches for the elder child subtree (of type `SSSTreeNode`) and adds the entry to this subtree.
        If subtree is not found, the new one is created.
        """
        cdef str key = element[0]
        cdef uint32_t value = element[1]
        return self.add(key, value)

    def __getitem__(self, str key):
        """
        Get the value from the tree for the provided key. If not found, `None` is returned.
        """
        cdef Py_ssize_t i = 0
        cdef SSSTreeNode node
        while i < len(self.children):
            node = self.children[i]
            if node.key[0] == key[0]:
                return node[key]
            i += 1
        return None

    def __call__(self, str key, Py_ssize_t start=0, bint fast=False):
        """
        Trace `key` by the tree. Finds all entries `(k, v)`, where `key` starts with `k` and `v` is not `None`.
        """
        cdef Py_ssize_t i = 0
        cdef Py_ssize_t j
        cdef SSSTreeNode node
        cdef list stack = []
        cdef list result = []
        cdef str sub_key
        cdef Py_ssize_t sub_key_len

        while i < len(self.children):
            node = self.children[i]
            if node.key[0] == key[start]:
                node(key, stack, start)
                if len(stack) > 0:
                    if not fast:
                        sub_key = stack[0][0]
                        if stack[0][1] is not None:
                            result.append((sub_key, stack[0][1]))
                        for j in range(1, len(stack)):
                            sub_key += stack[j][0]
                            if stack[j][1] is not None:
                                result.append((sub_key, stack[j][1]))
                        return result
                    else:
                        sub_key_len = len(stack[0][0])
                        if stack[0][1] is not None:
                            result.append((sub_key_len, stack[0][1]))
                        for j in range(1, len(stack)):
                            sub_key_len += len(stack[j][0])
                            if stack[j][1] is not None:
                                result.append((sub_key_len, stack[j][1]))
                        return result
                else:
                    return []
            i += 1
        return []
