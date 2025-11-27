__version__ = "0.1.0"

__all__ = [
    "UBPEClassic",
    "UBPE"
]

from .ubpe_classic import UbpeClassicInt, UbpeClassicChar
from .ubpe import UbpeInt, UbpeChar

UBPEClassic = {
    int: UbpeClassicInt, 
    str: UbpeClassicChar
}

UBPE = {
    int: UbpeInt,
    str: UbpeChar
}