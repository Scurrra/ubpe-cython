# distutils: language = c++

include "ubpe_classic.pyx"
include "ubpe.pyx"

__all__ = [
    "UBPEClassic",
    "UBPE"
]

UBPEClassic = {
    int: UbpeClassicInt, 
    str: UbpeClassicChar
}

UBPE = {
    int: UbpeInt,
    str: UbpeChar
}
