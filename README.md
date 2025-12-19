# ubpe-cython

> Cython implementation with C++ backend of the [Universal Byte Pair Encoding Tokenizer](https://github.com/Scurrra/ubpe). 
 
C++ implementation is complete, so it can be used natively in C++. Cython just provides only interface to the C++ implementation and wrappers to provide the same interfaces over implementations.  

> The package is a part of the general [`ubpe`](https://github.com/Scurrra/ubpe) package, where I divided general import and implementations, because I'm planning to provide other implementations as well. So the package should not be directly installed. Please, use `pip install ubpe[cython]` instead.