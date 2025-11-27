from setuptools import setup, Extension
from Cython.Build import cythonize
import os

# Determine the correct compiler flag based on the operating system
if os.name == "nt":  # Windows
    cpp_flag = "/std:c++20"
else:  # Linux/macOS
    cpp_flag = "-std=c++20"

extensions = [
    # Extension(
    #     "ubpe_classic",  # e.g., the module file is my_package/my_module.pyx
    #     sources=[
    #         "ubpe_cython/ubpe_classic.pyx",
    #     ],
    #     language="c++",  # Set language to C++
    #     extra_compile_args=[cpp_flag],  # Add the specific C++20 flag
    # ),
    # Extension(
    #     "ubpe",  # e.g., the module file is my_package/my_module.pyx
    #     sources=[
    #         "ubpe_cython/ubpe.pyx",
    #     ],
    #     language="c++",  # Set language to C++
    #     extra_compile_args=[cpp_flag],  # Add the specific C++20 flag
    # ),
    Extension(
        "ubpe_cython",  # e.g., the module file is my_package/my_module.pyx
        sources=[
            "ubpe_cython/ubpe_classic.pyx", "ubpe_cython/ubpe.pyx", "ubpe_cython/__init__.py",
        ],
        language="c++",  # Set language to C++
        extra_compile_args=[cpp_flag],  # Add the specific C++20 flag
    )
]

setup(
    name="ubpe_cython",
    version="0.1.0",
    ext_modules=cythonize(extensions, build_dir="build", annotate=True),
    # ... other package metadata
)
